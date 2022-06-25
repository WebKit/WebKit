/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#pragma once

#include <memory>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace WTF {

enum class ThreadGroupAddResult { NewlyAdded, AlreadyAdded, NotAdded };

class ThreadGroup final : public std::enable_shared_from_this<ThreadGroup> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ThreadGroup);
public:
    friend class Thread;

    static std::shared_ptr<ThreadGroup> create()
    {
        return std::allocate_shared<ThreadGroup>(FastAllocator<ThreadGroup>());
    }

    WTF_EXPORT_PRIVATE ThreadGroupAddResult add(Thread&);
    WTF_EXPORT_PRIVATE ThreadGroupAddResult add(const AbstractLocker&, Thread&);
    WTF_EXPORT_PRIVATE ThreadGroupAddResult addCurrentThread();

    const ListHashSet<Ref<Thread>>& threads(const AbstractLocker&) const { return m_threads; }

    WordLock& getLock() { return m_lock; }

    WTF_EXPORT_PRIVATE ~ThreadGroup();

    ThreadGroup() = default;

private:
    std::weak_ptr<ThreadGroup> weakFromThis()
    {
        return shared_from_this();
    }

    // We use WordLock since it can be used when deallocating TLS.
    WordLock m_lock;
    ListHashSet<Ref<Thread>> m_threads;
};

}

using WTF::ThreadGroup;
using WTF::ThreadGroupAddResult;
