/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include <wtf/Noncopyable.h>
#include <pthread.h>

namespace WebCore {

class ThreadCondition;

class Mutex : Noncopyable {
friend class ThreadCondition;
public:
    Mutex() { pthread_mutex_init(&m_mutex, NULL); }
    ~Mutex() { pthread_mutex_destroy(&m_mutex); }

    int lock() { return pthread_mutex_lock(&m_mutex);}
    int tryLock() { return pthread_mutex_trylock(&m_mutex); }
    int unlock() { return pthread_mutex_unlock(&m_mutex); }
    
private:
    pthread_mutex_t m_mutex;
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
    ThreadCondition() { pthread_cond_init(&m_condition, NULL); }
    ~ThreadCondition() { pthread_cond_destroy(&m_condition); }
    
    int wait(Mutex& mutex) { return pthread_cond_wait(&m_condition, &mutex.m_mutex); }
    int signal() { return pthread_cond_signal(&m_condition); }
    int broadcast() { return pthread_cond_broadcast(&m_condition); }
    
private:
    pthread_cond_t m_condition;
};

void callOnMainThread(void (*)());

#if PLATFORM(WIN)
void initializeThreading();
#else
inline void initializeThreading()
{
}
#endif

} // namespace WebCore

#endif // Threading_h
