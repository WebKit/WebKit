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

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "ScriptState.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {
class InjectedScriptHost;
class InspectorController;
class InspectorFrontend;
class InspectorObject;
class InspectorValue;

enum DebuggerEventType {
    DOMBreakpointDebuggerEventType,
    NativeBreakpointDebuggerEventType
};

class InspectorDebuggerAgent : public ScriptDebugListener, public Noncopyable {
public:
    static PassOwnPtr<InspectorDebuggerAgent> create(InspectorController*, InspectorFrontend*);
    virtual ~InspectorDebuggerAgent();

    static bool isDebuggerAlwaysEnabled();

    void activateBreakpoints();
    void deactivateBreakpoints();
    void setBreakpoint(const String& sourceID, unsigned lineNumber, bool enabled, const String& condition, bool* success, unsigned int* actualLineNumber);
    void removeBreakpoint(const String& sourceID, unsigned lineNumber);

    void editScriptSource(const String& sourceID, const String& newContent, bool* success, String* result, RefPtr<InspectorValue>* newCallFrames);
    void getScriptSource(const String& sourceID, String* scriptSource);

    void pause();
    void breakProgram(DebuggerEventType type, PassRefPtr<InspectorValue> data);
    void resume();
    void stepOverStatement();
    void stepIntoStatement();
    void stepOutOfFunction();

    void setPauseOnExceptionsState(long pauseState);

    void clearForPageNavigation();

    static String md5Base16(const String& string);

private:
    InspectorDebuggerAgent(InspectorController*, InspectorFrontend*);

    PassRefPtr<InspectorValue> currentCallFrames();

    void loadBreakpoints();
    void saveBreakpoints();

    virtual void didParseSource(const String& sourceID, const String& url, const String& data, int firstLine, ScriptWorldType);
    virtual void failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage);
    virtual void didPause(ScriptState*);
    virtual void didContinue();

    InspectorController* m_inspectorController;
    InspectorFrontend* m_frontend;
    ScriptState* m_pausedScriptState;
    HashMap<String, String> m_sourceIDToURL;
    HashMap<String, String> m_scriptIDToContent;
    HashMap<String, SourceBreakpoints> m_stickyBreakpoints;
    HashMap<String, unsigned> m_breakpointsMapping;
    bool m_breakpointsLoaded;
    static InspectorDebuggerAgent* s_debuggerAgentOnBreakpoint;
    RefPtr<InspectorObject> m_breakProgramDetails;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // !defined(InspectorDebuggerAgent_h)
