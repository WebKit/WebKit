/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef AsyncTask_h
#define AsyncTask_h

#include "BAssert.h"
#include "Inline.h"
#include "Mutex.h"
#include <atomic>
#include <condition_variable>
#include <pthread.h>
#include <thread>

namespace bmalloc {

template<typename Object, typename Function>
class AsyncTask {
public:
    AsyncTask(Object&, const Function&);

    void run();
    void join();

private:
    enum State { Exited, Sleeping, Running, Signaled };

    static const constexpr std::chrono::seconds exitDelay = std::chrono::seconds(1);

    void runSlowCase();

    static void* pthreadEntryPoint(void*);
    void entryPoint();

    std::atomic<State> m_state;

    Mutex m_conditionMutex;
    std::condition_variable_any m_condition;
    pthread_t m_thread;

    Object& m_object;
    Function m_function;
};

template<typename Object, typename Function> const std::chrono::seconds AsyncTask<Object, Function>::exitDelay;

template<typename Object, typename Function>
AsyncTask<Object, Function>::AsyncTask(Object& object, const Function& function)
    : m_state(Exited)
    , m_condition()
    , m_thread()
    , m_object(object)
    , m_function(function)
{
}

template<typename Object, typename Function>
void AsyncTask<Object, Function>::join()
{
    if (m_state == Exited)
        return;

    { std::lock_guard<Mutex> lock(m_conditionMutex); }
    m_condition.notify_one();

    while (m_state != Exited)
        std::this_thread::yield();
}

template<typename Object, typename Function>
inline void AsyncTask<Object, Function>::run()
{
    if (m_state == Signaled)
        return;
    runSlowCase();
}

template<typename Object, typename Function>
NO_INLINE void AsyncTask<Object, Function>::runSlowCase()
{
    State oldState = m_state.exchange(Signaled);
    if (oldState == Signaled || oldState == Running)
        return;

    if (oldState == Sleeping) {
        { std::lock_guard<Mutex> lock(m_conditionMutex); }
        m_condition.notify_one();
        return;
    }

    BASSERT(oldState == Exited);
    pthread_create(&m_thread, nullptr, &pthreadEntryPoint, this);
    pthread_detach(m_thread);
}

template<typename Object, typename Function>
void* AsyncTask<Object, Function>::pthreadEntryPoint(void* asyncTask)
{
    static_cast<AsyncTask*>(asyncTask)->entryPoint();
    return nullptr;
}

template<typename Object, typename Function>
void AsyncTask<Object, Function>::entryPoint()
{
    while (1) {
        State expectedState = Signaled;
        if (m_state.compare_exchange_weak(expectedState, Running))
            (m_object.*m_function)();

        expectedState = Running;
        if (m_state.compare_exchange_weak(expectedState, Sleeping)) {
            std::unique_lock<Mutex> lock(m_conditionMutex);
            m_condition.wait_for(lock, exitDelay, [=]() { return this->m_state != Sleeping; });
        }

        expectedState = Sleeping;
        if (m_state.compare_exchange_weak(expectedState, Exited))
            return;
    }
}

} // namespace bmalloc

#endif // AsyncTask_h
