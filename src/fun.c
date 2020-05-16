
#include "stm32f4xx.h"
#include "core_cm4.h"
#include "core_cmFunc.h"
#include "stm32f4xx_iwdg.h"

#include "fun.h"

void disable_irq(void)
{
    __disable_irq();
}

void enable_irq(void)
{
    __enable_irq();
}

void IWDG_config(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_32);       //32KHz/32=1KHz

    IWDG_SetReload(2000);
}

void watchdog_enable(void)
{
    IWDG_Enable();
}

void watchdog_feed(void)
{
    IWDG_ReloadCounter();
}





