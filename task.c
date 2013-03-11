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
#include "queue.h"
#include "task.h"

/******************************************************************************
 * static declaration
*******************************************************************************/
#define TASK_LOWEST_PRIORITY	0xffffU
#define TASK_PERIOD_RESERVE		50U		/* 保留给任务调度的开销，5us */

#define TASK_PENDING			0x80U
#define TASK_READY				0x40U
#define TASK_DELAY				0x08U

/******************************************************************************
 * static global words
*******************************************************************************/
static struct tcb g_tasks[PRIORITY_TASK_COUNT];
static int g_nextPriority;		/**< 下一个需要执行任务的优先级 */

SLIST_HEAD(listHead, tcb);
struct listHead g_ready = SLIST_HEAD_INITIALIZER(g_ready);
struct listHead g_delayed = SLIST_HEAD_INITIALIZER(g_delayed);

/******************************************************************************
 * static functions implementation
*******************************************************************************/
/**
 * @details       获取定时器计数值
 *                task模块使用定时器驱动的统一入口，程序其他部分不要直接使用驱动接口
 * @param[in]     void
 *
 * @return        当前的定时器计数值
 */
static time_t taskGetTick(void)
{
	time_t tick = 0U;
	
	//RTI_GetCounter(&tick);
	
	return tick;
}

/**
 * @details       系统重启
 *                task模块调用系统重启功能的统一入口，其他部分程序不要直接使用系统调用
 *                此函数执行后，多少时间重启依赖于系统实现，此函数后不要放置任何应用代码
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
 * @details			查找下一个需要执行的延时任务。
 * 					此函数需要使用全局变量，且会在中断里重入
 * 					如果执行时间不长，可以考虑把整个函数Lock住。这样可以减少各种局部变量的使用
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
 * @details			查找那个空闲任务需要执行，但要满足允许执行时间的要求
 * @param left		允许的执行时间
 * @return			返回相应任务索引，如果没有满足条件的，返回IDLE_TASK_COUNT
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
 * @details			判断延时的任务是否需要执行
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
 * @details			查找优先级任务是否需要执行，如果需要执行的任务优先级高于下一个延时任务，
 * 					则忽略允许执行时间
 *
 * @param left		允许执行时间
 *
 * @return			任务索引，如果没有任务需要执行，则返回PRIORITY_TASK_COUNT
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
 * @details			等待时间触发的任务执行，空闲时执行空闲任务
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
 * @details       任务触发运行函数
 *                将相应的条件执行任务置于预备运行状态
 * @param[in]     void
 *
 * @return        void
 */
void taskRun(enum TASK_FLAG index)
{
	/* LDRA_assert */
	/* index <　PRIORITY_TASK_COUNT */
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
 * @details			暂停任务，但此任务依旧能够接收触发条件
 * @param index		任务索引
 */
void taskPause(enum TASK_FLAG index)
{
	ASSERT(index < PRIORITY_TASK_COUNT);

	g_tasks[index].state |= TASK_PENDING;
}

/**
 * @details			恢复任务运行，在暂停中接收的触发条件会被立即执行
 * @param index		任务索引
 */
void taskResume(enum TASK_FLAG index)
{
	ASSERT(index < PRIORITY_TASK_COUNT);

	g_tasks[index].state &= ~TASK_PENDING;
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
	time_t left;

	while (1)
	{
		taskDelayedFlagSet();
		left = findNextTask();

		taskWaitEvent(left);
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
