/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2008, 2009, 2013, 2014 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "Breakpoint.h"
#include "CallData.h"
#include "DebuggerCallFrame.h"
#include "DebuggerParseData.h"
#include "DebuggerPrimitives.h"
#include "JSCJSValue.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/TextPosition.h>

namespace JSC {

class CallFrame;
class CodeBlock;
class Exception;
class JSGlobalObject;
class SourceProvider;
class VM;

class JS_EXPORT_PRIVATE Debugger {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Debugger(VM&);
    virtual ~Debugger();

    VM& vm() { return m_vm; }

    JSC::DebuggerCallFrame& currentDebuggerCallFrame();
    bool hasHandlerForExceptionCallback() const
    {
        ASSERT(m_reasonForPause == PausedForException);
        return m_hasHandlerForExceptionCallback;
    }
    JSValue currentException()
    {
        ASSERT(m_reasonForPause == PausedForException);
        return m_currentException;
    }

    bool needsExceptionCallbacks() const { return m_breakpointsActivated && m_pauseOnExceptionsState != DontPauseOnExceptions; }
    bool isInteractivelyDebugging() const { return m_breakpointsActivated; }

    enum ReasonForDetach {
        TerminatingDebuggingSession,
        GlobalObjectIsDestructing
    };
    void attach(JSGlobalObject*);
    void detach(JSGlobalObject*, ReasonForDetach);
    bool isAttached(JSGlobalObject*);

    void resolveBreakpoint(Breakpoint&, SourceProvider*);
    BreakpointID setBreakpoint(Breakpoint&, bool& existing);
    void removeBreakpoint(BreakpointID);
    void clearBreakpoints();

    void activateBreakpoints() { setBreakpointsActivated(true); }
    void deactivateBreakpoints() { setBreakpointsActivated(false); }
    bool breakpointsActive() const { return m_breakpointsActivated; }

    enum PauseOnExceptionsState {
        DontPauseOnExceptions,
        PauseOnAllExceptions,
        PauseOnUncaughtExceptions
    };
    PauseOnExceptionsState pauseOnExceptionsState() const { return m_pauseOnExceptionsState; }
    void setPauseOnExceptionsState(PauseOnExceptionsState);

    void setPauseOnDebuggerStatements(bool enabled) { m_pauseOnDebuggerStatements = enabled; }

    enum ReasonForPause {
        NotPaused,
        PausedForException,
        PausedAtStatement,
        PausedAtExpression,
        PausedBeforeReturn,
        PausedAtEndOfProgram,
        PausedForBreakpoint,
        PausedForDebuggerStatement,
        PausedAfterBlackboxedScript,
    };
    ReasonForPause reasonForPause() const { return m_reasonForPause; }
    BreakpointID pausingBreakpointID() const { return m_pausingBreakpointID; }

    void setPauseOnNextStatement(bool);
    void breakProgram();
    void continueProgram();
    void stepIntoStatement();
    void stepOverStatement();
    void stepOutOfFunction();

    enum class BlackboxType { Deferred, Ignored };
    void setBlackboxType(SourceID, Optional<BlackboxType>);
    void clearBlackbox();

    bool isPaused() const { return m_isPaused; }
    bool isStepping() const { return m_steppingMode == SteppingModeEnabled; }

    bool suppressAllPauses() const { return m_suppressAllPauses; }
    void setSuppressAllPauses(bool suppress) { m_suppressAllPauses = suppress; }

    virtual void sourceParsed(JSGlobalObject*, SourceProvider*, int errorLineNumber, const WTF::String& errorMessage) = 0;
    virtual void willRunMicrotask() { }
    virtual void didRunMicrotask() { }

    void exception(JSGlobalObject*, CallFrame*, JSValue exceptionValue, bool hasCatchHandler);
    void atStatement(CallFrame*);
    void atExpression(CallFrame*);
    void callEvent(CallFrame*);
    void returnEvent(CallFrame*);
    void unwindEvent(CallFrame*);
    void willExecuteProgram(CallFrame*);
    void didExecuteProgram(CallFrame*);
    void didReachDebuggerStatement(CallFrame*);

    virtual void recompileAllJSFunctions();

    void registerCodeBlock(CodeBlock*);

    class ProfilingClient {
    public:
        virtual ~ProfilingClient();
        virtual bool isAlreadyProfiling() const = 0;
        virtual Seconds willEvaluateScript() = 0;
        virtual void didEvaluateScript(Seconds startTime, ProfilingReason) = 0;
    };

    void setProfilingClient(ProfilingClient*);
    bool hasProfilingClient() const { return m_profilingClient != nullptr; }
    bool isAlreadyProfiling() const { return m_profilingClient && m_profilingClient->isAlreadyProfiling(); }
    Seconds willEvaluateScript();
    void didEvaluateScript(Seconds startTime, ProfilingReason);

protected:
    virtual void handleBreakpointHit(JSGlobalObject*, const Breakpoint&) { }
    virtual void handleExceptionInBreakpointCondition(JSGlobalObject*, Exception*) const { }
    virtual void handlePause(JSGlobalObject*, ReasonForPause) { }
    virtual void notifyDoneProcessingDebuggerEvents() { }

private:
    typedef HashMap<BreakpointID, Breakpoint*> BreakpointIDToBreakpointMap;

    typedef HashMap<unsigned, RefPtr<BreakpointsList>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> LineToBreakpointsMap;
    typedef HashMap<SourceID, LineToBreakpointsMap, WTF::IntHash<SourceID>, WTF::UnsignedWithZeroKeyHashTraits<SourceID>> SourceIDToBreakpointsMap;

    class ClearCodeBlockDebuggerRequestsFunctor;
    class ClearDebuggerRequestsFunctor;
    class SetSteppingModeFunctor;
    class ToggleBreakpointFunctor;

    class PauseReasonDeclaration {
    public:
        PauseReasonDeclaration(Debugger& debugger, ReasonForPause reason)
            : m_debugger(debugger)
        {
            m_debugger.m_reasonForPause = reason;
        }

        ~PauseReasonDeclaration()
        {
            m_debugger.m_reasonForPause = NotPaused;
        }
    private:
        Debugger& m_debugger;
    };

    bool hasBreakpoint(SourceID, const TextPosition&, Breakpoint* hitBreakpoint);

    DebuggerParseData& debuggerParseData(SourceID, SourceProvider*);

    void updateNeedForOpDebugCallbacks();

    // These update functions are only needed because our current breakpoints are
    // key'ed off the source position instead of the bytecode PC. This ensures
    // that we don't break on the same line more than once. Once we switch to a
    // bytecode PC key'ed breakpoint, we will not need these anymore and should
    // be able to remove them.
    enum CallFrameUpdateAction { AttemptPause, NoPause };
    void updateCallFrame(JSC::JSGlobalObject*, JSC::CallFrame*, CallFrameUpdateAction);
    void updateCallFrameInternal(JSC::CallFrame*);
    void pauseIfNeeded(JSC::JSGlobalObject*);
    void clearNextPauseState();

    enum SteppingMode {
        SteppingModeDisabled,
        SteppingModeEnabled
    };
    void setSteppingMode(SteppingMode);

    enum BreakpointState {
        BreakpointDisabled,
        BreakpointEnabled
    };
    void setBreakpointsActivated(bool);
    void toggleBreakpoint(CodeBlock*, Breakpoint&, BreakpointState);
    void applyBreakpoints(CodeBlock*);
    void toggleBreakpoint(Breakpoint&, BreakpointState);

    void clearDebuggerRequests(JSGlobalObject*);
    void clearParsedData();

    VM& m_vm;
    HashSet<JSGlobalObject*> m_globalObjects;
    HashMap<SourceID, DebuggerParseData, WTF::IntHash<SourceID>, WTF::UnsignedWithZeroKeyHashTraits<SourceID>> m_parseDataMap;
    HashMap<SourceID, BlackboxType, WTF::IntHash<SourceID>, WTF::UnsignedWithZeroKeyHashTraits<SourceID>> m_blackboxedScripts;

    PauseOnExceptionsState m_pauseOnExceptionsState;
    bool m_pauseOnDebuggerStatements : 1;
    bool m_pauseAtNextOpportunity : 1;
    bool m_pauseOnStepOut : 1;
    bool m_pastFirstExpressionInStatement : 1;
    bool m_isPaused : 1;
    bool m_breakpointsActivated : 1;
    bool m_hasHandlerForExceptionCallback : 1;
    bool m_suppressAllPauses : 1;
    unsigned m_steppingMode : 1; // SteppingMode

    ReasonForPause m_reasonForPause;
    JSValue m_currentException;
    CallFrame* m_pauseOnCallFrame { nullptr };
    CallFrame* m_currentCallFrame { nullptr };
    unsigned m_lastExecutedLine;
    SourceID m_lastExecutedSourceID;
    bool m_afterBlackboxedScript { false };

    BreakpointID m_topBreakpointID;
    BreakpointID m_pausingBreakpointID;
    BreakpointIDToBreakpointMap m_breakpointIDToBreakpoint;
    SourceIDToBreakpointsMap m_sourceIDToBreakpoints;

    RefPtr<JSC::DebuggerCallFrame> m_currentDebuggerCallFrame;

    ProfilingClient* m_profilingClient { nullptr };

    friend class DebuggerPausedScope;
    friend class TemporaryPausedState;
    friend class LLIntOffsetsExtractor;
};

} // namespace JSC
