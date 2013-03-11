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
#include "queue.h"
#include "task.h"

/******************************************************************************
 * static declaration
*******************************************************************************/
#define TASK_LOWEST_PRIORITY	0xffffU
#define TASK_PERIOD_RESERVE		50U		/* ������������ȵĿ�����5us */

#define TASK_PENDING			0x80U
#define TASK_READY				0x40U
#define TASK_DELAY				0x08U

/******************************************************************************
 * static global words
*******************************************************************************/
static struct tcb g_tasks[PRIORITY_TASK_COUNT];
static int g_nextPriority;		/**< ��һ����Ҫִ����������ȼ� */

SLIST_HEAD(listHead, tcb);
struct listHead g_ready = SLIST_HEAD_INITIALIZER(g_ready);
struct listHead g_delayed = SLIST_HEAD_INITIALIZER(g_delayed);

/******************************************************************************
 * static functions implementation
*******************************************************************************/
/**
 * @details       ��ȡ��ʱ������ֵ
 *                taskģ��ʹ�ö�ʱ��������ͳһ��ڣ������������ֲ�Ҫֱ��ʹ�������ӿ�
 * @param[in]     void
 *
 * @return        ��ǰ�Ķ�ʱ������ֵ
 */
static time_t taskGetTick(void)
{
	time_t tick = 0U;
	
	//RTI_GetCounter(&tick);
	
	return tick;
}

/**
 * @details       ϵͳ����
 *                taskģ�����ϵͳ�������ܵ�ͳһ��ڣ��������ֳ���Ҫֱ��ʹ��ϵͳ����
 *                �˺���ִ�к󣬶���ʱ������������ϵͳʵ�֣��˺�����Ҫ�����κ�Ӧ�ô���
 * @param[in]     void
 *
 * @return        void
 */
static void taskReset(void)
{
#if defined DEBUG || defined DEBUG_JTAG
	ASSERT(0);
#else
	sysReset();
#endif
}

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
static time_t findNextTask(void)
{
	struct tcb *task;
	struct tcb *next;
	time_t completeTime;
	
	if (SLIST_EMPTY(&g_delayed))
		return taskGetTick() + TASK_WAKE_INTERVAL;
	
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
 * @details			�����Ǹ�����������Ҫִ�У���Ҫ��������ִ��ʱ���Ҫ��
 * @param left		�����ִ��ʱ��
 * @return			������Ӧ�������������û�����������ģ�����IDLE_TASK_COUNT
 */
static int taskFindIdle(time_t left)
{
	static int idleIndex;
	int rc;

	if (g_IdleTask[idleIndex].duration >= left)
	{
		rc = IDLE_TASK_COUNT;
	}
	else
	{
		rc = idleIndex;
	}

	idleIndex++;
	if (idleIndex >= IDLE_TASK_COUNT)
	{
		idleIndex = 0U;
	}

	return rc;
}

/**
 * @details			�ж���ʱ�������Ƿ���Ҫִ��
 */
static void taskDelayedFlagSet(void)
{
	int index;
	time_t now;

	now = taskGetTick();

	for (index = 0U; index < PRIORITY_TASK_COUNT; index++)
	{
		if ((g_tasks[index].state & TASK_DELAY) > 0U)
		{
			if (tickLeft(g_tasks[index].until, now) == 0U)
			{
				g_tasks[index].state |= TASK_READY;
				if (g_tasks[index].circle > 0U)
				{
					g_tasks[index].until += g_tasks[index].circle;
				}
				else
				{
					g_tasks[index].state &= ~TASK_DELAY;
				}
			}
		}
	}
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
	
	SLIST_FOREACH(task, &g_ready, entries)
	{	
		if (((task->state & TASK_READY) > 0U)
			&& ((task->state & TASK_PENDING) == 0U))
		{
			if ((task->priority <= g_nextPriority)
				|| (task->duration < left - TASK_PERIOD_RESERVE))
			{
				break;
			}
		}
	}
	
	return task;
}

/**
 * @details			�ȴ�ʱ�䴥��������ִ�У�����ʱִ�п�������
 */
static void taskWaitEvent(time_t deadLine)
{
	time_t left = 0U;
	struct tcb *task = NULL;
	
	while (left = tickLeft(deadLine, taskGetTick()))
	{
		task = taskFindPriority(left);
		if (task != NULL)
		{
			task->state &= ~TASK_READY;
			
			task->action();
		}
		else	/* idle time */
		{
			index = taskFindIdle(left);
			if (index < IDLE_TASK_COUNT)
			{
				g_IdleTask[index].action();
			}
		}
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
void taskRun(enum TASK_FLAG index)
{
	/* LDRA_assert */
	/* index <��PRIORITY_TASK_COUNT */
	/* LDRA_end */
	g_tasks[index].state |= TASK_READY;
}

void taskDelayedRun(enum TASK_FLAG index, time_t usec)
{
	/* LDRA_assert */
	/* index < PRIORITY_TASK_COUNT */
	/* LDRA_end */

	g_tasks[index].state |= TASK_DELAY;
	g_tasks[index].until = taskGetTick() + (usec * 10U);

	findNextTask();
}

/**
 * @details			��ͣ���񣬵������������ܹ����մ�������
 * @param index		��������
 */
void taskPause(enum TASK_FLAG index)
{
	ASSERT(index < PRIORITY_TASK_COUNT);

	g_tasks[index].state |= TASK_PENDING;
}

/**
 * @details			�ָ��������У�����ͣ�н��յĴ��������ᱻ����ִ��
 * @param index		��������
 */
void taskResume(enum TASK_FLAG index)
{
	ASSERT(index < PRIORITY_TASK_COUNT);

	g_tasks[index].state &= ~TASK_PENDING;
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
	time_t left;

	while (1)
	{
		taskDelayedFlagSet();
		left = findNextTask();

		taskWaitEvent(left);
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
	int i;
	time_t now;
	
	if ((sizeof(g_tasks) / sizeof(struct taskInfo)) != PRIORITY_TASK_COUNT)
	{
		TRACE("ERROR! priority task definition is not compatible\n");
		ASSERT(0);
		/* LDRA_assert */
		/* 0 */
		/* LDRA_end */
	}
	
	if ((sizeof(g_IdleTask) / sizeof(struct taskInfo)) != IDLE_TASK_COUNT)
	{
		TRACE("ERROR! idle task definition is not compatible\n");
		ASSERT(0);
		/* LDRA_assert */
		/* 0 */
		/* LDRA_end */
	}
	
	now = taskGetTick();

	for (i = 0U; i < PRIORITY_TASK_COUNT; i++)
	{
		if (g_tasks[i].circle > 0U)
		{
			g_tasks[i].state |= TASK_DELAY;
			g_tasks[i].until = now + g_tasks[i].circle;
		}
		else
		{
			g_tasks[i].state &= ~TASK_DELAY;
		}
		
		g_tasks[i].state &= ~TASK_PENDING;

		ASSERT(g_tasks[i].action);
		/* LDRA_assert */
		/* g_tasks[i].action */
		/* LDRA_end */
	}
}
