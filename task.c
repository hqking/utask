/**
 * @file   task.c
 * @author  <hqking@gmail.com>
 * @date   Sun Mar 17 15:47:13 2013
 * 
 * @brief  
 * 
 * 
 */
#include <stdlib.h>
#include "assert.h"
#include "queue.h"
#include "task.h"

SLIST_HEAD(listHead, tcb);
static struct listHead g_ready = SLIST_HEAD_INITIALIZER(g_ready);
static struct listHead g_delayed = SLIST_HEAD_INITIALIZER(g_delayed);
static struct listHead g_pending = SLIST_HEAD_INITIALIZER(g_pending);
static struct tcb *g_nextTask;

/**
 * 
 * 
 * @param after 
 * @param before 
 * 
 * @return 
 */
static time_t tickLeft(time_t after, time_t before)
{
	S32 left;
	
	left = (S32)(after - before);
	
	if (left < 0L)
		left = 0L;
		
	return left;
}

/**
 * 
 * 
 * @param tsk1 
 * @param tsk2 
 * 
 * @return 
 */
static int taskCmp(struct tcb *tsk1, struct tcb *tsk2)
{
	assert(tsk1);
	assert(tsk2);

	if (tsk1->until == tsk2->until) {
		return tsk1->priority - tsk2->priority;

	} else {
		if (tickLeft(tsk1->until, tsk2->until) > 0) { /* task2 is early */
			if (tsk2->priority > tsk1->priority
			    || tickLeft(tsk2->until + tsk2->duration, tsk1->until))
				return -1;
		} else {		/* task1 is early */
			if (tsk1->priority > tsk2->until
			    || tickLeft(tsk1->until + tsk1->duration, tsk2->until))
				return 1;
		}
	}
}

/** 
 * 
 * 
 */
static void nothing(void)
{

}

/** 
 * 
 * 
 * 
 * @return 
 */
static struct tcb * findNextTask(void)
{
	struct tcb *next;
	struct tcb *prev;
	struct tcb *task;
	time_t completeTime;
	static struct tcb dummy = {
		.action = nothing,
		.circle = TASK_WAKE_INTERVAL,
		.duration = 0,
		.priority = -128,
	};
	
	if (SLIST_EMPTY(&g_delayed))
		return &dummy;

	next = SLIST_FIRST(&g_delayed);
	completeTime = next->until + next->duration;
	prev = NULL;
	task = next;

	while (task = SLIST_NEXT(task, entries)) {
		if (task->priority > next->priority
		    && tickLeft(next->until, completeTime) == 0) {
			next = task;
			completeTime = next->until + next->duration;
			break;
		} else {
			prev = task;
		}
	}
	
	if (prev)
		SLIST_NEXT(prev, entires) = SLIST_NEXT(next, entries);
	else
		SLIST_REMOVE_HEAD(&g_delayed, entires);

	next->queue = TASK_NONE;

	return next;
}

/**
 *
 * 
 * @param task 
 */
static void addReadyQueue(struct tcb *task)
{
	struct tcb *tp;
	struct tcb *prev;

	task->queue = TASK_READY;

	prev = NULL;
	SLIST_FOREACH(tp, &g_ready, entries) {
		if (tp->priority < task->priority)
			break;
		
		prev = tp;
	}
	
	if (prev) {
		SLIST_INSERT_AFTER(prev, task, entries);
	} else {
		SLIST_INSERT_HEAD(&g_ready, task, entries);

		task->until = taskGetTick();
		if (taskCmp(g_nextTask, task) < 0) {
			/* waken cpu from idle hook */
		}
	}
}

/**
 * 
 * 
 * @param task 
 */
static void addDelayedQueue(struct tcb *task)
{
	struct tcb *prev;

	task->queue = TASK_DELAYED;

	prev = NULL;
	SLIST_FOREACH(tp, &g_delayed, entries) {
		if (tickLeft(tp->until, task->until) == 0)
			break;

		prev = tp;
	}

	if (prev) {
		SLIST_INSERT_AFTER(prev, task, entries);
	} else {
		SLIST_INSERT_HEAD(&g_delayed, task, entries);
	}	
}

/**
 * 
 * 
 * @param nextTask 
 * @param left 
 * 
 * @return 
 */
static struct tcb * taskFindPriority(struct tcb *nextTask, time_t left)
{
	struct tcb *task;
	struct tcb *prev = NULL;

	if (SLIST_EMPTY(&g_ready))
		return NULL;

	SLIST_FOREACH(task, &g_ready, entries)
	{
		if (task->priority >= nextTask->priority
		    || task->duration <= left)
		{
			break;
		} else {
			prev = task;
		}
	}
	
	if (prev)
		SLIST_NEXT(prev, entires) = SLIST_NEXT(task, entries);
	else
		SLIST_REMOVE_HEAD(&g_ready, entires);
				
	task->queue = TASK_NONE;

	return task;
}

/**
 * 
 * 
 * @param nextTask 
 */
static void taskWaitEvent(struct tcb *nextTask)
{
	struct tcb *task = NULL;
	time_t left = 0U;
	
	assert(nextTask);

	left = tickLeft(nextTask->until, taskGetTick());

	task = taskFindPriority(nextTask, left);
	if (task != NULL) {
		task->action();
	} else {
		taskIdleHook(left);
	}
}

/**
 * 
 * 
 * @param task 
 * 
 * @return 
 */
enum TASK_CODE taskRun(struct tcb *task)
{
	if (task == NULL || task->action == NULL)
		return TSKRC_INVALID;

	if (task->queue == TASK_READY)
		return TSKRC_ALREADY;

	if (task->queue == TASK_PENDING) {
		task->until = taskGetTick();
		return TSKRC_PENDING;
	}

	if (task->queue == TASK_DELAYED) {
		SLIST_REMOVE(&g_delayed, task, tcb, entires);
	}

	addReadyQueue(task);

	return TSKRC_OK;
}

/**
 * 
 * 
 * @param task 
 * @param delay 
 * 
 * @return 
 */
enum TASK_CODE taskDelayedRun(struct tcb *task, time_t delay)
{
	if (task == NULL || task->action == NULL || delay == 0)
		return TSKRC_INVALID;

	if (task->queue == TASK_DELAYED || task->queue == TASK_READY)
		return TSKRC_ALREADY;

	task->until = taskGetTick() + delay;

	if (task->queue == TASK_PENDING) {
		return TSKRC_PENDING;
	}

	addDelayedQueue(task);

	if (taskCmp(g_nextTask, task) < 0) {
		/* waken cpu from idle hook */		
	}

	return TSKRC_OK;
}

/**
 * 
 * 
 * @param task 
 * 
 * @return 
 */
enum TASK_CODE taskPause(struct tcb *task)
{
	if (task == NULL || task->action == NULL)
		return TSKRC_INVALID;

	if (task->queue == TASK_PENDING) {
		return TSKRC_ALREADY;

	} else if (task->queue == TASK_READY) {
		task->until = taskGetTick();
		SLIST_REMOVE(&g_ready, task, tcb, entries);

	} else if (task->queue == TASK_DELAYED) {
		SLIST_REMOVE(&g_delayed, task, tcb, entires);
	}

	task->queue = TASK_PENDING;

	SLIST_INSERT_HEAD(&g_pending, task, entries);

	return TSKRC_OK;
}

/**
 * 
 * 
 * @param task 
 * 
 * @return 
 */
enum TASK_CODE taskResume(struct tcb *task)
{
	if (task == NULL || task->action == NULL)
		return TSKRC_INVALID;

	if (task->queue != TASK_PENDING)
		return TSKRC_INVALID;

	SLIST_REMOVE(&g_pending, task, tcb, entries);

	if (task->circle > 0) {	/* not correct */
		addDelayedQueue(task);
	}

	return TSKRC_OK;
}

/**
 * 
 * 
 */
void taskSchedule(void)
{
	while (1) {
		g_nextTask = findNextTask();
		taskWaitEvent(g_nextTask);

		if (tickLeft(g_nextTask->unitl, taskGetTick()) == 0) {
			g_nextTask->action();

			if (g_nextTask->circle > 0) {
				g_nextTask->until += g_nextTask->circle;
				addDelayedQueue(g_nextTask);
			}
		}
	}
}

/**
 * 
 * 
 */
void taskInit(void)
{

}
