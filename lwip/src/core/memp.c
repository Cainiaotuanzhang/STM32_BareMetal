/**
 * @file
 * Dynamic pool memory manager
 *
 * lwIP has dedicated pools for many structures (netconn, protocol control blocks,
 * packet buffers, ...). All these pools are managed here.
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "lwip/opt.h"

#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/raw.h"
#include "lwip/tcp_impl.h"
#include "lwip/igmp.h"
#include "lwip/api.h"
#include "lwip/api_msg.h"
#include "lwip/tcpip.h"
#include "lwip/sys.h"
#include "lwip/timers.h"
#include "lwip/stats.h"
#include "netif/etharp.h"
#include "lwip/ip_frag.h"
#include "lwip/snmp_structs.h"
#include "lwip/snmp_msg.h"
#include "lwip/dns.h"
#include "netif/ppp_oe.h"

#include <string.h>

#if !MEMP_MEM_MALLOC /* don't build if not configured for use in lwipopts.h */

struct memp {
    struct memp *next;
};


/** 没有健全性检查.我们无需在未分配时保留struct memp,
    因此可以节省一些空间并将MEMP_SIZE设置为0.
 */
#define MEMP_SIZE           0
#define MEMP_ALIGN_SIZE(x) (LWIP_MEM_ALIGN_SIZE(x))


/** This array holds the first free element of each pool.
 *  Elements form a linked list. */
static struct memp *memp_tab[MEMP_MAX];

#else /* MEMP_MEM_MALLOC */

#define MEMP_ALIGN_SIZE(x) (LWIP_MEM_ALIGN_SIZE(x))

#endif /* MEMP_MEM_MALLOC */

/** 该数组保存每个池的元素大小 */
static const u16_t memp_sizes[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  LWIP_MEM_ALIGN_SIZE(size),
#include "lwip/memp_std.h"
};

#if !MEMP_MEM_MALLOC /* don't build if not configured for use in lwipopts.h */

/** This array holds the number of elements in each pool. */
static const u16_t memp_num[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc)  (num),
#include "lwip/memp_std.h"
};

/** 这是池(一个大块中的所有池)使用的实际内存. */
static u8_t memp_memory[MEM_ALIGNMENT - 1 
#define LWIP_MEMPOOL(name,num,size,desc) + ( (num) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size) ) )
#include "lwip/memp_std.h"
];


/**
 * 初始化此模块
 * 
 * 将memp_memory放入每种池类型的链接列表中.
 */
//ZHENXIAOBO:第一次循环memp的值是不重要的.所有的memp->next均指向了memp_tab[i],但是memp_tab[i]指向了最后一个mempp
void memp_init(void)
{
    struct memp *memp;
    u16_t i, j;

    /* for every pool: */
    for (i = 0; i < MEMP_MAX; ++i)
    {
        memp_tab[i] = NULL;

        /* 创建一个memp元素的链接列表 */
        for (j = 0; j < memp_num[i]; ++j)
        {
            memp->next = memp_tab[i];
            memp_tab[i] = memp;
            memp = (struct memp *)(void *)((u8_t *)memp + MEMP_SIZE + memp_sizes[i]);
        }
    }
}

/**
 * Get an element from a specific pool.
 *
 * @param type the pool to get an element from
 *
 * the debug version has two more parameters:
 * @param file file name calling this function
 * @param line number of line where this function is called
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
void *memp_malloc(memp_t type)
{
    struct memp *memp;
    SYS_ARCH_DECL_PROTECT(old_level);

    SYS_ARCH_PROTECT(old_level);

    memp = memp_tab[type];

    if (memp != NULL)
    {
        memp_tab[type] = memp->next;
        MEMP_STATS_INC_USED(used, type);
        memp = (struct memp*)(void *)((u8_t*)memp + MEMP_SIZE);
    }
    else
    {
        MEMP_STATS_INC(err, type);
    }

    SYS_ARCH_UNPROTECT(old_level);

    return memp;
}

/**
 * Put an element back into its pool.
 *
 * @param type the pool where to put mem
 * @param mem the memp element to free
 */
void memp_free(memp_t type, void *mem)
{
    struct memp *memp;

    SYS_ARCH_DECL_PROTECT(old_level);

    if (mem == NULL)
    {
        return;
    }

    memp = (struct memp *)(void *)((u8_t*)mem - MEMP_SIZE);

    SYS_ARCH_PROTECT(old_level);

    MEMP_STATS_DEC(used, type);

    memp->next = memp_tab[type];
    memp_tab[type] = memp;

    SYS_ARCH_UNPROTECT(old_level);
}

#endif /* MEMP_MEM_MALLOC */

