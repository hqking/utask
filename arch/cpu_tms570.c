#include "std.h"
#include "common.h"
#include "types.h"
#include "rti.h"
#include "rti_pub.h"
#include "task.h"

static bool g_wake;

uint_t sysTick(void)
{
	uint_t tick;

	RTI_GetCounter(&tick);

	return tick;
}

void taskIdleHook(time_t delay)
{
	time_t until;

	g_wake = false;
	until = sysTick() + delay;

	while (tickLeft(until, sysTick())) {
		if (g_wake)
			break;
	}
}

void taskWake(void)
{
	g_wake = true;
}

time_t taskGetTick(void)
{
	return sysTick();
}

void taskLock(void)
{
	/* TODO: disable all int */
}

void taskUnlock(void)
{
	/* TODO: recover all int */
}

void sysTickStart(void)
{

}

void sysTickStop(void)
{

}

void sysTickInit(void)
{
	RTI_Init();
}
