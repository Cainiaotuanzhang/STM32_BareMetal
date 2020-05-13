/**
 * @file
 * Ethernet Interface Skeleton
 *
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

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include "stm32f4x7_eth.h"
#include "lan8720.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'Z'
#define IFNAME1 'H'

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif);

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
    int i;
    unsigned char mac[6];

    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC hardware address */
    netif->hwaddr[0] = mac[0];
    netif->hwaddr[1] = mac[1];
    netif->hwaddr[2] = mac[2];
    netif->hwaddr[3] = mac[3];
    netif->hwaddr[4] = mac[4];
    netif->hwaddr[5] = mac[5];

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    /* Do whatever else is needed to initialize interface. */
    //硬件的实际初始化.当前STM32F407,STM32F407内置了以太网控制器?ZHENXIAOBO.

    ETH_MACAddressConfig(ETH_MAC_Address0, netif->hwaddr);
    ETH_DMATxDescChainInit(DMATxDscrTab, Tx_Buff, ETH_TXBUFNB);
    ETH_DMARxDescChainInit(DMARxDscrTab, Rx_Buff, ETH_RXBUFNB);

#ifdef CHECKSUM_BY_HARDWARE //使用硬件帧校验
    for (i = 0; i < ETH_TXBUFNB; i++)
    {
        //使能TCP,UDP和ICMP的发送帧校验,TCP,UDP和ICMP的接收帧校验在DMA中配置了
        ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
    }
#endif
    ETH_Start();    //开启MAC和DMA
    return ERR_OK;
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  struct ethernetif *ethernetif = netif->state;
  struct pbuf *q;

  initiate transfer();
  
#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    send data from(q->payload, q->len);
  }

  signal that packet should be sent();

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p, *q;
    u16_t len;
    u32_t i = 0;
    FrameTypeDef frame;

    frame = ETH_Rx_Packet();
    len = frame.length;

    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p != NULL)
    {
        for (q = p; q != NULL; q = q->next)
        {
            memcpy((u8_t *)q->payload, (u8_t *)frame.buffer+i, q->len);
            i += q->len;
        }
    }

    frame.descriptor->Status = ETH_DMARxDesc_OWN;   //设置Rx描述符OWN位,buffer重归DMA
    if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET)
    {
        ETH->DMASR = ETH_DMASR_RBUS;    //重置DMA RBUS位
        ETH->DMARPDR = 0;               //恢复DMA接收
    }
    return p;
}

/**
 当准备好从接口读取数据包时,应调用此函数.
 它使用函数low_level_input()来处理从网络接口实际接收的字节.
 然后确定接收到的数据包的类型,并调用适当的输入函数.
 @param netif此ethernetif的lwip网络接口结构.
 */
static void ethernetif_input(struct netif *netif)
{
    struct eth_hdr *ethhdr;
    struct pbuf *p;

    /* move received packet into a new pbuf */
    p = low_level_input(netif);

    /* no packet could be read, silently ignore this */
    if (p == NULL) return;

    /* 指向以太网报头开头的数据包 */
    ethhdr = p->payload;

    switch (htons(ethhdr->type))
    {
        /* IP or ARP packet? */
        case ETHTYPE_IP:
        case ETHTYPE_ARP:
            /* 完整的数据包发送到tcpip_thread进行处理 */
            if (netif->input(p, netif) != ERR_OK)
            {
                pbuf_free(p);
                p = NULL;
            }
            break;

        default:
            pbuf_free(p);
            p = NULL;
            break;
    }
}

/**
 * 应该在程序开始时调用以设置网络接口.它调用函数low_level_init()进行硬件的实际设置.
 * 该函数应作为参数传递给netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

    /* 我们直接在此处使用etharp_output()保存函数调用,
    如果必须在发送之前进行一些检查(例如,如果链接可用...),
    则可以从其中声明自己的函数调用etharp_output(). */
    netif->output = etharp_output;          //ZHENXIAOBO:IP层发送数据包函数
    netif->linkoutput = low_level_output;   //ZHENXIAOBO:发送ETH包,ARP层调用.

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

