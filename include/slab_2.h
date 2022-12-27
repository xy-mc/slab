#ifndef SLAB_H
#define SLAB_H 1

#include <stdlib.h>
#include <unistd.h>

#define PAGE_SZ (size_t)sysconf(_SC_PAGESIZE)//此宏查看缓存内存页面的大小；打印用%ld长整型。
#define SLAB_SMALL_OBJ_SZ PAGE_SZ/8
#define SLAB_DEFAULT_ALIGN 8
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
    kmem_slab_t next;//双链表结构
    kmem_slab_t prev;
    kmem_bufctl_t start;//slab 中空闲的 bufctl下一个空闲对象的下标
    void* free_list; /* may point to bufctl or buf directly  指向slab的第一个对象*/   
    int bufcount;//slab 被使用的次数
};

struct kmem_cache {
    char * name;
    size_t size;//大小
    size_t effsize;//取整之后的大小
    int slab_maxbuf;//最大的个数(应该)
    void (*constructor)(void *, size_t);//构造函数
    void (*destructor)(void *, size_t);//析构函数
    kmem_slab_t slabs;// 首个 kmem_slab 
    kmem_slab_t slabs_back;// 最新的 kmem_slab
};


kmem_cache_t
kmem_cache_create(char *name, size_t size, int align, 
                  void (*constructor)(void *, size_t),
                  void (*destructor)(void *, size_t));


void *
kmem_cache_alloc(kmem_cache_t cp, int flags);

void 
kmem_cache_free(kmem_cache_t cp, void *buf);

void 
kmem_cache_destroy(kmem_cache_t cp);

// TODO
void 
kmem_cache_grow(kmem_cache_t cp);

void 
kmem_cache_reap(void);

inline void
__slab_remove(kmem_cache_t cp, kmem_slab_t slab);

inline void
__slab_move_to_front(kmem_cache_t cp, kmem_slab_t slab);

inline void
__slab_move_to_back(kmem_cache_t cp, kmem_slab_t slab);

#endif