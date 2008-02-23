/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include <windows.h>
#include <wtf/HashMap.h>

namespace WTF {

static Mutex& threadMapMutex()
{
    static Mutex mutex;
    return mutex;
}

static HashMap<DWORD, HANDLE>& threadMap()
{
    static HashMap<DWORD, HANDLE> map;
    return map;
}

static void storeThreadHandleByIdentifier(DWORD threadID, HANDLE threadHandle)
{
    MutexLocker locker(threadMapMutex());
    threadMap().add(threadID, threadHandle);
}

static HANDLE threadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());
    return threadMap().get(id);
}

static void clearThreadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());
    ASSERT(threadMap().contains(id));
    threadMap().remove(id);
}

ThreadIdentifier createThread(ThreadFunction entryPoint, void* data)
{
    DWORD threadIdentifier = 0;
    ThreadIdentifier threadID = 0;
    HANDLE hEvent = ::CreateEvent(0, FALSE, FALSE, 0);
    HANDLE threadHandle = ::CreateThread(0, 0, (LPTHREAD_START_ROUTINE)entryPoint, data, 0, &threadIdentifier);
    if (!threadHandle) {
        LOG_ERROR("Failed to create thread at entry point %p with data %p", entryPoint, data);
        return 0;
    }

    threadID = static_cast<ThreadIdentifier>(threadIdentifier);
    storeThreadHandleByIdentifier(threadIdentifier, threadHandle);

    return threadID;
}

int waitForThreadCompletion(ThreadIdentifier threadID, void** result)
{
    ASSERT(threadID);
    
    HANDLE threadHandle = threadHandleForIdentifier(threadID);
    if (!threadHandle)
        LOG_ERROR("ThreadIdentifier %u did not correspond to an active thread when trying to quit", threadID);
 
    DWORD joinResult = ::WaitForSingleObject(threadHandle, INFINITE);
    if (joinResult == WAIT_FAILED)
        LOG_ERROR("ThreadIdentifier %u was found to be deadlocked trying to quit", threadID);

    ::CloseHandle(threadHandle);
    clearThreadHandleForIdentifier(threadID);

    return joinResult;
}

void detachThread(ThreadIdentifier threadID)
{
    ASSERT(threadID);
    
    HANDLE threadHandle = threadHandleForIdentifier(threadID);
    if (threadHandle)
        ::CloseHandle(threadHandle);
    clearThreadHandleForIdentifier(threadID);
}

ThreadIdentifier currentThread()
{
    return static_cast<ThreadIdentifier>(::GetCurrentThreadId());
}

Mutex::Mutex()
{
    m_mutex.m_recursionCount = 0;
    ::InitializeCriticalSection(&m_mutex.m_internalMutex);
}

Mutex::~Mutex()
{
    ::DeleteCriticalSection(&m_mutex.m_internalMutex);
}

void Mutex::lock()
{
    ::EnterCriticalSection(&m_mutex.m_internalMutex);
    ++m_mutex.m_recursionCount;
}
    
bool Mutex::tryLock()
{
    // This method is modeled after the behavior of pthread_mutex_trylock,
    // which will return an error if the lock is already owned by the
    // current thread.  Since the primitive Win32 'TryEnterCriticalSection'
    // treats this as a successful case, it changes the behavior of several
    // tests in WebKit that check to see if the current thread already
    // owned this mutex (see e.g., IconDatabase::getOrCreateIconRecord)
    DWORD result = ::TryEnterCriticalSection(&m_mutex.m_internalMutex);
    
    if (result != 0) {       // We got the lock
        // If this thread already had the lock, we must unlock and
        // return false so that we mimic the behavior of POSIX's
        // pthread_mutex_trylock:
        if (m_mutex.m_recursionCount > 0) {
            ::LeaveCriticalSection(&m_mutex.m_internalMutex);
            return false;
        }

        ++m_mutex.m_recursionCount;
        return true;
    }

    return false;
}

void Mutex::unlock()
{
    --m_mutex.m_recursionCount;
    ::LeaveCriticalSection(&m_mutex.m_internalMutex);
}

} // namespace WTF
