/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#if USE(PTHREADS)

#include "CurrentTime.h"
#include "HashMap.h"
#include "MainThread.h"
#include "RandomNumberSeed.h"
#include "StdLibExtras.h"
#include "ThreadIdentifierDataPthreads.h"
#include "ThreadSpecific.h"
#include "UnusedParam.h"
#include <errno.h>

#if !COMPILER(MSVC)
#include <limits.h>
#include <sys/time.h>
#endif

#if OS(ANDROID)
#include "jni_utility.h"
#endif

namespace WTF {

typedef HashMap<ThreadIdentifier, pthread_t> ThreadMap;

static Mutex* atomicallyInitializedStaticMutex;

#if !OS(DARWIN) || PLATFORM(CHROMIUM) || USE(WEB_THREAD)
static pthread_t mainThread; // The thread that was the first to call initializeThreading(), which must be the main thread.
#endif

void clearPthreadHandleForIdentifier(ThreadIdentifier);

static Mutex& threadMapMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

void initializeThreading()
{
    if (!atomicallyInitializedStaticMutex) {
        atomicallyInitializedStaticMutex = new Mutex;
        threadMapMutex();
        initializeRandomNumberGenerator();
#if !OS(DARWIN) || PLATFORM(CHROMIUM) || USE(WEB_THREAD)
        mainThread = pthread_self();
#endif
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

static ThreadMap& threadMap()
{
    DEFINE_STATIC_LOCAL(ThreadMap, map, ());
    return map;
}

static ThreadIdentifier identifierByPthreadHandle(const pthread_t& pthreadHandle)
{
    MutexLocker locker(threadMapMutex());

    ThreadMap::iterator i = threadMap().begin();
    for (; i != threadMap().end(); ++i) {
        if (pthread_equal(i->second, pthreadHandle))
            return i->first;
    }

    return 0;
}

static ThreadIdentifier establishIdentifierForPthreadHandle(const pthread_t& pthreadHandle)
{
    ASSERT(!identifierByPthreadHandle(pthreadHandle));

    MutexLocker locker(threadMapMutex());

    static ThreadIdentifier identifierCount = 1;

    threadMap().add(identifierCount, pthreadHandle);

    return identifierCount++;
}

static pthread_t pthreadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());

    return threadMap().get(id);
}

void clearPthreadHandleForIdentifier(ThreadIdentifier id)
{
    MutexLocker locker(threadMapMutex());

    ASSERT(threadMap().contains(id));

    threadMap().remove(id);
}

#if OS(ANDROID)
// On the Android platform, threads must be registered with the VM before they run.
struct ThreadData {
    ThreadFunction entryPoint;
    void* arg;
};

static void* runThreadWithRegistration(void* arg)
{
    ThreadData* data = static_cast<ThreadData*>(arg);
    JavaVM* vm = JSC::Bindings::getJavaVM();
    JNIEnv* env;
    void* ret = 0;
    if (vm->AttachCurrentThread(&env, 0) == JNI_OK) {
        ret = data->entryPoint(data->arg);
        vm->DetachCurrentThread();
    }
    delete data;
    return ret;
}

ThreadIdentifier createThreadInternal(ThreadFunction entryPoint, void* data, const char*)
{
    pthread_t threadHandle;
    ThreadData* threadData = new ThreadData();
    threadData->entryPoint = entryPoint;
    threadData->arg = data;

    if (pthread_create(&threadHandle, 0, runThreadWithRegistration, static_cast<void*>(threadData))) {
        LOG_ERROR("Failed to create pthread at entry point %p with data %p", entryPoint, data);
        delete threadData;
        return 0;
    }
    return establishIdentifierForPthreadHandle(threadHandle);
}
#else
ThreadIdentifier createThreadInternal(ThreadFunction entryPoint, void* data, const char*)
{
    pthread_t threadHandle;
    if (pthread_create(&threadHandle, 0, entryPoint, data)) {
        LOG_ERROR("Failed to create pthread at entry point %p with data %p", entryPoint, data);
        return 0;
    }

    return establishIdentifierForPthreadHandle(threadHandle);
}
#endif

void initializeCurrentThreadInternal(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    pthread_setname_np(threadName);
#else
    UNUSED_PARAM(threadName);
#endif

    ThreadIdentifier id = identifierByPthreadHandle(pthread_self());
    ASSERT(id);
    ThreadIdentifierData::initialize(id);
}

int waitForThreadCompletion(ThreadIdentifier threadID, void** result)
{
    ASSERT(threadID);

    pthread_t pthreadHandle = pthreadHandleForIdentifier(threadID);
    if (!pthreadHandle)
        return 0;

    int joinResult = pthread_join(pthreadHandle, result);
    if (joinResult == EDEADLK)
        LOG_ERROR("ThreadIdentifier %u was found to be deadlocked trying to quit", threadID);

    return joinResult;
}

void detachThread(ThreadIdentifier threadID)
{
    ASSERT(threadID);

    pthread_t pthreadHandle = pthreadHandleForIdentifier(threadID);
    if (!pthreadHandle)
        return;

    pthread_detach(pthreadHandle);
}

ThreadIdentifier currentThread()
{
    ThreadIdentifier id = ThreadIdentifierData::identifier();
    if (id)
        return id;

    // Not a WTF-created thread, ThreadIdentifier is not established yet.
    id = establishIdentifierForPthreadHandle(pthread_self());
    ThreadIdentifierData::initialize(id);
    return id;
}

bool isMainThread()
{
#if OS(DARWIN) && !PLATFORM(CHROMIUM) && !USE(WEB_THREAD)
    return pthread_main_np();
#else
    return pthread_equal(pthread_self(), mainThread);
#endif
}

Mutex::Mutex()
{
    pthread_mutex_init(&m_mutex, NULL);
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void Mutex::lock()
{
    int result = pthread_mutex_lock(&m_mutex);
    ASSERT_UNUSED(result, !result);
}

bool Mutex::tryLock()
{
    int result = pthread_mutex_trylock(&m_mutex);

    if (result == 0)
        return true;
    if (result == EBUSY)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

void Mutex::unlock()
{
    int result = pthread_mutex_unlock(&m_mutex);
    ASSERT_UNUSED(result, !result);
}

#if HAVE(PTHREAD_RWLOCK)
ReadWriteLock::ReadWriteLock()
{
    pthread_rwlock_init(&m_readWriteLock, NULL);
}

ReadWriteLock::~ReadWriteLock()
{
    pthread_rwlock_destroy(&m_readWriteLock);
}

void ReadWriteLock::readLock()
{
    int result = pthread_rwlock_rdlock(&m_readWriteLock);
    ASSERT_UNUSED(result, !result);
}

bool ReadWriteLock::tryReadLock()
{
    int result = pthread_rwlock_tryrdlock(&m_readWriteLock);

    if (result == 0)
        return true;
    if (result == EBUSY || result == EAGAIN)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

void ReadWriteLock::writeLock()
{
    int result = pthread_rwlock_wrlock(&m_readWriteLock);
    ASSERT_UNUSED(result, !result);
}

bool ReadWriteLock::tryWriteLock()
{
    int result = pthread_rwlock_trywrlock(&m_readWriteLock);

    if (result == 0)
        return true;
    if (result == EBUSY || result == EAGAIN)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

void ReadWriteLock::unlock()
{
    int result = pthread_rwlock_unlock(&m_readWriteLock);
    ASSERT_UNUSED(result, !result);
}
#endif  // HAVE(PTHREAD_RWLOCK)

ThreadCondition::ThreadCondition()
{ 
    pthread_cond_init(&m_condition, NULL);
}

ThreadCondition::~ThreadCondition()
{
    pthread_cond_destroy(&m_condition);
}
    
void ThreadCondition::wait(Mutex& mutex)
{
    int result = pthread_cond_wait(&m_condition, &mutex.impl());
    ASSERT_UNUSED(result, !result);
}

bool ThreadCondition::timedWait(Mutex& mutex, double absoluteTime)
{
    if (absoluteTime < currentTime())
        return false;

    if (absoluteTime > INT_MAX) {
        wait(mutex);
        return true;
    }

    int timeSeconds = static_cast<int>(absoluteTime);
    int timeNanoseconds = static_cast<int>((absoluteTime - timeSeconds) * 1E9);

    timespec targetTime;
    targetTime.tv_sec = timeSeconds;
    targetTime.tv_nsec = timeNanoseconds;

    return pthread_cond_timedwait(&m_condition, &mutex.impl(), &targetTime) == 0;
}

void ThreadCondition::signal()
{
    int result = pthread_cond_signal(&m_condition);
    ASSERT_UNUSED(result, !result);
}

void ThreadCondition::broadcast()
{
    int result = pthread_cond_broadcast(&m_condition);
    ASSERT_UNUSED(result, !result);
}

} // namespace WTF

#endif // USE(PTHREADS)
