
#include "lwip_init.h"

#include "lwip/init.h"

struct netif lwip_netif;    //定义一个全局的网络接口

s32_t my_lwip_init(void)
{
    u8_t buff[4];
    struct ip_addr ip_addr;
    struct ip_addr net_mask;
    struct ip_addr gw_addr;

    buff[0] = 192;
    buff[1] = 168;
    buff[2] = 1;
    buff[3] = 18;
    IP4_ADDR(&ip_addr, buff[0], buff[1], buff[2], buff[3]);     //设置IP地址格式

    buff[0] = 255;
    buff[1] = 255;
    buff[2] = 255;
    buff[3] = 0;
    IP4_ADDR(&net_mask, buff[0], buff[1], buff[2], buff[3]);    //设置子网掩码格式

    buff[0] = 192;
    buff[1] = 168;
    buff[2] = 1;
    buff[3] = 1;
    IP4_ADDR(&gw_addr, buff[0], buff[1], buff[2], buff[3]);     //设置默认网关格式


    lwip_init();    //lwip内核初始化
    if (NULL == netif_add(&lwip_netif, &ip_addr, &net_mask, &gw_addr, NULL, NULL, NULL))    //ZHENXIAOBO:最后2个参数待补充.
    {
        return -1;
    }
    netif_set_default(&lwip_netif); //设置默认网口
    netif_set_up(&lwip_netif);      //开启网口

    return 0;
}

