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
 * \brief Unix implementation of semaphore.
 *//*--------------------------------------------------------------------*/

#include "deSemaphore.h"

#if (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_ANDROID || DE_OS == DE_OS_SYMBIAN || DE_OS == DE_OS_QNX) || \
    (DE_OS == DE_OS_FUCHSIA)

#include "deMemory.h"

#include <semaphore.h>

DE_STATIC_ASSERT(sizeof(deSemaphore) >= sizeof(sem_t *));

deSemaphore deSemaphore_create(int initialValue, const deSemaphoreAttributes *attributes)
{
    sem_t *sem = (sem_t *)deMalloc(sizeof(sem_t));

    DE_UNREF(attributes);
    DE_ASSERT(initialValue >= 0);

    if (!sem)
        return 0;

    if (sem_init(sem, 0, (unsigned int)initialValue) != 0)
    {
        deFree(sem);
        return 0;
    }

    return (deSemaphore)sem;
}

void deSemaphore_destroy(deSemaphore semaphore)
{
    sem_t *sem = (sem_t *)semaphore;
    DE_ASSERT(sem);
    sem_destroy(sem);
    deFree(sem);
}

void deSemaphore_increment(deSemaphore semaphore)
{
    sem_t *sem = (sem_t *)semaphore;
    int ret    = sem_post(sem);
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

void deSemaphore_decrement(deSemaphore semaphore)
{
    sem_t *sem = (sem_t *)semaphore;
    int ret    = sem_wait(sem);
    DE_ASSERT(ret == 0);
    DE_UNREF(ret);
}

bool deSemaphore_tryDecrement(deSemaphore semaphore)
{
    sem_t *sem = (sem_t *)semaphore;
    DE_ASSERT(sem);
    return (sem_trywait(sem) == 0);
}

#endif /* DE_OS */
