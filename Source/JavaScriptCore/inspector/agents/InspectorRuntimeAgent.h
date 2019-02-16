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
class VM;
}

namespace Inspector {

class InjectedScript;
class InjectedScriptManager;
class ScriptDebugServer;
typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorRuntimeAgent : public InspectorAgentBase, public RuntimeBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~InspectorRuntimeAgent();

    void willDestroyFrontendAndBackend(DisconnectReason) override;

    void enable(ErrorString&) override { m_enabled = true; }
    void disable(ErrorString&) override { m_enabled = false; }
    void parse(ErrorString&, const String& expression, Protocol::Runtime::SyntaxErrorType* result, Optional<String>& message, RefPtr<Protocol::Runtime::ErrorRange>&) final;
    void evaluate(ErrorString&, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* emulateUserGesture, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex) override;
    void awaitPromise(const String& promiseObjectId, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, Ref<AwaitPromiseCallback>&&) final;
    void callFunctionOn(ErrorString&, const String& objectId, const String& expression, const JSON::Array* optionalArguments, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown) final;
    void releaseObject(ErrorString&, const ErrorString& objectId) final;
    void getPreview(ErrorString&, const String& objectId, RefPtr<Protocol::Runtime::ObjectPreview>&) final;
    void getProperties(ErrorString&, const String& objectId, const bool* ownProperties, const bool* generatePreview, RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>& result, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>& internalProperties) final;
    void getDisplayableProperties(ErrorString&, const String& objectId, const bool* generatePreview, RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>& result, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>& internalProperties) final;
    void getCollectionEntries(ErrorString&, const String& objectId, const String* objectGroup, const int* startIndex, const int* numberToFetch, RefPtr<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>>& entries) final;
    void saveResult(ErrorString&, const JSON::Object& callArgument, const int* executionContextId, Optional<int>& savedResultIndex) final;
    void releaseObjectGroup(ErrorString&, const String& objectGroup) final;
    void getRuntimeTypesForVariablesAtOffsets(ErrorString&, const JSON::Array& locations, RefPtr<JSON::ArrayOf<Protocol::Runtime::TypeDescription>>&) override;
    void enableTypeProfiler(ErrorString&) override;
    void disableTypeProfiler(ErrorString&) override;
    void enableControlFlowProfiler(ErrorString&) override;
    void disableControlFlowProfiler(ErrorString&) override;
    void getBasicBlocks(ErrorString&, const String& in_sourceID, RefPtr<JSON::ArrayOf<Protocol::Runtime::BasicBlock>>& out_basicBlocks) override;

    bool enabled() const { return m_enabled; }

protected:
    InspectorRuntimeAgent(AgentContext&);

    InjectedScriptManager& injectedScriptManager() { return m_injectedScriptManager; }

    virtual InjectedScript injectedScriptForEval(ErrorString&, const int* executionContextId) = 0;

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

private:
    void setTypeProfilerEnabledState(bool);
    void setControlFlowProfilerEnabledState(bool);

    InjectedScriptManager& m_injectedScriptManager;
    ScriptDebugServer& m_scriptDebugServer;
    JSC::VM& m_vm;
    bool m_enabled {false};
    bool m_isTypeProfilingEnabled {false};
    bool m_isControlFlowProfilingEnabled {false};
};

} // namespace Inspector
