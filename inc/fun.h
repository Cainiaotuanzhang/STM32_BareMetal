#ifndef _FUN_H_
#define _FUN_H_

void disable_irq(void);
void enable_irq(void);
void IWDG_config(void);
void watchdog_enable(void);
void watchdog_feed(void);

#endif /* _FUN_H_ */
