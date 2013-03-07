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
#include "task.h"
typedef unsigned int			time_t;

/******************************************************************************
 * static declaration
*******************************************************************************/
#define TICK_ROUND_HALF			0x7fffffffU
#define TICK_ROUND				0xffffffffU

#define TASK_LOWEST_PRIORITY	0xffffU
#define TASK_PERIOD_RESERVE		50U		/* 保留给任务调度的开销，5us */

#define TASK_PENDING			0x80U
#define TASK_READY				0x40U
#define TASK_DELAY				0x08U

struct taskCtlBlk
{
	time_t until;		/**< 延时运行开始时间，基于systime local时间 */
	char state;		/**< 运行状态 */
};

static time_t taskGetTick(void);
static void taskReset(void);
static time_t tickLeft(time_t after, time_t before);
static int taskFindIdle(time_t left);
static void taskDelayedFlagSet(void);
static int taskFindPriority(time_t left);
static void taskWaitEvent(void);
extern void sysReset(void);

/******************************************************************************
 * static global words
*******************************************************************************/
static struct taskCtlBlk g_tasks[PRIORITY_TASK_COUNT];
static int g_nextPriority;		/**< 下一个需要执行任务的优先级 */
static time_t g_nextStart;			/**< 下一个需要执行任务的开始时间 */

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
 * @details			判断任务是否能够在目标时间内执行完毕。
 *
 * @param index		任务索引
 * @param deadLine	目标时间
 * @return
 */
static BOOLEAN taskAhead(int index, time_t deadLine)
{
	BOOLEAN ahead;

	if (tickLeft(deadLine, g_tasks[index].until + g_PriorityTask[index].duration) > TASK_PERIOD_RESERVE)
	{
		ahead = TRUE;
	}
	else
	{
		ahead = FALSE;
	}

	return ahead;
}

/**
 * @details			查找下一个需要执行的延时任务。
 * 					此函数需要使用全局变量，且会在中断里重入
 * 					如果执行时间不长，可以考虑把整个函数Lock住。这样可以减少各种局部变量的使用
 *
 */
static void findNextTask(void)
{
	int index;
	int nextTask;				/**< 局部变量，防止重入时被修改 */
	static U8 reentrant = 0U;	/**< 用于判断是否重入 */
	U8 saved;					/**< 局部变量保存，当前的重入值 */

	/* 严格讲，下面两行需要Lock，但即便被中断了，也没有关系，中断中的计算先于后面的计算 */
	reentrant++;
	saved = reentrant;

	nextTask = PRIORITY_TASK_COUNT;

	for (index = 0U; index < PRIORITY_TASK_COUNT; index++)
	{
		if ((g_tasks[index].state & TASK_DELAY) > 0U)
		{
			nextTask = index;
			break;
		}
	}

	for (index++; index < PRIORITY_TASK_COUNT; index++)
	{
		if ((g_tasks[index].state & TASK_DELAY) > 0U
			&& taskAhead(index, g_tasks[nextTask].until) == TRUE)
		{
			nextTask = index;
		}
	}

	/* lock */
	if (saved == reentrant)
	{
		if (nextTask < PRIORITY_TASK_COUNT)
		{
			g_nextPriority = nextTask;
			g_nextStart = g_tasks[nextTask].until;
		}
		else
		{
			g_nextPriority = TASK_LOWEST_PRIORITY;
			g_nextStart = taskGetTick() + TASK_PERIOD;
		}
	}
	/* unlock */
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
				if (g_PriorityTask[index].circle > 0U)
				{
					g_tasks[index].until += g_PriorityTask[index].circle;
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
static int taskFindPriority(time_t left)
{
	int index;
	
	for (index = 0U; index < PRIORITY_TASK_COUNT; index++)
	{
		if (((g_tasks[index].state & TASK_READY) > 0U)
			&& ((g_tasks[index].state & TASK_PENDING) == 0U))
		{
			if ((index <= g_nextPriority)
				|| (g_PriorityTask[index].duration < left))
			{
				break;
			}
		}
	}
	
	return index;
}

/**
 * @details			等待时间触发的任务执行，空闲时执行空闲任务
 */
static void taskWaitEvent(void)
{
	time_t left = 0U;
	int index = PRIORITY_TASK_COUNT;
	
	while (left = tickLeft(g_nextStart - TASK_PERIOD_RESERVE, taskGetTick()))
	{
		index = taskFindPriority(left);
		if (index < PRIORITY_TASK_COUNT)
		{
			g_tasks[index].state &= ~TASK_READY;
			
			g_PriorityTask[index].action();
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
	while (1)
	{
		taskDelayedFlagSet();
		findNextTask();

		taskWaitEvent();
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
	
	if ((sizeof(g_PriorityTask) / sizeof(struct taskInfo)) != PRIORITY_TASK_COUNT)
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
		if (g_PriorityTask[i].circle > 0U)
		{
			g_tasks[i].state |= TASK_DELAY;
			g_tasks[i].until = now + g_PriorityTask[i].circle;
		}
		else
		{
			g_tasks[i].state &= ~TASK_DELAY;
		}
		
		g_tasks[i].state &= ~TASK_PENDING;

		ASSERT(g_PriorityTask[i].action);
		/* LDRA_assert */
		/* g_PriorityTask[i].action */
		/* LDRA_end */
	}
}
