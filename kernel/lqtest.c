#include "type.h"
#include "tty.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "proto.h"
#include "global.h"
#include "stdio.h"
#include "slab.h"
#define ITERATIONS 10000
#define BIGITERATIONS 50
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#define assertt(message, test) do { if (!(test)) return message; } while (0)
#define run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return (char*)message; } while (0)
int tests_run = 0;

static char *
test_cache_create() {
    kmem_cache_t cp = do_kmem_cache_create("test", 12, 0, NULL, NULL);
    assertt("cache creation returned null?", cp);
    kprintf("name:%s\n",cp->name);
    kprintf("effsize:%x\n",cp->effsize);
    kprintf("size:%x\n",12);
    kprintf("%u\n",cp->slab_maxbuf);
    assertt("effective size miscalculated", cp->effsize == 16);
    do_kmem_cache_destroy(cp);
    return 0;
}

static char *
test_cache_grow() {
    kmem_cache_t cp = do_kmem_cache_create("test", 12, 0, NULL, NULL);
    do_kmem_cache_grow(cp);
    do_kmem_cache_destroy(cp);
    return 0;
}

static char *
test_cache_alloc() {
    // 12-byte struct
    struct test {
        int a, b, c;
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
    obj->a=1;
    obj->b=1;
    obj->c=1;
    obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
    obj->a=1;
    obj->b=2;
    obj->c=3;
    do_kmem_cache_destroy(cp);
    return 0;
}
static char *
test_perf_cache_alloc() {
    unsigned long long start, end;    
    int i;
    // 12-byte struct
    struct test {
        int a, b, c;
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    rdtscll(start);
    for (i=0; i<ITERATIONS; i++)
    {
        obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
        obj->a = 1,obj->b=1,obj->c=1;
    }
    rdtscll(end);
    kprintf("# %lld cycles for cache mem alloc\n", (end-start)/ITERATIONS);
    rdtscll(start);
    for (i=0; i<ITERATIONS; i++)
    {
        obj = (struct test *)sys_kmalloc(sizeof(struct test));
        obj->a = 1,obj->b=1,obj->c=1;
    }
    rdtscll(end);
    kprintf("# %lld cycles for malloc\n", (end-start)/ITERATIONS);
    do_kmem_cache_destroy(cp);
    return 0;
}
static char *
test_perf_cache_alloc_free() {
    unsigned long long start, end;    
    struct test {// 12-byte struct
        int a, b, c;
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    rdtscll(start);
    for (int i=0; i<ITERATIONS; i++)
    {
        obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
        obj->a = 1,obj->b=1,obj->c=1;
        do_kmem_cache_free(cp, obj);
    }
    rdtscll(end);
    kprintf("# %lld cycles for cache mem alloc_free\n", (end-start)/ITERATIONS);
    rdtscll(start);
    for (int i=0; i<ITERATIONS; i++)
    {
        obj = (struct test *)sys_kmalloc(sizeof(struct test));
        obj->a = 1,obj->b=1,obj->c=1;
        sys_free(obj);
    }
    rdtscll(end);
    kprintf("# %lld cycles for malloc_free\n", (end-start)/ITERATIONS);
    do_kmem_cache_destroy(cp);
    return 0;
}

static char *
test_cache_free() {
    // 12-byte struct
    struct test {
        int a, b, c;
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
    obj->a=1;
    obj->b=1;
    obj->c=1;
    do_kmem_cache_free(cp, obj);
    do_kmem_cache_destroy(cp);
    return 0;
}

static char *
test_big_object() {
    int i;
    void * pos[9];
    kmem_cache_t cp = do_kmem_cache_create("test", 1000, 0, NULL, NULL);
    for (i = 0; i < 10; i++)  
    {
        pos[i] = do_kmem_cache_alloc(cp, KM_NOSLEEP);
    }
    for (i = 0; i < 10; i++) 
    {
        do_kmem_cache_free(cp, pos[i]);
    }
    do_kmem_cache_destroy(cp);
    return 0;
}
static char *
test_big_perf_cache_alloc() {
    unsigned long long start, end;    
    int i;
    // 1000-byte struct
    struct test {
        int a[250];
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    rdtscll(start);
    for (i=0; i<BIGITERATIONS; i++)
    {
        obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
        obj->a[0]=1;
    }
    rdtscll(end);
    kprintf("# %lld cycles for big cache mem alloc\n", (end-start)/BIGITERATIONS);
    rdtscll(start);
    for (i=0; i<BIGITERATIONS; i++)
    {
        obj = (struct test *)sys_kmalloc(sizeof(struct test));
        obj->a[0]=1;
    }
    rdtscll(end);
    kprintf("# %lld cycles for big malloc\n", (end-start)/BIGITERATIONS);
    do_kmem_cache_destroy(cp);
    return 0;
}
static char *
test_big_perf_cache_alloc_free() {
    unsigned long long start, end;    
    struct test {// 1000-byte struct
        int a[250];
    };
    struct test * obj;
    kmem_cache_t cp = do_kmem_cache_create("test", sizeof(struct test), 0, NULL, NULL);
    rdtscll(start);
    for (int i=0; i<BIGITERATIONS; i++)
    {
        obj = (struct test *)do_kmem_cache_alloc(cp, KM_NOSLEEP);
        obj->a[0]=1;
        do_kmem_cache_free(cp, obj);
    }
    rdtscll(end);
    kprintf("# %lld cycles for big cache mem alloc_free\n", (end-start)/BIGITERATIONS);
    rdtscll(start);
    for (int i=0; i<BIGITERATIONS; i++)
    {
        obj = (struct test *)sys_kmalloc(sizeof(struct test));
        obj->a[0]=1;
        sys_free(obj);
    }
    rdtscll(end);
    kprintf("# %lld cycles for big malloc_free\n", (end-start)/BIGITERATIONS);
    do_kmem_cache_destroy(cp);
    return 0;
}
char *
test_all () {
    run_test(test_cache_create);
    run_test(test_cache_grow);
    run_test(test_cache_alloc);
    run_test(test_cache_free);
    run_test(test_perf_cache_alloc);
    run_test(test_perf_cache_alloc_free);
    run_test(test_big_object);
    run_test(test_big_perf_cache_alloc);
    run_test(test_big_perf_cache_alloc_free);
    return 0;
}
