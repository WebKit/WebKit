/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "GenericTaskQueue.h"

namespace WebCore {

template <typename T>
class DeferrableTask : public CanMakeWeakPtr<DeferrableTask<T>> {
public:
    DeferrableTask()
        : m_dispatcher()
    {
    }

    DeferrableTask(T& t)
        : m_dispatcher(&t)
    {
    }

    typedef WTF::Function<void ()> TaskFunction;

    void scheduleTask(TaskFunction&& task)
    {
        if (m_isClosed)
            return;

        cancelTask();

        m_pendingTask = true;
        m_dispatcher.postTask([weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            ASSERT(weakThis->m_pendingTask);
            weakThis->m_pendingTask = false;
            task();
        });
    }

    void close()
    {
        cancelTask();
        m_isClosed = true;
    }

    void cancelTask()
    {
        CanMakeWeakPtr<DeferrableTask<T>>::weakPtrFactory().revokeAll();
        m_pendingTask = false;
    }
    bool hasPendingTask() const { return m_pendingTask; }

private:
    TaskDispatcher<T> m_dispatcher;
    bool m_pendingTask { false };
    bool m_isClosed { false };
};

}
