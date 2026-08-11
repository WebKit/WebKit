#ifndef _DEMEMPOOL_H
#define _DEMEMPOOL_H
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

#include "deDefs.h"

#if defined(DE_DEV_BUILD)
/** Enable support for failure-simulating pool allocations. */
#define DE_SUPPORT_FAILING_POOL_ALLOC

/** Enable support for debug pools, which directly pass all allocations to deMalloc(). */
#define DE_SUPPORT_DEBUG_POOLS

/** Enable support for memory tracking of pools (will collect the maximum memory consumption to root pool). */
#define DE_SUPPORT_POOL_MEMORY_TRACKING
#endif /* DE_DEV_BUILD */

typedef enum deMemPoolFlag_e
{
    DE_MEMPOOL_ENABLE_FAILING_ALLOCS = (1 << 0),
    DE_MEMPOOL_ENABLE_DEBUG_ALLOCS   = (1 << 1)
} deMemPoolFlag;

enum
{
    DE_POOL_DEFAULT_ALLOC_ALIGNMENT = DE_PTR_SIZE /*!< Default alignment for pool allocations (in bytes). */
};

/** Macro for allocating a new struct from a pool (leaves it uninitialized!). */
#define DE_POOL_NEW(POOL, TYPE) ((TYPE *)deMemPool_alloc(POOL, sizeof(TYPE)))

typedef void (*deMemPoolAllocFailFunc)(void *userPtr);

typedef struct deMemPoolUtil_s
{
    void *userPointer;
    deMemPoolAllocFailFunc allocFailCallback;
} deMemPoolUtil;

typedef struct deMemPool_s deMemPool;

DE_BEGIN_EXTERN_C

deMemPool *deMemPool_createRoot(const deMemPoolUtil *util, uint32_t flags);
deMemPool *deMemPool_create(deMemPool *parent);
void deMemPool_destroy(deMemPool *pool);
int deMemPool_getNumChildren(const deMemPool *pool);
int deMemPool_getNumAllocatedBytes(const deMemPool *pool, bool recurse);
int deMemPool_getCapacity(const deMemPool *pool, bool recurse);

void *deMemPool_alloc(deMemPool *pool, size_t numBytes);
void *deMemPool_alignedAlloc(deMemPool *pool, size_t numBytes, uint32_t alignBytes);
void *deMemPool_memDup(deMemPool *pool, const void *ptr, size_t numBytes);
char *deMemPool_strDup(deMemPool *pool, const char *str);
char *deMemPool_strnDup(deMemPool *pool, const char *str, int maxLength);

#if defined(DE_SUPPORT_POOL_MEMORY_TRACKING)
int deMemPool_getMaxNumAllocatedBytes(const deMemPool *pool);
int deMemPool_getMaxCapacity(const deMemPool *pool);
#endif

DE_END_EXTERN_C

#endif /* _DEMEMPOOL_H */
