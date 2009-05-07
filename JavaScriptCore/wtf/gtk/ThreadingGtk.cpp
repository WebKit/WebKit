/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Threading.h"

#if !USE(PTHREADS)

#include "CurrentTime.h"
#include "HashMap.h"
#include "MainThread.h"
#include "RandomNumberSeed.h"

#include <glib.h>
#include <limits.h>

namespace WTF {

bool ThreadIdentifier::operator==(const ThreadIdentifier& another) const
{
    return m_platformId == another.m_platformId;
}

bool ThreadIdentifier::operator!=(const ThreadIdentifier& another) const
{
    return m_platformId != another.m_platformId;
}

static Mutex* atomicallyInitializedStaticMutex;

static ThreadIdentifier mainThreadIdentifier;

void initializeThreading()
{
    if (!g_thread_supported())
        g_thread_init(NULL);
    ASSERT(g_thread_supported());

    if (!atomicallyInitializedStaticMutex) {
        atomicallyInitializedStaticMutex = new Mutex;
        initializeRandomNumberGenerator();
        mainThreadIdentifier = currentThread();
        initializeMainThread();
    }
}

void lockAtomicallyInitializedStaticMutex()
{
    ASSERT(atomicallyInitializedStaticMutex);
    atomicallyInitializedStaticMutex->lock();
}

void unlockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex->unlock();
}

ThreadIdentifier createThreadInternal(ThreadFunction entryPoint, void* data, const char*)
{
    GThread* thread;
    if (!(thread = g_thread_create(entryPoint, data, TRUE, 0))) {
        LOG_ERROR("Failed to create thread at entry point %p with data %p", entryPoint, data);
        return ThreadIdentifier();
    }

    return ThreadIdentifier(thread);
}

void setThreadNameInternal(const char*)
{
}

int waitForThreadCompletion(ThreadIdentifier threadID, void** result)
{
    ASSERT(threadID.isValid());

    GThread* thread = threadID.platformId();

    void* joinResult = g_thread_join(thread);
    if (result)
        *result = joinResult;

    return 0;
}

void detachThread(ThreadIdentifier)
{
}

ThreadIdentifier currentThread()
{
    return ThreadIdentifier(g_thread_self());
}

bool isMainThread()
{
    return currentThread() == mainThreadIdentifier;
}

Mutex::Mutex()
    : m_mutex(g_mutex_new())
{
}

Mutex::~Mutex()
{
}

void Mutex::lock()
{
    g_mutex_lock(m_mutex.get());
}

bool Mutex::tryLock()
{
    return g_mutex_trylock(m_mutex.get());
}

void Mutex::unlock()
{
    g_mutex_unlock(m_mutex.get());
}

ThreadCondition::ThreadCondition()
    : m_condition(g_cond_new())
{
}

ThreadCondition::~ThreadCondition()
{
}

void ThreadCondition::wait(Mutex& mutex)
{
    g_cond_wait(m_condition.get(), mutex.impl().get());
}

bool ThreadCondition::timedWait(Mutex& mutex, double absoluteTime)
{
    // Time is in the past - return right away.
    if (absoluteTime < currentTime())
        return false;
    
    // Time is too far in the future for g_cond_timed_wait - wait forever.
    if (absoluteTime > INT_MAX) {
        wait(mutex);
        return true;
    }

    int timeSeconds = static_cast<int>(absoluteTime);
    int timeMicroseconds = static_cast<int>((absoluteTime - timeSeconds) * 1000000.0);
    
    GTimeVal targetTime;
    targetTime.tv_sec = timeSeconds;
    targetTime.tv_usec = timeMicroseconds;

    return g_cond_timed_wait(m_condition.get(), mutex.impl().get(), &targetTime);
}

void ThreadCondition::signal()
{
    g_cond_signal(m_condition.get());
}

void ThreadCondition::broadcast()
{
    g_cond_broadcast(m_condition.get());
}


}

#endif // !USE(PTHREADS)
