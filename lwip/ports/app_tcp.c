#include "app_tcp.h"

static err_t app_tcp_poll(void *arg, struct tcp_pcb *pcb)
{
    if (pcb != NULL)
        tcp_abort(pcb);

    return ERR_OK;
}
static err_t app_tcp_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    struct pbuf *q;
    struct rcev_buf *buffer = (struct rcev_buf *)arg;
    char *c;
    int i;
    unsigned char buf_code[4];
    unsigned char dcu_timeover[2];

    if (p != NULL)
    {
        if (!buffer)
        {
            pbuf_free(p);
            return err;
        }

        for (q = p; q != NULL; q = q->next)
        {
            c = q->payload;
            for (i = 0; i < q->len; i++)
            {
                if (buffer->length < MAX_STRING)
                {
                    buffer->bytes[buffer->length++] = c[i];
                }
            }
        }
    }
    else if (err == ERR_OK)
    {
        mem_free(buffer);
        tcp_close(pcb);
    }

    pbuf_free(p);

    return ERR_OK;
}

static void app_tcp_conn_err(void *arg, err_t err)
{
    struct rcev_buf *buffer;
    buffer = (struct rcev_buf *)arg;

    mem_free(buffer);
}

static err_t app_tcp_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    tcp_arg(pcb, mem_calloc(sizeof(struct rcev_buf), 1));
    tcp_err(pcb, app_tcp_conn_err);
    tcp_recv(pcb, app_tcp_recv);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, app_tcp_poll, 10);

    return err;
}

s32 app_tcp_init(void)
{
    struct tcp_pcb *pcb;
    u16_t port = 4090;

    pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, port);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, app_tcp_accept);

    return 0;
}


