#ifndef _LED_INIT_
#define _LED_INIT_

#define LED_ON      GPIO_ResetBits(GPIOF,GPIO_Pin_9)
#define LED_OFF     GPIO_SetBits(GPIOF,GPIO_Pin_9)

void led_run_proc(void);
void led_init(void);

#endif /* _LED_INIT_ */

