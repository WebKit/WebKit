/*
 * Copyright (C) 2010, 2013, 2015-2016 Apple Inc. All rights reserved.
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

#include "Breakpoint.h"
#include "Debugger.h"
#include "DebuggerPrimitives.h"
#include "InspectorAgentBase.h"
#include "InspectorBackendDispatchers.h"
#include "InspectorFrontendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace Inspector {

class AsyncStackTrace;
class InjectedScript;
class InjectedScriptManager;

class JS_EXPORT_PRIVATE InspectorDebuggerAgent : public InspectorAgentBase, public DebuggerBackendDispatcherHandler, public JSC::Debugger::Observer {
    WTF_MAKE_NONCOPYABLE(InspectorDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorDebuggerAgent() override;

    static RefPtr<JSC::Breakpoint> debuggerBreakpointFromPayload(Protocol::ErrorString&, RefPtr<JSON::Object>&& options);

    static const char* const backtraceObjectGroup;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(DisconnectReason) final;
    virtual bool enabled() const { return m_enabled; }

    // DebuggerBackendDispatcherHandler
    Protocol::ErrorStringOr<void> enable() final;
    Protocol::ErrorStringOr<void> disable() final;
    Protocol::ErrorStringOr<void> setAsyncStackTraceDepth(int) final;
    Protocol::ErrorStringOr<void> setBreakpointsActive(bool) final;
    Protocol::ErrorStringOr<std::tuple<Protocol::Debugger::BreakpointId, Ref<JSON::ArrayOf<Protocol::Debugger::Location>>>> setBreakpointByUrl(int lineNumber, const String& url, const String& urlRegex, Optional<int>&& columnNumber, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<std::tuple<Protocol::Debugger::BreakpointId, Ref<Protocol::Debugger::Location>>> setBreakpoint(Ref<JSON::Object>&& location, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<void> removeBreakpoint(const Protocol::Debugger::BreakpointId&) final;
    Protocol::ErrorStringOr<void> continueUntilNextRunLoop() final;
    Protocol::ErrorStringOr<void> continueToLocation(Ref<JSON::Object>&& location) final;
    Protocol::ErrorStringOr<void> stepNext() final;
    Protocol::ErrorStringOr<void> stepOver() final;
    Protocol::ErrorStringOr<void> stepInto() final;
    Protocol::ErrorStringOr<void> stepOut() final;
    Protocol::ErrorStringOr<void> pause() final;
    Protocol::ErrorStringOr<void> resume() final;
    Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>> searchInContent(const Protocol::Debugger::ScriptId&, const String& query, Optional<bool>&& caseSensitive, Optional<bool>&& isRegex) final;
    Protocol::ErrorStringOr<String> getScriptSource(const Protocol::Debugger::ScriptId&) final;
    Protocol::ErrorStringOr<Ref<Protocol::Debugger::FunctionDetails>> getFunctionDetails(const String& functionId) final;
    Protocol::ErrorStringOr<void> setPauseOnDebuggerStatements(bool enabled, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<void> setPauseOnExceptions(const String& state, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<void> setPauseOnAssertions(bool enabled, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<void> setPauseOnMicrotasks(bool enabled, RefPtr<JSON::Object>&& options) final;
    Protocol::ErrorStringOr<void> setPauseForInternalScripts(bool shouldPause) final;
    Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */, Optional<int> /* savedResultIndex */>> evaluateOnCallFrame(const Protocol::Debugger::CallFrameId&, const String& expression, const String& objectGroup, Optional<bool>&& includeCommandLineAPI, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& saveResult, Optional<bool>&& emulateUserGesture) override;
    Protocol::ErrorStringOr<void> setShouldBlackboxURL(const String& url, bool shouldBlackbox, Optional<bool>&& caseSensitive, Optional<bool>&& isRegex) final;

    // JSC::Debugger::Observer
    void didParseSource(JSC::SourceID, const JSC::Debugger::Script&) final;
    void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage) final;
    void willRunMicrotask() final;
    void didRunMicrotask() final;
    void didPause(JSC::JSGlobalObject*, JSC::DebuggerCallFrame&, JSC::JSValue exceptionOrCaughtValue) final;
    void didContinue() final;
    void breakpointActionSound(JSC::BreakpointActionID) final;
    void breakpointActionProbe(JSC::JSGlobalObject*, JSC::BreakpointActionID, unsigned batchId, unsigned sampleId, JSC::JSValue sample) final;

    bool isPaused() const;
    bool breakpointsActive() const;

    void setSuppressAllPauses(bool);

    void handleConsoleAssert(const String& message);

    enum class AsyncCallType {
        DOMTimer,
        EventListener,
        PostMessage,
        RequestAnimationFrame,
    };

    void didScheduleAsyncCall(JSC::JSGlobalObject*, AsyncCallType, int callbackId, bool singleShot);
    void didCancelAsyncCall(AsyncCallType, int callbackId);
    void willDispatchAsyncCall(AsyncCallType, int callbackId);
    void didDispatchAsyncCall();

    void schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason, RefPtr<JSON::Object>&& data = nullptr);
    void cancelPauseAtNextOpportunity();
    bool pauseOnNextStatementEnabled() const { return m_javaScriptPauseScheduled; }

    bool schedulePauseForSpecialBreakpoint(JSC::Breakpoint&, DebuggerFrontendDispatcher::Reason, RefPtr<JSON::Object>&& data = nullptr);
    bool cancelPauseForSpecialBreakpoint(JSC::Breakpoint&);

    void breakProgram(DebuggerFrontendDispatcher::Reason, RefPtr<JSON::Object>&& data = nullptr, RefPtr<JSC::Breakpoint>&& specialBreakpoint = nullptr);
    void scriptExecutionBlockedByCSP(const String& directiveText);

    class Listener {
    public:
        virtual ~Listener() { }
        virtual void debuggerWasEnabled() = 0;
        virtual void debuggerWasDisabled() = 0;
    };
    void addListener(Listener& listener) { m_listeners.add(&listener); }
    void removeListener(Listener& listener) { m_listeners.remove(&listener); }

protected:
    InspectorDebuggerAgent(AgentContext&);
    virtual void internalEnable();
    virtual void internalDisable(bool isBeingDestroyed);

    InjectedScriptManager& injectedScriptManager() const { return m_injectedScriptManager; }
    virtual InjectedScript injectedScriptForEval(Protocol::ErrorString&, Optional<Protocol::Runtime::ExecutionContextId>&&) = 0;

    JSC::Debugger& debugger() { return m_debugger; }

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    virtual String sourceMapURLForScript(const JSC::Debugger::Script&);

    void didClearGlobalObject();
    virtual void didClearAsyncStackTraceData() { }

private:
    bool shouldBlackboxURL(const String&) const;

    Ref<JSON::ArrayOf<Protocol::Debugger::CallFrame>> currentCallFrames(const InjectedScript&);

    class ProtocolBreakpoint {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static Optional<ProtocolBreakpoint> fromPayload(Protocol::ErrorString&, JSC::SourceID, unsigned lineNumber, unsigned columnNumber, RefPtr<JSON::Object>&& options = nullptr);
        static Optional<ProtocolBreakpoint> fromPayload(Protocol::ErrorString&, const String& url, bool isRegex, unsigned lineNumber, unsigned columnNumber, RefPtr<JSON::Object>&& options = nullptr);

        ProtocolBreakpoint();
        ProtocolBreakpoint(JSC::SourceID, unsigned lineNumber, unsigned columnNumber, const String& condition = nullString(), JSC::Breakpoint::ActionsVector&& actions = { }, bool autoContinue = false, size_t ignoreCount = 0);
        ProtocolBreakpoint(const String& url, bool isRegex, unsigned lineNumber, unsigned columnNumber, const String& condition = nullString(), JSC::Breakpoint::ActionsVector&& actions = { }, bool autoContinue = false, size_t ignoreCount = 0);

        Ref<JSC::Breakpoint> createDebuggerBreakpoint(JSC::BreakpointID, JSC::SourceID) const;

        const Protocol::Debugger::BreakpointId& id() const { return m_id; }

        bool matchesScriptURL(const String&) const;

    private:
        Protocol::Debugger::BreakpointId m_id;

#if ASSERT_ENABLED
        JSC::SourceID m_sourceID { JSC::noSourceID };
#endif
        String m_url;
        bool m_isRegex { false };

        // FIXME: <https://webkit.org/b/162771> Web Inspector: Adopt TextPosition in Inspector to avoid oneBasedInt/zeroBasedInt ambiguity
        unsigned m_lineNumber { 0 };
        unsigned m_columnNumber { 0 };

        String m_condition;
        JSC::Breakpoint::ActionsVector m_actions;
        bool m_autoContinue { false };
        size_t m_ignoreCount { 0 };
    };

    bool resolveBreakpoint(const JSC::Debugger::Script&, JSC::Breakpoint&);
    bool setBreakpoint(JSC::Breakpoint&);
    void didSetBreakpoint(ProtocolBreakpoint&, JSC::Breakpoint&);

    bool assertPaused(Protocol::ErrorString&);
    void clearDebuggerBreakpointState();
    void clearInspectorBreakpointState();
    void clearPauseDetails();
    void clearExceptionValue();
    void clearAsyncStackTraceData();

    enum class ShouldDispatchResumed { No, WhenIdle, WhenContinued };
    void registerIdleHandler();
    void willStepAndMayBecomeIdle();
    void didBecomeIdle();

    void updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason, RefPtr<JSON::Object>&& data);

    RefPtr<JSON::Object> buildBreakpointPauseReason(JSC::BreakpointID);
    RefPtr<JSON::Object> buildExceptionPauseReason(JSC::JSValue exception, const InjectedScript&);

    typedef std::pair<unsigned, int> AsyncCallIdentifier;
    static AsyncCallIdentifier asyncCallIdentifier(AsyncCallType, int callbackId);

    std::unique_ptr<DebuggerFrontendDispatcher> m_frontendDispatcher;
    RefPtr<DebuggerBackendDispatcher> m_backendDispatcher;

    JSC::Debugger& m_debugger;
    InjectedScriptManager& m_injectedScriptManager;
    HashMap<JSC::SourceID, JSC::Debugger::Script> m_scripts;

    struct BlackboxConfig {
        String url;
        bool caseSensitive { false };
        bool isRegex { false };

        inline bool operator==(const BlackboxConfig& other) const
        {
            return url == other.url
                && caseSensitive == other.caseSensitive
                && isRegex == other.isRegex;
        }
    };
    Vector<BlackboxConfig> m_blackboxedURLs;

    HashSet<Listener*> m_listeners;

    JSC::JSGlobalObject* m_pausedGlobalObject { nullptr };
    JSC::Strong<JSC::Unknown> m_currentCallStack;

    HashMap<Protocol::Debugger::BreakpointId, ProtocolBreakpoint> m_protocolBreakpointForProtocolBreakpointID;
    HashMap<Protocol::Debugger::BreakpointId, JSC::BreakpointsVector> m_debuggerBreakpointsForProtocolBreakpointID;
    JSC::BreakpointID m_nextDebuggerBreakpointID { JSC::noBreakpointID + 1 };

    RefPtr<JSC::Breakpoint> m_continueToLocationDebuggerBreakpoint;

    ShouldDispatchResumed m_conditionToDispatchResumed { ShouldDispatchResumed::No };

    DebuggerFrontendDispatcher::Reason m_pauseReason;
    RefPtr<JSON::Object> m_pauseData;

    DebuggerFrontendDispatcher::Reason m_preBlackboxPauseReason;
    RefPtr<JSON::Object> m_preBlackboxPauseData;

    HashMap<AsyncCallIdentifier, RefPtr<AsyncStackTrace>> m_pendingAsyncCalls;
    Optional<AsyncCallIdentifier> m_currentAsyncCallIdentifier;
    int m_asyncStackTraceDepth { 0 };

    RefPtr<JSC::Breakpoint> m_pauseOnAssertionsBreakpoint;
    RefPtr<JSC::Breakpoint> m_pauseOnMicrotasksBreakpoint;

    bool m_enabled { false };
    bool m_enablePauseWhenIdle { false };
    bool m_pauseForInternalScripts { false };
    bool m_javaScriptPauseScheduled { false };
    bool m_didPauseStopwatch { false };
    bool m_hasExceptionValue { false };
    bool m_registeredIdleCallback { false };
};

} // namespace Inspector
