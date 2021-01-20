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

#include "EventTarget.h"
#include "ScriptExecutionContext.h"
#include "WorkerThreadType.h"

namespace WebCore {

class EventLoopTaskGroup;
class WorkerEventLoop;
class WorkerInspectorController;
class WorkerOrWorkletScriptController;
class WorkerOrWorkletThread;

class WorkerOrWorkletGlobalScope : public ScriptExecutionContext, public RefCounted<WorkerOrWorkletGlobalScope>, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(WorkerOrWorkletGlobalScope);
    WTF_MAKE_NONCOPYABLE(WorkerOrWorkletGlobalScope);
public:
    virtual ~WorkerOrWorkletGlobalScope();

    bool isClosing() const { return m_isClosing; }
    WorkerOrWorkletThread* workerOrWorkletThread() const { return m_thread; }

    WorkerOrWorkletScriptController* script() const { return m_script.get(); }
    void clearScript();

    JSC::VM& vm() final;
    WorkerInspectorController& inspectorController() const { return *m_inspectorController; }

    unsigned long createUniqueIdentifier() { return m_uniqueIdentifier++; }

    // ScriptExecutionContext.
    EventLoopTaskGroup& eventLoop() final;
    bool isContextThread() const final;
    void postTask(Task&&) final; // Executes the task on context's thread asynchronously.

    virtual void prepareForDestruction();

    using RefCounted::ref;
    using RefCounted::deref;

    virtual void suspend() { }
    virtual void resume() { }

protected:
    WorkerOrWorkletGlobalScope(WorkerThreadType, Ref<JSC::VM>&&, WorkerOrWorkletThread*);

    // ScriptExecutionContext.
    bool isJSExecutionForbidden() const final;

    void markAsClosing() { m_isClosing = true; }

private:
    // ScriptExecutionContext.
    void disableEval(const String& errorMessage) final;
    void disableWebAssembly(const String& errorMessage) final;
    void refScriptExecutionContext() final { ref(); }
    void derefScriptExecutionContext() final { deref(); }

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final { return const_cast<WorkerOrWorkletGlobalScope*>(this); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    std::unique_ptr<WorkerOrWorkletScriptController> m_script;
    WorkerOrWorkletThread* m_thread;
    RefPtr<WorkerEventLoop> m_eventLoop;
    std::unique_ptr<EventLoopTaskGroup> m_defaultTaskGroup;
    std::unique_ptr<WorkerInspectorController> m_inspectorController;
    unsigned long m_uniqueIdentifier { 1 };
    bool m_isClosing { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerOrWorkletGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkerGlobalScope() || context.isWorkletGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
