
#include "config.h"
#include "fun.h"
#include "sys_init.h"
#include "led.h"

int main(void)
{
    system_init();

    /* Infinite loop */
    while (1)
    {
        WATCHDOG_FEED();

        led_run_proc();
    }
}

