/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
#include "InjectedScript.h"
#include "InspectorBaseAgent.h"
#include "InspectorFrontend.h"
#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "ScriptState.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class InjectedScriptManager;
class InspectorFrontend;
class InspectorArray;
class InspectorObject;
class InspectorState;
class InspectorValue;
class InstrumentingAgents;
class ScriptDebugServer;
class ScriptValue;

typedef String ErrorString;

class InspectorDebuggerAgent : public InspectorBaseAgent<InspectorDebuggerAgent>, public ScriptDebugListener, public InspectorBackendDispatcher::DebuggerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDebuggerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    static const char* backtraceObjectGroup;

    virtual ~InspectorDebuggerAgent();

    virtual void causesRecompilation(ErrorString*, bool*);
    virtual void canSetScriptSource(ErrorString*, bool*);
    virtual void supportsNativeBreakpoints(ErrorString*, bool*);

    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    void didClearMainFrameWindowObject();
    bool isPaused();

    // Part of the protocol.
    virtual void setBreakpointsActive(ErrorString*, bool active);

    virtual void setBreakpointByUrl(ErrorString*, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const String* optionalCondition, String* breakpointId, RefPtr<InspectorArray>& locations);
    virtual void setBreakpoint(ErrorString*, const RefPtr<InspectorObject>& location, const String* optionalCondition, String* breakpointId, RefPtr<InspectorObject>& actualLocation);
    virtual void removeBreakpoint(ErrorString*, const String& breakpointId);
    virtual void continueToLocation(ErrorString*, const RefPtr<InspectorObject>& location);

    virtual void searchInContent(ErrorString*, const String& scriptId, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<InspectorArray>&);
    virtual void setScriptSource(ErrorString*, const String& scriptId, const String& newContent, const bool* preview, RefPtr<InspectorArray>& newCallFrames, RefPtr<InspectorObject>& result);
    virtual void getScriptSource(ErrorString*, const String& scriptId, String* scriptSource);
    virtual void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<InspectorObject>& details);
    void schedulePauseOnNextStatement(const String& breakReason, PassRefPtr<InspectorObject> data);
    void cancelPauseOnNextStatement();
    void breakProgram(const String& breakReason, PassRefPtr<InspectorObject> data);
    virtual void pause(ErrorString*);
    virtual void resume(ErrorString*);
    virtual void stepOver(ErrorString*);
    virtual void stepInto(ErrorString*);
    virtual void stepOut(ErrorString*);
    virtual void setPauseOnExceptions(ErrorString*, const String& pauseState);
    virtual void evaluateOnCallFrame(ErrorString*,
                             const String& callFrameId,
                             const String& expression,
                             const String* objectGroup,
                             const bool* includeCommandLineAPI,
                             const bool* returnByValue,
                             RefPtr<InspectorObject>& result,
                             bool* wasThrown);

    class Listener {
    public:
        virtual ~Listener() { }
        virtual void debuggerWasEnabled() = 0;
        virtual void debuggerWasDisabled() = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

    virtual ScriptDebugServer& scriptDebugServer() = 0;

protected:
    InspectorDebuggerAgent(InstrumentingAgents*, InspectorState*, InjectedScriptManager*);

    virtual void startListeningScriptDebugServer() = 0;
    virtual void stopListeningScriptDebugServer() = 0;

private:
    void enable();
    void disable();
    bool enabled();

    PassRefPtr<InspectorArray> currentCallFrames();

    virtual void didParseSource(const String& scriptId, const Script&);
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);
    virtual void didPause(ScriptState*, const ScriptValue& callFrames, const ScriptValue& exception);
    virtual void didContinue();

    void setPauseOnExceptionsImpl(ErrorString*, int);

    PassRefPtr<InspectorObject> resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint&);
    void clear();
    bool assertPaused(ErrorString*);
    void clearBreakDetails();

    String sourceMapURLForScript(const Script&);

    typedef HashMap<String, Script> ScriptsMap;
    typedef HashMap<String, Vector<String> > BreakpointIdToDebugServerBreakpointIdsMap;

    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Debugger* m_frontend;
    ScriptState* m_pausedScriptState;
    ScriptValue m_currentCallStack;
    ScriptsMap m_scripts;
    BreakpointIdToDebugServerBreakpointIdsMap m_breakpointIdToDebugServerBreakpointIds;
    String m_continueToLocationBreakpointId;
    String m_breakReason;
    RefPtr<InspectorObject> m_breakAuxData;
    bool m_javaScriptPauseScheduled;
    Listener* m_listener;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#endif // !defined(InspectorDebuggerAgent_h)
