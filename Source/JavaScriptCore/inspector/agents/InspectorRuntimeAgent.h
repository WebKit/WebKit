/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#pragma once

#include "InspectorAgentBase.h"
#include "InspectorBackendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace JSC {
class Debugger;
class VM;
}

namespace Inspector {

class InjectedScript;
class InjectedScriptManager;

class JS_EXPORT_PRIVATE InspectorRuntimeAgent : public InspectorAgentBase, public RuntimeBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorRuntimeAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(DisconnectReason) final;

    // RuntimeBackendDispatcherHandler
    Protocol::ErrorStringOr<void> enable() override;
    Protocol::ErrorStringOr<void> disable() override;
    Protocol::ErrorStringOr<std::tuple<Protocol::Runtime::SyntaxErrorType, String /* message */, RefPtr<Protocol::Runtime::ErrorRange>>> parse(const String& expression) final;
    Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */, Optional<int> /* savedResultIndex */>> evaluate(const String& expression, const String& objectGroup, Optional<bool>&& includeCommandLineAPI, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<Protocol::Runtime::ExecutionContextId>&&, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& saveResult, Optional<bool>&& emulateUserGesture) override;
    void awaitPromise(const Protocol::Runtime::RemoteObjectId&, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& saveResult, Ref<AwaitPromiseCallback>&&) final;
    Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */>> callFunctionOn(const Protocol::Runtime::RemoteObjectId&, const String& functionDeclaration, RefPtr<JSON::Array>&& arguments, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& emulateUserGesture) override;
    Protocol::ErrorStringOr<void> releaseObject(const Protocol::Runtime::RemoteObjectId&) final;
    Protocol::ErrorStringOr<Ref<Protocol::Runtime::ObjectPreview>> getPreview(const Protocol::Runtime::RemoteObjectId&) final;
    Protocol::ErrorStringOr<std::tuple<Ref<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>>> getProperties(const Protocol::Runtime::RemoteObjectId&, Optional<bool>&& ownProperties, Optional<int>&& fetchStart, Optional<int>&& fetchCount, Optional<bool>&& generatePreview) final;
    Protocol::ErrorStringOr<std::tuple<Ref<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>>> getDisplayableProperties(const Protocol::Runtime::RemoteObjectId&, Optional<int>&& fetchStart, Optional<int>&& fetchCount, Optional<bool>&& generatePreview) final;
    Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>>> getCollectionEntries(const Protocol::Runtime::RemoteObjectId&, const String& objectGroup, Optional<int>&& fetchStart, Optional<int>&& fetchCount) final;
    Protocol::ErrorStringOr<Optional<int> /* saveResultIndex */> saveResult(Ref<JSON::Object>&& callArgument, Optional<Protocol::Runtime::ExecutionContextId>&&) final;
    Protocol::ErrorStringOr<void> setSavedResultAlias(const String&) final;
    Protocol::ErrorStringOr<void> releaseObjectGroup(const String&) final;
    Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::TypeDescription>>> getRuntimeTypesForVariablesAtOffsets(Ref<JSON::Array>&& locations) final;
    Protocol::ErrorStringOr<void> enableTypeProfiler() final;
    Protocol::ErrorStringOr<void> disableTypeProfiler() final;
    Protocol::ErrorStringOr<void> enableControlFlowProfiler() final;
    Protocol::ErrorStringOr<void> disableControlFlowProfiler() final;
    Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::BasicBlock>>> getBasicBlocks(const String& sourceID) final;

protected:
    InspectorRuntimeAgent(AgentContext&);

    InjectedScriptManager& injectedScriptManager() { return m_injectedScriptManager; }

    virtual InjectedScript injectedScriptForEval(Protocol::ErrorString&, Optional<Protocol::Runtime::ExecutionContextId>&&) = 0;

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

private:
    void setTypeProfilerEnabledState(bool);
    void setControlFlowProfilerEnabledState(bool);

    InjectedScriptManager& m_injectedScriptManager;
    JSC::Debugger& m_debugger;
    JSC::VM& m_vm;
    bool m_enabled {false};
    bool m_isTypeProfilingEnabled {false};
    bool m_isControlFlowProfilingEnabled {false};
};

} // namespace Inspector
