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
 * \brief Unix implementation of thread management.
 *//*--------------------------------------------------------------------*/

#include "deThread.h"

#if (DE_OS == DE_OS_UNIX || DE_OS == DE_OS_OSX || DE_OS == DE_OS_ANDROID || DE_OS == DE_OS_SYMBIAN || \
     DE_OS == DE_OS_IOS || DE_OS == DE_OS_QNX || DE_OS == DE_OS_FUCHSIA)

#include "deMemory.h"
#include "deInt32.h"

#if !defined(_XOPEN_SOURCE) || (_XOPEN_SOURCE < 500)
#error "You are using too old posix API!"
#endif

#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
#include <sys/syscall.h>
#endif

#if (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
#if !defined(_SC_NPROCESSORS_CONF)
#define _SC_NPROCESSORS_CONF 57
#endif
#if !defined(_SC_NPROCESSORS_ONLN)
#define _SC_NPROCESSORS_ONLN 58
#endif
#endif

typedef struct Thread_s
{
    pthread_t thread;
    deThreadFunc func;
    void *arg;
} Thread;

DE_STATIC_ASSERT(sizeof(deThread) >= sizeof(Thread *));

static void *startThread(void *entryPtr)
{
    Thread *thread    = (Thread *)entryPtr;
    deThreadFunc func = thread->func;
    void *arg         = thread->arg;

    /* Start actual thread. */
    func(arg);

    return NULL;
}

deThread deThread_create(deThreadFunc func, void *arg, const deThreadAttributes *attributes)
{
    pthread_attr_t attr;
    Thread *thread = (Thread *)deCalloc(sizeof(Thread));

    if (!thread)
        return 0;

    thread->func = func;
    thread->arg  = arg;

    if (pthread_attr_init(&attr) != 0)
    {
        deFree(thread);
        return 0;
    }

    /* \todo [2009-11-12 pyry] Map attributes. */
    DE_UNREF(attributes);

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        pthread_attr_destroy(&attr);
        deFree(thread);
        return 0;
    }

    if (pthread_create(&thread->thread, &attr, startThread, thread) != 0)
    {
        pthread_attr_destroy(&attr);
        deFree(thread);
        return 0;
    }
    DE_ASSERT(thread->thread);

    pthread_attr_destroy(&attr);

    return (deThread)thread;
}

bool deThread_join(deThread threadptr)
{
    Thread *thread = (Thread *)threadptr;
    int ret;

    DE_ASSERT(thread->thread);
    ret = pthread_join(thread->thread, NULL);

    /* If join fails for some reason, at least mark as detached. */
    if (ret != 0)
        pthread_detach(thread->thread);

    /* Thread is no longer valid as far as we are concerned. */
    thread->thread = 0;

    return (ret == 0);
}

void deThread_destroy(deThread threadptr)
{
    Thread *thread = (Thread *)threadptr;

    if (thread->thread)
    {
        /* Not joined, detach. */
        int ret = pthread_detach(thread->thread);
        DE_ASSERT(ret == 0);
        DE_UNREF(ret);
    }

    deFree(thread);
}

void deSleep(uint32_t milliseconds)
{
    /* Maximum value for usleep is 10^6. */
    uint32_t seconds = milliseconds / 1000;

    milliseconds = milliseconds - seconds * 1000;

    if (seconds > 0)
        sleep(seconds);

    usleep((useconds_t)milliseconds * (useconds_t)1000);
}

void deYield(void)
{
    sched_yield();
}

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)

uint32_t deGetNumAvailableLogicalCores(void)
{
#if !defined(__FreeBSD__)
    unsigned long mask          = 0;
    const unsigned int maskSize = sizeof(mask);
    long ret;

    deMemset(&mask, 0, sizeof(mask));

    ret = syscall(__NR_sched_getaffinity, 0, maskSize, &mask);

    if (ret > 0)
    {
        return (uint32_t)dePop64(mask);
    }
    else
    {
#endif
#if defined(_SC_NPROCESSORS_ONLN)
        const long count = sysconf(_SC_NPROCESSORS_ONLN);

        if (count <= 0)
            return 1;
        else
            return (uint32_t)count;
#else
    return 1;
#endif

#if !defined(__FreeBSD__)
    }
#endif
}

#else

uint32_t deGetNumAvailableLogicalCores(void)
{
#if defined(_SC_NPROCESSORS_ONLN)
    const long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count <= 0)
        return 1;
    else
        return (uint32_t)count;
#else
    return 1;
#endif
}

#endif

uint32_t deGetNumTotalLogicalCores(void)
{
#if defined(_SC_NPROCESSORS_CONF)
    const long count = sysconf(_SC_NPROCESSORS_CONF);

    if (count <= 0)
        return 1;
    else
        return (uint32_t)count;
#else
    return 1;
#endif
}

uint32_t deGetNumTotalPhysicalCores(void)
{
    /* \todo [2015-04-09 pyry] Parse /proc/cpuinfo perhaps? */
    return deGetNumTotalLogicalCores();
}

#endif /* DE_OS */
