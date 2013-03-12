/**
 * @copyright Copyright (c) 1993~2015  ZheJiang SUPCON Technology CO., LTD.
 *                          All rights reserved.
 * @file     
 * @brief    
 * @author   
 * @date     
 * @version  
 *
 * @par ��ϸ����:
 * ��ϸ����..............
 *
 * @par �޶���¼:
 * -# ʱ�䣬�޶��ߣ��޶�ԭ��
 * -# 2012/03/19��ĳĳĳ, ��������������������������
 *
 * @todo
 * - �˴�����δ��ɵ㣬��д����
 *
 * @warning
 * - �˴����ñ���ע��ľ�ʾ�㣺��д����
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
#define TASK_PERIOD_RESERVE		50U		/* ������������ȵĿ�����5us */

#define TASK_PENDING			0x80U

SLIST_HEAD(listHead, tcb);
static struct listHead g_ready = SLIST_HEAD_INITIALIZER(g_ready);
static struct listHead g_delayed = SLIST_HEAD_INITIALIZER(g_delayed);

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

/**
 * @details			������һ����Ҫִ�е���ʱ����
 * 					�˺�����Ҫʹ��ȫ�ֱ������һ����ж�������
 * 					���ִ��ʱ�䲻�������Կ��ǰ���������Lockס���������Լ��ٸ��־ֲ�������ʹ��
 *
 */
static struct tcb * findNextTask(void)
{
	struct tcb *next;
	time_t fcompleteTime;
	static struct tcb dummy = {
		.action = NULL,
		.circle = TASK_WAKE_INTERVAL,
		.duration = 0,
		.priority = 0,
	};
	struct tcb *task;
	
	if (SLIST_EMPTY(&g_delayed))
		return &dummy;
	
	task = SLIST_FIRST(&g_delayed);
	completeTime = task->until + task->duration;
	
	next = task;
	while (next = SLIST_NEXT(next, entries)) {
		/* lower or equal priority, ignored */
		if (next->priority <= task->priority)
			continue;
		
		/* start after current complete time, ignored */
		if (tickLeft(task->until, completeTime) > TASK_PERIOD_RESERVE)
			continue;
			
		task = next;
		completeTime = task->until + task->duration;
	}
	
	return completeTime;
}

/**
 * @details			�������ȼ������Ƿ���Ҫִ�У������Ҫִ�е��������ȼ�������һ����ʱ����
 * 					���������ִ��ʱ��
 *
 * @param left		����ִ��ʱ��
 *
 * @return			�������������û��������Ҫִ�У��򷵻�PRIORITY_TASK_COUNT
 */
static struct tcb * taskFindPriority(time_t left)
{
	struct tcb *task;
	struct tcb *prev = NULL;
	
	SLIST_FOREACH(task, &g_ready, entries)
	{
		if (task->state & TASK_PENDING == 0U
		    && task->priority > g_nextPriority
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
 * @details			�ȴ�ʱ�䴥��������ִ�У�����ʱִ�п�������
 */
static void taskWaitEvent(struct tcb *nextTask)
{
	time_t left = 0U;
	struct tcb *task = NULL;
	
	assert(nextTask);

	left = tickLeft(nextTask->until, taskGetTick());
	task = taskFindPriority(left);
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
 * @details       ���񴥷����к���
 *                ����Ӧ������ִ����������Ԥ������״̬
 * @param[in]     void
 *
 * @return        void
 */
void taskRun(struct tcb *task)
{
	assert(task);

	SLIST_INSERT_HEAD(&g_ready, task, entries);
	/* waken cpu from idle hook */
}

void taskDelayedRun(struct tcb *task, time_t t)
{
	assert(task);
	assert(t);

	g_tasks[index].until = taskGetTick() + t;

	SLIST_INSERT_HEAD(&g_delayed, task, entries);
	/* waken cpu from idle hook */
	/* not complete */
	
	findNextTask();
}

/**
 * @details			��ͣ���񣬵������������ܹ����մ�������
 * @param index		��������
 */
void taskPause(struct tcb *task)
{
	assert(task);

	task->state |= TASK_PENDING;
}

/**
 * @details			�ָ��������У�����ͣ�н��յĴ��������ᱻ����ִ��
 * @param index		��������
 */
void taskResume(struct tcb *task)
{
	assert(task);

	task->state &= ~TASK_PENDING;
}

/**
 * @details       ���Ⱥ���
 *                �����г�ʼ��������ִ�У�������������²��˳�
 * @param[in]     void
 *
 * @return        void
 */
void taskSchedule(void)
{
	struct tcb *nextTask;

	while (1) {
		nextTask = findNextTask();
		taskWaitEvent(nextTask->until);

		if (tickLeft(nextTask->unitl, taskGetTick()) == 0) {
			if (nextTask->action)
				nextTask->action();
		}
	}
}

/**
 * @details       ��ʼ���������ģ�飬��Ҫ����ȫ�ֱ����ĳ�ʼ������ȫ�ֱ����Ϸ��Լ��
 *                ��Ҫ�������ӿڵ���ǰʹ��
 * @param[in]     void
 *
 * @return        void
 */
void taskInit(void)
{

}
