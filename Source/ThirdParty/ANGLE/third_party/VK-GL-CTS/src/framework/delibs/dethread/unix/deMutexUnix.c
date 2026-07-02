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
 * \brief Unix implementation of mutex.
 *//*--------------------------------------------------------------------*/

#include "deMutex.h"

#if (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_ANDROID || DE_OS == DE_OS_SYMBIAN || DE_OS == DE_OS_QNX || \
     DE_OS == DE_OS_OSX || DE_OS == DE_OS_IOS) ||                                                     \
    (DE_OS == DE_OS_FUCHSIA)

#include "deMemory.h"

#include <pthread.h>

/* \todo [2009-11-12 pyry] It is quite nasty to allocate mutex structs from heap. */

DE_STATIC_ASSERT(sizeof(deMutex) >= sizeof(pthread_mutex_t *));

deMutex deMutex_create(const deMutexAttributes *attributes)
{
    pthread_mutexattr_t attr;
    int ret;
    pthread_mutex_t *mutex = deMalloc(sizeof(pthread_mutex_t));

    if (!mutex)
        return 0;

    if (pthread_mutexattr_init(&attr) != 0)
    {
        deFree(mutex);
        return 0;
    }

#if defined(DE_DEBUG)
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0)
#else
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) != 0)
#endif
    {
        pthread_mutexattr_destroy(&attr);
        deFree(mutex);
        return 0;
    }

    if (attributes)
    {
        if (attributes->flags & DE_MUTEX_RECURSIVE)
        {
            if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
            {
                pthread_mutexattr_destroy(&attr);
                deFree(mutex);
                return 0;
            }
        }
    }

    ret = pthread_mutex_init(mutex, &attr);
    if (ret != 0)
    {
        pthread_mutexattr_destroy(&attr);
        deFree(mutex);
        return 0;
    }

    pthread_mutexattr_destroy(&attr);

    return (deMutex)mutex;
}

void deMutex_destroy(deMutex mutex)
{
    pthread_mutex_t *pMutex = (pthread_mutex_t *)mutex;
    DE_ASSERT(pMutex);
    pthread_mutex_destroy(pMutex);
    deFree(pMutex);
}

void deMutex_lock(deMutex mutex)
{
    int ret = pthread_mutex_lock((pthread_mutex_t *)mutex);
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

void deMutex_unlock(deMutex mutex)
{
    int ret = pthread_mutex_unlock((pthread_mutex_t *)mutex);
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

bool deMutex_tryLock(deMutex mutex)
{
    return (pthread_mutex_trylock((pthread_mutex_t *)mutex) == 0);
}

#endif /* DE_OS */
