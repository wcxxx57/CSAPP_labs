/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "IMSB",
    /* First member's full name */
    "wcxx57",
    /* First member's email address */
    "10242150443@stu.ecnu.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*
*宏定义
*/
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p)) //从地址p读取一个无符号整数
#define PUT(p, val) (*(unsigned int*)(p) = (val))//!将val写入地址p，转换为unsigned long?
#define GET_SIZE(p) (GET(p) & ~0x7)//获取内存块的大小
#define GET_ALLOC(p) (GET(p) & 0x1)//读取p处值的最低位 检查该内存块是否已分配
#define HDRP(bp) ((char *)(bp) - WSIZE)//从指向有效载荷的指针bp，得到指向该内存块的头部指针
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)//从指向有效载荷的指针bp，得到指向该内存块的脚部指针
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char*)(bp)-WSIZE))//通过该块的头部获取其大小，得到指向该内存块的下一个内存块的指针
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char*)(bp)-DSIZE))//通过前一个块的脚部获取其大小，得到指向该内存块的前一个内存块的指针

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


/*根据分离链表新增的宏*/
// #define GET_HEAD(num) ((void*)(long)(GET(heap_listp+WSIZE*num)))
// #define GET_PRE(bp) ((void*)(long)(GET(bp)))
// #define GET_SUC(bp) ((void*)(long)(GET((unsigned int*)(bp)+1))) 
#define GET_HEAD(num) ((unsigned int*)(long)(GET(heap_listp+WSIZE*num)))
#define GET_PRE(bp) ((unsigned int*)(long)(GET(bp)))
#define GET_SUC(bp) ((unsigned int*)(long)(GET((unsigned int*)(bp)+1))) 
#define CLASS_SIZE 20

static char *heap_listp;//!指向堆结构的开头
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static int search(size_t size);
static void insert(void* bp);
static void delete(void* bp);

/* 
 * mm_init - 堆初始化函数
 */
int mm_init(void){
    //申请初始堆空间
    if((heap_listp = mem_sbrk((4+CLASS_SIZE)*WSIZE)) == (void *)-1)
        return -1;
    //初始化大小类数组
    for(int i=0;i<CLASS_SIZE;i++){
        PUT(heap_listp+i*WSIZE,NULL);//!到底应该初始化为0还是null？？
    }
    
    // 创建空堆
    PUT(heap_listp+CLASS_SIZE*WSIZE, 0);//!对齐？（为什么这里要对齐？                            // 对齐填充
    PUT(heap_listp + ((1+CLASS_SIZE)*WSIZE), PACK(DSIZE, 1));   // 序言块头部
    PUT(heap_listp + ((2+CLASS_SIZE)*WSIZE), PACK(DSIZE, 1));   // 序言块脚部
    PUT(heap_listp + ((3+CLASS_SIZE)*WSIZE), PACK(0, 1));       // 结尾块头部
    
    // 移动指针到序言块之后 指向有效负载
    //!heap_listp += ((2+CLASS_SIZE)*WSIZE);
    
    // 扩展空堆，创建初始空闲块
    if((extend_heap(CHUNKSIZE/WSIZE)) == NULL) //!这个extend到底extend在了哪？
        return -1; 
    return 0;
}

/*
 * extend_heap - 扩展堆(分配后也检查是否需要合并)
 */
void *extend_heap(size_t words){
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;//八字节对齐
    if((long)(bp = mem_sbrk(size)) == -1)//向系统申请size大小的内存//!这个申请的内存到底在哪 好像这样申请的也不连续啊(?)
        return NULL;
    //mem_sbrk函数返回指向旧堆尾部的指针，因此指向新空闲块的有效载荷(可以直接将原始堆的尾部覆写为新空闲块的头部)
    PUT(HDRP(bp), PACK(size, 0));//写头部
    PUT(FTRP(bp), PACK(size, 0));//写脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));//新的结尾块
    return coalesce(bp);//合并空闲块//!!
}

/*
 * search - 找到块大小对应的等价类的序号
 */
int search(size_t size){
    int i;
    for(i=4;i<24;i++){//!在4到22之间计算 因为内存通常不会超过2的22字节
        if(size<=(1<<(i))){//因为块最小大小为16字节
            return i-4;
        }
    }
    return i-4;
}

/*
 * insert - 插入块到表头
 */
void insert(void* bp){
    if (bp == NULL) return;
    
    size_t size=GET_SIZE(HDRP(bp));
    int class=search(size);
    if(GET_HEAD(class)==NULL){//为头
        PUT(heap_listp+class*WSIZE,bp);//设置为链表头
        PUT(bp,NULL);//前驱
        PUT((unsigned int*)bp+1,NULL);//后继
    }else{//否则插入到头节点
        PUT(bp,NULL);//bp前驱
        PUT((unsigned int*)bp+1,GET_HEAD(class));//bp后继
        PUT(GET_HEAD(class),bp);//原来头的前驱
        PUT(heap_listp+class*WSIZE,bp);//修改链表数组中的链表头
    }
}


/*
 * delete - 删除块，清理指针
 */
void delete(void* bp){
    if (bp == NULL) return;
    
    size_t size=GET_SIZE(HDRP(bp));//!
    int class=search(size);
    /* 
     * 唯一节点,后继为null,前驱为null 
     * 头节点设为null
     */
    if (GET_PRE(bp) == NULL && GET_SUC(bp) == NULL) { 
        PUT(heap_listp + WSIZE * class, NULL);
    } 
    /* 
     * 最后一个节点 
     * 前驱的后继设为null
     */
    else if (GET_PRE(bp) != NULL && GET_SUC(bp) == NULL) {
        PUT(GET_PRE(bp) + 1, NULL);
    } 
    /* 
     * 第一个结点 
     * 头节点设为bp的后继
     */
    else if (GET_SUC(bp) != NULL && GET_PRE(bp) == NULL){
        PUT(heap_listp + WSIZE * class, GET_SUC(bp));
        PUT(GET_SUC(bp), NULL);
    }
    /* 
     * 中间结点 
     * 前驱的后继设为后继
     * 后继的前驱设为前驱
     */
    else if (GET_SUC(bp) != NULL && GET_PRE(bp) != NULL) {
        PUT(GET_PRE(bp) + 1, GET_SUC(bp));
        PUT(GET_SUC(bp), GET_PRE(bp));
    }//!!!!!!!!

    
    // void* pre = GET_PRE(bp);
    // void* suc = GET_SUC(bp);
    // //printf("Deleting block at %p: pre=%p, suc=%p\n", bp, pre, suc);
    // if(pre != NULL){//不为头
    //     //printf("Updating predecessor's successor: pre=%p, suc=%p\n", pre, suc);
    //     PUT(pre+1, suc);//处理前驱的后继
    // }else{//为头
    //     //printf("Updating head of free list: heap_listp[%d]=%p\n", class, suc);
    //     PUT(heap_listp+class*WSIZE, suc);//改数组中的头节点
    // }
    // if(suc != NULL){
    //     //printf("Updating successor's predecessor: suc=%p, pre=%p\n", suc, pre);
    //     PUT(suc, pre);
    // } 
}


/*
 * mm_free - 释放内存块(释放后立即合并策略)
 */
void mm_free(void *ptr){
    if(ptr==0)
        return;
    size_t size = GET_SIZE(HDRP(ptr));//获取内存块大小
    
    PUT(HDRP(ptr), PACK(size, 0));//写头部
    PUT(FTRP(ptr), PACK(size, 0));//写脚部
    coalesce(ptr);//合并空闲块 立即合并
}

/*
 * place - 放置内存块(判断是否能分离空闲块)
 */
void place(void *bp, size_t asize) //asize为请求放置的内存大小（已对齐）
{
    size_t csize = GET_SIZE(HDRP(bp));//获取当前空闲块未放置前的总大小
    delete(bp);//原块不再空闲
    /* 判断是否能够分离空闲块 */
    if((csize - asize) >= 2*DSIZE) { //分配后剩余的大小大于等于最小块大小//!其实我觉得不是16字节 还要更大 因为有效载荷不能为0？
        PUT(HDRP(bp), PACK(asize, 1));//写头部 修改块大小为需要放置的内存的大小
        PUT(FTRP(bp), PACK(asize, 1));//写脚部
        bp = NEXT_BLKP(bp);//移动到下一个内存块
        PUT(HDRP(bp), PACK(csize - asize, 0));//写头部 修改块大小为剩余的大小
        PUT(FTRP(bp), PACK(csize - asize, 0));//写脚部
        insert(bp);
    }
    //无法分离出空闲
    else{
        PUT(HDRP(bp), PACK(csize, 1));//不能分离 直接分配整个空闲块
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * coalesce - 合并空闲块
*/
void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));//后一个块是否已分配
    size_t size = GET_SIZE(HDRP(bp));//当前块大小

    /*
     * 四种情况：前后都不空, 前不空后空, 前空后不空, 前后都空
     */
    /* 前后都不空 */
    if(prev_alloc && next_alloc){
        insert(bp);//!靠！原先就因为没加这一行卡了好久！
        return bp;
    }
    /* 前不空后空 */
    else if(prev_alloc && !next_alloc){
        delete(NEXT_BLKP(bp));//相比较隐式就是多了个从链表中删除
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(HDRP(bp), PACK(size, 0));           //先修改头
        PUT(FTRP(bp), PACK(size, 0));           //根据头部中的大小来定位尾部
    }
    /* 前空后不空 */
    else if(!prev_alloc && next_alloc){
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);//注意bp要变 方便后面insert
    }
    /* 都空 */
    else{
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert(bp);
    return bp;
}

/*
 * find_first_fit - 找到合适的空闲块(首次适配)
*/
void* find_fit(size_t size){
    int class=search(size);
    while(class < CLASS_SIZE){
        unsigned int* cur = GET_HEAD(class);//!这个用void类型和unsigned int有影响吗？
        while (cur != NULL){
            if(GET_SIZE(HDRP(cur)) >= size){
                return cur;//!到底要不要转为void*（？？）
            }else{
                cur = GET_SUC(cur);
            }
        }
        class++;
    }
    return NULL;
}
/*
 * find_best_fit - 找到合适的空闲块(最佳适配)
 ?这种方法不需要再分best_fit因为是按大小分的class 最先找到的也就是最小的
*/


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
/*        *(size_t *)p = size;*/
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }//最弱智的原始方法
void *mm_malloc(size_t size){
    if(size == 0) 
        return NULL;
    size_t asize = ALIGN(size + DSIZE);
    size_t extendsize;
    char *bp;

    /* 寻找合适的空闲块 */
    if((bp = find_fit(asize)) == NULL){//找不到则拓展堆
        //!extendsize = MAX(asize, CHUNKSIZE); //一次至少拓展chunksize 增大效率
        extendsize=asize;//!其实直接拓展相应大小也可以
        if((bp = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;
    }
    place(bp, asize);
    return bp;
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    if((newptr = mm_malloc(size))==NULL)
        return NULL;//分配新的内存
    size_t copysize = GET_SIZE(HDRP(ptr));
    if(size < copysize)
        copysize = size;//防止溢出
    memcpy(newptr, ptr, copysize);//将原先内容拷贝到新地址
    mm_free(ptr);//释放原先内存
    return newptr;
}














