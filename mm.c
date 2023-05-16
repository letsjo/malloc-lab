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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define WSIZE 4 // word size
#define DSIZE 8 // double word size
#define CHUNKSIZE (1<<12)   // 4KB

#define MAX(x,y) ((x) > (y)? (x) : (y)) // (x) > (y)? (x) : (y) is a macro

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))    // (size) | (alloc) is a macro

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))   // p is a pointer   *(unsigned int *)(p) is a macro
#define PUT(p,val) (*(unsigned int *)(p) = (val))   // p is a pointer   val is a value  (val) is a macro    *(unsigned int *)(p) = (val) is a macro

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // ~0x7 = 1000 // 0x7 = 0111 
#define GET_ALLOC(p) (GET(p) & 0x1) // 0x1 = 0001

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) -WSIZE)  // header
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) -DSIZE) // footer

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))   // next block pointer   (char *)(bp) is a pointer   ((char *)(bp)-WSIZE) is a macro
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))   // previous block pointer   (char *)(bp) is a pointer   ((char *)(bp)-DSIZE) is a macro

#define ALIGNMENT 8 // alignment    8 bytes = 64 bits   64 bits is a word
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)   // (ALIGNMENT-1) = 7 = 0111   ~0x7 = 1000
#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // size_t is a type  sizeof(size_t) is a macro

static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *first_fit(size_t asize);

static char *heap_listp;    // heap_listp is a pointer

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);     // padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); // epilogue header
    heap_listp += (2*WSIZE);    // heap_listp points to prologue footer
    
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)   // extend heap
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   // asize는 할당할 메모리 블록의 크기입니다.
    size_t extendsize;  // extendsize는 힙을 확장할 크기입니다.
    char *bp;   // bp는 메모리 블록의 주소입니다.

    if (size == 0)  // size가 0이면 NULL을 반환합니다.
        return NULL;

    if (size <= DSIZE)
        asize = 2*DSIZE;    // asize는 최소 16바이트입니다.
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);  // asize는 size를 8의 배수로 올림한 값입니다.
    
    if ((bp = first_fit(asize)) != NULL) {   // find_fit 함수를 호출하여 메모리 블록을 할당합니다.
        place(bp,asize);        // place 함수를 호출하여 메모리 블록을 할당합니다.
        return bp;              // 할당된 메모리 블록의 주소를 반환합니다.
    }

    extendsize = MAX(asize,CHUNKSIZE);  // extendsize는 asize와 CHUNKSIZE 중 큰 값입니다.
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)   // extend heap
        return NULL;
    place(bp,asize);        // place 함수를 호출하여 메모리 블록을 할당합니다.
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));   // size는 이전 메모리 블록의 데이터 크기입니다.

    PUT(HDRP(bp), PACK(size,0));        // 현재 메모리 블록의 헤더를 수정합니다.
    PUT(FTRP(bp), PACK(size,0));        // 현재 메모리 블록의 푸터를 수정합니다.
    coalesce(bp);                       // 현재 메모리 블록을 합칩니다.
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size)    // ptr은 이전 메모리 블록의 주소입니다. size는 새로운 메모리 블록의 데이터 크기입니다.
{
    void *oldptr = ptr;     // 이전 메모리 블록의 주소를 저장할 변수입니다.
    void *newptr;           // 새로운 메모리 블록의 주소를 저장할 변수입니다.
    size_t copySize;        // 이전 메모리 블록의 데이터 크기를 저장할 변수입니다.
    
    newptr = mm_malloc(size);   // 새로운 메모리 블록을 할당합니다.
    if (newptr == NULL)         // 메모리 할당에 실패하면 NULL을 반환합니다.
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));   // 이전 메모리 블록의 데이터 크기를 가져옵니다.
    if (size < copySize)        // 이전 메모리 블록의 데이터 크기가 새로운 메모리 블록의 데이터 크기보다 큰 경우에
      copySize = size;          // 새로운 메모리 블록의 데이터 크기만큼만 복사합니다.
    memcpy(newptr, oldptr, copySize);   // 이전 메모리 블록의 데이터를 새로운 메모리 블록으로 복사합니다.
    mm_free(oldptr);            // 이전 메모리 블록을 해제합니다.
    return newptr;              // 새로운 메모리 블록의 포인터를 반환합니다.
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));      
    if ((csize - asize) >= (2*DSIZE)) {     // (csize - asize) is a size_t type
        PUT(HDRP(bp), PACK(asize,1));       // PACK(asize,1) is a macro
        PUT(FTRP(bp), PACK(asize,1));       // PACK(asize,1) is a macro
        bp = NEXT_BLKP(bp);                 // bp is a pointer
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else {
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}

static void *coalesce(void *bp)     // bp is a pointer      coalesce is a function
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size       = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
    /* Case 1 */    // prev_alloc = 1  next_alloc = 1
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
    /* Case 2 */  // prev_alloc = 1  next_alloc = 0
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));

    }
    else if (!prev_alloc && next_alloc) {
    /* Case 3 */  // prev_alloc = 0  next_alloc = 1
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else {
    /* Case 4 */   // prev_alloc = 0  next_alloc = 0
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2)? (words+1)*WSIZE : words*WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
    
    return coalesce(bp);
}

static void *first_fit(size_t asize)
{
    char *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;
    }
    return NULL;
}
