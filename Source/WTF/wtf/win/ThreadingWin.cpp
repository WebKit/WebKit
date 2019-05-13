/*
 * Copyright (C) 2007, 2008, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. All rights reserved.
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

/*
 * There are numerous academic and practical works on how to implement pthread_cond_wait/pthread_cond_signal/pthread_cond_broadcast
 * functions on Win32. Here is one example: http://www.cs.wustl.edu/~schmidt/win32-cv-1.html which is widely credited as a 'starting point'
 * of modern attempts. There are several more or less proven implementations, one in Boost C++ library (http://www.boost.org) and another
 * in pthreads-win32 (http://sourceware.org/pthreads-win32/).
 *
 * The number of articles and discussions is the evidence of significant difficulties in implementing these primitives correctly.
 * The brief search of revisions, ChangeLog entries, discussions in comp.programming.threads and other places clearly documents
 * numerous pitfalls and performance problems the authors had to overcome to arrive to the suitable implementations.
 * Optimally, WebKit would use one of those supported/tested libraries directly. To roll out our own implementation is impractical,
 * if even for the lack of sufficient testing. However, a faithful reproduction of the code from one of the popular supported
 * libraries seems to be a good compromise.
 *
 * The early Boost implementation (http://www.boxbackup.org/trac/browser/box/nick/win/lib/win32/boost_1_32_0/libs/thread/src/condition.cpp?rev=30)
 * is identical to pthreads-win32 (http://sourceware.org/cgi-bin/cvsweb.cgi/pthreads/pthread_cond_wait.c?rev=1.10&content-type=text/x-cvsweb-markup&cvsroot=pthreads-win32).
 * Current Boost uses yet another (although seemingly equivalent) algorithm which came from their 'thread rewrite' effort.
 *
 * This file includes timedWait/signal/broadcast implementations translated to WebKit coding style from the latest algorithm by
 * Alexander Terekhov and Louis Thomas, as captured here: http://sourceware.org/cgi-bin/cvsweb.cgi/pthreads/pthread_cond_wait.c?rev=1.10&content-type=text/x-cvsweb-markup&cvsroot=pthreads-win32
 * It replaces the implementation of their previous algorithm, also documented in the same source above.
 * The naming and comments are left very close to original to enable easy cross-check.
 *
 * The corresponding Pthreads-win32 License is included below, and CONTRIBUTORS file which it refers to is added to
 * source directory (as CONTRIBUTORS.pthreads-win32).
 */

/*
 *      Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "config.h"
#include <wtf/Threading.h>

#include <errno.h>
#include <process.h>
#include <windows.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadingPrimitives.h>

namespace WTF {

static Lock globalSuspendLock;

Thread::~Thread()
{
    // It is OK because FLSAlloc's callback will be called even before there are some open handles.
    // This easily ensures that all the thread resources are automatically closed.
    if (m_handle != INVALID_HANDLE_VALUE)
        CloseHandle(m_handle);
}

void Thread::initializeCurrentThreadEvenIfNonWTFCreated()
{
}

// MS_VC_EXCEPTION, THREADNAME_INFO, and setThreadNameInternal all come from <http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx>.
static const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
    DWORD dwType; // must be 0x1000
    LPCSTR szName; // pointer to name (in user addr space)
    DWORD dwThreadID; // thread ID (-1=caller thread)
    DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;
#pragma pack(pop)

void Thread::initializeCurrentThreadInternal(const char* szThreadName)
{
#if COMPILER(MINGW)
    // FIXME: Implement thread name setting with MingW.
    UNUSED_PARAM(szThreadName);
#else
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = Thread::normalizeThreadName(szThreadName);
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
    } __except(EXCEPTION_CONTINUE_EXECUTION) { }
#endif
    initializeCurrentThreadEvenIfNonWTFCreated();
}

void Thread::initializePlatformThreading()
{
}

static unsigned __stdcall wtfThreadEntryPoint(void* data)
{
    Thread::entryPoint(reinterpret_cast<Thread::NewThreadContext*>(data));
    return 0;
}

bool Thread::establishHandle(NewThreadContext* data)
{
    unsigned threadIdentifier = 0;
    HANDLE threadHandle = reinterpret_cast<HANDLE>(_beginthreadex(0, 0, wtfThreadEntryPoint, data, 0, &threadIdentifier));
    if (!threadHandle) {
        LOG_ERROR("Failed to create thread at entry point %p with data %p: %ld", wtfThreadEntryPoint, data, errno);
        return false;
    }
    establishPlatformSpecificHandle(threadHandle, threadIdentifier);
    return true;
}

void Thread::changePriority(int delta)
{
    auto locker = holdLock(m_mutex);
    SetThreadPriority(m_handle, THREAD_PRIORITY_NORMAL + delta);
}

int Thread::waitForCompletion()
{
    HANDLE handle;
    {
        auto locker = holdLock(m_mutex);
        handle = m_handle;
    }

    DWORD joinResult = WaitForSingleObject(handle, INFINITE);
    if (joinResult == WAIT_FAILED)
        LOG_ERROR("Thread %p was found to be deadlocked trying to quit", this);

    auto locker = holdLock(m_mutex);
    ASSERT(joinableState() == Joinable);

    // The thread has already exited, do nothing.
    // The thread hasn't exited yet, so don't clean anything up. Just signal that we've already joined on it so that it will clean up after itself.
    if (!hasExited())
        didJoin();

    return joinResult;
}

void Thread::detach()
{
    // We follow the pthread semantics: even after the detach is called,
    // we can still perform various operations onto the thread. For example,
    // we can do pthread_kill for the detached thread. The problem in Windows
    // is that closing HANDLE loses the way to do such operations.
    // To do so, we do nothing here in Windows. Original detach's purpose,
    // releasing thread resource when the thread exits, will be achieved by
    // FlsCallback automatically. FlsCallback will call CloseHandle to clean up
    // resource. So in this function, we just mark the thread as detached to
    // avoid calling waitForCompletion for this thread.
    auto locker = holdLock(m_mutex);
    if (!hasExited())
        didBecomeDetached();
}

auto Thread::suspend() -> Expected<void, PlatformSuspendError>
{
    RELEASE_ASSERT_WITH_MESSAGE(this != &Thread::current(), "We do not support suspending the current thread itself.");
    LockHolder locker(globalSuspendLock);
    DWORD result = SuspendThread(m_handle);
    if (result != (DWORD)-1)
        return { };
    return makeUnexpected(result);
}

// During resume, suspend or resume should not be executed from the other threads.
void Thread::resume()
{
    LockHolder locker(globalSuspendLock);
    ResumeThread(m_handle);
}

size_t Thread::getRegisters(PlatformRegisters& registers)
{
    LockHolder locker(globalSuspendLock);
    registers.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    GetThreadContext(m_handle, &registers);
    return sizeof(CONTEXT);
}

Thread& Thread::initializeCurrentTLS()
{
    // Not a WTF-created thread, ThreadIdentifier is not established yet.
    Ref<Thread> thread = adoptRef(*new Thread());

    HANDLE handle;
    bool isSuccessful = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    RELEASE_ASSERT(isSuccessful);

    thread->establishPlatformSpecificHandle(handle, currentID());
    thread->initializeInThread();
    initializeCurrentThreadEvenIfNonWTFCreated();

    return initializeTLS(WTFMove(thread));
}

ThreadIdentifier Thread::currentID()
{
    return static_cast<ThreadIdentifier>(GetCurrentThreadId());
}

void Thread::establishPlatformSpecificHandle(HANDLE handle, ThreadIdentifier threadID)
{
    auto locker = holdLock(m_mutex);
    m_handle = handle;
    m_id = threadID;
}

#define InvalidThread reinterpret_cast<Thread*>(static_cast<uintptr_t>(0xbbadbeef))

static WordLock threadMapMutex;

static HashMap<ThreadIdentifier, Thread*>& threadMap()
{
    static NeverDestroyed<HashMap<ThreadIdentifier, Thread*>> map;
    return map.get();
}

void Thread::initializeTLSKey()
{
    threadMap();
    threadSpecificKeyCreate(&s_key, destructTLS);
}

Thread* Thread::currentDying()
{
    ASSERT(s_key != InvalidThreadSpecificKey);
    // After FLS is destroyed, this map offers the value until the second thread exit callback is called.
    auto locker = holdLock(threadMapMutex);
    return threadMap().get(currentID());
}

RefPtr<Thread> Thread::get(ThreadIdentifier id)
{
    auto locker = holdLock(threadMapMutex);
    Thread* thread = threadMap().get(id);
    if (thread)
        return thread;
    return nullptr;
}

Thread& Thread::initializeTLS(Ref<Thread>&& thread)
{
    ASSERT(s_key != InvalidThreadSpecificKey);
    // FIXME: Remove this workaround code once <rdar://problem/31793213> is fixed.
    auto id = thread->id();
    // We leak the ref to keep the Thread alive while it is held in TLS. destructTLS will deref it later at thread destruction time.
    auto& threadInTLS = thread.leakRef();
    threadSpecificSet(s_key, &threadInTLS);
    {
        auto locker = holdLock(threadMapMutex);
        threadMap().add(id, &threadInTLS);
    }
    return threadInTLS;
}

void Thread::destructTLS(void* data)
{
    if (data == InvalidThread)
        return;

    Thread* thread = static_cast<Thread*>(data);
    ASSERT(thread);

    // Delay the deallocation of Thread more.
    // It defers Thread deallocation after the other ThreadSpecific values are deallocated.
    static thread_local class ThreadExitCallback {
    public:
        ThreadExitCallback(Thread* thread)
            : m_thread(thread)
        {
        }

        ~ThreadExitCallback()
        {
            Thread::destructTLS(m_thread);
        }

    private:
        Thread* m_thread;
    } callback(thread);

    if (thread->m_isDestroyedOnce) {
        {
            auto locker = holdLock(threadMapMutex);
            ASSERT(threadMap().contains(thread->id()));
            threadMap().remove(thread->id());
        }
        thread->didExit();
        thread->deref();

        // Fill the FLS with the non-nullptr value. While FLS destructor won't be called for that,
        // non-nullptr value tells us that we already destructed Thread. This allows us to
        // detect incorrect use of Thread::current() after this point because it will crash.
        threadSpecificSet(s_key, InvalidThread);
        return;
    }
    threadSpecificSet(s_key, InvalidThread);
    thread->m_isDestroyedOnce = true;
}

Mutex::~Mutex()
{
}

void Mutex::lock()
{
    AcquireSRWLockExclusive(&m_mutex);
}

bool Mutex::tryLock()
{
    return TryAcquireSRWLockExclusive(&m_mutex);
}

void Mutex::unlock()
{
    ReleaseSRWLockExclusive(&m_mutex);
}

// Returns an interval in milliseconds suitable for passing to one of the Win32 wait functions (e.g., ::WaitForSingleObject).
static DWORD absoluteTimeToWaitTimeoutInterval(WallTime absoluteTime)
{
    WallTime currentTime = WallTime::now();

    // Time is in the past - return immediately.
    if (absoluteTime < currentTime)
        return 0;

    // Time is too far in the future (and would overflow unsigned long) - wait forever.
    if ((absoluteTime - currentTime) > Seconds::fromMilliseconds(INT_MAX))
        return INFINITE;

    return static_cast<DWORD>((absoluteTime - currentTime).milliseconds());
}

ThreadCondition::~ThreadCondition()
{
}

void ThreadCondition::wait(Mutex& mutex)
{
    SleepConditionVariableSRW(&m_condition, &mutex.impl(), INFINITE, 0);
}

bool ThreadCondition::timedWait(Mutex& mutex, WallTime absoluteTime)
{
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms686304(v=vs.85).aspx
    DWORD interval = absoluteTimeToWaitTimeoutInterval(absoluteTime);
    if (!interval) {
        // Consider the wait to have timed out, even if our condition has already been signaled, to
        // match the pthreads implementation.
        return false;
    }

    if (SleepConditionVariableSRW(&m_condition, &mutex.impl(), interval, 0))
        return true;
    ASSERT(GetLastError() == ERROR_TIMEOUT);
    return false;
}

void ThreadCondition::signal()
{
    WakeConditionVariable(&m_condition);
}

void ThreadCondition::broadcast()
{
    WakeAllConditionVariable(&m_condition);
}

void Thread::yield()
{
    SwitchToThread();
}

} // namespace WTF
