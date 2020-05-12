/**
 * @file
 * Dynamic memory manager
 *
 * This is a lightweight replacement for the standard C library malloc().
 *
 * If you want to use the standard C library malloc() instead, define
 * MEM_LIBC_MALLOC to 1 in your lwipopts.h
 *
 * To let mem_malloc() use pools (prevents fragmentation and is much faster than
 * a heap but might waste some memory), define MEM_USE_POOLS to 1, define
 * MEM_USE_CUSTOM_POOLS to 1 and create a file "lwippools.h" that includes a list
 * of pools like this (more pools can be added between _START and _END):
 *
 * Define three pools with sizes 256, 512, and 1512 bytes
 * LWIP_MALLOC_MEMPOOL_START
 * LWIP_MALLOC_MEMPOOL(20, 256)
 * LWIP_MALLOC_MEMPOOL(10, 512)
 * LWIP_MALLOC_MEMPOOL(5, 1512)
 * LWIP_MALLOC_MEMPOOL_END
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
 *         Simon Goldschmidt
 *
 */

#include "lwip/opt.h"

#if !MEM_LIBC_MALLOC /* don't build if not configured for use in lwipopts.h */

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/err.h"

#include <string.h>

/* lwIP replacement for your libc malloc() */

/**
 * 堆由这种类型的结构列表组成.
 * 这不必对齐,因为要获取其大小,
 * 我们仅使用宏SIZEOF_STRUCT_MEM,它会自动对齐.
 */
struct mem {
    /** 下一个结构体的索引 */
    mem_size_t next;
    /** 前一个结构体的索引 */
    mem_size_t prev;
    /** 1: 该区域被使用; 0: 该区域未被使用 */
    u8_t used;
};

/** 所有被分配的块都是MIN_SIZE大小.MIN_SIZE可根据需求不同被覆写,较小的值节省空间,较大的值可防止块太小而使RAM碎片过多 */
#ifndef MIN_SIZE
#define MIN_SIZE             12
#endif /* MIN_SIZE */

/* 一些对齐宏:我们在此处定义它们以获得更好的源代码布局 */
#define MIN_SIZE_ALIGNED     LWIP_MEM_ALIGN_SIZE(MIN_SIZE)
#define SIZEOF_STRUCT_MEM    LWIP_MEM_ALIGN_SIZE(sizeof(struct mem))
#define MEM_SIZE_ALIGNED     LWIP_MEM_ALIGN_SIZE(MEM_SIZE)

/** 如果要将堆重定位到外部存储器,只需定义
    LWIP_RAM_HEAP_POINTER作为指向该位置的空指针.
    如果是这样,请确保该位置的内存足够大(有关如何计算该空间，请参见下文).
 */
#ifndef LWIP_RAM_HEAP_POINTER
/** 堆. 我们最后需要一个struct mem和一些空间用于对齐 *///ZHENXIAOBO:定义与注释不符合.看下.
u8_t ram_heap[MEM_SIZE_ALIGNED + (2*SIZEOF_STRUCT_MEM) + MEM_ALIGNMENT];
#define LWIP_RAM_HEAP_POINTER ram_heap
#endif /* LWIP_RAM_HEAP_POINTER */

/** 指向堆的指针(ram_heap):为了对齐,ram现在是指针而不是数组 */
static u8_t *ram;

/** the last entry, always unused! */
static struct mem *ram_end;

/** 指向最低空闲块的指针,用于更快的搜索 */
static struct mem *lfree;

/** 并发访问保护 */
#if !NO_SYS
static sys_mutex_t mem_mutex;
#endif

#if LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT

static volatile u8_t mem_free_count;

/* Allow mem_free from other (e.g. interrupt) context */
#define LWIP_MEM_FREE_DECL_PROTECT()  SYS_ARCH_DECL_PROTECT(lev_free)
#define LWIP_MEM_FREE_PROTECT()       SYS_ARCH_PROTECT(lev_free)
#define LWIP_MEM_FREE_UNPROTECT()     SYS_ARCH_UNPROTECT(lev_free)
#define LWIP_MEM_ALLOC_DECL_PROTECT() SYS_ARCH_DECL_PROTECT(lev_alloc)
#define LWIP_MEM_ALLOC_PROTECT()      SYS_ARCH_PROTECT(lev_alloc)
#define LWIP_MEM_ALLOC_UNPROTECT()    SYS_ARCH_UNPROTECT(lev_alloc)

#else /* LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

/* Protect the heap only by using a semaphore */
#define LWIP_MEM_FREE_DECL_PROTECT()
#define LWIP_MEM_FREE_PROTECT()    sys_mutex_lock(&mem_mutex)       //ZHENXIAOBO: NO_SYS==1 && LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT==0,mem_mutex的处理.
#define LWIP_MEM_FREE_UNPROTECT()  sys_mutex_unlock(&mem_mutex)
/* mem_malloc is protected using semaphore AND LWIP_MEM_ALLOC_PROTECT */
#define LWIP_MEM_ALLOC_DECL_PROTECT()
#define LWIP_MEM_ALLOC_PROTECT()
#define LWIP_MEM_ALLOC_UNPROTECT()

#endif /* LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */


/**
 * "Plug holes" by combining adjacent empty struct mems.
 * After this function is through, there should not exist
 * one empty struct mem pointing to another empty struct mem.
 *
 * @param mem this points to a struct mem which just has been freed
 * @internal this function is only called by mem_free() and mem_trim()
 *
 * This assumes access to the heap is protected by the calling function
 * already.
 */
static void
plug_holes(struct mem *mem)
{
  struct mem *nmem;
  struct mem *pmem;

  LWIP_ASSERT("plug_holes: mem >= ram", (u8_t *)mem >= ram);
  LWIP_ASSERT("plug_holes: mem < ram_end", (u8_t *)mem < (u8_t *)ram_end);
  LWIP_ASSERT("plug_holes: mem->used == 0", mem->used == 0);

  /* plug hole forward */
  LWIP_ASSERT("plug_holes: mem->next <= MEM_SIZE_ALIGNED", mem->next <= MEM_SIZE_ALIGNED);

  nmem = (struct mem *)(void *)&ram[mem->next];
  if (mem != nmem && nmem->used == 0 && (u8_t *)nmem != (u8_t *)ram_end) {
    /* if mem->next is unused and not end of ram, combine mem and mem->next */
    if (lfree == nmem) {
      lfree = mem;
    }
    mem->next = nmem->next;
    ((struct mem *)(void *)&ram[nmem->next])->prev = (mem_size_t)((u8_t *)mem - ram);
  }

  /* plug hole backward */
  pmem = (struct mem *)(void *)&ram[mem->prev];
  if (pmem != mem && pmem->used == 0) {
    /* if mem->prev is unused, combine mem and mem->prev */
    if (lfree == mem) {
      lfree = pmem;
    }
    pmem->next = mem->next;
    ((struct mem *)(void *)&ram[mem->next])->prev = (mem_size_t)((u8_t *)pmem - ram);
  }
}

/**
 * 将堆清零并初始化开始,结束和最低释放.//ZHENXIAOBO:回头重新理解下.
 */
void mem_init(void)
{
    struct mem *mem;

    /* 对齐堆 */ //ZHENXIAOBO:对齐的算法了解清楚了,看下如果堆指针不同情况下,针对不同对齐方式,ram指针是怎样的值.
    ram = (u8_t *)LWIP_MEM_ALIGN(LWIP_RAM_HEAP_POINTER);

    /* 初始化堆的开始 */
    mem = (struct mem *)(void *)ram;
    mem->next = MEM_SIZE_ALIGNED;
    mem->prev = 0;
    mem->used = 0;

    /* 初始化堆的结束 */
    ram_end = (struct mem *)(void *)&ram[MEM_SIZE_ALIGNED];
    ram_end->used = 1;
    ram_end->next = MEM_SIZE_ALIGNED;
    ram_end->prev = MEM_SIZE_ALIGNED;

    /* 初始化最低可用指针以指向堆的开始 */
    lfree = (struct mem *)(void *)ram;
}

/**
 * Put a struct mem back on the heap
 *
 * @param rmem is the data portion of a struct mem as returned by a previous
 *             call to mem_malloc()
 */
void mem_free(void *rmem)
{
    struct mem *mem;

    LWIP_MEM_FREE_DECL_PROTECT();

    if (rmem == NULL)
    {
        return;
    }

    if ((u8_t *)rmem < (u8_t *)ram || (u8_t *)rmem >= (u8_t *)ram_end)
    {
        SYS_ARCH_DECL_PROTECT(lev);

        /* 保护内存统计信息免受并发访问. *///ZHENXIAOBO:这部分的原理了解清楚,后面可以借用.
        SYS_ARCH_PROTECT(lev);
        MEM_STATS_INC(illegal);
        SYS_ARCH_UNPROTECT(lev);

        return;
    }

    /* 保护堆免受并发访问 */
    LWIP_MEM_FREE_PROTECT();

    /* 获取相应的struct mem ... */
    mem = (struct mem *)(void *)((u8_t *)rmem - SIZEOF_STRUCT_MEM);

    /* ... 处于已使用状态 ... */
    LWIP_ASSERT("mem_free: mem->used", mem->used);

    /* ... 现在是未使用. */
    mem->used = 0;

    if (mem < lfree)
    {
        /* 新释放的结构现在是最低的 */
        lfree = mem;
    }

    MEM_STATS_DEC_USED(used, mem->next - (mem_size_t)(((u8_t *)mem - ram)));

    /* finally, see if prev or next are free also */
    plug_holes(mem);

#if LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
    mem_free_count = 1;
#endif /* LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */

    LWIP_MEM_FREE_UNPROTECT();
}

/**
 * Shrink memory returned by mem_malloc().
 *
 * @param rmem pointer to memory allocated by mem_malloc the is to be shrinked
 * @param newsize required size after shrinking (needs to be smaller than or
 *                equal to the previous size)
 * @return for compatibility reasons: is always == rmem, at the moment
 *         or NULL if newsize is > old size, in which case rmem is NOT touched
 *         or freed!
 */
void *mem_trim(void *rmem, mem_size_t newsize)
{
  mem_size_t size;
  mem_size_t ptr, ptr2;
  struct mem *mem, *mem2;
  /* use the FREE_PROTECT here: it protects with sem OR SYS_ARCH_PROTECT */
  LWIP_MEM_FREE_DECL_PROTECT();

  /* Expand the size of the allocated memory region so that we can
     adjust for alignment. */
  newsize = LWIP_MEM_ALIGN_SIZE(newsize);

  if(newsize < MIN_SIZE_ALIGNED) {
    /* every data block must be at least MIN_SIZE_ALIGNED long */
    newsize = MIN_SIZE_ALIGNED;
  }

  if (newsize > MEM_SIZE_ALIGNED) {
    return NULL;
  }

  LWIP_ASSERT("mem_trim: legal memory", (u8_t *)rmem >= (u8_t *)ram &&
   (u8_t *)rmem < (u8_t *)ram_end);

  if ((u8_t *)rmem < (u8_t *)ram || (u8_t *)rmem >= (u8_t *)ram_end) {
    SYS_ARCH_DECL_PROTECT(lev);
    LWIP_DEBUGF(MEM_DEBUG | LWIP_DBG_LEVEL_SEVERE, ("mem_trim: illegal memory\n"));
    /* protect mem stats from concurrent access */
    SYS_ARCH_PROTECT(lev);
    MEM_STATS_INC(illegal);
    SYS_ARCH_UNPROTECT(lev);
    return rmem;
  }
  /* Get the corresponding struct mem ... */
  mem = (struct mem *)(void *)((u8_t *)rmem - SIZEOF_STRUCT_MEM);
  /* ... and its offset pointer */
  ptr = (mem_size_t)((u8_t *)mem - ram);

  size = mem->next - ptr - SIZEOF_STRUCT_MEM;
  LWIP_ASSERT("mem_trim can only shrink memory", newsize <= size);
  if (newsize > size) {
    /* not supported */
    return NULL;
  }
  if (newsize == size) {
    /* No change in size, simply return */
    return rmem;
  }

  /* protect the heap from concurrent access */
  LWIP_MEM_FREE_PROTECT();

  mem2 = (struct mem *)(void *)&ram[mem->next];
  if(mem2->used == 0) {
    /* The next struct is unused, we can simply move it at little */
    mem_size_t next;
    /* remember the old next pointer */
    next = mem2->next;
    /* create new struct mem which is moved directly after the shrinked mem */
    ptr2 = ptr + SIZEOF_STRUCT_MEM + newsize;
    if (lfree == mem2) {
      lfree = (struct mem *)(void *)&ram[ptr2];
    }
    mem2 = (struct mem *)(void *)&ram[ptr2];
    mem2->used = 0;
    /* restore the next pointer */
    mem2->next = next;
    /* link it back to mem */
    mem2->prev = ptr;
    /* link mem to it */
    mem->next = ptr2;
    /* last thing to restore linked list: as we have moved mem2,
     * let 'mem2->next->prev' point to mem2 again. but only if mem2->next is not
     * the end of the heap */
    if (mem2->next != MEM_SIZE_ALIGNED) {
      ((struct mem *)(void *)&ram[mem2->next])->prev = ptr2;
    }
    MEM_STATS_DEC_USED(used, (size - newsize));
    /* no need to plug holes, we've already done that */
  } else if (newsize + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED <= size) {
    /* Next struct is used but there's room for another struct mem with
     * at least MIN_SIZE_ALIGNED of data.
     * Old size ('size') must be big enough to contain at least 'newsize' plus a struct mem
     * ('SIZEOF_STRUCT_MEM') with some data ('MIN_SIZE_ALIGNED').
     * @todo we could leave out MIN_SIZE_ALIGNED. We would create an empty
     *       region that couldn't hold data, but when mem->next gets freed,
     *       the 2 regions would be combined, resulting in more free memory */
    ptr2 = ptr + SIZEOF_STRUCT_MEM + newsize;
    mem2 = (struct mem *)(void *)&ram[ptr2];
    if (mem2 < lfree) {
      lfree = mem2;
    }
    mem2->used = 0;
    mem2->next = mem->next;
    mem2->prev = ptr;
    mem->next = ptr2;
    if (mem2->next != MEM_SIZE_ALIGNED) {
      ((struct mem *)(void *)&ram[mem2->next])->prev = ptr2;
    }
    MEM_STATS_DEC_USED(used, (size - newsize));
    /* the original mem->next is used, so no need to plug holes! */
  }
  /* else {
    next struct mem is used but size between mem and mem2 is not big enough
    to create another struct mem
    -> don't do anyhting. 
    -> the remaining space stays unused since it is too small
  } */
#if LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT
  mem_free_count = 1;
#endif /* LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT */
  LWIP_MEM_FREE_UNPROTECT();
  return rmem;
}

/**
 * Adam's mem_malloc() plus solution for bug #17922
 * Allocate a block of memory with a minimum of 'size' bytes.
 *
 * @param size is the minimum size of the requested block in bytes.
 * @return pointer to allocated memory or NULL if no free memory was found.
 *
 * Note that the returned value will always be aligned (as defined by MEM_ALIGNMENT).
 */
void *mem_malloc(mem_size_t size)
{
    mem_size_t ptr, ptr2;
    struct mem *mem, *mem2;

    LWIP_MEM_ALLOC_DECL_PROTECT();
    if (size == 0)
    {
        return NULL;
    }

    /* 扩展分配的内存区域的大小,以便我们可以调整对齐。*/
    size = LWIP_MEM_ALIGN_SIZE(size);

    if (size < MIN_SIZE_ALIGNED)
    {
        /* every data block must be at least MIN_SIZE_ALIGNED long */
        size = MIN_SIZE_ALIGNED;
    }

    if (size > MEM_SIZE_ALIGNED)
    {
        return NULL;
    }

    /* protect the heap from concurrent access */
    sys_mutex_lock(&mem_mutex);
    LWIP_MEM_ALLOC_PROTECT();

    /* 浏览堆,寻找足够大的可用块,从最低的空闲块开始.*/
    for (ptr = (mem_size_t)((u8_t *)lfree - ram); ptr < MEM_SIZE_ALIGNED - size;
            ptr = ((struct mem *)(void *)&ram[ptr])->next)
    {
        mem = (struct mem *)(void *)&ram[ptr];

        if ((!mem->used) && (mem->next - (ptr + SIZEOF_STRUCT_MEM)) >= size)
        {
            /* 不使用mem，并且至少可能完美匹配:mem-> next-(ptr + SIZEOF_STRUCT_MEM)
                为我们提供了mem的"用户数据大小" */

            if (mem->next - (ptr + SIZEOF_STRUCT_MEM) >= (size + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED))
            {
                /* (in addition to the above, we test if another struct mem (SIZEOF_STRUCT_MEM) containing
                * at least MIN_SIZE_ALIGNED of data also fits in the 'user data space' of 'mem')
                * -> split large block, create empty remainder,
                * remainder must be large enough to contain MIN_SIZE_ALIGNED data: if
                * mem->next - (ptr + (2*SIZEOF_STRUCT_MEM)) == size,
                * struct mem would fit in but no data between mem2 and mem2->next
                * @todo we could leave out MIN_SIZE_ALIGNED. We would create an empty
                *       region that couldn't hold data, but when mem->next gets freed,
                *       the 2 regions would be combined, resulting in more free memory
                */
                ptr2 = ptr + SIZEOF_STRUCT_MEM + size;

                /* create mem2 struct */
                mem2 = (struct mem *)(void *)&ram[ptr2];
                mem2->used = 0;
                mem2->next = mem->next;
                mem2->prev = ptr;

                /* and insert it between mem and mem->next */
                mem->next = ptr2;
                mem->used = 1;

                if (mem2->next != MEM_SIZE_ALIGNED)
                {
                    ((struct mem *)(void *)&ram[mem2->next])->prev = ptr2;
                }
                MEM_STATS_INC_USED(used, (size + SIZEOF_STRUCT_MEM));
            }
            else
            {
                /* (a mem2 struct does no fit into the user data space of mem and mem->next will always
                * be used at this point: if not we have 2 unused structs in a row, plug_holes should have
                * take care of this).
                * -> near fit or excact fit: do not split, no mem2 creation
                * also can't move mem->next directly behind mem, since mem->next
                * will always be used at this point!
                */
                mem->used = 1;
                MEM_STATS_INC_USED(used, mem->next - (mem_size_t)((u8_t *)mem - ram));
            }

            if (mem == lfree)
            {
                struct mem *cur = lfree;
                /* Find next free block after mem and update lowest free pointer */
                while (cur->used && cur != ram_end)
                {
                    cur = (struct mem *)(void *)&ram[cur->next];
                }
                lfree = cur;
            }

            LWIP_MEM_ALLOC_UNPROTECT();
            sys_mutex_unlock(&mem_mutex);

            return (u8_t *)mem + SIZEOF_STRUCT_MEM;
        }
    }
    MEM_STATS_INC(err);
    LWIP_MEM_ALLOC_UNPROTECT();
    sys_mutex_unlock(&mem_mutex);
    return NULL;
}

/**
 * Contiguously allocates enough space for count objects that are size bytes
 * of memory each and returns a pointer to the allocated memory.
 *
 * The allocated memory is filled with bytes of value zero.
 *
 * @param count number of objects to allocate
 * @param size size of the objects to allocate
 * @return pointer to allocated memory / NULL pointer if there is an error
 */
void *mem_calloc(mem_size_t count, mem_size_t size)
{
    void *p;

    /* allocate 'count' objects of size 'size' */
    p = mem_malloc(count * size);
    if (p)
    {
        /* zero the memory */
        memset(p, 0, count * size);
    }

    return p;
}

#endif /* !MEM_LIBC_MALLOC */

