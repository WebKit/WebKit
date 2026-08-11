/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief Memory management.
 *//*--------------------------------------------------------------------*/

#include "deMemory.h"
#include "deInt32.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define DE_ALIGNED_MALLOC_POSIX 0
#define DE_ALIGNED_MALLOC_WIN32 1
#define DE_ALIGNED_MALLOC_GENERIC 2

#if (DE_OS == DE_OS_UNIX) || ((DE_OS == DE_OS_ANDROID) && (DE_ANDROID_API >= 21))
#define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_POSIX
#if defined(__FreeBSD__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#elif (DE_OS == DE_OS_WIN32)
#define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_WIN32
#include <malloc.h>
#else
#define DE_ALIGNED_MALLOC DE_ALIGNED_MALLOC_GENERIC
#endif

#if defined(DE_VALGRIND_BUILD)
#include <valgrind/valgrind.h>
#if defined(HAVE_VALGRIND_MEMCHECK_H)
#include <valgrind/memcheck.h>
#endif
#endif

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Allocate a chunk of memory.
 * \param numBytes    Number of bytes to allocate.
 * \return Pointer to the allocated memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void *deMalloc(size_t numBytes)
{
    void *ptr;

    DE_ASSERT(numBytes > 0);

    ptr = malloc((size_t)numBytes);

#if defined(DE_DEBUG)
    /* Trash memory in debug builds (if under Valgrind, don't make it think we're initializing data here). */

    if (ptr)
        memset(ptr, 0xcd, numBytes);

#if defined(DE_VALGRIND_BUILD) && defined(HAVE_VALGRIND_MEMCHECK_H)
    if (ptr && RUNNING_ON_VALGRIND)
    {
        VALGRIND_MAKE_MEM_UNDEFINED(ptr, numBytes);
    }
#endif
#endif

    return ptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Allocate a chunk of memory and initialize it to zero.
 * \param numBytes    Number of bytes to allocate.
 * \return Pointer to the allocated memory (or null on failure).
 *//*--------------------------------------------------------------------*/
void *deCalloc(size_t numBytes)
{
    void *ptr = deMalloc(numBytes);
    if (ptr)
        deMemset(ptr, 0, numBytes);
    return ptr;
}

/*--------------------------------------------------------------------*//*!
 * \brief Reallocate a chunk of memory.
 * \param ptr        Pointer to previously allocated memory block
 * \param numBytes    New size in bytes
 * \return Pointer to the reallocated (and possibly moved) memory block
 *//*--------------------------------------------------------------------*/
void *deRealloc(void *ptr, size_t numBytes)
{
    return realloc(ptr, numBytes);
}

/*--------------------------------------------------------------------*//*!
 * \brief Free a chunk of memory.
 * \param ptr    Pointer to memory to free.
 *//*--------------------------------------------------------------------*/
void deFree(void *ptr)
{
    free(ptr);
}

#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)

typedef struct AlignedAllocHeader_s
{
    void *basePtr;
    size_t numBytes;
} AlignedAllocHeader;

static inline AlignedAllocHeader *getAlignedAllocHeader(void *ptr)
{
    const size_t hdrSize    = sizeof(AlignedAllocHeader);
    const uintptr_t hdrAddr = (uintptr_t)ptr - hdrSize;

    return (AlignedAllocHeader *)hdrAddr;
}

#endif

void *deAlignedMalloc(size_t numBytes, size_t alignBytes)
{
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_POSIX)
    /* posix_memalign() requires that alignment must be 2^N * sizeof(void*) */
    const size_t ptrAlignedAlign = deAlignSize(alignBytes, sizeof(void *));
    void *ptr                    = NULL;

    DE_ASSERT(deIsPowerOfTwoSize(alignBytes) && deIsPowerOfTwoSize(ptrAlignedAlign / sizeof(void *)));

    if (posix_memalign(&ptr, ptrAlignedAlign, numBytes) == 0)
    {
        DE_ASSERT(ptr);
        return ptr;
    }
    else
    {
        DE_ASSERT(!ptr);
        return NULL;
    }

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_WIN32)
    DE_ASSERT(deIsPowerOfTwoSize(alignBytes));

    return _aligned_malloc(numBytes, alignBytes);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)
    void *const basePtr = deMalloc(numBytes + alignBytes + sizeof(AlignedAllocHeader));

    DE_ASSERT(deIsPowerOfTwoSize(alignBytes));

    if (basePtr)
    {
        void *const alignedPtr = deAlignPtr((void *)((uintptr_t)basePtr + sizeof(AlignedAllocHeader)), alignBytes);
        AlignedAllocHeader *const hdr = getAlignedAllocHeader(alignedPtr);

        hdr->basePtr  = basePtr;
        hdr->numBytes = numBytes;

        return alignedPtr;
    }
    else
        return NULL;
#else
#error "Invalid DE_ALIGNED_MALLOC"
#endif
}

void *deAlignedRealloc(void *ptr, size_t numBytes, size_t alignBytes)
{
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_WIN32)
    return _aligned_realloc(ptr, numBytes, alignBytes);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC) || (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_POSIX)
    if (ptr)
    {
        if (numBytes > 0)
        {
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)
            const size_t oldSize = getAlignedAllocHeader(ptr)->numBytes;
#else /* DE_ALIGNED_MALLOC_GENERIC */
            const size_t oldSize = malloc_usable_size(ptr);
#endif

            DE_ASSERT(deIsAlignedPtr(ptr, alignBytes));

            if (oldSize < numBytes || oldSize > numBytes * 2)
            {
                /* Create a new alloc if original is smaller, or more than twice the requested size */
                void *const newPtr = deAlignedMalloc(numBytes, alignBytes);

                if (newPtr)
                {
                    const size_t copyBytes = numBytes < oldSize ? numBytes : oldSize;

                    deMemcpy(newPtr, ptr, copyBytes);
                    deAlignedFree(ptr);

                    return newPtr;
                }
                else
                    return NULL;
            }
            else
                return ptr;
        }
        else
        {
            deAlignedFree(ptr);
            return NULL;
        }
    }
    else
        return deAlignedMalloc(numBytes, alignBytes);

#else
#error "Invalid DE_ALIGNED_MALLOC"
#endif
}

void deAlignedFree(void *ptr)
{
#if (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_POSIX)
    free(ptr);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_WIN32)
    _aligned_free(ptr);

#elif (DE_ALIGNED_MALLOC == DE_ALIGNED_MALLOC_GENERIC)
    if (ptr)
    {
        AlignedAllocHeader *const hdr = getAlignedAllocHeader(ptr);

        deFree(hdr->basePtr);
    }
#else
#error "Invalid DE_ALIGNED_MALLOC"
#endif
}

char *deStrdup(const char *str)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _strdup(str);
#elif (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
    /* For some reason Steve doesn't like stdrup(). */
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = 0;
    return copy;
#else
    return strdup(str);
#endif
}

void deMemory_selfTest(void)
{
    static const struct
    {
        size_t numBytes;
        size_t alignment;
    } s_alignedAllocCases[] = {
        {1, 1},        {1, 2},         {1, 256},        {1, 4096},        {547389, 1},      {547389, 2},
        {547389, 256}, {547389, 4096}, {52532, 1 << 4}, {52532, 1 << 10}, {52532, 1 << 16},
    };
    static const struct
    {
        size_t initialSize;
        size_t newSize;
        size_t alignment;
    } s_alignedReallocCases[] = {
        {1, 1, 1},
        {1, 1, 2},
        {1, 1, 256},
        {1, 1, 4096},
        {1, 1241, 1},
        {1, 1241, 2},
        {1, 1241, 256},
        {1, 1241, 4096},
        {547389, 234, 1},
        {547389, 234, 2},
        {547389, 234, 256},
        {547389, 234, 4096},
        {52532, 421523, 1 << 4},
        {52532, 421523, 1 << 10},
        {52532, 421523, 1 << 16},
    };

    int caseNdx;

    for (caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_alignedAllocCases); caseNdx++)
    {
        void *const ptr =
            deAlignedMalloc(s_alignedAllocCases[caseNdx].numBytes, s_alignedAllocCases[caseNdx].alignment);

        DE_TEST_ASSERT(ptr);
        DE_TEST_ASSERT(deIsAlignedPtr(ptr, s_alignedAllocCases[caseNdx].alignment));

        deMemset(ptr, 0xaa, s_alignedAllocCases[caseNdx].numBytes);

        deAlignedFree(ptr);
    }

    for (caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_alignedReallocCases); caseNdx++)
    {
        void *const ptr =
            deAlignedMalloc(s_alignedReallocCases[caseNdx].initialSize, s_alignedReallocCases[caseNdx].alignment);

        DE_TEST_ASSERT(ptr);
        DE_TEST_ASSERT(deIsAlignedPtr(ptr, s_alignedReallocCases[caseNdx].alignment));

        deMemset(ptr, 0xaa, s_alignedReallocCases[caseNdx].initialSize);

        {
            void *const newPtr =
                deAlignedRealloc(ptr, s_alignedReallocCases[caseNdx].newSize, s_alignedReallocCases[caseNdx].alignment);
            const size_t numPreserved =
                s_alignedReallocCases[caseNdx].newSize < s_alignedReallocCases[caseNdx].initialSize ?
                    s_alignedReallocCases[caseNdx].newSize :
                    s_alignedReallocCases[caseNdx].initialSize;
            size_t off;

            DE_TEST_ASSERT(newPtr);
            DE_TEST_ASSERT(deIsAlignedPtr(ptr, s_alignedReallocCases[caseNdx].alignment));

            for (off = 0; off < numPreserved; off++)
                DE_TEST_ASSERT(*((const uint8_t *)newPtr + off) == 0xaa);

            deAlignedFree(newPtr);
        }
    }
}

DE_END_EXTERN_C
