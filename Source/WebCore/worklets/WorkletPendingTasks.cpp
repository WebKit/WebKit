/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WorkletPendingTasks.h"

#include "Worklet.h"

namespace WebCore {

WorkletPendingTasks::WorkletPendingTasks(Worklet& worklet, DOMPromiseDeferred<void>&& promise, int counter)
    : m_worklet(worklet)
    , m_promise(WTFMove(promise))
    , m_counter(counter)
{
    ASSERT(isMainThread());
}

void WorkletPendingTasks::abort(Exception&& exception)
{
    ASSERT(isMainThread());

    if (m_counter == -1)
        return;

    m_counter = -1;
    m_promise.reject(WTFMove(exception));
    if (m_worklet)
        m_worklet->finishPendingTasks(*this);
}

void WorkletPendingTasks::decrementCounter()
{
    ASSERT(isMainThread());

    if (m_counter == -1)
        return;

    --m_counter;
    if (!m_counter) {
        m_promise.resolve();
        if (m_worklet)
            m_worklet->finishPendingTasks(*this);
    }
}

} // namespace WebCore
