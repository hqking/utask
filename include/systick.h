#ifndef __SYSTICK_H__
#define __SYSTICK_H__

uint_t sysTick(void);
void sysTickInit(void);
void sysTickStart(void);
void sysTickStop(void);

#ifndef SYSTICK_HZ
#error "SYSTICK_HZ should be systick frequency in HZ"
#endif

#define SYSTICK_KHZ			((SYSTICK_HZ) / 1000)
#define SYSTICK_MHZ			((SYSTICK_HZ) / 1000000)

#endif /* __SYSTICK_H__ */
