/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef Threading_h
#define Threading_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

#if USE(PTHREADS)
#include <pthread.h>
#endif

#if PLATFORM(GTK)
typedef struct _GMutex GMutex;
typedef struct _GCond GCond;
#endif

#if PLATFORM(QT)
class QMutex;
class QWaitCondition;
#endif

#include <stdint.h>

namespace WebCore {

typedef uint32_t ThreadIdentifier;
typedef void* (*ThreadFunction)(void* argument);

// Returns 0 if thread creation failed
ThreadIdentifier createThread(ThreadFunction, void*);
ThreadIdentifier currentThread();
int waitForThreadCompletion(ThreadIdentifier, void**);
void detachThread(ThreadIdentifier);

#if USE(PTHREADS)
typedef pthread_mutex_t PlatformMutex;
typedef pthread_cond_t PlatformCondition;
#elif PLATFORM(GTK)
typedef GMutex* PlatformMutex;
typedef GCond* PlatformCondition;
#elif PLATFORM(QT)
typedef QMutex* PlatformMutex;
typedef QWaitCondition* PlatformCondition;
#else
typedef void* PlatformMutex;
typedef void* PlatformCondition;
#endif
    
class Mutex : Noncopyable {
public:
    Mutex();
    ~Mutex();

    void lock();
    bool tryLock();
    void unlock();

public:
    PlatformMutex& impl() { return m_mutex; }
private:
    PlatformMutex m_mutex;
};

class MutexLocker : Noncopyable {
public:
    MutexLocker(Mutex& mutex) : m_mutex(mutex) { m_mutex.lock(); }
    ~MutexLocker() { m_mutex.unlock(); }

private:
    Mutex& m_mutex;
};

class ThreadCondition : Noncopyable {
public:
    ThreadCondition();
    ~ThreadCondition();
    
    void wait(Mutex& mutex);
    void signal();
    void broadcast();
    
private:
    PlatformCondition m_condition;
};
    
template<class T> class ThreadSafeShared : Noncopyable {
public:
    ThreadSafeShared()
        : m_refCount(0)
#ifndef NDEBUG
        , m_inDestructor(0)
#endif
    {
    }

    void ref()
    {
        MutexLocker locker(m_mutex);
        ASSERT(!m_inDestructor);
        ++m_refCount;
    }

    void deref()
    {
        {
            MutexLocker locker(m_mutex);
            ASSERT(!m_inDestructor);
            --m_refCount;
        }
        
        if (m_refCount <= 0) {
#ifndef NDEBUG
            m_inDestructor = true;
#endif
            delete static_cast<T*>(this);
        }
    }

    bool hasOneRef()
    {
        MutexLocker locker(m_mutex);
        ASSERT(!m_inDestructor);
        return m_refCount == 1;
    }

    int refCount() const
    {
        MutexLocker locker(m_mutex);
        return m_refCount;
    }

    bool isThreadSafe() { return true; }
    
private:
    mutable Mutex m_mutex;
    int m_refCount;
#ifndef NDEBUG
    bool m_inDestructor;
#endif
};

void callOnMainThread(void (*)());

void initializeThreading();

#if !PLATFORM(WIN) && !PLATFORM(GTK)
inline void initializeThreading()
{
}
#endif

} // namespace WebCore

#endif // Threading_h
