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

#include "FrameIdentifier.h"
#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorRuntimeAgent.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class DOMWrapperWorld;
class LocalFrame;
class SecurityOrigin;

class FrameRuntimeAgent final : public Inspector::InspectorRuntimeAgent {
    WTF_MAKE_NONCOPYABLE(FrameRuntimeAgent);
    WTF_MAKE_TZONE_ALLOCATED(FrameRuntimeAgent);
public:
    FrameRuntimeAgent(FrameAgentContext&);
    ~FrameRuntimeAgent();

    // RuntimeBackendDispatcherHandler
    Inspector::CommandResult<void> enable() override;
    Inspector::CommandResult<void> disable() override;
    Inspector::CommandResultOf<Ref<Inspector::Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */> evaluate(const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&&, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture) override;
    void callFunctionOn(const Inspector::Protocol::Runtime::RemoteObjectId&, const String& functionDeclaration, RefPtr<JSON::Array>&& arguments, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& emulateUserGesture, std::optional<bool>&& awaitPromise, Ref<CallFunctionOnCallback>&&) override;

    // InspectorInstrumentation
    void didClearWindowObjectInWorld(DOMWrapperWorld&);

private:
    Inspector::InjectedScript injectedScriptForEval(Inspector::Protocol::ErrorString&, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&&) override;
    void muteConsole() override;
    void unmuteConsole() override;
    void reportExecutionContextCreation();
    void notifyContextCreated(JSC::JSGlobalObject*, const DOMWrapperWorld&, SecurityOrigin* = nullptr);
    String frameIdForProtocol() const;

    const UniqueRef<Inspector::RuntimeFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::RuntimeBackendDispatcher> m_backendDispatcher;

    WeakRef<InstrumentingAgents> m_instrumentingAgents;
    WeakRef<LocalFrame> m_inspectedFrame;
    const FrameIdentifier m_frameIdentifier;
};

} // namespace WebCore
