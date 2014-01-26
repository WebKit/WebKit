/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef InspectorDebuggerAgent_h
#define InspectorDebuggerAgent_h

#if ENABLE(INSPECTOR)
#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "InspectorJSBackendDispatchers.h"
#include "InspectorJSFrontendDispatchers.h"
#include "bindings/ScriptValue.h"
#include "debugger/Debugger.h"
#include "inspector/InspectorAgentBase.h"
#include "inspector/ScriptBreakpoint.h"
#include "inspector/ScriptDebugListener.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace Inspector {

class InjectedScript;
class InjectedScriptManager;
class InspectorArray;
class InspectorObject;
class InspectorValue;
class ScriptDebugServer;
typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorDebuggerAgent : public InspectorAgentBase, public ScriptDebugListener, public InspectorDebuggerBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static const char* backtraceObjectGroup;

    virtual ~InspectorDebuggerAgent();

    virtual void didCreateFrontendAndBackend(InspectorFrontendChannel*, InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(InspectorDisconnectReason) override;

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void setBreakpointsActive(ErrorString*, bool active) override;
    virtual void setBreakpointByUrl(ErrorString*, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const RefPtr<Inspector::InspectorObject>* options, Inspector::TypeBuilder::Debugger::BreakpointId*, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::Location>>& locations, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>>& breakpointActionIdentifiers) override;
    virtual void setBreakpoint(ErrorString*, const RefPtr<Inspector::InspectorObject>& location, const RefPtr<Inspector::InspectorObject>* options, Inspector::TypeBuilder::Debugger::BreakpointId*, RefPtr<Inspector::TypeBuilder::Debugger::Location>& actualLocation, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>>& breakpointActionIdentifiers) override;
    virtual void removeBreakpoint(ErrorString*, const String& breakpointIdentifier) override;
    virtual void continueToLocation(ErrorString*, const RefPtr<InspectorObject>& location) override;
    virtual void searchInContent(ErrorString*, const String& scriptID, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::GenericTypes::SearchMatch>>&) override;
    virtual void getScriptSource(ErrorString*, const String& scriptID, String* scriptSource) override;
    virtual void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<Inspector::TypeBuilder::Debugger::FunctionDetails>&) override;
    virtual void pause(ErrorString*) override;
    virtual void resume(ErrorString*) override;
    virtual void stepOver(ErrorString*) override;
    virtual void stepInto(ErrorString*) override;
    virtual void stepOut(ErrorString*) override;
    virtual void setPauseOnExceptions(ErrorString*, const String& pauseState) override;
    virtual void evaluateOnCallFrame(ErrorString*, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown) override;
    virtual void setOverlayMessage(ErrorString*, const String*) override;

    bool isPaused();
    
    void handleConsoleAssert();

    void schedulePauseOnNextStatement(InspectorDebuggerFrontendDispatcher::Reason::Enum breakReason, PassRefPtr<InspectorObject> data);
    void cancelPauseOnNextStatement();
    void breakProgram(InspectorDebuggerFrontendDispatcher::Reason::Enum breakReason, PassRefPtr<InspectorObject> data);
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

    virtual ScriptDebugServer& scriptDebugServer() = 0;

protected:
    InspectorDebuggerAgent(InjectedScriptManager*);

    InjectedScriptManager* injectedScriptManager() const { return m_injectedScriptManager; }
    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) = 0;

    virtual void startListeningScriptDebugServer() = 0;
    virtual void stopListeningScriptDebugServer(bool skipRecompile) = 0;
    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    virtual void enable();
    virtual void disable(bool skipRecompile);
    virtual void didPause(JSC::ExecState*, const Deprecated::ScriptValue& callFrames, const Deprecated::ScriptValue& exception) override;
    virtual void didContinue() override;

    virtual String sourceMapURLForScript(const Script&);

    void didClearGlobalObject();

private:
    PassRefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::CallFrame>> currentCallFrames();

    virtual void didParseSource(JSC::SourceID, const Script&) override final;
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage) override final;
    virtual void didSampleProbe(JSC::ExecState*, int probeIdentifier, int hitCount, const Deprecated::ScriptValue& sample) override final;

    virtual void breakpointActionSound() override;

    PassRefPtr<Inspector::TypeBuilder::Debugger::Location> resolveBreakpoint(const String& breakpointIdentifier, JSC::SourceID, const ScriptBreakpoint&);
    bool assertPaused(ErrorString*);
    void clearResolvedBreakpointState();
    void clearBreakDetails();

    bool breakpointActionsFromProtocol(ErrorString*, RefPtr<InspectorArray>& actions, Vector<ScriptBreakpointAction>* result);

    typedef HashMap<JSC::SourceID, Script> ScriptsMap;
    typedef HashMap<String, Vector<JSC::BreakpointID>> BreakpointIdentifierToDebugServerBreakpointIDsMap;
    typedef HashMap<String, RefPtr<InspectorObject>> BreakpointIdentifierToBreakpointMap;

    InjectedScriptManager* m_injectedScriptManager;
    std::unique_ptr<InspectorDebuggerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<InspectorDebuggerBackendDispatcher> m_backendDispatcher;
    Listener* m_listener;
    JSC::ExecState* m_pausedScriptState;
    Deprecated::ScriptValue m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdentifierToDebugServerBreakpointIDsMap m_breakpointIdentifierToDebugServerBreakpointIDs;
    BreakpointIdentifierToBreakpointMap m_javaScriptBreakpoints;
    JSC::BreakpointID m_continueToLocationBreakpointID;
    InspectorDebuggerFrontendDispatcher::Reason::Enum m_breakReason;
    RefPtr<InspectorObject> m_breakAuxData;
    bool m_enabled;
    bool m_javaScriptPauseScheduled;
    int m_nextProbeSampleId;
    int m_nextBreakpointActionIdentifier;
};

} // namespace Inspector

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorDebuggerAgent_h)
