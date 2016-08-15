/*
 * Copyright (C) 2010, 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorBackendDispatchers.h"
#include "InspectorFrontendDispatchers.h"
#include "bindings/ScriptValue.h"
#include "debugger/Debugger.h"
#include "inspector/InspectorAgentBase.h"
#include "inspector/ScriptBreakpoint.h"
#include "inspector/ScriptDebugListener.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace Inspector {

class InjectedScript;
class InjectedScriptManager;
class InspectorArray;
class InspectorObject;
class InspectorValue;
class ScriptDebugServer;
typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorDebuggerAgent : public InspectorAgentBase, public ScriptDebugListener, public DebuggerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static const char* backtraceObjectGroup;

    virtual ~InspectorDebuggerAgent();

    void didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(DisconnectReason) final;

    void enable(ErrorString&) final;
    void disable(ErrorString&) final;
    void setBreakpointsActive(ErrorString&, bool active) final;
    void setBreakpointByUrl(ErrorString&, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const Inspector::InspectorObject* options, Inspector::Protocol::Debugger::BreakpointId*, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Debugger::Location>>& locations) final;
    void setBreakpoint(ErrorString&, const Inspector::InspectorObject& location, const Inspector::InspectorObject* options, Inspector::Protocol::Debugger::BreakpointId*, RefPtr<Inspector::Protocol::Debugger::Location>& actualLocation) final;
    void removeBreakpoint(ErrorString&, const String& breakpointIdentifier) final;
    void continueToLocation(ErrorString&, const InspectorObject& location) final;
    void searchInContent(ErrorString&, const String& scriptID, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::GenericTypes::SearchMatch>>&) final;
    void getScriptSource(ErrorString&, const String& scriptID, String* scriptSource) final;
    void getFunctionDetails(ErrorString&, const String& functionId, RefPtr<Inspector::Protocol::Debugger::FunctionDetails>&) final;
    void pause(ErrorString&) final;
    void resume(ErrorString&) final;
    void stepOver(ErrorString&) final;
    void stepInto(ErrorString&) final;
    void stepOut(ErrorString&) final;
    void setPauseOnExceptions(ErrorString&, const String& pauseState) final;
    void evaluateOnCallFrame(ErrorString&, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result, Inspector::Protocol::OptOutput<bool>* wasThrown, Inspector::Protocol::OptOutput<int>* savedResultIndex) final;
    void setOverlayMessage(ErrorString&, const String*) override;

    bool isPaused() const;
    bool breakpointsActive() const;

    void setSuppressAllPauses(bool);

    void handleConsoleAssert(const String& message);

    void schedulePauseOnNextStatement(DebuggerFrontendDispatcher::Reason breakReason, RefPtr<InspectorObject>&& data);
    void cancelPauseOnNextStatement();
    void breakProgram(DebuggerFrontendDispatcher::Reason breakReason, RefPtr<InspectorObject>&& data);
    void scriptExecutionBlockedByCSP(const String& directiveText);

    class Listener {
    public:
        virtual ~Listener() { }
        virtual void debuggerWasEnabled() = 0;
        virtual void debuggerWasDisabled() = 0;
        virtual void stepInto() = 0;
        virtual void didPause() = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

protected:
    InspectorDebuggerAgent(AgentContext&);

    InjectedScriptManager& injectedScriptManager() const { return m_injectedScriptManager; }
    virtual InjectedScript injectedScriptForEval(ErrorString&, const int* executionContextId) = 0;

    ScriptDebugServer& scriptDebugServer() { return m_scriptDebugServer; }

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    virtual void enable();
    virtual void disable(bool skipRecompile);
    void didPause(JSC::ExecState&, JSC::JSValue callFrames, JSC::JSValue exceptionOrCaughtValue) final;
    void didContinue() final;

    virtual String sourceMapURLForScript(const Script&);

    void didClearGlobalObject();

private:
    Ref<Inspector::Protocol::Array<Inspector::Protocol::Debugger::CallFrame>> currentCallFrames(const InjectedScript&);

    void didParseSource(JSC::SourceID, const Script&) final;
    void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage) final;

    void breakpointActionSound(int breakpointActionIdentifier) final;
    void breakpointActionProbe(JSC::ExecState&, const ScriptBreakpointAction&, unsigned batchId, unsigned sampleId, JSC::JSValue sample) final;

    RefPtr<Inspector::Protocol::Debugger::Location> resolveBreakpoint(const String& breakpointIdentifier, JSC::SourceID, const ScriptBreakpoint&);
    bool assertPaused(ErrorString&);
    void clearDebuggerBreakpointState();
    void clearInspectorBreakpointState();
    void clearBreakDetails();
    void clearExceptionValue();

    RefPtr<InspectorObject> buildBreakpointPauseReason(JSC::BreakpointID);
    RefPtr<InspectorObject> buildExceptionPauseReason(JSC::JSValue exception, const InjectedScript&);

    bool breakpointActionsFromProtocol(ErrorString&, RefPtr<InspectorArray>& actions, BreakpointActions* result);

    typedef HashMap<JSC::SourceID, Script> ScriptsMap;
    typedef HashMap<String, Vector<JSC::BreakpointID>> BreakpointIdentifierToDebugServerBreakpointIDsMap;
    typedef HashMap<String, RefPtr<InspectorObject>> BreakpointIdentifierToBreakpointMap;
    typedef HashMap<JSC::BreakpointID, String> DebugServerBreakpointIDToBreakpointIdentifier;

    InjectedScriptManager& m_injectedScriptManager;
    std::unique_ptr<DebuggerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<DebuggerBackendDispatcher> m_backendDispatcher;
    ScriptDebugServer& m_scriptDebugServer;
    Listener* m_listener { nullptr };
    JSC::ExecState* m_pausedScriptState { nullptr };
    Deprecated::ScriptValue m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdentifierToDebugServerBreakpointIDsMap m_breakpointIdentifierToDebugServerBreakpointIDs;
    BreakpointIdentifierToBreakpointMap m_javaScriptBreakpoints;
    DebugServerBreakpointIDToBreakpointIdentifier m_debuggerBreakpointIdentifierToInspectorBreakpointIdentifier;
    JSC::BreakpointID m_continueToLocationBreakpointID;
    DebuggerFrontendDispatcher::Reason m_breakReason;
    RefPtr<InspectorObject> m_breakAuxData;
    bool m_enabled { false };
    bool m_javaScriptPauseScheduled { false };
    bool m_hasExceptionValue { false };
    bool m_didPauseStopwatch { false };
};

} // namespace Inspector
