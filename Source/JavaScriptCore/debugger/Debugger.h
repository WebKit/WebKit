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

#ifndef Debugger_h
#define Debugger_h

#include "Breakpoint.h"
#include "DebuggerCallFrame.h"
#include "DebuggerPrimitives.h"
#include "JSCJSValue.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace JSC {

class ExecState;
class JSGlobalObject;
class SourceProvider;
class VM;

typedef ExecState CallFrame;

class JS_EXPORT_PRIVATE Debugger {
public:
    Debugger(bool isInWorkerThread = false);
    virtual ~Debugger();

    JSC::DebuggerCallFrame* currentDebuggerCallFrame() const;
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

    bool needsExceptionCallbacks() const { return m_pauseOnExceptionsState != DontPauseOnExceptions; }

    void attach(JSGlobalObject*);
    enum ReasonForDetach {
        TerminatingDebuggingSession,
        GlobalObjectIsDestructing
    };
    virtual void detach(JSGlobalObject*, ReasonForDetach);

    BreakpointID setBreakpoint(Breakpoint, unsigned& actualLine, unsigned& actualColumn);
    void removeBreakpoint(BreakpointID);
    void clearBreakpoints();
    void setBreakpointsActivated(bool);
    void activateBreakpoints() { setBreakpointsActivated(true); }
    void deactivateBreakpoints() { setBreakpointsActivated(false); }

    enum PauseOnExceptionsState {
        DontPauseOnExceptions,
        PauseOnAllExceptions,
        PauseOnUncaughtExceptions
    };
    PauseOnExceptionsState pauseOnExceptionsState() const { return m_pauseOnExceptionsState; }
    void setPauseOnExceptionsState(PauseOnExceptionsState);

    void setPauseOnNextStatement(bool);
    void breakProgram();
    void continueProgram();
    void stepIntoStatement();
    void stepOverStatement();
    void stepOutOfFunction();

    bool isPaused() { return m_isPaused; }
    bool isStepping() const { return m_steppingMode == SteppingModeEnabled; }

    virtual void sourceParsed(ExecState*, SourceProvider*, int errorLineNumber, const WTF::String& errorMessage) = 0;

    void exception(CallFrame*, JSValue exceptionValue, bool hasHandler);
    void atStatement(CallFrame*);
    void callEvent(CallFrame*);
    void returnEvent(CallFrame*);
    void willExecuteProgram(CallFrame*);
    void didExecuteProgram(CallFrame*);
    void didReachBreakpoint(CallFrame*);

    void recompileAllJSFunctions(VM*);

    void registerCodeBlock(CodeBlock*);

protected:
    virtual bool needPauseHandling(JSGlobalObject*) { return false; }
    virtual void handleBreakpointHit(const Breakpoint&) { }
    virtual void handleExceptionInBreakpointCondition(ExecState*, JSValue exception) const { UNUSED_PARAM(exception); }

    enum ReasonForPause {
        NotPaused,
        PausedForException,
        PausedAtStatement,
        PausedAfterCall,
        PausedBeforeReturn,
        PausedAtStartOfProgram,
        PausedAtEndOfProgram,
        PausedForBreakpoint
    };

    virtual void handlePause(ReasonForPause, JSGlobalObject*) { }
    virtual void notifyDoneProcessingDebuggerEvents() { }

private:
    typedef HashMap<BreakpointID, Breakpoint*> BreakpointIDToBreakpointMap;

    typedef Vector<Breakpoint> BreakpointsInLine;
    typedef HashMap<unsigned, BreakpointsInLine, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> LineToBreakpointsMap;
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

    void updateNeedForOpDebugCallbacks();

    // These update functions are only needed because our current breakpoints are
    // key'ed off the source position instead of the bytecode PC. This ensures
    // that we don't break on the same line more than once. Once we switch to a
    // bytecode PC key'ed breakpoint, we will not need these anymore and should
    // be able to remove them.
    void updateCallFrame(JSC::CallFrame*);
    void updateCallFrameAndPauseIfNeeded(JSC::CallFrame*);
    void pauseIfNeeded(JSC::CallFrame*);

    enum SteppingMode {
        SteppingModeDisabled,
        SteppingModeEnabled
    };
    void setSteppingMode(SteppingMode);

    enum BreakpointState {
        BreakpointDisabled,
        BreakpointEnabled
    };
    void toggleBreakpoint(CodeBlock*, Breakpoint&, BreakpointState);
    void applyBreakpoints(CodeBlock*);
    void toggleBreakpoint(Breakpoint&, BreakpointState);

    void clearDebuggerRequests(JSGlobalObject*);

    VM* m_vm;
    HashSet<JSGlobalObject*> m_globalObjects;

    PauseOnExceptionsState m_pauseOnExceptionsState;
    bool m_pauseOnNextStatement : 1;
    bool m_isPaused : 1;
    bool m_breakpointsActivated : 1;
    bool m_hasHandlerForExceptionCallback : 1;
    bool m_isInWorkerThread : 1;
    SteppingMode m_steppingMode : 1;

    ReasonForPause m_reasonForPause;
    JSValue m_currentException;
    CallFrame* m_pauseOnCallFrame;
    CallFrame* m_currentCallFrame;
    unsigned m_lastExecutedLine;
    SourceID m_lastExecutedSourceID;

    BreakpointID m_topBreakpointID;
    BreakpointIDToBreakpointMap m_breakpointIDToBreakpoint;
    SourceIDToBreakpointsMap m_sourceIDToBreakpoints;

    RefPtr<JSC::DebuggerCallFrame> m_currentDebuggerCallFrame;

    friend class DebuggerCallFrameScope;
    friend class TemporaryPausedState;
    friend class LLIntOffsetsExtractor;
};

} // namespace JSC

#endif // Debugger_h
