#ifndef _APP_TCP_H_
#define _APP_TCP_H_


#define MAX_STRING      256

struct rcev_buf
{
    u16_t   length;
    u8_t    bytes[MAX_STRING];
};


#endif /* _APP_TCP_H_ */

