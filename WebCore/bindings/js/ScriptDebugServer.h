/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ScriptDebugServer_h
#define ScriptDebugServer_h

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "ScriptDebugListener.h"
#include "PlatformString.h"
#include "ScriptBreakpoint.h"
#include "Timer.h"

#include <debugger/Debugger.h>
#include <runtime/UString.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace JSC {
class DebuggerCallFrame;
class JSGlobalObject;
}
namespace WebCore {

class Frame;
class FrameView;
class Page;
class PageGroup;
class ScriptDebugListener;
class JavaScriptCallFrame;

class ScriptDebugServer : JSC::Debugger, public Noncopyable {
public:
    static ScriptDebugServer& shared();

    void addListener(ScriptDebugListener*, Page*);
    void removeListener(ScriptDebugListener*, Page*);

    bool setBreakpoint(const String& sourceID, ScriptBreakpoint breakpoint, unsigned lineNumber, unsigned* actualLineNumber);
    void removeBreakpoint(const String& sourceID, unsigned lineNumber);
    void clearBreakpoints();
    void setBreakpointsActivated(bool activated);
    void activateBreakpoints() { setBreakpointsActivated(true); }
    void deactivateBreakpoints() { setBreakpointsActivated(false); }

    enum PauseOnExceptionsState {
        DontPauseOnExceptions,
        PauseOnAllExceptions,
        PauseOnUncaughtExceptions
    };
    PauseOnExceptionsState pauseOnExceptionsState() const { return m_pauseOnExceptionsState; }
    void setPauseOnExceptionsState(PauseOnExceptionsState);

    void setPauseOnNextStatement(bool pause);
    void breakProgram();
    void continueProgram();
    void stepIntoStatement();
    void stepOverStatement();
    void stepOutOfFunction();

    bool editScriptSource(const String& sourceID, const String& newContent, String& newSourceOrErrorMessage);

    void recompileAllJSFunctionsSoon();
    void recompileAllJSFunctions(Timer<ScriptDebugServer>* = 0);

    JavaScriptCallFrame* currentCallFrame();

    void pageCreated(Page*);

    bool isDebuggerAlwaysEnabled();

private:
    typedef HashSet<ScriptDebugListener*> ListenerSet;
    typedef void (ScriptDebugServer::*JavaScriptExecutionCallback)(ScriptDebugListener*);

    ScriptDebugServer();
    ~ScriptDebugServer();

    bool hasBreakpoint(intptr_t sourceID, unsigned lineNumber) const;
    bool hasListenersInterestedInPage(Page*);

    void setJavaScriptPaused(const PageGroup&, bool paused);
    void setJavaScriptPaused(Page*, bool paused);
    void setJavaScriptPaused(Frame*, bool paused);
    void setJavaScriptPaused(FrameView*, bool paused);

    void dispatchFunctionToListeners(JavaScriptExecutionCallback, Page*);
    void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback callback);
    void dispatchDidPause(ScriptDebugListener*);
    void dispatchDidContinue(ScriptDebugListener*);
    void dispatchDidParseSource(const ListenerSet& listeners, const JSC::SourceCode& source, enum ScriptWorldType);
    void dispatchFailedToParseSource(const ListenerSet& listeners, const JSC::SourceCode& source, int errorLine, const String& errorMessage);

    void pauseIfNeeded(Page*);

    virtual void detach(JSC::JSGlobalObject*);

    virtual void sourceParsed(JSC::ExecState*, const JSC::SourceCode&, int errorLine, const JSC::UString& errorMsg);
    virtual void callEvent(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineNumber);
    virtual void atStatement(const JSC::DebuggerCallFrame&, intptr_t sourceID, int firstLine);
    virtual void returnEvent(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineNumber);
    virtual void exception(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineNumber, bool hasHandler);
    virtual void willExecuteProgram(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineno);
    virtual void didExecuteProgram(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineno);
    virtual void didReachBreakpoint(const JSC::DebuggerCallFrame&, intptr_t sourceID, int lineno);

    void didAddListener(Page*);
    void didRemoveListener(Page*);

    typedef HashMap<Page*, ListenerSet*> PageListenersMap;
    typedef HashMap<intptr_t, SourceBreakpoints> BreakpointsMap;

    PageListenersMap m_pageListenersMap;
    bool m_callingListeners;
    PauseOnExceptionsState m_pauseOnExceptionsState;
    bool m_pauseOnNextStatement;
    bool m_paused;
    Page* m_pausedPage;
    bool m_doneProcessingDebuggerEvents;
    bool m_breakpointsActivated;
    JavaScriptCallFrame* m_pauseOnCallFrame;
    RefPtr<JavaScriptCallFrame> m_currentCallFrame;
    BreakpointsMap m_breakpoints;
    Timer<ScriptDebugServer> m_recompileTimer;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // ScriptDebugServer_h
