#ifndef __TASK_H__
#define __TASK_H__

struct tcb
{
	const void (*action)(void);
	const time_t circle;
	const time_t duration;
	time_t until;		/**< 延时运行开始时间，基于systime local时间 */
	char state;		/**< 运行状态 */
	int priority;
	SLIST_ENTRY(tcb) entries;
};

typedef unsigned int			time_t;

void taskRun(enum TASK_FLAG index);
void taskDelayedRun(enum TASK_FLAG index, time_t usec);
void taskPause(enum TASK_FLAG index)j;
void taskResume(enum TASK_FLAG index);
void taskSchedule(void);
void taskInit(void);

#endif /* __TASK_H__ */
