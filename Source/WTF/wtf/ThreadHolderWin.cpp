/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ThreadHolder.h"

#include <mutex>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Threading.h>

namespace WTF {

#define InvalidThreadHolder ((ThreadHolder*)(0xbbadbeef))

static std::mutex& threadMapMutex()
{
    static NeverDestroyed<std::mutex> mutex;
    return mutex.get();
}

static HashMap<ThreadIdentifier, ThreadHolder*>& threadMap()
{
    static NeverDestroyed<HashMap<ThreadIdentifier, ThreadHolder*>> map;
    return map.get();
}

void ThreadHolder::initializeOnce()
{
    threadMapMutex();
    threadMap();
    threadSpecificKeyCreate(&m_key, destruct);
}

ThreadHolder* ThreadHolder::current()
{
    ASSERT(m_key != InvalidThreadSpecificKey);
    ThreadHolder* holder = static_cast<ThreadHolder*>(threadSpecificGet(m_key));
    if (holder) {
        ASSERT(holder != InvalidThreadHolder);
        return holder;
    }

    // After FLS is destroyed, this map offers the value until the second thread exit callback is called.
    std::lock_guard<std::mutex> locker(threadMapMutex());
    return threadMap().get(currentThread());
}

// FIXME: Remove this workaround code once <rdar://problem/31793213> is fixed.
RefPtr<Thread> ThreadHolder::get(ThreadIdentifier id)
{
    std::lock_guard<std::mutex> locker(threadMapMutex());
    ThreadHolder* holder = threadMap().get(id);
    if (holder)
        return &holder->thread();
    return nullptr;
}

void ThreadHolder::initialize(Thread& thread, ThreadIdentifier id)
{
    if (!current()) {
        // Ideally we'd have this as a release assert everywhere, but that would hurt performance.
        // Having this release assert here means that we will catch "didn't call
        // WTF::initializeThreading() soon enough" bugs in release mode.
        ASSERT(m_key != InvalidThreadSpecificKey);
        // FIXME: Remove this workaround code once <rdar://problem/31793213> is fixed.
        auto* holder = new ThreadHolder(thread);
        threadSpecificSet(m_key, holder);

        // Since Thread is not established yet, we use the given id instead of thread->id().
        {
            std::lock_guard<std::mutex> locker(threadMapMutex());
            threadMap().add(id, holder);
        }
    }
}

void ThreadHolder::destruct(void* data)
{
    if (data == InvalidThreadHolder)
        return;

    ThreadHolder* holder = static_cast<ThreadHolder*>(data);
    ASSERT(holder);

    // Delay the deallocation of ThreadHolder more.
    // It defers ThreadHolder deallocation after the other ThreadSpecific values are deallocated.
    static thread_local class ThreadExitCallback {
    public:
        ThreadExitCallback(ThreadHolder* holder)
            : m_holder(holder)
        {
        }

        ~ThreadExitCallback()
        {
            ThreadHolder::destruct(m_holder);
        }

    private:
        ThreadHolder* m_holder;
    } callback(holder);

    if (holder->m_isDestroyedOnce) {
        {
            std::lock_guard<std::mutex> locker(threadMapMutex());
            ASSERT(threadMap().contains(holder->m_thread->id()));
            threadMap().remove(holder->m_thread->id());
        }
        delete holder;

        // Fill the FLS with the non-nullptr value. While FLS destructor won't be called for that,
        // non-nullptr value tells us that we already destructed ThreadHolder. This allows us to
        // detect incorrect use of Thread::current() after this point because it will crash.
        threadSpecificSet(m_key, InvalidThreadHolder);
        return;
    }
    threadSpecificSet(m_key, InvalidThreadHolder);
    holder->m_isDestroyedOnce = true;
}

} // namespace WTF
