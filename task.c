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
#include "task.h"
typedef unsigned int			time_t;

/******************************************************************************
 * static declaration
*******************************************************************************/
#define TICK_ROUND_HALF			0x7fffffffU
#define TICK_ROUND				0xffffffffU

#define TASK_LOWEST_PRIORITY	0xffffU
#define TASK_PERIOD_RESERVE		50U		/* ������������ȵĿ�����5us */

#define TASK_PENDING			0x80U
#define TASK_READY				0x40U
#define TASK_DELAY				0x08U

struct taskCtlBlk
{
	time_t until;		/**< ��ʱ���п�ʼʱ�䣬����systime localʱ�� */
	char state;		/**< ����״̬ */
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
static int g_nextPriority;		/**< ��һ����Ҫִ����������ȼ� */
static time_t g_nextStart;			/**< ��һ����Ҫִ������Ŀ�ʼʱ�� */

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
 * @details			�ж������Ƿ��ܹ���Ŀ��ʱ����ִ����ϡ�
 *
 * @param index		��������
 * @param deadLine	Ŀ��ʱ��
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
 * @details			������һ����Ҫִ�е���ʱ����
 * 					�˺�����Ҫʹ��ȫ�ֱ������һ����ж�������
 * 					���ִ��ʱ�䲻�������Կ��ǰ���������Lockס���������Լ��ٸ��־ֲ�������ʹ��
 *
 */
static void findNextTask(void)
{
	int index;
	int nextTask;				/**< �ֲ���������ֹ����ʱ���޸� */
	static U8 reentrant = 0U;	/**< �����ж��Ƿ����� */
	U8 saved;					/**< �ֲ��������棬��ǰ������ֵ */

	/* �ϸ񽲣�����������ҪLock�������㱻�ж��ˣ�Ҳû�й�ϵ���ж��еļ������ں���ļ��� */
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
 * @details			�������ȼ������Ƿ���Ҫִ�У������Ҫִ�е��������ȼ�������һ����ʱ����
 * 					���������ִ��ʱ��
 *
 * @param left		����ִ��ʱ��
 *
 * @return			�������������û��������Ҫִ�У��򷵻�PRIORITY_TASK_COUNT
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
 * @details			�ȴ�ʱ�䴥��������ִ�У�����ʱִ�п�������
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
	while (1)
	{
		taskDelayedFlagSet();
		findNextTask();

		taskWaitEvent();
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
