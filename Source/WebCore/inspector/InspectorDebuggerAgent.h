/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "ScriptDebugListener.h"
#include "ScriptState.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class InjectedScriptHost;
class InspectorController;
class InspectorFrontend;
class InspectorObject;
class InspectorValue;

enum DebuggerEventType {
    JavaScriptPauseEventType,
    JavaScriptBreakpointEventType,
    NativeBreakpointDebuggerEventType
};

class InspectorDebuggerAgent : public ScriptDebugListener, public Noncopyable {
public:
    static PassOwnPtr<InspectorDebuggerAgent> create(InspectorController*, InspectorFrontend*);
    virtual ~InspectorDebuggerAgent();

    static bool isDebuggerAlwaysEnabled();

    void activateBreakpoints();
    void deactivateBreakpoints();
    void setStickyBreakpoint(const String& url, unsigned lineNumber, const String& condition, bool enabled);
    void setBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition, bool enabled, String* breakpointId, unsigned int* actualLineNumber);
    void removeBreakpoint(const String& breakpointId);

    void editScriptSource(const String& sourceID, const String& newContent, bool* success, String* result, RefPtr<InspectorValue>* newCallFrames);
    void getScriptSource(const String& sourceID, String* scriptSource);

    void schedulePauseOnNextStatement(DebuggerEventType type, PassRefPtr<InspectorValue> data);
    void cancelPauseOnNextStatement();
    void breakProgram(DebuggerEventType type, PassRefPtr<InspectorValue> data);
    void pause();
    void resume();
    void stepOver();
    void stepInto();
    void stepOut();

    void setPauseOnExceptionsState(long pauseState, long* newState);
    long pauseOnExceptionsState();

    void clearForPageNavigation();

private:
    InspectorDebuggerAgent(InspectorController*, InspectorFrontend*);

    PassRefPtr<InspectorValue> currentCallFrames();

    virtual void didParseSource(const String& sourceID, const String& url, const String& data, int lineOffset, int columnOffset, ScriptWorldType);
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);
    virtual void didPause(ScriptState*);
    virtual void didContinue();

    void restoreBreakpoint(const String& sourceID, unsigned lineNumber, const String& condition, bool enabled);

    InspectorController* m_inspectorController;
    InspectorFrontend* m_frontend;
    ScriptState* m_pausedScriptState;
    HashMap<String, String> m_scriptIDToContent;
    typedef HashMap<String, Vector<String> > URLToSourceIDsMap;
    URLToSourceIDsMap m_urlToSourceIDs;
    typedef std::pair<String, bool> Breakpoint;
    typedef HashMap<unsigned, Breakpoint> ScriptBreakpoints;
    HashMap<String, ScriptBreakpoints> m_stickyBreakpoints;
    RefPtr<InspectorObject> m_breakProgramDetails;
    bool m_javaScriptPauseScheduled;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#endif // !defined(InspectorDebuggerAgent_h)
