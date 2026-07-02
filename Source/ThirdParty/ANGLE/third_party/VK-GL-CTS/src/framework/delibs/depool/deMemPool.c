/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Memory pool management.
 *//*--------------------------------------------------------------------*/

#include "deMemPool.h"
#include "deMemory.h"
#include "deInt32.h"

#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
#include "deRandom.h"
#endif

#include <stdlib.h>
#include <string.h>

enum
{
    INITIAL_PAGE_SIZE   = 128,  /*!< Size for the first allocated memory page.            */
    MAX_PAGE_SIZE       = 8096, /*!< Maximum size for a memory page.                    */
    MEM_PAGE_BASE_ALIGN = 4     /*!< Base alignment guarantee for mem page data ptr.    */
};

typedef struct MemPage_s MemPage;

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Memory page header.
 *
 * Represent a page of memory allocate by a memory pool.
 *//*--------------------------------------------------------------------*/
struct MemPage_s
{
    int capacity;
    int bytesAllocated;

    MemPage *nextPage;
};

#if defined(DE_SUPPORT_DEBUG_POOLS)
typedef struct DebugAlloc_s DebugAlloc;

struct DebugAlloc_s
{
    void *memPtr;
    DebugAlloc *next;
};
#endif

/*--------------------------------------------------------------------*//*!
 * \brief Memory pool.
 *
 * A pool of memory from which individual memory allocations can be made.
 * The memory pools don't have a freeing operation for individual allocations,
 * but rather all of the memory allocated from a pool is freed when the pool
 * is destroyed.
 *
 * The pools can be arranged into a hierarchy. If a pool with children is
 * destroyed, all of the children are first recursively destroyed and then
 * the pool itself.
 *
 * The memory pools support a feature where individual allocations can be
 * made to simulate failure (i.e., return null). This can be enabled by
 * creating the root pool with the deMemPool_createFailingRoot() function.
 * When the feature is enabled, also creation of sub-pools occasionally
 * fails.
 *//*--------------------------------------------------------------------*/
struct deMemPool_s
{
    uint32_t flags;        /*!< Flags.                                            */
    deMemPool *parent;     /*!< Pointer to parent (null for root pools).        */
    deMemPoolUtil *util;   /*!< Utilities (callbacks etc.).                    */
    int numChildren;       /*!< Number of child pools.                            */
    deMemPool *firstChild; /*!< Pointer to first child pool in linked list.    */
    deMemPool *prevPool;   /*!< Previous pool in parent's linked list.            */
    deMemPool *nextPool;   /*!< Next pool in parent's linked list.                */

    MemPage *currentPage; /*!< Current memory page from which to allocate.    */

#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
    bool allowFailing;   /*!< Is allocation failure simulation enabled?        */
    deRandom failRandom; /*!< RNG for failing allocations.                    */
#endif
#if defined(DE_SUPPORT_DEBUG_POOLS)
    bool enableDebugAllocs;         /*!< If true, always allocates using deMalloc().    */
    DebugAlloc *debugAllocListHead; /*!< List of allocation in debug mode.                */

    int lastAllocatedIndex; /*!< Index of last allocated pool (rootPool only).    */
    int allocIndex;         /*!< Allocation index (running counter).            */
#endif
#if defined(DE_SUPPORT_POOL_MEMORY_TRACKING)
    int maxMemoryAllocated; /*!< Maximum amount of memory allocated from pools.    */
    int maxMemoryCapacity;  /*!< Maximum amount of memory allocated for pools.    */
#endif
};

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Initialize a memory page.
 * \param page        Memory page to initialize.
 * \param capacity    Capacity allocated for the memory page.
 *//*--------------------------------------------------------------------*/
static void MemPage_init(MemPage *page, size_t capacity)
{
    memset(page, 0, sizeof(MemPage));
#if defined(DE_DEBUG)
    memset(page + 1, 0xCD, capacity);
#endif
    page->capacity = (int)capacity;
}

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Create a new memory page.
 * \param capacity    Capacity for the memory page.
 * \return The created memory page (or null on failure).
 *//*--------------------------------------------------------------------*/
static MemPage *MemPage_create(size_t capacity)
{
    MemPage *page = (MemPage *)deMalloc(sizeof(MemPage) + capacity);
    if (!page)
        return NULL;

    DE_ASSERT(deIsAlignedPtr(page + 1, MEM_PAGE_BASE_ALIGN));

    MemPage_init(page, capacity);
    return page;
}

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Destroy a memory page.
 * \param page    Memory page to destroy.
 *//*--------------------------------------------------------------------*/
static void MemPage_destroy(MemPage *page)
{
#if defined(DE_DEBUG)
    /* Fill with garbage to hopefully catch dangling pointer bugs easier. */
    uint8_t *dataPtr = (uint8_t *)(page + 1);
    memset(dataPtr, 0xCD, (size_t)page->capacity);
#endif
    deFree(page);
}

/*--------------------------------------------------------------------*//*!
 * \internal
 * \brief Internal function for creating a new memory pool.
 * \param parent    Parent pool (may be null).
 * \return The created memory pool (or null on failure).
 *//*--------------------------------------------------------------------*/
static deMemPool *createPoolInternal(deMemPool *parent)
{
    deMemPool *pool;
    MemPage *initialPage;

#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
    if (parent && parent->allowFailing)
    {
        if ((deRandom_getUint32(&parent->failRandom) & 16383) <= 15)
            return NULL;
    }
#endif

    /* Init first page. */
    initialPage = MemPage_create(INITIAL_PAGE_SIZE);
    if (!initialPage)
        return NULL;

    /* Alloc pool from initial page. */
    DE_ASSERT((int)sizeof(deMemPool) <= initialPage->capacity);
    pool = (deMemPool *)(initialPage + 1);
    initialPage->bytesAllocated += (int)sizeof(deMemPool);

    memset(pool, 0, sizeof(deMemPool));
    pool->currentPage = initialPage;

    /* Register to parent. */
    pool->parent = parent;
    if (parent)
    {
        parent->numChildren++;
        if (parent->firstChild)
            parent->firstChild->prevPool = pool;
        pool->nextPool     = parent->firstChild;
        parent->firstChild = pool;
    }

    /* Get utils from parent. */
    pool->util = parent ? parent->util : NULL;

#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
    pool->allowFailing = parent ? parent->allowFailing : false;
    deRandom_init(&pool->failRandom, parent ? deRandom_getUint32(&parent->failRandom) : 0x1234abcd);
#endif

#if defined(DE_SUPPORT_DEBUG_POOLS)
    pool->enableDebugAllocs  = parent ? parent->enableDebugAllocs : false;
    pool->debugAllocListHead = NULL;

    /* Pool allocation index. */
    {
        deMemPool *root = pool;
        while (root->parent)
            root = root->parent;

        if (pool == root)
            root->lastAllocatedIndex = 0;

        pool->allocIndex = ++root->lastAllocatedIndex;

        /* \note Put the index of leaking pool here and add a breakpoint to catch leaks easily. */
        /*        if (pool->allocIndex == 51)
                    root = root;*/
    }
#endif

    return pool;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a new root memory pool.
 * \return The created memory pool (or null on failure).
 *//*--------------------------------------------------------------------*/
deMemPool *deMemPool_createRoot(const deMemPoolUtil *util, uint32_t flags)
{
    deMemPool *pool = createPoolInternal(NULL);
    if (!pool)
        return NULL;
#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
    if (flags & DE_MEMPOOL_ENABLE_FAILING_ALLOCS)
        pool->allowFailing = true;
#endif
#if defined(DE_SUPPORT_DEBUG_POOLS)
    if (flags & DE_MEMPOOL_ENABLE_DEBUG_ALLOCS)
    {
        pool->enableDebugAllocs  = true;
        pool->debugAllocListHead = NULL;
    }
#endif
    DE_UNREF(flags); /* in case no debug features enabled */

    /* Get copy of utilities. */
    if (util)
    {
        deMemPoolUtil *utilCopy = DE_POOL_NEW(pool, deMemPoolUtil);
        DE_ASSERT(util->allocFailCallback);
        if (!utilCopy)
        {
            deMemPool_destroy(pool);
            return NULL;
        }

        memcpy(utilCopy, util, sizeof(deMemPoolUtil));
        pool->util = utilCopy;
    }

    return pool;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a sub-pool for an existing memory pool.
 * \return The created memory pool (or null on failure).
 *//*--------------------------------------------------------------------*/
deMemPool *deMemPool_create(deMemPool *parent)
{
    deMemPool *pool;
    DE_ASSERT(parent);
    pool = createPoolInternal(parent);
    if (!pool && parent->util)
        parent->util->allocFailCallback(parent->util->userPointer);
    return pool;
}

/*--------------------------------------------------------------------*//*!
 * \brief Destroy a memory pool.
 * \param pool    Pool to be destroyed.
 *
 * Frees all the memory allocated from the pool. Also destroyed any child
 * pools that the pool has (recursively).
 *//*--------------------------------------------------------------------*/
void deMemPool_destroy(deMemPool *pool)
{
    deMemPool *iter;
    deMemPool *iterNext;

#if defined(DE_SUPPORT_POOL_MEMORY_TRACKING)
    /* Update memory consumption statistics. */
    if (pool->parent)
    {
        deMemPool *root = pool->parent;
        while (root->parent)
            root = root->parent;
        root->maxMemoryAllocated = deMax32(root->maxMemoryAllocated, deMemPool_getNumAllocatedBytes(root, true));
        root->maxMemoryCapacity  = deMax32(root->maxMemoryCapacity, deMemPool_getCapacity(root, true));
    }
#endif

    /* Destroy all children. */
    iter = pool->firstChild;
    while (iter)
    {
        iterNext = iter->nextPool;
        deMemPool_destroy(iter);
        iter = iterNext;
    }

    DE_ASSERT(pool->numChildren == 0);

    /* Update pointers. */
    if (pool->prevPool)
        pool->prevPool->nextPool = pool->nextPool;
    if (pool->nextPool)
        pool->nextPool->prevPool = pool->prevPool;

    if (pool->parent)
    {
        deMemPool *parent = pool->parent;
        if (parent->firstChild == pool)
            parent->firstChild = pool->nextPool;

        parent->numChildren--;
        DE_ASSERT(parent->numChildren >= 0);
    }

#if defined(DE_SUPPORT_DEBUG_POOLS)
    /* Free all debug allocations. */
    if (pool->enableDebugAllocs)
    {
        DebugAlloc *alloc = pool->debugAllocListHead;
        DebugAlloc *next;

        while (alloc)
        {
            next = alloc->next;
            deAlignedFree(alloc->memPtr);
            deFree(alloc);
            alloc = next;
        }

        pool->debugAllocListHead = NULL;
    }
#endif

    /* Free pages. */
    /* \note Pool itself is allocated from first page, so we must not touch the pool after freeing the page! */
    {
        MemPage *page = pool->currentPage;
        MemPage *nextPage;

        while (page)
        {
            nextPage = page->nextPage;
            MemPage_destroy(page);
            page = nextPage;
        }
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get the number of children for a pool.
 * \return The number of (immediate) child pools a memory pool has.
 *//*--------------------------------------------------------------------*/
int deMemPool_getNumChildren(const deMemPool *pool)
{
    return pool->numChildren;
}

/*--------------------------------------------------------------------*//*!
 * \brief Get the number of bytes allocated (by the user) from the pool.
 * \param pool        Pool pointer.
 * \param recurse    Is operation recursive to child pools?
 * \return The number of bytes allocated by the pool (including child pools
 *           if 'recurse' is true).
 *//*--------------------------------------------------------------------*/
int deMemPool_getNumAllocatedBytes(const deMemPool *pool, bool recurse)
{
    int numAllocatedBytes = 0;
    MemPage *memPage;

    for (memPage = pool->currentPage; memPage; memPage = memPage->nextPage)
        numAllocatedBytes += memPage->bytesAllocated;

    if (recurse)
    {
        deMemPool *child;
        for (child = pool->firstChild; child; child = child->nextPool)
            numAllocatedBytes += deMemPool_getNumAllocatedBytes(child, true);
    }

    return numAllocatedBytes;
}

int deMemPool_getCapacity(const deMemPool *pool, bool recurse)
{
    int numCapacityBytes = 0;
    MemPage *memPage;

    for (memPage = pool->currentPage; memPage; memPage = memPage->nextPage)
        numCapacityBytes += memPage->capacity;

    if (recurse)
    {
        deMemPool *child;
        for (child = pool->firstChild; child; child = child->nextPool)
            numCapacityBytes += deMemPool_getCapacity(child, true);
    }

    return numCapacityBytes;
}

void *deMemPool_allocInternal(deMemPool *pool, size_t numBytes, uint32_t alignBytes)
{
    MemPage *curPage = pool->currentPage;

#if defined(DE_SUPPORT_FAILING_POOL_ALLOC)
    if (pool->allowFailing)
    {
        if ((deRandom_getUint32(&pool->failRandom) & 16383) <= 15)
            return NULL;
    }
#endif

#if defined(DE_SUPPORT_DEBUG_POOLS)
    if (pool->enableDebugAllocs)
    {
        DebugAlloc *header = DE_NEW(DebugAlloc);
        void *ptr          = deAlignedMalloc(numBytes, alignBytes);

        if (!header || !ptr)
        {
            deFree(header);
            deAlignedFree(ptr);
            return NULL;
        }

        header->memPtr           = ptr;
        header->next             = pool->debugAllocListHead;
        pool->debugAllocListHead = header;

        return ptr;
    }
#endif

    DE_ASSERT(curPage);
    DE_ASSERT(deIsPowerOfTwo32((int)alignBytes));
    {
        void *curPagePtr    = (void *)((uint8_t *)(curPage + 1) + curPage->bytesAllocated);
        void *alignedPtr    = deAlignPtr(curPagePtr, alignBytes);
        size_t alignPadding = (size_t)((uintptr_t)alignedPtr - (uintptr_t)curPagePtr);

        if (numBytes + alignPadding > (size_t)(curPage->capacity - curPage->bytesAllocated))
        {
            /* Does not fit to current page. */
            int maxAlignPadding = deMax32(0, ((int)alignBytes) - MEM_PAGE_BASE_ALIGN);
            int newPageCapacity =
                deMax32(deMin32(2 * curPage->capacity, MAX_PAGE_SIZE), ((int)numBytes) + maxAlignPadding);

            curPage = MemPage_create((size_t)newPageCapacity);
            if (!curPage)
                return NULL;

            curPage->nextPage = pool->currentPage;
            pool->currentPage = curPage;

            DE_ASSERT(curPage->bytesAllocated == 0);

            curPagePtr   = (void *)(curPage + 1);
            alignedPtr   = deAlignPtr(curPagePtr, alignBytes);
            alignPadding = (size_t)((uintptr_t)alignedPtr - (uintptr_t)curPagePtr);

            DE_ASSERT(numBytes + alignPadding <= (size_t)curPage->capacity);
        }

        curPage->bytesAllocated += (int)(numBytes + alignPadding);
        return alignedPtr;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Allocate memory from a pool.
 * \param pool        Memory pool to allocate from.
 * \param numBytes    Number of bytes to allocate.
 * \return Pointer to the allocate memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void *deMemPool_alloc(deMemPool *pool, size_t numBytes)
{
    void *ptr;
    DE_ASSERT(pool);
    DE_ASSERT(numBytes > 0);
    ptr = deMemPool_allocInternal(pool, numBytes, DE_POOL_DEFAULT_ALLOC_ALIGNMENT);
    if (!ptr && pool->util)
        pool->util->allocFailCallback(pool->util->userPointer);
    return ptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Allocate aligned memory from a pool.
 * \param pool            Memory pool to allocate from.
 * \param numBytes        Number of bytes to allocate.
 * \param alignBytes    Required alignment in bytes, must be power of two.
 * \return Pointer to the allocate memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void *deMemPool_alignedAlloc(deMemPool *pool, size_t numBytes, uint32_t alignBytes)
{
    void *ptr;
    DE_ASSERT(pool);
    DE_ASSERT(numBytes > 0);
    DE_ASSERT(deIsPowerOfTwo32((int)alignBytes));
    ptr = deMemPool_allocInternal(pool, numBytes, alignBytes);
    DE_ASSERT(deIsAlignedPtr(ptr, alignBytes));
    if (!ptr && pool->util)
        pool->util->allocFailCallback(pool->util->userPointer);
    return ptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Duplicate a piece of memory into a memory pool.
 * \param pool    Memory pool to allocate from.
 * \param ptr    Piece of memory to duplicate.
 * \return Pointer to the copied memory block (or null on failure).
 *//*--------------------------------------------------------------------*/
void *deMemPool_memDup(deMemPool *pool, const void *ptr, size_t numBytes)
{
    void *newPtr = deMemPool_alloc(pool, numBytes);
    if (newPtr)
        memcpy(newPtr, ptr, numBytes);
    return newPtr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Duplicate a string into a memory pool.
 * \param pool    Memory pool to allocate from.
 * \param str    String to duplicate.
 * \return Pointer to the new string (or null on failure).
 *//*--------------------------------------------------------------------*/
char *deMemPool_strDup(deMemPool *pool, const char *str)
{
    size_t len   = strlen(str);
    char *newStr = (char *)deMemPool_alloc(pool, len + 1);
    if (newStr)
        memcpy(newStr, str, len + 1);
    return newStr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Duplicate a string into a memory pool, with a maximum length.
 * \param pool        Memory pool to allocate from.
 * \param str        String to duplicate.
 * \param maxLength    Maximum number of characters to duplicate.
 * \return Pointer to the new string (or null on failure).
 *//*--------------------------------------------------------------------*/
char *deMemPool_strnDup(deMemPool *pool, const char *str, int maxLength)
{
    size_t len   = (size_t)deMin32((int)strlen(str), deMax32(0, maxLength));
    char *newStr = (char *)deMemPool_alloc(pool, len + 1);

    DE_ASSERT(maxLength >= 0);

    if (newStr)
    {
        memcpy(newStr, str, len);
        newStr[len] = 0;
    }
    return newStr;
}

#if defined(DE_SUPPORT_POOL_MEMORY_TRACKING)

int deMemPool_getMaxNumAllocatedBytes(const deMemPool *pool)
{
    DE_ASSERT(pool && !pool->parent); /* must be root */
    return deMax32(pool->maxMemoryAllocated, deMemPool_getNumAllocatedBytes(pool, true));
}

int deMemPool_getMaxCapacity(const deMemPool *pool)
{
    DE_ASSERT(pool && !pool->parent); /* must be root */
    return deMax32(pool->maxMemoryCapacity, deMemPool_getCapacity(pool, true));
}

#endif
