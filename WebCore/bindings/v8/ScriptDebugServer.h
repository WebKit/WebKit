/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef ScriptDebugServer_h
#define ScriptDebugServer_h

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "JavaScriptCallFrame.h"
#include "PlatformString.h"
#include "ScriptBreakpoint.h"
#include "Timer.h"
#include <v8-debug.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Page;
class ScriptDebugListener;

class ScriptDebugServer : public Noncopyable {
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
    PauseOnExceptionsState pauseOnExceptionsState();
    void setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState);

    void setPauseOnNextStatement(bool pause);
    void breakProgram();
    void continueProgram();
    void stepIntoStatement();
    void stepOverStatement();
    void stepOutOfFunction();

    bool editScriptSource(const String& sourceID, const String& newContent, String& newSourceOrErrorMessage);

    void recompileAllJSFunctionsSoon() { }
    void recompileAllJSFunctions(Timer<ScriptDebugServer>* = 0) { }

    void pageCreated(Page*) { }

    // v8-specific methods.
    void setDebuggerScriptSource(const String& scriptSource);

    class ClientMessageLoop {
    public:
        virtual ~ClientMessageLoop() { }
        virtual void run(Page*) = 0;
        virtual void quitNow() = 0;
    };
    void setClientMessageLoop(PassOwnPtr<ClientMessageLoop> clientMessageLoop) { m_clientMessageLoop = clientMessageLoop; }

    PassRefPtr<JavaScriptCallFrame> currentCallFrame();

    void setEnabled(bool);
    bool isDebuggerAlwaysEnabled();

    class Task {
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };
    static void interruptAndRun(PassOwnPtr<Task>);
    void runPendingTasks();

private:
    ScriptDebugServer();
    ~ScriptDebugServer() { }

    static v8::Handle<v8::Value> breakProgramCallback(const v8::Arguments& args);
    void breakProgram(v8::Handle<v8::Object> executionState);

    static void v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails);
    void handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails);

    void dispatchDidParseSource(ScriptDebugListener* listener, v8::Handle<v8::Object> sourceObject);

    void ensureDebuggerScriptCompiled();
    void didResume();

    typedef HashMap<Page*, ScriptDebugListener*> ListenersMap;
    ListenersMap m_listenersMap;
    String m_debuggerScriptSource;
    PauseOnExceptionsState m_pauseOnExceptionsState;
    OwnHandle<v8::Object> m_debuggerScript;
    RefPtr<JavaScriptCallFrame> m_currentCallFrame;
    OwnHandle<v8::Object> m_executionState;
    OwnPtr<ClientMessageLoop> m_clientMessageLoop;
    Page* m_pausedPage;
    v8::Local<v8::Context> m_pausedPageContext;
    bool m_enabled;

    bool m_breakpointsActivated;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // ScriptDebugServer_h
