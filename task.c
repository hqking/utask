/**
 * @copyright Copyright (c) 1993~2015  ZheJiang SUPCON Technology CO., LTD.
 *                          All rights reserved.
 * @file     
 * @brief    
 * @author   
 * @date     
 * @version  
 *
 * @par 详细描述:
 * 详细描述..............
 *
 * @par 修订记录:
 * -# 时间，修订者，修订原因
 * -# 2012/03/19，某某某, 例。。。。。。。。。。。。
 *
 * @todo
 * - 此处放置未完成点，可写多行
 *
 * @warning
 * - 此处放置必须注意的警示点：可写多行
 *
 */

/******************************************************************************
 * include
 *******************************************************************************/
#include <stdlib.h>
#include "assert.h"
#include "queue.h"
#include "task.h"

/******************************************************************************
 * static declaration
 *******************************************************************************/
#define TASK_PERIOD_RESERVE		50U		/* 保留给任务调度的开销，5us */

#define TASK_PENDING			0x80U

SLIST_HEAD(listHead, tcb);
static struct listHead g_ready = SLIST_HEAD_INITIALIZER(g_ready);
static struct listHead g_delayed = SLIST_HEAD_INITIALIZER(g_delayed);
static struct tcb *g_nextTask;

/******************************************************************************
 * static functions implementation
 *******************************************************************************/

static time_t tickLeft(time_t after, time_t before)
{
	S32 left;
	
	left = (S32)(after - before);
	
	if (left < 0L)
		left = 0L;
		
	return left;
}

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

static void nothing(void)
{

}

/**
 * @details			查找下一个需要执行的延时任务。
 * 					此函数需要使用全局变量，且会在中断里重入
 * 					如果执行时间不长，可以考虑把整个函数Lock住。这样可以减少各种局部变量的使用
 *
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

	return next;
}

static void addDelayedQueue(struct tcb *task)
{
	struct tcb *prev;

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
 * @details			查找优先级任务是否需要执行，如果需要执行的任务优先级高于下一个延时任务，
 * 					则忽略允许执行时间
 *
 * @param left		允许执行时间
 *
 * @return			任务索引，如果没有任务需要执行，则返回PRIORITY_TASK_COUNT
 */
static struct tcb * taskFindPriority(struct tcb *nextTask)
{
	struct tcb *task;
	struct tcb *prev = NULL;
	time_t left = 0U;
	
	left = tickLeft(nextTask->until, taskGetTick());

	SLIST_FOREACH(task, &g_ready, entries)
	{
		if (task->state & TASK_PENDING == 0U
		    && task->priority > nextTask->priority
		    || task->duration < left - TASK_PERIOD_RESERVE)
		{
			if (prev)
				SLIST_NEXT(prev, entires) = SLIST_NEXT(task, entries);
			else
				SLIST_REMOVE_HEAD(&g_ready, entires);
				
			break;
		} else {
			prev = task;
		}
	}
	
	return task;
}

/**
 * @details			等待时间触发的任务执行，空闲时执行空闲任务
 */
static void taskWaitEvent(struct tcb *nextTask)
{
	struct tcb *task = NULL;
	
	assert(nextTask);

	task = taskFindPriority(nextTask);
	if (task != NULL) {
		task->state &= ~TASK_READY;
			
		task->action();
	} else {
		taskIdleHook(left);
	}
}

/******************************************************************************
 * public functions implementation
 *******************************************************************************/
/**
 * @details       任务触发运行函数
 *                将相应的条件执行任务置于预备运行状态
 * @param[in]     void
 *
 * @return        void
 */
void taskRun(struct tcb *task)
{
	struct tcb *tp;
	struct tcb *prev;

	assert(task);
	assert(task->action);

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

void taskDelayedRun(struct tcb *task, time_t delay)
{
	assert(task);
	assert(task->action);
	assert(delay);

	task->until = taskGetTick() + delay;

	addDelayedQueue(task);

	if (taskCmp(g_nextTask, task) < 0) {
		/* waken cpu from idle hook */		
	}
}

/**
 * @details			暂停任务，但此任务依旧能够接收触发条件
 * @param index		任务索引
 */
void taskPause(struct tcb *task)
{
	assert(task);

	task->state |= TASK_PENDING;
}

/**
 * @details			恢复任务运行，在暂停中接收的触发条件会被立即执行
 * @param index		任务索引
 */
void taskResume(struct tcb *task)
{
	assert(task);

	task->state &= ~TASK_PENDING;
}

/**
 * @details       调度函数
 *                在所有初始化结束后执行，函数正常情况下不退出
 * @param[in]     void
 *
 * @return        void
 */
void taskSchedule(void)
{
	while (1) {
		g_nextTask = findNextTask();
		taskWaitEvent(g_nextTask->until);

		if (tickLeft(g_nextTask->unitl, taskGetTick()) == 0) {
			g_nextTask->action();

			if (g_nextTask->circle > 0)
				addDelayedQueue(g_nextTask);
		}
	}
}

/**
 * @details       初始化任务调度模块，主要负责全局变脸的初始化，和全局变量合法性检查
 *                需要在其他接口调用前使用
 * @param[in]     void
 *
 * @return        void
 */
void taskInit(void)
{

}
