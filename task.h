#ifndef __TASK_H__
#define __TASK_H__

struct tcb {
	const void (*action)(void);
	const time_t circle;
	const time_t duration;
	time_t until;		/**< 延时运行开始时间，基于systime local时间 */
	enum {
		TASK_NONE,
		TASK_READY,
		TASK_DELAYED,
		TASK_PENDING,
	} queue;
	int priority;
	SLIST_ENTRY(tcb) entries;
};

typedef unsigned int			time_t;

/* API */
void taskRun(struct tcb *task);
void taskDelayedRun(struct tcb *task, time_t t);
void taskPause(struct tcb *task);
void taskResume(struct tcb *task);
void taskSchedule(void);
void taskInit(void);

/* user implemented functions */
extern time_t taskGetTick(void);
extern void taskIdleHook(time_t);

#endif /* __TASK_H__ */
