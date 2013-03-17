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

static struct listHead g_pending = SLIST_HEAD_INITIALIZER(g_pending);
static struct listHead g_priority = SLIST_HEAD_INITIALIZER(g_priority);

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
			if (tsk2->priority >= tsk1->priority
			    || tickLeft(tsk1->until, tsk2->until + tsk2->duration))
				return -1;
			else
				return 1;
		} else {		/* task1 is early */
			if (tsk1->priority >= tsk2->until
			    || tickLeft(tsk2->until, tsk1->until + tsk1->duration))
				return 1;
			else
				return -1;
		}
	}
}

/** 
 * 
 * 
 */
static void taskIdle(void)
{
	taskIdleHook(65536);	/* should be the longest time */
}

/**
 * 
 * 
 * @param task 
 */
static void addPriorityQueue(struct tcb *task)
{
	struct tcb *prev;

	task->queue = TASK_PRIORITY;

	prev = NULL;
	SLIST_FOREACH(tp, &g_priority, entries) {
		if (taskCmp(task, tp) > 0)
			break;
		else
			prev = tp;
	}

	if (prev) {
		SLIST_INSERT_AFTER(prev, task, entries);
	} else {
		SLIST_INSERT_HEAD(&g_priority, task, entries);
	}
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
	if (task == NULL || task->action == NULL || delay >= 0)
		return TSKRC_INVALID;

	if (task->queue == TASK_PRIORITY)
		return TSKRC_ALREADY;

	task->until = taskGetTick() + delay;

	if (task->queue == TASK_PENDING) {
		return TSKRC_PENDING;
	}

	addPriorityQueue(task);

	if (taskCmp(SLIST_FIRST(&g_priority), task) < 0) {
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

	} else if (task->queue == TASK_PRIORITY) {
		SLIST_REMOVE(&g_priority, task, tcb, entries);
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
		addPriorityQueue(task);
	}

	return TSKRC_OK;
}

/**
 * 
 * 
 */
void taskSchedule(void)
{
	struct tcb *task;
	time_t left;

	/* lock */
	while (1) {
		task = SLIST_FIRST(&g_priority);
		left = tickLeft(task->until, taskGetTick());

		if (left > 0) {
			/* unlock */
			taskIdleHook(left);
			/* lock */
		} else {
			SLIST_REMOVE_HEAD(&g_priority, entires);
			/* unlock */
			task->action();
			/* lock */

			if (task->circle > 0) {
				task->until += task->circle;
				addPriorityQueue(task);
			}
		}
	}
	/* unlock */
}

/**
 * 
 * 
 */
void taskInit(void)
{
	static struct tcb idleTask = {
		.action = taskIdle,
		.circle = TASK_WAKE_INTERVAL,
		.duration = 0,	/* not correct */
		.priority = -128,
	};

	SLIST_INSERT_HEAD(&g_priority, &idleTask, entires);
}
