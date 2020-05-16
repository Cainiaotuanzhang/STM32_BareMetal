#ifndef _CONFIG_H_
#define _CONFIG_H_

#define IWDG_ENABLE     1   //开门狗使能


#if IWDG_ENABLE
#define WATCHDOG_ENABLE()   watchdog_enable()
#define WATCHDOG_FEED()     watchdog_feed()
#else
#define WATCHDOG_ENABLE()
#define WATCHDOG_FEED()
#endif




#endif /* _CONFIG_H_ */
