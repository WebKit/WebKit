/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Function.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WTF {

namespace Detail {
struct CancellableTaskImpl : public CanMakeWeakPtr<CancellableTaskImpl> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    explicit CancellableTaskImpl(Function<void()>&& task) : task(WTFMove(task)) { }
    Function<void()> task;
};
}

class CancellableTask {
public:
    explicit CancellableTask(Function<void()>&&);

    void operator()();
    bool isPending() const { return !!m_taskWrapper->task; }
    void cancel() { m_taskWrapper->task = nullptr; }

    class Handle {
    public:
        Handle() = default;
        bool isPending() const { return m_taskWrapper && m_taskWrapper->task; }
        void cancel();
    private:
        friend class CancellableTask;
        explicit Handle(Detail::CancellableTaskImpl&);
        WeakPtr<Detail::CancellableTaskImpl> m_taskWrapper;
    };

    Handle createHandle() { return Handle { m_taskWrapper.get() }; }

private:
    UniqueRef<Detail::CancellableTaskImpl> m_taskWrapper;
};

inline CancellableTask::CancellableTask(Function<void()>&& task)
    : m_taskWrapper(makeUniqueRef<Detail::CancellableTaskImpl>(WTFMove(task)))
{ }

inline void CancellableTask::operator()()
{
    if (auto task = std::exchange(m_taskWrapper->task, nullptr))
        task();
}

inline CancellableTask::Handle::Handle(Detail::CancellableTaskImpl& task)
    : m_taskWrapper(makeWeakPtr(task))
{ }

inline void CancellableTask::Handle::cancel()
{
    if (m_taskWrapper)
        m_taskWrapper->task = nullptr;
}

} // namespace WTF

using WTF::CancellableTask;
