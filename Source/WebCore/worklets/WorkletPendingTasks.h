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

#pragma once

#include "JSDOMPromiseDeferred.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Worklet;

// https://drafts.css-houdini.org/worklets/#pending-tasks-struct
class WorkletPendingTasks : public ThreadSafeRefCounted<WorkletPendingTasks> {
public:
    static Ref<WorkletPendingTasks> create(Worklet& worklet, DOMPromiseDeferred<void>&& promise, int counter)
    {
        return adoptRef(*new WorkletPendingTasks(worklet, WTFMove(promise), counter));
    }

    void abort(Exception&&);
    void decrementCounter();

private:
    WorkletPendingTasks(Worklet&, DOMPromiseDeferred<void>&&, int counter);

    WeakPtr<Worklet> m_worklet;
    DOMPromiseDeferred<void> m_promise;
    int m_counter;
};

} // namespace WebCore
