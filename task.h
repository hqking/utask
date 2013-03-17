#ifndef __TASK_H__
#define __TASK_H__

enum TASK_CODE {
	TSKRC_OK = 0,
	TSKRC_INVALID = -1,
	TSKRC_ALREADY = -2,
	TSKRC_PENDING = -3,
};

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
#define taskRun(task)	taskDelayedRun(task, 0)
enum TASK_CODE taskDelayedRun(struct tcb *task, time_t t);
enum TASK_CODE taskPause(struct tcb *task);
enum TASK_CODE taskResume(struct tcb *task);
void taskSchedule(void);
void taskInit(void);

/* user implemented functions */
extern time_t taskGetTick(void);
extern void taskIdleHook(time_t);

#endif /* __TASK_H__ */
