/**
 * @file   sysdelay.c
 * @author Yifeng Jin <hqking@gmail.com>
 * @date   Sun Apl  1 10:50:06 2013
 * 
 * @brief  
 * 
 * 
 */
#include "types.h"
#include "systick.h"
#include "delay.h"

static uint_t g_ms_count;

static mdelayCalc(void)
{
	long long count;
	uint_t i;
	uint_t tick;
	uint_t tick_next;
	uint_t test_count;

	test_count = SYSTICK_KHZ;
	do {
		test_count *= 2;
		tick = sysTick();
		for (i = 0; i < test_count; i++) 
			count++;
		tick_next = sysTick();
	} while (tick_next - tick < SYSTICK_KHZ); /* at least 1ms */
	
	g_ms_count = count * SYSTICK_KHZ / (tick_next - tick);
}

void mdelay(int count)
{
	int i, j;
	
	for (j = 0; j < count; j++)
		for (i = 0; i < g_ms_count; i++) 
			;
}

void udelay(count)
{
	int i, j;
	uint_t us_count;
	
	us_count = g_ms_count / 1000;
	
	for (j = 0; j < count; j++)
		for (i = 0; i < us_count; i++) 
			;
}

void sysdelayInit(void)
{
	mdelayCalc();
}
