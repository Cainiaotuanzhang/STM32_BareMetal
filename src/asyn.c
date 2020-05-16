
#include "stm32f4xx_rcc.h"
#include "core_cm4.h"

#include "asyn.h"

unsigned int jiffies;

void SysTick_Handler(void)
{
    ++jiffies;
}

void SysTick_init(void)
{
    RCC_ClocksTypeDef RCC_Clocks;

    jiffies = 0;
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency / 100);
}

unsigned int jiffies_get(void)
{
    unsigned int jiffies_tmp;

    jiffies_tmp = jiffies;

    return jiffies_tmp;
}

unsigned int jiffies_before(unsigned int jiffies_bef)
{
    unsigned int jiffies_tmp;

    jiffies_tmp = jiffies;

    if (jiffies_tmp >= jiffies_bef)
    {
        return jiffies_tmp - jiffies_bef;
    }
    else
    {
        jiffies_tmp = jiffies + (~jiffies_bef);
        return jiffies + (~jiffies_bef);
    }
}



