/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WorkerEventLoop.h"

#include "WorkerGlobalScope.h"
#include "WorkletGlobalScope.h"

namespace WebCore {

Ref<WorkerEventLoop> WorkerEventLoop::create(WorkerGlobalScope& context)
{
    return adoptRef(*new WorkerEventLoop(context));
}

#if ENABLE(CSS_PAINTING_API)
Ref<WorkerEventLoop> WorkerEventLoop::create(WorkletGlobalScope& context)
{
    return adoptRef(*new WorkerEventLoop(context));
}
#endif

WorkerEventLoop::WorkerEventLoop(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
{
    suspendIfNeeded();
}

void WorkerEventLoop::queueTask(TaskSource source, ScriptExecutionContext& context, TaskFunction&& function)
{
    if (!scriptExecutionContext())
        return;
    ASSERT(scriptExecutionContext()->isContextThread());
    ASSERT_UNUSED(context, scriptExecutionContext() == &context);
    m_tasks.append({ source, WTFMove(function) });
    scheduleToRunIfNeeded();
}

const char* WorkerEventLoop::activeDOMObjectName() const
{
    return "WorkerEventLoop";
}

void WorkerEventLoop::suspend(ReasonForSuspension)
{
    ASSERT_NOT_REACHED();
}

void WorkerEventLoop::resume()
{
    ASSERT_NOT_REACHED();
}

void WorkerEventLoop::stop()
{
    m_tasks.clear();
}

void WorkerEventLoop::scheduleToRunIfNeeded()
{
    auto* context = scriptExecutionContext();
    ASSERT(context);
    if (m_isScheduledToRun || m_tasks.isEmpty())
        return;

    m_isScheduledToRun = true;
    context->postTask([eventLoop = makeRef(*this)] (ScriptExecutionContext&) {
        eventLoop->m_isScheduledToRun = false;
        eventLoop->run();
    });
}

void WorkerEventLoop::run()
{
    auto* context = scriptExecutionContext();
    if (!context || context->activeDOMObjectsAreStopped() || context->activeDOMObjectsAreSuspended())
        return;
    for (auto& task : m_tasks)
        task.task();
}

} // namespace WebCore

