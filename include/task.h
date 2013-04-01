#ifndef __TASK_H__
#define __TASK_H__

#include "queue.h"

enum TASK_CODE {
	TSKRC_OK = 0,
	TSKRC_INVALID = -1,
	TSKRC_ALREADY = -2,
	TSKRC_PENDING = -3,
};

typedef unsigned int			time_t;

struct tcb {
	void (*action)(struct tcb *);
	time_t circle;
	time_t duration;
	time_t until;
	enum {
		TASK_NONE,
		TASK_PRIORITY,
		TASK_PENDING,
	} queue;
	int priority;
	SLIST_ENTRY(tcb) entries;
};

/* API */
#define taskRun(task)	taskDelayedRun(task, 0)
enum TASK_CODE taskDelayedRun(struct tcb *task, time_t t);
enum TASK_CODE taskCircleRun(struct tcb *task, time_t circle);
enum TASK_CODE taskPause(struct tcb *task);
enum TASK_CODE taskResume(struct tcb *task);
void taskSchedule(void);
void taskInit(void);

/* aux functions */
time_t tickLeft(time_t after, time_t before);

/* user implemented functions */
extern time_t taskGetTick(void);
extern void taskIdleHook(time_t);
extern void taskWake(void);
extern void taskLock(void);
extern void taskUnlock(void);

#endif /* __TASK_H__ */
