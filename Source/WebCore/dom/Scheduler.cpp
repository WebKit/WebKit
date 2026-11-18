/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Scheduler.h"

#include "JSDOMPromiseDeferred.h"
#include "SchedulerPostTaskCallback.h"
#include "SchedulerPostTaskOptions.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Scheduler);

Ref<Scheduler> Scheduler::create(ScriptExecutionContext* context)
{
    return adoptRef(*new Scheduler(context));
}

Scheduler::Scheduler(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
{
}

Scheduler::~Scheduler() = default;

// https://wicg.github.io/scheduling-apis/#dom-scheduler-posttask
void Scheduler::postTask(Ref<SchedulerPostTaskCallback>&&, SchedulerPostTaskOptions&&, Ref<DeferredPromise>&& promise)
{
    // FIXME: Implement scheduling of the callback.
    promise->reject(ExceptionCode::NotSupportedError, "scheduler.postTask() is not implemented yet"_s);
}

// https://wicg.github.io/scheduling-apis/#dom-scheduler-yield
void Scheduler::yield(DOMPromiseDeferred<void>&& promise)
{
    // FIXME: Implement yielding to the event loop.
    promise.reject(ExceptionCode::NotSupportedError, "scheduler.yield() is not implemented yet"_s);
}

} // namespace WebCore
