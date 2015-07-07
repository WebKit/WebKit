/*
 * Copyright (C) 2008, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef ScriptDebugServer_h
#define ScriptDebugServer_h

#include "ScriptBreakpoint.h"
#include "ScriptDebugListener.h"
#include "bindings/ScriptObject.h"
#include "debugger/Debugger.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ExecState;
class JSGlobalObject;
}

namespace Inspector {

class JS_EXPORT_PRIVATE ScriptDebugServer : public JSC::Debugger {
    WTF_MAKE_NONCOPYABLE(ScriptDebugServer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    JSC::BreakpointID setBreakpoint(JSC::SourceID, const ScriptBreakpoint&, unsigned* actualLineNumber, unsigned* actualColumnNumber);
    void removeBreakpoint(JSC::BreakpointID);
    void clearBreakpoints();

    virtual void recompileAllJSFunctions() = 0;

    const BreakpointActions& getActionsForBreakpoint(JSC::BreakpointID);

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
    virtual bool isContentScript(JSC::ExecState*) const = 0;
    virtual void reportException(JSC::ExecState*, JSC::JSValue) const = 0;

    bool evaluateBreakpointAction(const ScriptBreakpointAction&);

    void dispatchFunctionToListeners(JavaScriptExecutionCallback, JSC::JSGlobalObject*);
    void dispatchFunctionToListeners(const ListenerSet& listeners, JavaScriptExecutionCallback);
    void dispatchDidPause(ScriptDebugListener*);
    void dispatchDidContinue(ScriptDebugListener*);
    void dispatchDidParseSource(const ListenerSet& listeners, JSC::SourceProvider*, bool isContentScript);
    void dispatchFailedToParseSource(const ListenerSet& listeners, JSC::SourceProvider*, int errorLine, const String& errorMessage);
    void dispatchBreakpointActionLog(JSC::ExecState*, const String&);
    void dispatchBreakpointActionSound(JSC::ExecState*, int breakpointActionIdentifier);
    void dispatchBreakpointActionProbe(JSC::ExecState*, const ScriptBreakpointAction&, const Deprecated::ScriptValue& sample);

    bool m_doneProcessingDebuggerEvents;

private:
    typedef HashMap<JSC::BreakpointID, BreakpointActions> BreakpointIDToActionsMap;

    virtual void sourceParsed(JSC::ExecState*, JSC::SourceProvider*, int errorLine, const String& errorMsg) override final;
    virtual bool needPauseHandling(JSC::JSGlobalObject*) override final;
    virtual void handleBreakpointHit(const JSC::Breakpoint&) override final;
    virtual void handleExceptionInBreakpointCondition(JSC::ExecState*, JSC::JSValue exception) const override final;
    virtual void handlePause(JSC::Debugger::ReasonForPause, JSC::JSGlobalObject*) override final;
    virtual void notifyDoneProcessingDebuggerEvents() override final;

    unsigned m_hitCount;
    bool m_callingListeners;
    BreakpointIDToActionsMap m_breakpointIDToActions;
};

} // namespace Inspector

#endif // ScriptDebugServer_h
