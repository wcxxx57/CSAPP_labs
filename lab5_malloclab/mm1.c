//?隐式链表版


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
#define PUT(p, val) ((*(unsigned int *)(p)) = (val))//将val写入地址p
#define GET_SIZE(p) (GET(p) & ~0x7)//获取内存块的大小
#define GET_ALLOC(p) (GET(p) & 0x1)//读取p处值的最低位 检查该内存块是否已分配
#define HDRP(bp) ((char *)(bp) - WSIZE)//从指向有效载荷的指针bp，得到指向该内存块的头部指针
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)//从指向有效载荷的指针bp，得到指向该内存块的脚部指针
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char*)(bp)-WSIZE))//通过该块的头部获取其大小，得到指向该内存块的下一个内存块的指针
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char*)(bp)-DSIZE))//通过前一个块的脚部获取其大小，得到指向该内存块的前一个内存块的指针

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
//#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define find_fit find_first_fit //通过宏来改变空闲块的选择策略

static char *heap_listp;//指向序言块
static void *extend_heap(size_t words);
static void *find_first_fit(size_t asize);
static void *find_best_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);

/* 
 * mm_init - 堆初始化函数
 */
int mm_init(void){
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
    // 创建空堆
    PUT(heap_listp, 0);                            // 对齐填充
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));   // 序言块头部
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));   // 序言块脚部
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));       // 结尾块头部
    
    // 移动指针到序言块之后
    heap_listp += (2*WSIZE);
    
    // 扩展空堆，创建初始空闲块
    if((extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1; 
    return 0;
}

/*
 * extend_heap - 扩展堆(分配后也检查是否需要合并)
 */
void *extend_heap(size_t words){
    char *bp;
    size_t size = ALIGN(words * WSIZE);//这样对齐也可以
    //size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;//八字节对齐
    if((long)(bp = mem_sbrk(size)) ==-1)//向系统申请size大小的内存
        return NULL;
    //mem_sbrk函数返回指向旧堆尾部的指针，因此指向新空闲块的有效载荷(可以直接将原始堆的尾部覆写为新空闲块的头部)
    PUT(HDRP(bp), PACK(size, 0));//写头部
    PUT(FTRP(bp), PACK(size, 0));//写脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));//新的结尾块
    return coalesce(bp);//合并空闲块//!!
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
        return bp;
    }
    /* 前不空后空 */
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(HDRP(bp), PACK(size, 0));           //先修改头
        PUT(FTRP(bp), PACK(size, 0));           //根据头部中的大小来定位尾部
    }//!不需要显示地删除当前块的头部和脚部，因为它们已经合并到前一个块中
    /* 前空后不空 */
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                     //注意指针要变
    }
    /* 都空 */
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));  //增加当前块大小
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * find_fit - 找到合适的空闲块(首次适配)
*/
void* find_first_fit(size_t asize){
    void *bp;
    for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
        if(GET_ALLOC(HDRP(bp))==0&&GET_SIZE(HDRP(bp))>=asize){
            return bp;
        }
    }
    return NULL;
}
/*
 * find_best_fit - 找到合适的空闲块(最佳适配)
*/
void* find_best_fit(size_t asize){
    void *bp;
    void *best_bp = NULL;
    for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
        if(GET_ALLOC(HDRP(bp))==0&&GET_SIZE(HDRP(bp))>=asize){
            if(best_bp==NULL||GET_SIZE(HDRP(bp))<GET_SIZE(HDRP(best_bp))){
                best_bp = bp;
            }
        }
    }
    return best_bp;
}
/*
 * place - 放置内存块(判断是否能分离空闲块)
 */
void place(void *bp, size_t asize) //asize为请求放置的内存大小（已对齐）
{
    size_t csize = GET_SIZE(HDRP(bp));//获取当前空闲块未放置前的总大小

    /* 判断是否能够分离空闲块 */
    if((csize - asize) >= 2*DSIZE) { //分配后剩余的大小大于等于最小块大小（2*DSIZE:头加脚是8字节 有效载荷不为零 最小块大小为16字节）
        PUT(HDRP(bp), PACK(asize, 1));//写头部 修改块大小为需要放置的内存的大小
        PUT(FTRP(bp), PACK(asize, 1));//写脚部
        bp = NEXT_BLKP(bp);//移动到下一个内存块
        PUT(HDRP(bp), PACK(csize - asize, 0));//写头部 修改块大小为剩余的大小
        PUT(FTRP(bp), PACK(csize - asize, 0));//写脚部
    }
    /* 设置为填充 */
    else{
        PUT(HDRP(bp), PACK(csize, 1));//不能分离 直接分配整个空闲块
        PUT(FTRP(bp), PACK(csize, 1));
    }
}




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
    if(size == 0) return NULL;
    size_t asize = ALIGN(size + DSIZE);
    size_t extendsize;
    char *bp;

    /* 寻找合适的空闲块 */
    if((bp = find_fit(asize)) == NULL){//找不到则拓展堆
        //extendsize = MAX(asize, CHUNKSIZE); //一次至少拓展chunksize 增大效率
        extendsize=asize;//!起始直接拓展相应大小也可以
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














