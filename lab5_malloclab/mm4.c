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
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
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
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p)) //从地址p读取一个无符号整数
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(long)(val))//将val写入地址p，正确转换指针
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
#define GET_HEAD(num) ((unsigned int*)(long)(GET(heap_listp - (2+CLASS_SIZE)*WSIZE + WSIZE*num)))
#define GET_PRE(bp) ((unsigned int*)(long)(GET(bp)))
#define GET_SUC(bp) ((unsigned int*)(long)(GET((char *)(bp) + WSIZE)))
#define CLASS_SIZE 20
#define PTR(p) ((char *)(p))
#define GET_P(p) (GET(p))

static char *heap_listp;//!指向堆结构的开头
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static int search(size_t size);
static void insert(void* bp);
static void delete(void* bp);
static unsigned int *get_list_head(int i); // 声明函数
static void print_free_lists(void); // 声明函数

//
static void realloc_place(void *bp, size_t asize);
static void *realloc_coalesce(void *bp, size_t asize, int *is_next_free);
void chain2prevnext(void *bp);
#define PUT_PREV(p, val) (PUT(p, (unsigned int)(long)(val)))  /* 别名 */
#define PUT_SUCC(p, val) (PUT((char *)(p) + WSIZE, (unsigned int)(long)(val)))
//


//=================mm_check===============//
#define DEBUG

enum ERRCODE {
    OK = 0, ERR1, ERR2, ERR3, ERR4, ERR5, ERR6, ERR7, ERR8, ERR9, ERR10
};
static void print_list(int i)
{
    int print_flag = 0;
    unsigned int *p = (unsigned int *)get_list_head(i);

    while (p != NULL) {
        print_flag = 1;
        printf("(%p, %d) -> ", p, GET_SIZE(HDRP(p)));
        p = GET_SUC(p);
    }
    if (print_flag) {
        printf("end. list [%d]\n", i);
    }
}
static enum ERRCODE check_ptr_in_heap(char *p)
{
    char *ptr = (char *)mem_heap_lo() + CLASS_SIZE * WSIZE + DSIZE;
    for( ; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
        if(ptr != p) {
            continue;
        }
        if (GET_ALLOC(HDRP(p)) == 1) {
            return ERR6;
        }
        return OK;
    }
    return ERR7;
}
static int find_ptr_in_free_lists(char *p)
{
    unsigned int *ptr;
    for (int i = 0; i < CLASS_SIZE; ++i) {
        ptr = GET_HEAD(i);
        while (ptr != NULL) {
            if ((char *)ptr == p) {
                return 1;
            }
            ptr = GET_SUC(ptr);
        }
    }
    return 0;
}
static enum ERRCODE check_heap_arr()
{
    // 检查堆
    char *ptr = (char *)mem_heap_lo() + CLASS_SIZE * WSIZE;
    // 检查序言块大小和分配位
    if (GET(ptr + WSIZE) != PACK(DSIZE, 1)) {
        return ERR1;
    }
    if (GET(ptr + 2 * WSIZE) != PACK(DSIZE, 1)) {
        return ERR2;
    }

    ptr += DSIZE;
    for( ; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
        if ((long)ptr % WSIZE ) {     // 检查每个块是否对齐
            return ERR3;
        }
        if (GET(HDRP(ptr)) != GET(FTRP(ptr))) { // 检查头部和脚部一致性
            printf("not consitent ptr: %p, size(head): %d, size(foot): %d\n", ptr, GET_SIZE(HDRP(ptr)), GET_SIZE(FTRP(ptr)));
            return ERR4;
        }
        if (ptr < (char *)mem_heap_lo() || ptr > (char *)mem_heap_hi()) {
            return ERR5;
        }
    }
    return OK;
}
// 检查分离链表
static enum ERRCODE check_free_lists()
{
    unsigned int *ptr;
    enum ERRCODE ret = OK;

    // 检查分离链表中所有空闲块是否都在堆中
    for (int i = 0; i < CLASS_SIZE; ++i) {
        ptr = GET_HEAD(i);
        while (ptr != NULL) {
            if ((ret = check_ptr_in_heap((char *)ptr)) != OK) {
                return ret;
            }
            ptr = GET_SUC(ptr);
        }
    }

    // 检查数组中每个空闲块是否在链表中找到, 每个已占用块是否都不在链表里
    char *block_ptr = (char *)mem_heap_lo() + CLASS_SIZE * WSIZE + 2 * WSIZE;

    for( ; GET_SIZE(HDRP(block_ptr)) != 0; block_ptr = NEXT_BLKP(block_ptr)) {
        int alloc = GET_ALLOC(HDRP(block_ptr));
        if (alloc) {
            if (find_ptr_in_free_lists(block_ptr)) {
                return ERR8;
            }
        } else {
            if (!find_ptr_in_free_lists(block_ptr)) {
                printf("ERR9: ptr: %p\n", block_ptr);
                return ERR9;
            }
        }
    }
    return OK;
}

// 检查堆, 正确返回OK，否则返回错误吗
static enum ERRCODE check_heap()
{
    enum ERRCODE ret = OK;
    if ((ret = check_heap_arr()) != OK) {
        return ret;
    }

    if ((ret = check_free_lists()) != OK) {
        return ret;
    }
    return OK;
}
// 打印堆数组
static void print_heap_arr()
{
    char *head;
    char *ptr = (char *)mem_heap_lo();
    char *heap_start = heap_listp - (2+CLASS_SIZE)*WSIZE;
    
    printf("=================================ARRAY BEGIN====================================\n");
    for (int i = 0; i < CLASS_SIZE; ++i) {
        head = (char *)(long)(GET(heap_start + i * WSIZE));
        printf("|%p|", head);
    }
    printf("|\n");

    ptr = heap_start + (CLASS_SIZE * WSIZE) + DSIZE;
    for ( ; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
        printf("(%p, %d, %d) ->", ptr, GET_SIZE(HDRP(ptr)), GET_ALLOC(HDRP(ptr)));
    }
    printf("end\n");
    printf("===============================ARRAY END====================================\n\n");
}
static void mm_print(char *func)
{
    printf("\n\n===FUNC: %s\n", func);
    print_free_lists();
    print_heap_arr();
}
static int mm_check(char *func)
{
#ifdef DEBUG
    mm_print(func);
    enum ERRCODE errCode;
    if ((errCode = check_heap()) != OK) {
        printf("*********************************check_heap failed! errCode: %d***************************************\n", errCode);
        exit(1);
    }
#endif
    return 1;
}
// 打印分离链表
static void print_free_lists()
{
    printf("==============================FREE LIST BEGIN===============================\n");
    for (int i = 0; i < CLASS_SIZE; ++i) {
        print_list(i);
    }
    printf("==============================FREE LIST END=================================\n");

}
// 针对malloc检查
static void mm_check_malloc(void *p, size_t size)
{
#ifdef DEBUG
    int tmpsize;
    char *head = (char *)mem_heap_lo() + CLASS_SIZE*WSIZE + DSIZE;
    for ( ; GET_SIZE(HDRP(head)) != 0; head = NEXT_BLKP(head)) {
        if (head != p) {
            continue;
        }
        tmpsize = GET_SIZE(HDRP(head));
        if(tmpsize >= size) {
            return;
        }
        printf("mm_check_malloc failed!, size %d should be smaller than %d in heap_arr\n", size, tmpsize);
        exit(-1);
    }
    printf("mm_check_malloc failed!, can't find %p in heap_arr!\n", p);
    exit(-1);
#endif
}

// 针对mm_free做检查, 判断要释放的指针是否可以在堆数组中找到
static enum ERRCODE check_free(void *ptr)
{
    if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
        return ERR5;
    }

    char *head = (char *)mem_heap_lo() + CLASS_SIZE*WSIZE + DSIZE;
    for ( ; GET_SIZE(HDRP(head)) != 0; head = NEXT_BLKP(head)) {
        if (head == ptr) {
            return OK;
        }
    }
    return ERR10;
}

// 针对free检查
static void mm_check_free(void *ptr)
{
#ifdef DEBUG
    enum ERRCODE errCode;
    if ((errCode = check_free(ptr)) != OK) {
        printf("*********************************check_free failed! errCode: %d***************************************\n", errCode);
        exit(-1);
    }
#endif
}

/*================END CHECK==================*/



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
    heap_listp += ((2+CLASS_SIZE)*WSIZE);
    
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
 * search - 找到块大小对于的等价类的序号
 */
int search(size_t size){
    int i;
    for(i=4;i<=22;i++){//!在4到22之间计算 因为内存通常不会超过2的22字节
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
    char *list_head_ptr = heap_listp - (2+CLASS_SIZE)*WSIZE + class*WSIZE;
    
    if(GET_HEAD(class)==NULL){//为头
        PUT(list_head_ptr, bp);//设置为链表头
        PUT(bp,NULL);//前驱
        PUT((char *)bp+WSIZE,NULL);//后继
    }else{//否则插入到头节点
        PUT(bp,NULL);//bp前驱
        PUT((char *)bp+WSIZE,GET_HEAD(class));//bp后继
        PUT(GET_HEAD(class),bp);//原来头的前驱
        PUT(list_head_ptr, bp);//修改链表数组中的链表头
    }
}

/*
 * delete - 删除块，清理指针
 */
void delete(void* bp){
    if (bp == NULL) return;
    
    size_t size=GET_SIZE(HDRP(bp));//!
    int class=search(size);
    char *list_head_ptr = heap_listp - (2+CLASS_SIZE)*WSIZE + class*WSIZE;
    
    /* 
     * 唯一节点,后继为null,前驱为null 
     * 头节点设为null
     */
    if (GET_PRE(bp) == NULL && GET_SUC(bp) == NULL) { 
        PUT(list_head_ptr, NULL);
    } 
    /* 
     * 最后一个节点 
     * 前驱的后继设为null
     */
    else if (GET_PRE(bp) != NULL && GET_SUC(bp) == NULL) {
        PUT((char *)GET_PRE(bp) + WSIZE, NULL);
    } 
    /* 
     * 第一个结点 
     * 头节点设为bp的后继
     */
    else if (GET_SUC(bp) != NULL && GET_PRE(bp) == NULL){
        PUT(list_head_ptr, GET_SUC(bp));
        PUT(GET_SUC(bp), NULL);
    }
    /* 
     * 中间结点 
     * 前驱的后继设为后继
     * 后继的前驱设为前驱
     */
    else if (GET_SUC(bp) != NULL && GET_PRE(bp) != NULL) {
        PUT((char *)GET_PRE(bp) + WSIZE, GET_SUC(bp));
        PUT(GET_SUC(bp), GET_PRE(bp));
    }
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
    size_t prev_alloc = GET_ALLOC((char*)(bp)-DSIZE);
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
        void* cur = GET_HEAD(class);//!这个用void类型和unsigned int有影响吗？
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
    size_t asize = ALIGN(size + DSIZE);//!!!!!这个asize可能也可以优化（？）
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

/**********************/
/*
 * chain2prevnext - chain the free prev and next of a free blk
 */
void chain2prevnext(void *bp)
{
  void *succ_free, *prev_free;
  succ_free = GET_SUC(bp);
  prev_free = GET_PRE(bp);
  
  if (prev_free != NULL) {
    PUT((char *)prev_free + WSIZE, succ_free);
  }
  
  if (succ_free != NULL) {
    PUT(succ_free, prev_free);
  }
}


void realloc_place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    // 将剩余的块添加到空闲链表中
    insert(bp);
  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

void *realloc_coalesce(void *bp, size_t asize, int *is_next_free)
{
  void *prev_bp = PREV_BLKP(bp), *next_bp = NEXT_BLKP(bp);
  size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
  size_t next_alloc = GET_ALLOC(HDRP(next_bp));
  size_t size = GET_SIZE(HDRP(bp));
  *is_next_free = 0;

  if (prev_alloc && next_alloc) {}           /* Case 1 */

  else if (prev_alloc && !next_alloc) {      /* Case 2 */
    size += GET_SIZE(HDRP(next_bp));
    if(size >= asize)
      {
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(next_bp), PACK(size,0));
	if (GET_PRE(next_bp) != NULL || GET_SUC(next_bp) != NULL) {
	  chain2prevnext(next_bp);
	}
	*is_next_free = 1;
      }
  }

  else if (!prev_alloc && next_alloc) {      /* Case 3 */
    size += GET_SIZE(HDRP(prev_bp));
    if(size >= asize)
      {
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(prev_bp), PACK(size, 0));
	if (GET_PRE(prev_bp) != NULL || GET_SUC(prev_bp) != NULL) {
	  chain2prevnext(prev_bp);
	}
	bp = prev_bp;
      }
  }

  else {                                     /* Case 4 */
    size += GET_SIZE(HDRP(prev_bp)) +
      GET_SIZE(FTRP(next_bp));
    if(size >= asize)
      {
	PUT(HDRP(prev_bp), PACK(size, 0));
	PUT(FTRP(next_bp), PACK(size, 0));
	if (GET_PRE(next_bp) != NULL || GET_SUC(next_bp) != NULL) {
	  chain2prevnext(next_bp);
	}
	if (GET_PRE(prev_bp) != NULL || GET_SUC(prev_bp) != NULL) {
	  chain2prevnext(prev_bp);
	}
	bp = prev_bp;
      }
  }
  return bp;
}


/*
 * mm_realloc - 调整已分配块的大小，尝试原地扩展以提高性能
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t oldsize, newsize;
    
    /* 特殊情况处理 */
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    
    /* 计算新旧块大小 */
    oldsize = GET_SIZE(HDRP(ptr));
    newsize = ALIGN(size + DSIZE);
    
    /* 如果新大小小于等于旧大小，可以直接使用当前块 */
    if (newsize <= oldsize) {
        /* 如果剩余空间足够大，可以分割 */
        if (oldsize - newsize >= 2*DSIZE) {
            PUT(HDRP(ptr), PACK(newsize, 1));
            PUT(FTRP(ptr), PACK(newsize, 1));
            void *next_block = NEXT_BLKP(ptr);
            PUT(HDRP(next_block), PACK(oldsize - newsize, 0));
            PUT(FTRP(next_block), PACK(oldsize - newsize, 0));
            insert(next_block);
        }
        return ptr;
    }
    
    /* 检查下一个块是否空闲且合并后大小足够 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    
    /* 如果下一个块空闲且合并后大小足够，可以直接扩展 */
    if (!next_alloc && (oldsize + next_size >= newsize)) {
        delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(oldsize + next_size, 1));
        PUT(FTRP(ptr), PACK(oldsize + next_size, 1));
        
        /* 如果合并后空间仍然足够大，可以分割 */
        if (oldsize + next_size - newsize >= 2*DSIZE) {
            PUT(HDRP(ptr), PACK(newsize, 1));
            PUT(FTRP(ptr), PACK(newsize, 1));
            void *next_block = NEXT_BLKP(ptr);
            PUT(HDRP(next_block), PACK(oldsize + next_size - newsize, 0));
            PUT(FTRP(next_block), PACK(oldsize + next_size - newsize, 0));
            insert(next_block);
        }
        return ptr;
    }
    
    /* 检查前一个块是否空闲且合并后大小足够 */
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(ptr)));
    
    /* 如果前一个块空闲且合并后大小足够，可以向前扩展 */
    if (!prev_alloc && (prev_size + oldsize >= newsize)) {
        void *prev_block = PREV_BLKP(ptr);
        delete(prev_block);
        
        /* 复制数据到新位置 */
        memmove(prev_block, ptr, oldsize - DSIZE);
        
        PUT(HDRP(prev_block), PACK(prev_size + oldsize, 1));
        PUT(FTRP(prev_block), PACK(prev_size + oldsize, 1));
        
        /* 如果合并后空间仍然足够大，可以分割 */
        if (prev_size + oldsize - newsize >= 2*DSIZE) {
            PUT(HDRP(prev_block), PACK(newsize, 1));
            PUT(FTRP(prev_block), PACK(newsize, 1));
            void *next_block = NEXT_BLKP(prev_block);
            PUT(HDRP(next_block), PACK(prev_size + oldsize - newsize, 0));
            PUT(FTRP(next_block), PACK(prev_size + oldsize - newsize, 0));
            insert(next_block);
        }
        return prev_block;
    }
    
    /* 检查前后块都空闲且合并后大小足够的情况 */
    if (!prev_alloc && !next_alloc && (prev_size + oldsize + next_size >= newsize)) {
        void *prev_block = PREV_BLKP(ptr);
        delete(prev_block);
        delete(NEXT_BLKP(ptr));
        
        /* 复制数据到新位置 */
        memmove(prev_block, ptr, oldsize - DSIZE);
        
        PUT(HDRP(prev_block), PACK(prev_size + oldsize + next_size, 1));
        PUT(FTRP(prev_block), PACK(prev_size + oldsize + next_size, 1));
        
        /* 如果合并后空间仍然足够大，可以分割 */
        if (prev_size + oldsize + next_size - newsize >= 2*DSIZE) {
            PUT(HDRP(prev_block), PACK(newsize, 1));
            PUT(FTRP(prev_block), PACK(newsize, 1));
            void *next_block = NEXT_BLKP(prev_block);
            PUT(HDRP(next_block), PACK(prev_size + oldsize + next_size - newsize, 0));
            PUT(FTRP(next_block), PACK(prev_size + oldsize + next_size - newsize, 0));
            insert(next_block);
        }
        return prev_block;
    }
    
    /* 如果以上策略都不可行，则分配新块并复制数据 */
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    
    /* 复制数据并释放旧块 */
    size_t copySize = oldsize - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    
    return newptr;
}

/*
 * get_list_head - 获取分离链表头指针
 */
static unsigned int *get_list_head(int i) {
    return GET_HEAD(i);
}














