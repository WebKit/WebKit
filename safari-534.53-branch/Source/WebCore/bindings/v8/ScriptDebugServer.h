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

#include "OwnHandle.h"
#include "PlatformString.h"
#include "ScriptBreakpoint.h"
#include "Timer.h"
#include <v8-debug.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class ScriptDebugListener;
class ScriptValue;

class ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(ScriptDebugServer);
public:
    String setBreakpoint(const String& sourceID, const ScriptBreakpoint&, int* actualLineNumber, int* actualColumnNumber);
    void removeBreakpoint(const String& breakpointId);
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

    bool editScriptSource(const String& sourceID, const String& newContent, String* error, ScriptValue* newCallFrames);

    void recompileAllJSFunctionsSoon() { }
    void recompileAllJSFunctions(Timer<ScriptDebugServer>* = 0) { }

    class Task {
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };
    static void interruptAndRun(PassOwnPtr<Task>);
    void runPendingTasks();

protected:
    ScriptDebugServer();
    ~ScriptDebugServer() { }
    
    ScriptValue currentCallFrame();

    virtual ScriptDebugListener* getDebugListenerForContext(v8::Handle<v8::Context>) = 0;
    virtual void runMessageLoopOnPause(v8::Handle<v8::Context>) = 0;
    virtual void quitMessageLoopOnPause() = 0;

    static v8::Handle<v8::Value> breakProgramCallback(const v8::Arguments& args);
    void breakProgram(v8::Handle<v8::Object> executionState, v8::Handle<v8::Value> exception);

    static void v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails);
    void handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails);

    void dispatchDidParseSource(ScriptDebugListener* listener, v8::Handle<v8::Object> sourceObject);

    void ensureDebuggerScriptCompiled();
    
    bool isPaused();

    PauseOnExceptionsState m_pauseOnExceptionsState;
    OwnHandle<v8::Object> m_debuggerScript;
    OwnHandle<v8::Object> m_executionState;
    v8::Local<v8::Context> m_pausedContext;

    bool m_breakpointsActivated;
    OwnHandle<v8::FunctionTemplate> m_breakProgramCallbackTemplate;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // ScriptDebugServer_h
