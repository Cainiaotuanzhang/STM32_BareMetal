
#include "stm32f4xx_gpio.h"
#include "asyn.h"
#include "led.h"

static unsigned long led_run_time = 0;

void led_run_proc(void)
{
    if (jiffies_before(led_run_time) >= 100)
    {
        LED_OFF;
        led_run_time = jiffies_get();
    }
    else if (jiffies_before(led_run_time) >= 50)
    {
        LED_ON;
    }
}


void led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(GPIOF, &GPIO_InitStructure);
}

