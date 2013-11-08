/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
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

#include "BreakpointID.h"
#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "SourceID.h"
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

class ScriptDebugServer : public JSC::Debugger {
    WTF_MAKE_NONCOPYABLE(ScriptDebugServer); WTF_MAKE_FAST_ALLOCATED;
public:
    BreakpointID setBreakpoint(SourceID, const ScriptBreakpoint&, unsigned* actualLineNumber, unsigned* actualColumnNumber);
    void removeBreakpoint(BreakpointID);
    void clearBreakpoints();

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

    bool runningNestedMessageLoop() { return m_runningNestedMessageLoop; }

    void compileScript(JSC::ExecState*, const String& expression, const String& sourceURL, String* scriptID, String* exceptionMessage);
    void clearCompiledScripts();
    void runScript(JSC::ExecState*, const String& scriptID, ScriptValue* result, bool* wasThrown, String* exceptionMessage);

    class Task {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };

protected:
    typedef HashSet<ScriptDebugListener*> ListenerSet;
    typedef void (ScriptDebugServer::*JavaScriptExecutionCallback)(ScriptDebugListener*);

    ScriptDebugServer(bool isInWorkerThread = false);
    ~ScriptDebugServer();

    virtual ListenerSet* getListenersForGlobalObject(JSC::JSGlobalObject*) = 0;
    virtual void didPause(JSC::JSGlobalObject*) = 0;
    virtual void didContinue(JSC::JSGlobalObject*) = 0;

    virtual void runEventLoopWhilePaused() = 0;

    virtual bool isContentScript(JSC::ExecState*);

    bool evaluateBreakpointAction(const ScriptBreakpointAction&) const;

    void dispatchFunctionToListeners(JavaScriptExecutionCallback, JSC::JSGlobalObject*);
    void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback callback);
    void dispatchDidPause(ScriptDebugListener*);
    void dispatchDidContinue(ScriptDebugListener*);
    void dispatchDidParseSource(const ListenerSet& listeners, JSC::SourceProvider*, bool isContentScript);
    void dispatchFailedToParseSource(const ListenerSet& listeners, JSC::SourceProvider*, int errorLine, const String& errorMessage);

    virtual void sourceParsed(JSC::ExecState*, JSC::SourceProvider*, int errorLine, const String& errorMsg) OVERRIDE;

    bool m_doneProcessingDebuggerEvents;

private:
    typedef Vector<ScriptBreakpointAction> BreakpointActions;
    typedef HashMap<BreakpointID, BreakpointActions> BreakpointIDToActionsMap;

    virtual bool needPauseHandling(JSC::JSGlobalObject*) OVERRIDE;
    virtual void handleBreakpointHit(const JSC::Breakpoint&) OVERRIDE;
    virtual void handleExceptionInBreakpointCondition(JSC::ExecState*, JSC::JSValue exception) const OVERRIDE;
    virtual void handlePause(JSC::Debugger::ReasonForPause, JSC::JSGlobalObject*) OVERRIDE;
    virtual void notifyDoneProcessingDebuggerEvents() OVERRIDE;

    bool m_callingListeners;
    bool m_runningNestedMessageLoop;
    BreakpointIDToActionsMap m_breakpointIDToActions;
    Timer<ScriptDebugServer> m_recompileTimer;

    friend class DebuggerCallFrameScope;
};

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER)

#endif // ScriptDebugServer_h
