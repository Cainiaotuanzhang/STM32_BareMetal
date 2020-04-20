
#include "app_udp.h"

void UDP_Receive(void *arg, struct udp_pcb *upcb, struct pbuf *p,struct ip_addr *addr, u16_t port)
{
    int i;
    struct pbuf *q;
    char *c;
    struct rcev_buf udp_buffer;

    memset(&udp_buffer, 0, sizeof(struct rcev_buf));
    //udp_destAddr = addr;
    //udp_send_port = port;

    if (p != NULL)
    {
        for (q = p; q != NULL; q = q->next)
        {
            c = q->payload;
            for (i = 0; i < q->len; i++)
            {
                if (udp_buffer.length < MAX_STRING) 
                {
                    udp_buffer.bytes[udp_buffer.length++] = c[i];
                }
            }
        }
        pbuf_free(p);
        //xxx
    }
    else
    {
        pbuf_free(p);
    }
}

void app_udp_init(void)
{
    struct udp_pcb *UdpPcb;

    UdpPcb = udp_new();
    udp_bind(UdpPcb,IP_ADDR_ANY,6000);
    udp_recv(UdpPcb,UDP_Receive,NULL);
}

