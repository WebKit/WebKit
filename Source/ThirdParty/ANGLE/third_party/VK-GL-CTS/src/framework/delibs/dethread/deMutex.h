#ifndef _DEMUTEX_H
#define _DEMUTEX_H
/*-------------------------------------------------------------------------
 * drawElements Thread Library
 * ---------------------------
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
 * \brief Mutex class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

typedef uintptr_t deMutex;

/*--------------------------------------------------------------------*//*!
 * \brief Mutex attributes.
 *//*--------------------------------------------------------------------*/
typedef enum deMutexFlag_e
{
    /**
     * \brief Create recursive mutex.
     *
     * Recursive mutex can be locked multiple times from single thread
     * and it is unlocked when lock count from owning thread reaches
     * zero.
     */
    DE_MUTEX_RECURSIVE = (1 << 0)
} deMutexFlag;

/*--------------------------------------------------------------------*//*!
 * \brief Mutex attributes.
 *//*--------------------------------------------------------------------*/
typedef struct deMutexAttributes_s
{
    uint32_t flags;
} deMutexAttributes;

deMutex deMutex_create(const deMutexAttributes *attributes);
void deMutex_destroy(deMutex mutex);
void deMutex_lock(deMutex mutex);
bool deMutex_tryLock(deMutex mutex);
void deMutex_unlock(deMutex mutex);

DE_END_EXTERN_C

#endif /* _DEMUTEX_H */
