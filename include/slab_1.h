#ifndef __GNUC__
# error Can be compiled only with GCC.
#endif

#pragma once

#include <stdint.h>
#include <stddef.h>

extern size_t slab_pagesize;

struct slab_header {
    //双指针
    struct slab_header *prev, *next;
    uint64_t slots;
    uintptr_t refcount;
    struct slab_header *page;
    uint8_t data[] __attribute__((aligned(sizeof(void *))));
};

struct slab_chain {
    size_t itemsize, itemcount;         //一个项大小，一个slab的项数
    size_t slabsize, pages_per_alloc;   //slab大小，一个slab多少个页
    uint64_t initial_slotmask, empty_slotmask;
    uintptr_t alignment_mask;   //对齐
    //部分满队列，空队列，满队列
    struct slab_header *partial, *empty, *full;
};

void slab_init(struct slab_chain *, size_t);
void *slab_alloc(struct slab_chain *);
void slab_free(struct slab_chain *, const void *);
void slab_traverse(const struct slab_chain *, void (*)(const void *));
void slab_destroy(const struct slab_chain *);