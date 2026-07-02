#ifndef _DEMEMORY_H
#define _DEMEMORY_H
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

#include "deDefs.h"

#include <string.h>

DE_BEGIN_EXTERN_C

#define DE_NEW(TYPE) ((TYPE *)deMalloc(sizeof(TYPE)))
#define DE_DELETE(TYPE, PTR) deFree(PTR)

void *deMalloc(size_t numBytes);
void *deCalloc(size_t numBytes);
void *deRealloc(void *ptr, size_t numBytes);
void deFree(void *ptr);

void *deAlignedMalloc(size_t numBytes, size_t alignBytes);
void *deAlignedRealloc(void *ptr, size_t numBytes, size_t alignBytes);
void deAlignedFree(void *ptr);

char *deStrdup(const char *str);

/*--------------------------------------------------------------------*//*!
 * \brief Fill a block of memory with an 8-bit value.
 * \param ptr        Pointer to memory to free.
 * \param value        Value to fill with.
 * \param numBytes    Number of bytes to write.
 *//*--------------------------------------------------------------------*/
static inline void deMemset(void *ptr, int value, size_t numBytes)
{
    DE_ASSERT((value & 0xFF) == value);
    memset(ptr, value, numBytes);
}

static inline int deMemCmp(const void *a, const void *b, size_t numBytes)
{
    return memcmp(a, b, numBytes);
}

/*--------------------------------------------------------------------*//*!
 * \brief Copy bytes between buffers
 * \param dst        Destination buffer
 * \param src        Source buffer
 * \param numBytes    Number of bytes to copy
 * \return Destination buffer.
 *//*--------------------------------------------------------------------*/
static inline void *deMemcpy(void *dst, const void *src, size_t numBytes)
{
    return memcpy(dst, src, numBytes);
}

static inline void *deMemmove(void *dst, const void *src, size_t numBytes)
{
    return memmove(dst, src, numBytes);
}

void deMemory_selfTest(void);

DE_END_EXTERN_C

#endif /* _DEMEMORY_H */
