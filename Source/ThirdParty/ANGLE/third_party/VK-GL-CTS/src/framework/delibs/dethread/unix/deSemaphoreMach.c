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
 * \brief Mach implementation of semaphore.
 *//*--------------------------------------------------------------------*/

#include "deSemaphore.h"

#if (DE_OS == DE_OS_IOS || DE_OS == DE_OS_OSX)

#include "deMemory.h"

#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach_init.h>

DE_STATIC_ASSERT(sizeof(deSemaphore) >= sizeof(semaphore_t));

deSemaphore deSemaphore_create(int initialValue, const deSemaphoreAttributes *attributes)
{
    semaphore_t sem;

    DE_UNREF(attributes);
    DE_ASSERT(initialValue >= 0);

    if (semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO, initialValue) != KERN_SUCCESS)
        return 0;

    return (deSemaphore)sem;
}

void deSemaphore_destroy(deSemaphore semaphore)
{
    semaphore_t sem   = (semaphore_t)semaphore;
    kern_return_t res = semaphore_destroy(mach_task_self(), sem);
    DE_ASSERT(res == 0);
    DE_UNREF(res);
}

void deSemaphore_increment(deSemaphore semaphore)
{
    semaphore_t sem   = (semaphore_t)semaphore;
    kern_return_t res = semaphore_signal(sem);
    DE_ASSERT(res == 0);
    DE_UNREF(res);
}

void deSemaphore_decrement(deSemaphore semaphore)
{
    semaphore_t sem   = (semaphore_t)semaphore;
    kern_return_t res = semaphore_wait(sem);
    DE_ASSERT(res == 0);
    DE_UNREF(res);
}

bool deSemaphore_tryDecrement(deSemaphore semaphore)
{
    semaphore_t sem    = (semaphore_t)semaphore;
    mach_timespec_t ts = {0, 1}; /* one nanosecond */
    return (semaphore_timedwait(sem, ts) == KERN_SUCCESS);
}

#endif /* DE_OS */
