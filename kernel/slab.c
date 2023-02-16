#include "slab.h"
#include "stdio.h"
#include "protect.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "memman.h"
#include "assert.h"
#include "string.h"

kmem_cache_t cache_cache, cache_slab, cache_bufctl;

void do_kmem_init()
{
    void *mem = sys_kmalloc_4k();
    cache_cache = (kmem_cache_t)mem;
    cache_cache -> name = "cache";
    cache_cache -> size = sizeof(struct kmem_cache);
    cache_cache -> effsize = SLAB_DEFAULT_ALIGN * ((cache_cache -> size - 1)/SLAB_DEFAULT_ALIGN + 1);
    cache_cache -> constructor = NULL;
    cache_cache -> destructor = NULL;
    cache_cache -> slabs = NULL;
    cache_cache -> slabs_back = NULL;
    cache_cache -> slab_maxbuf = (PAGE_SZ - sizeof(struct kmem_slab)) / cache_cache -> effsize;
    cache_slab = do_kmem_cache_create("slab", sizeof(struct kmem_slab), 0, NULL, NULL);
    cache_bufctl = do_kmem_cache_create("bufctl", sizeof(struct kmem_bufctl), 0, NULL, NULL);
}

kmem_cache_t
do_kmem_cache_create(char *name, size_t size, int align,
                  void (*constructor)(void *, size_t),
                  void (*destructor)(void *, size_t)) 
{
    //panic("error");
    // kmem_cache_t cp = sys_malloc(sizeof(struct kmem_cache));
    kmem_cache_t cp = do_kmem_cache_alloc(cache_cache, KM_NOSLEEP);
    //kprintf("error:%d\n",sizeof (struct kmem_cache));
    //panic("error");
    if (cp != NULL) 
    {
        if (align == 0) 
            align = SLAB_DEFAULT_ALIGN ;//内存对齐
        cp->name = name;
        cp->size = size;    //zys:对象大小
        cp->effsize = align * ((size-1)/align + 1);//取整感觉像是对齐   zys:对象对其后大小
        cp->constructor = constructor;  //zys:构造函数
        cp->destructor = destructor;    //zys:析构函数
        cp->slabs = NULL;       //zys:刚开始初始化为空
        cp->slabs_back = NULL;
        if (cp->size <= SLAB_SMALL_OBJ_SZ) 
        {    //zys:小对象，对象大小不超过1/8页，即512B
            cp->slab_maxbuf = (PAGE_SZ - sizeof(struct kmem_slab)) / cp->effsize;   //记录能容纳slab的最大个数
        }
        else 
        {      //zys:大对象
            cp->slab_maxbuf = 8;
        }
    }
    
    return cp;
    //创建的对象
}
void 
do_kmem_cache_grow(kmem_cache_t cp) {  //根据缓冲器模板创建一个新的slab
    void *mem;
    kmem_slab_t slab;
    void *p, *lastbuf;
    int i;
    kmem_bufctl_t bufctl;
    if (cp->size <= SLAB_SMALL_OBJ_SZ) {    //zys:小对象，slab分配一个页
        if (0 != posix_memalign(&mem, PAGE_SZ, PAGE_SZ))//申请PAGE_SZ的内存指向mem,申请成功返回0    zys:我想知道这个函数在哪  你自己查
            return;
        // mem=sys_kmalloc_4k();
        slab = mem + PAGE_SZ - sizeof(struct kmem_slab);    //zys:放在页后面，最后一个字节为页尾

        slab->next = slab->prev = slab;         //zys:目前队列中就它一个
        slab->bufcount = 0;                     //zys:用了0个对象
        slab->free_list = mem;                  //zys:第一个空闲位置为第一个对象
        lastbuf = mem + (cp->effsize * (cp->slab_maxbuf-1));    //zys:最后一个对象位置
        for (p=mem; p < lastbuf; p+=cp->effsize)                //zys:看不懂这个操作    zys:想了下，应该是刚开始指针指向下一个的位置
             *((void **)p) = p + cp->effsize;
        __slab_move_to_front(cp, slab);         //zys:放到队头
        assert(cp->slabs == slab);      //zys:队头是不是这个slab

        // kprintf("\n%x\n%x\n%x\n%x\n", mem, slab, sizeof(struct kmem_slab), sizeof(struct kmem_cache));
    }
    else {          //zys:大对象
        if (0 != posix_memalign(&mem,PAGE_SZ,cp->slab_maxbuf * cp->effsize))//第三个参数是第二个参数的整数倍,第二个参数为2的幂
            //我感觉这里有点问题,第三个参数大概率很小啊
            return;
        // mem=sys_kmalloc_4k();
        // slab = (kmem_slab_t)sys_malloc(sizeof(struct kmem_slab));
        slab = do_kmem_cache_alloc(cache_slab, KM_NOSLEEP);
        // zys: 这点和上面差不多
        slab->next = slab->prev = slab;        
        slab->bufcount = 0;
        // bufctl = (kmem_bufctl_t)sys_malloc(sizeof(struct kmem_bufctl) * cp->slab_maxbuf);   //zys:分配大小
        bufctl = do_kmem_cache_alloc(cache_bufctl, KM_NOSLEEP);
        bufctl[0].next = NULL;      //zys:前一个为空
        bufctl[0].buf = mem + 8;        //zys:第一个位置
        *((void **)mem) = &bufctl[0];
        bufctl[0].slab = slab;      //zys:所属slab
        slab->start = &bufctl[0];   //zys:slab开始
        slab->free_list = &bufctl[0];   //zys:目前第一个空闲
        for (i=1; i < cp->slab_maxbuf; i++) {
            bufctl[i].next = slab->free_list;   //zys:空闲列表最后一个
            bufctl[i].buf = mem + i*cp->effsize + (i + 1) * 8;//没太看懂(这里可能是染色)
            *((void **)(mem + i*cp->effsize + i * 8)) = &bufctl[i];
            //zys:上一行中，指的是buf位置
            //zys:例如大小3K，4K%3K=1K，加数分别为1K，2K，3K，3K，4K，5K，6K
            //zys:除了buf[0]外，偏移量也就分别是4K,8K,12K,15K,19K,23K,27K
            bufctl[i].slab = slab;          //zys:没什么说的
            slab->free_list = &bufctl[i];   //zys:指向新的
        }
        __slab_move_to_front(cp, slab);        //zys:还是插队头
        // printf("\n%p\n%p\n%#x\n%#x\n", mem, slab, sizeof(struct kmem_slab), sizeof(struct kmem_cache));
    }
}

/* Requests an allocated object from the cache. 

    @cp cache pointer
    @flags flags KM_SLEEP or KM_NOSLEEP
*/
void *
do_kmem_cache_alloc(kmem_cache_t cp, int flags) {//modified by lq 2023.1.11
    void *buf;
    if (cp->slabs == NULL)      //zys:没有slab
        do_kmem_cache_grow(cp);
    if (cp->slabs->bufcount == cp->slab_maxbuf) //zys:队头的slab满了
        do_kmem_cache_grow(cp);//已经满了搞一个新的
    if (cp->size <= SLAB_SMALL_OBJ_SZ) {    //zys:小对象
        buf = cp->slabs->free_list;         //zys:空闲位置
        //kprintf("\ndizhi:%x\n",buf);
        cp->slabs->free_list = *((void**)buf);  //zys:指向下一个位置
        cp->slabs->bufcount++;   //链表操作看看最下面那三个涉及链表的就能懂了   zys:已用+1
    }
    else {
        kmem_bufctl_t bufctl = cp->slabs->free_list;    //zys:第一个空闲bufct1
        cp->slabs->free_list = bufctl->next;            //zys:空闲链表指向下一个
        buf = bufctl->buf;                  //返回地址
        cp->slabs->bufcount++;//这也是链表操作
    }
    if (cp->slabs->bufcount == cp->slab_maxbuf) //满的放队尾，这块也就能理解为什么只检查队头是否满了
        __slab_move_to_back(cp, cp->slabs);
    //kprintf("error\n");
    return buf;     //返回分配的地址
}
void 
do_kmem_cache_free(kmem_cache_t cp, void *buf) {       //zys:释放一个对象
    void * mem;
    kmem_slab_t slab;
    kmem_bufctl_t bufctl;
    if (cp->size <= SLAB_SMALL_OBJ_SZ) {        //小对象
        mem = (void*)((long)buf >> 12 << 12); //mem是4kb对齐的,移掉低12位就可以找到所属的mem
        //zys:上面的这个操作的目的是找到这个buf属于的slab，因为不会超过一个页，所以起始位置一定在这个页页首地址
        slab = mem + PAGE_SZ - sizeof(struct kmem_slab);    //zys:根据上一行所说的，这样就能找到所属slab，小对象没有bufct1
        *((void **)buf) = slab->free_list;  //zys:加入空闲队列，头插法
        slab->free_list = buf;
        slab->bufcount--;           //zys:已用-1
        if (slab->bufcount == 0) {  //zys:空了，直接free，需要时候再分配
            //zys:题外话，这块也体现了一点三链表思想
            __slab_remove(cp, slab);
            sys_free(mem);
        }
        if (slab->bufcount == cp->slab_maxbuf-1)    //zys:如果原本是满的，放回队头
            __slab_move_to_front(cp, slab);

    }
    else {      //zys:free大对象操作，估计是没有完善，这部分后续再补充吧
        bufctl = (kmem_bufctl_t)(*((void **)(buf - 8)));
        slab = bufctl->slab;
        bufctl->next = slab->free_list;
        slab->free_list = bufctl;
        if (slab->bufcount == 0) {
            __slab_remove(cp, slab);
            sys_free(slab->start->buf); // free objects
            // free(slab->start); // free bufctls
            // free(slab); // free slab
            do_kmem_cache_free(cache_bufctl, slab -> start);
            do_kmem_cache_free(cache_slab, slab);
        }   
        if (slab->bufcount == cp->slab_maxbuf-1)    //zys:如果原本是满的，放回队头
            __slab_move_to_front(cp, slab);
    }
}
void 
do_kmem_cache_destroy(kmem_cache_t cp) {       //zys:直接删除一个slab的cache
    kmem_slab_t slab;
    void * mem;
    if (cp->size <= SLAB_SMALL_OBJ_SZ) {    //小对象slab的cache
        while (cp->slabs) {     //如果还有slab
            slab = cp->slabs;
            __slab_remove(cp, slab);    //移除这个slab
            mem = (void*)slab - PAGE_SZ + sizeof(struct kmem_slab);
            sys_free(mem);          //free掉slab
        }
    }
    else {
        while (cp->slabs) {
            slab = cp->slabs;
            __slab_remove(cp, slab);
            //zys:这块因为涉及别的因为，需要全部free
            sys_free(slab->start->buf); // free objects
            // sys_free(slab->start); // free bufctls
            // sys_free(slab); // free slab
            do_kmem_cache_free(cache_bufctl, slab -> start);
            do_kmem_cache_free(cache_slab, slab);
        }
    }
    // sys_free(cp);   //最后free掉cache结构体
    do_kmem_cache_free(cache_cache, cp);
}
void
__slab_remove(kmem_cache_t cp, kmem_slab_t slab) {  //从cp中删除slab
    slab->next->prev = slab->prev;
    slab->prev->next = slab->next;
    //zys:循环双链表操作
    if (cp->slabs == slab) {    //zys:如果是队头
        if (slab->prev == slab)     //zys:如果还是队尾（队头后一个为自己）
            cp->slabs = NULL;       //zys:队头指向空
        else
            cp->slabs = slab->prev; //zys:队头为其后一个
    }
    if (cp->slabs_back == slab) {   //zys:如果是队尾
        if (slab->next == slab)     //zys:如果还是队头
            cp->slabs_back = NULL;  //zys:队尾指向空
        else
            cp->slabs_back = slab->next;    //zys:队尾为其前一个
    }
    //zys:注意以上两个判断是并行的，即都需要判断
}
void
__slab_move_to_front(kmem_cache_t cp, kmem_slab_t slab) {   //zys:放到队列头
    if (cp->slabs == slab) return;

    __slab_remove(cp, slab);    //zys:把这个删了
    if (cp->slabs == NULL) {    //zys:删除后为空
        slab->prev = slab;
        slab->next = slab;

        cp->slabs_back = slab;  //zys:队尾也是它
    }
    else {
        slab->prev = cp->slabs; //zys:后一个是当前头
        cp->slabs->next = slab; //zys:当前头前一个是其
        //zys:同样的链表操作，双向循环链表
        slab->next = cp->slabs_back;
        cp->slabs_back->prev = slab;
    }
    cp->slabs = slab;
}
void
__slab_move_to_back(kmem_cache_t cp, kmem_slab_t slab) {    //zys:插到队尾，参考上一个函数的注释
    if (cp->slabs_back == slab) return;
    
    __slab_remove(cp, slab);
    if (cp->slabs == NULL) {
        slab->prev = slab;
        slab->next = slab;

        cp->slabs = slab;
    }
    else {
        slab->prev = cp->slabs;
        cp->slabs->next = slab;

        slab->next = cp->slabs_back;
        cp->slabs_back->prev = slab;
    }
    cp->slabs_back = slab;
}
int posix_memalign(void **memptr, size_t alignment, size_t size)//added by lq   2023.1.7
{
    if( size == (size_t) 0 )
    {
        *memptr = NULL;
        return 0;
    }
    else if(alignment % 2 != 0 || alignment % sizeof( void*) != 0 )
    {
        return EINVAL;
    }
    *memptr = sys_kmalloc(size);
    if(*memptr == NULL)
    {
        return ENOMEM;
    }
    else
    {
        return 0;
    }
}