
#include "misc.h"
#include "system_stm32f4xx.h"
#include "stm32f4xx_rcc.h"

#include "config.h"
#include "fun.h"
#include "asyn.h"
#include "led.h"

#include "sys_init.h"


static void NVIC_config(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
}

static void RCC_config(void)
{
    SystemInit();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
}

void system_init(void)
{
    RCC_config();
    NVIC_config();

    disable_irq();

    IWDG_config();
    SysTick_init();
    led_init();

    enable_irq();

    WATCHDOG_ENABLE();
    WATCHDOG_FEED();


}


