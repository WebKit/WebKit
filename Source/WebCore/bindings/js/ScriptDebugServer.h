/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ScriptDebugServer_h
#define ScriptDebugServer_h

#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "ScriptDebugListener.h"
#include "ScriptBreakpoint.h"
#include "Timer.h"
#include <debugger/Debugger.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class DebuggerCallFrame;
class JSGlobalObject;
class ExecState;
}
namespace WebCore {

class ScriptDebugListener;
class ScriptObject;
class ScriptValue;

class ScriptDebugServer : protected JSC::Debugger {
    WTF_MAKE_NONCOPYABLE(ScriptDebugServer); WTF_MAKE_FAST_ALLOCATED;
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
    PauseOnExceptionsState pauseOnExceptionsState() const { return m_pauseOnExceptionsState; }
    void setPauseOnExceptionsState(PauseOnExceptionsState);

    void setPauseOnNextStatement(bool pause);
    void breakProgram();
    void continueProgram();
    void stepIntoStatement();
    void stepOverStatement();
    void stepOutOfFunction();

    bool canSetScriptSource();
    bool setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, ScriptValue* newCallFrames, ScriptObject* result);
    void updateCallStack(ScriptValue* callFrame);

    bool causesRecompilation() { return true; }
    bool supportsSeparateScriptCompilationAndExecution() { return false; }

    void recompileAllJSFunctionsSoon();
    virtual void recompileAllJSFunctions(Timer<ScriptDebugServer>* = 0) = 0;

    void setScriptPreprocessor(const String&)
    {
        // FIXME(webkit.org/b/82203): Implement preprocessor.
    }

    bool isPaused() { return m_paused; }
    bool runningNestedMessageLoop() { return m_runningNestedMessageLoop; }

    void compileScript(JSC::ExecState*, const String& expression, const String& sourceURL, String* scriptId, String* exceptionMessage);
    void clearCompiledScripts();
    void runScript(JSC::ExecState*, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionMessage);

    class Task {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };

protected:
    typedef HashSet<ScriptDebugListener*> ListenerSet;
    typedef void (ScriptDebugServer::*JavaScriptExecutionCallback)(ScriptDebugListener*);

    ScriptDebugServer();
    ~ScriptDebugServer();

    virtual ListenerSet* getListenersForGlobalObject(JSC::JSGlobalObject*) = 0;
    virtual void didPause(JSC::JSGlobalObject*) = 0;
    virtual void didContinue(JSC::JSGlobalObject*) = 0;

    virtual void runEventLoopWhilePaused() = 0;

    virtual bool isContentScript(JSC::ExecState*);

    bool hasBreakpoint(intptr_t sourceID, const TextPosition&, ScriptBreakpoint* hitBreakpoint) const;
    bool evaluateBreakpointAction(const ScriptBreakpointAction&) const;
    bool evaluateBreakpointActions(const ScriptBreakpoint&) const;

    void dispatchFunctionToListeners(JavaScriptExecutionCallback, JSC::JSGlobalObject*);
    void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback callback);
    void dispatchDidPause(ScriptDebugListener*);
    void dispatchDidContinue(ScriptDebugListener*);
    void dispatchDidParseSource(const ListenerSet& listeners, JSC::SourceProvider*, bool isContentScript);
    void dispatchFailedToParseSource(const ListenerSet& listeners, JSC::SourceProvider*, int errorLine, const String& errorMessage);

    // These update functions are only needed because our current breakpoints are
    // key'ed off the source position instead of the bytecode PC. This ensures
    // that we don't break on the same line more than once. Once we switch to a
    // bytecode PC key'ed breakpoint, we will not need these anymore and should
    // be able to remove them.
    void updateCallFrame(JSC::CallFrame*);
    void updateCallFrameAndPauseIfNeeded(JSC::CallFrame*);
    void pauseIfNeeded(JSC::CallFrame*);

    JSC::DebuggerCallFrame* currentDebuggerCallFrame() const;

    virtual void detach(JSC::JSGlobalObject*) OVERRIDE;

    virtual void sourceParsed(JSC::ExecState*, JSC::SourceProvider*, int errorLine, const String& errorMsg) OVERRIDE;
    virtual void callEvent(JSC::CallFrame*) OVERRIDE;
    virtual void atStatement(JSC::CallFrame*) OVERRIDE;
    virtual void returnEvent(JSC::CallFrame*) OVERRIDE;
    virtual void exception(JSC::CallFrame*, JSC::JSValue exceptionValue, bool hasHandler) OVERRIDE;
    virtual void willExecuteProgram(JSC::CallFrame*) OVERRIDE;
    virtual void didExecuteProgram(JSC::CallFrame*) OVERRIDE;
    virtual void didReachBreakpoint(JSC::CallFrame*) OVERRIDE;

    typedef Vector<ScriptBreakpoint> BreakpointsInLine;
    typedef HashMap<int, BreakpointsInLine, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int> > LineToBreakpointsMap;
    typedef HashMap<intptr_t, LineToBreakpointsMap> SourceIdToBreakpointsMap;

    bool m_callingListeners;
    PauseOnExceptionsState m_pauseOnExceptionsState;
    bool m_pauseOnNextStatement;
    bool m_paused;
    bool m_runningNestedMessageLoop;
    bool m_doneProcessingDebuggerEvents;
    bool m_breakpointsActivated;
    JSC::CallFrame* m_pauseOnCallFrame;
    JSC::CallFrame* m_currentCallFrame;
    RefPtr<JSC::DebuggerCallFrame> m_currentDebuggerCallFrame;
    SourceIdToBreakpointsMap m_sourceIdToBreakpoints;
    Timer<ScriptDebugServer> m_recompileTimer;

    int m_lastExecutedLine;
    intptr_t m_lastExecutedSourceId;

    friend class DebuggerCallFrameScope;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // ScriptDebugServer_h
