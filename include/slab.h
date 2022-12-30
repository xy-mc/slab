#ifndef SLAB_H
#define SLAB_H 1

// #include <stdlib.h>
// #include <unistd.h>
#include "stdio.h"

#define PAGE_SZ 0x1000
#define SLAB_SMALL_OBJ_SZ PAGE_SZ/8
#define SLAB_DEFAULT_ALIGN 8    //默认对齐
#define CACHE_LINE_SZ 0x40

#define KM_SLEEP    0x00
#define KM_NOSLEEP  0x01

struct kmem_bufctl;
struct kmem_slab;
struct kmem_cache;

typedef struct kmem_cache * kmem_cache_t;
typedef struct kmem_bufctl * kmem_bufctl_t;
typedef struct kmem_slab * kmem_slab_t;

struct kmem_bufctl {
    void * buf;//bufctl指向的空间
    kmem_bufctl_t next;//链表结构
    kmem_slab_t slab;//所属的slab
};

struct kmem_slab {
    kmem_slab_t next;//双链表结构   zys:循环双链表  next:前一个 prev:后一个
    kmem_slab_t prev;
    kmem_bufctl_t start;//slab 中空闲的 bufctl下一个空闲对象的下标  zys:开始的第一个对象，貌似只有大对象用了
    void* free_list; // may point to bufctl or buf directly  指向slab的第一个对象   zys:第一个空闲对象或者第一个空闲buf
    int bufcount;//slab 被使用的次数    zys:已经使用的slab对象数目？
};

struct kmem_cache { //slab缓存
    char * name;
    size_t size;//大小      zys:对象大小
    size_t effsize;//取整之后的大小     zys:对象对齐后大小
    int slab_maxbuf;//最大的个数(应该)
    void (*constructor)(void *, size_t);//构造函数
    void (*destructor)(void *, size_t);//析构函数
    kmem_slab_t slabs;// 首个 kmem_slab     zys:队列头
    kmem_slab_t slabs_back;// 最新的 kmem_slab  zys:队尾
};


// kmem_cache_t
// kmem_cache_create(char *name, size_t size, int align, 
//                   void (*constructor)(void *, size_t),
//                   void (*destructor)(void *, size_t));


// void *
// kmem_cache_alloc(kmem_cache_t cp, int flags);

// void 
// kmem_cache_free(kmem_cache_t cp, void *buf);

// void 
// kmem_cache_destroy(kmem_cache_t cp);

// // TODO
// void 
// kmem_cache_grow(kmem_cache_t cp);

// void 
// kmem_cache_reap(void);

void
__slab_remove(kmem_cache_t cp, kmem_slab_t slab);

void
__slab_move_to_front(kmem_cache_t cp, kmem_slab_t slab);

void
__slab_move_to_back(kmem_cache_t cp, kmem_slab_t slab);

#endif