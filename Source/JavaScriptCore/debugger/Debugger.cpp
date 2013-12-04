/*
 *  Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#include "config.h"
#if ENABLE(JAVASCRIPT_DEBUGGER)

#include "Debugger.h"

#include "DebuggerCallFrame.h"
#include "Error.h"
#include "HeapIterationScope.h"
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Operations.h"
#include "Parser.h"
#include "Protect.h"
#include "VMEntryScope.h"

namespace {

using namespace JSC;

class Recompiler : public MarkedBlock::VoidFunctor {
public:
    Recompiler(JSC::Debugger*);
    ~Recompiler();
    void operator()(JSCell*);

private:
    typedef HashSet<FunctionExecutable*> FunctionExecutableSet;
    typedef HashMap<SourceProvider*, ExecState*> SourceProviderMap;
    
    JSC::Debugger* m_debugger;
    FunctionExecutableSet m_functionExecutables;
    SourceProviderMap m_sourceProviders;
};

inline Recompiler::Recompiler(JSC::Debugger* debugger)
    : m_debugger(debugger)
{
}

inline Recompiler::~Recompiler()
{
    // Call sourceParsed() after reparsing all functions because it will execute
    // JavaScript in the inspector.
    SourceProviderMap::const_iterator end = m_sourceProviders.end();
    for (SourceProviderMap::const_iterator iter = m_sourceProviders.begin(); iter != end; ++iter)
        m_debugger->sourceParsed(iter->value, iter->key, -1, String());
}

inline void Recompiler::operator()(JSCell* cell)
{
    if (!cell->inherits(JSFunction::info()))
        return;

    JSFunction* function = jsCast<JSFunction*>(cell);
    if (function->executable()->isHostFunction())
        return;

    FunctionExecutable* executable = function->jsExecutable();

    // Check if the function is already in the set - if so,
    // we've already retranslated it, nothing to do here.
    if (!m_functionExecutables.add(executable).isNewEntry)
        return;

    ExecState* exec = function->scope()->globalObject()->JSGlobalObject::globalExec();
    executable->clearCodeIfNotCompiling();
    executable->clearUnlinkedCodeForRecompilationIfNotCompiling();
    if (m_debugger == function->scope()->globalObject()->debugger())
        m_sourceProviders.add(executable->source().provider(), exec);
}

} // namespace

namespace JSC {

class DebuggerCallFrameScope {
public:
    DebuggerCallFrameScope(Debugger& debugger)
        : m_debugger(debugger)
    {
        ASSERT(!m_debugger.m_currentDebuggerCallFrame);
        if (m_debugger.m_currentCallFrame)
            m_debugger.m_currentDebuggerCallFrame = DebuggerCallFrame::create(debugger.m_currentCallFrame);
    }

    ~DebuggerCallFrameScope()
    {
        if (m_debugger.m_currentDebuggerCallFrame) {
            m_debugger.m_currentDebuggerCallFrame->invalidate();
            m_debugger.m_currentDebuggerCallFrame = 0;
        }
    }

private:
    Debugger& m_debugger;
};

// This is very similar to TemporaryChange<bool>, but that cannot be used
// as the m_isPaused field uses only one bit.
class TemporaryPausedState {
public:
    TemporaryPausedState(Debugger& debugger)
        : m_debugger(debugger)
    {
        ASSERT(!m_debugger.m_isPaused);
        m_debugger.m_isPaused = true;
    }

    ~TemporaryPausedState()
    {
        m_debugger.m_isPaused = false;
    }

private:
    Debugger& m_debugger;
};

Debugger::Debugger(bool isInWorkerThread)
    : m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_pauseOnNextStatement(false)
    , m_isPaused(false)
    , m_breakpointsActivated(true)
    , m_hasHandlerForExceptionCallback(false)
    , m_isInWorkerThread(isInWorkerThread)
    , m_reasonForPause(NotPaused)
    , m_pauseOnCallFrame(0)
    , m_currentCallFrame(0)
    , m_lastExecutedLine(UINT_MAX)
    , m_lastExecutedSourceID(noSourceID)
    , m_topBreakpointID(noBreakpointID)
    , m_needsOpDebugCallbacks(false)
    , m_shouldPause(false)
{
}

Debugger::~Debugger()
{
    HashSet<JSGlobalObject*>::iterator end = m_globalObjects.end();
    for (HashSet<JSGlobalObject*>::iterator it = m_globalObjects.begin(); it != end; ++it)
        (*it)->setDebugger(0);
}

void Debugger::attach(JSGlobalObject* globalObject)
{
    ASSERT(!globalObject->debugger());
    globalObject->setDebugger(this);
    m_globalObjects.add(globalObject);
}

void Debugger::detach(JSGlobalObject* globalObject)
{
    // If we're detaching from the currently executing global object, manually tear down our
    // stack, since we won't get further debugger callbacks to do so. Also, resume execution,
    // since there's no point in staying paused once a window closes.
    if (m_currentCallFrame && m_currentCallFrame->vmEntryGlobalObject() == globalObject) {
        m_currentCallFrame = 0;
        m_pauseOnCallFrame = 0;
        continueProgram();
    }

    ASSERT(m_globalObjects.contains(globalObject));
    m_globalObjects.remove(globalObject);
    globalObject->setDebugger(0);
}

void Debugger::setShouldPause(bool value)
{
    m_shouldPause = value;
    updateNeedForOpDebugCallbacks();
}

void Debugger::recompileAllJSFunctions(VM* vm)
{
    // If JavaScript is running, it's not safe to recompile, since we'll end
    // up throwing away code that is live on the stack.
    ASSERT(!vm->entryScope);
    if (vm->entryScope)
        return;
    
    vm->prepareToDiscardCode();

    Recompiler recompiler(this);
    HeapIterationScope iterationScope(vm->heap);
    vm->heap.objectSpace().forEachLiveCell(iterationScope, recompiler);
}

void Debugger::updateNeedForOpDebugCallbacks()
{
    size_t numberOfBreakpoints = m_breakpointIDToBreakpoint.size();
    m_needsOpDebugCallbacks = m_shouldPause || numberOfBreakpoints;
}

BreakpointID Debugger::setBreakpoint(Breakpoint breakpoint, unsigned& actualLine, unsigned& actualColumn)
{
    SourceID sourceID = breakpoint.sourceID;
    unsigned line = breakpoint.line;
    unsigned column = breakpoint.column;

    SourceIDToBreakpointsMap::iterator it = m_sourceIDToBreakpoints.find(sourceID);
    if (it == m_sourceIDToBreakpoints.end())
        it = m_sourceIDToBreakpoints.set(sourceID, LineToBreakpointsMap()).iterator;
    LineToBreakpointsMap::iterator breaksIt = it->value.find(line);
    if (breaksIt == it->value.end())
        breaksIt = it->value.set(line, BreakpointsInLine()).iterator;

    BreakpointsInLine& breakpoints = breaksIt->value;
    unsigned breakpointsCount = breakpoints.size();
    for (unsigned i = 0; i < breakpointsCount; i++)
        if (breakpoints[i].column == column) {
            // The breakpoint already exists. We're not allowed to create a new
            // breakpoint at this location. Rather than returning the breakpointID
            // of the pre-existing breakpoint, we need to return noBreakpointID
            // to indicate that we're not creating a new one.
            return noBreakpointID;
        }

    BreakpointID id = ++m_topBreakpointID;
    RELEASE_ASSERT(id != noBreakpointID);

    breakpoint.id = id;
    actualLine = line;
    actualColumn = column;

    breakpoints.append(breakpoint);
    m_breakpointIDToBreakpoint.set(id, &breakpoints.last());

    updateNeedForOpDebugCallbacks();

    return id;
}

void Debugger::removeBreakpoint(BreakpointID id)
{
    ASSERT(id != noBreakpointID);

    BreakpointIDToBreakpointMap::iterator idIt = m_breakpointIDToBreakpoint.find(id);
    ASSERT(idIt != m_breakpointIDToBreakpoint.end());
    Breakpoint& breakpoint = *idIt->value;

    SourceID sourceID = breakpoint.sourceID;
    ASSERT(sourceID);
    SourceIDToBreakpointsMap::iterator it = m_sourceIDToBreakpoints.find(sourceID);
    ASSERT(it != m_sourceIDToBreakpoints.end());
    LineToBreakpointsMap::iterator breaksIt = it->value.find(breakpoint.line);
    ASSERT(breaksIt != it->value.end());

    BreakpointsInLine& breakpoints = breaksIt->value;
    unsigned breakpointsCount = breakpoints.size();
    for (unsigned i = 0; i < breakpointsCount; i++) {
        if (breakpoints[i].id == breakpoint.id) {
            breakpoints.remove(i);
            m_breakpointIDToBreakpoint.remove(idIt);

            if (breakpoints.isEmpty()) {
                it->value.remove(breaksIt);
                if (it->value.isEmpty())
                    m_sourceIDToBreakpoints.remove(it);
            }
            break;
        }
    }

    updateNeedForOpDebugCallbacks();
}

bool Debugger::hasBreakpoint(SourceID sourceID, const TextPosition& position, Breakpoint *hitBreakpoint)
{
    if (!m_breakpointsActivated)
        return false;

    SourceIDToBreakpointsMap::const_iterator it = m_sourceIDToBreakpoints.find(sourceID);
    if (it == m_sourceIDToBreakpoints.end())
        return false;

    unsigned line = position.m_line.zeroBasedInt();
    unsigned column = position.m_column.zeroBasedInt();

    LineToBreakpointsMap::const_iterator breaksIt = it->value.find(line);
    if (breaksIt == it->value.end())
        return false;

    bool hit = false;
    const BreakpointsInLine& breakpoints = breaksIt->value;
    unsigned breakpointsCount = breakpoints.size();
    unsigned i;
    for (i = 0; i < breakpointsCount; i++) {
        unsigned breakLine = breakpoints[i].line;
        unsigned breakColumn = breakpoints[i].column;
        // Since frontend truncates the indent, the first statement in a line must match the breakpoint (line,0).
        if ((line != m_lastExecutedLine && line == breakLine && !breakColumn)
            || (line == breakLine && column == breakColumn)) {
            hit = true;
            break;
        }
    }
    if (!hit)
        return false;

    if (hitBreakpoint)
        *hitBreakpoint = breakpoints[i];

    if (breakpoints[i].condition.isEmpty())
        return true;

    // We cannot stop in the debugger while executing condition code,
    // so make it looks like the debugger is already paused.
    TemporaryPausedState pausedState(*this);

    JSValue exception;
    JSValue result = DebuggerCallFrame::evaluateWithCallFrame(m_currentCallFrame, breakpoints[i].condition, exception);

    // We can lose the debugger while executing JavaScript.
    if (!m_currentCallFrame)
        return false;

    if (exception) {
        // An erroneous condition counts as "false".
        handleExceptionInBreakpointCondition(m_currentCallFrame, exception);
        return false;
    }

    return result.toBoolean(m_currentCallFrame);
}

void Debugger::clearBreakpoints()
{
    m_topBreakpointID = noBreakpointID;
    m_breakpointIDToBreakpoint.clear();
    m_sourceIDToBreakpoints.clear();

    updateNeedForOpDebugCallbacks();
}

void Debugger::setBreakpointsActivated(bool activated)
{
    m_breakpointsActivated = activated;
}

void Debugger::setPauseOnExceptionsState(PauseOnExceptionsState pause)
{
    m_pauseOnExceptionsState = pause;
}

void Debugger::setPauseOnNextStatement(bool pause)
{
    m_pauseOnNextStatement = pause;
    if (pause)
        setShouldPause(true);
}

void Debugger::breakProgram()
{
    if (m_isPaused || !m_currentCallFrame)
        return;

    m_pauseOnNextStatement = true;
    setShouldPause(true);
    pauseIfNeeded(m_currentCallFrame);
}

void Debugger::continueProgram()
{
    if (!m_isPaused)
        return;

    m_pauseOnNextStatement = false;
    notifyDoneProcessingDebuggerEvents();
}

void Debugger::stepIntoStatement()
{
    if (!m_isPaused)
        return;

    m_pauseOnNextStatement = true;
    setShouldPause(true);
    notifyDoneProcessingDebuggerEvents();
}

void Debugger::stepOverStatement()
{
    if (!m_isPaused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame;
    notifyDoneProcessingDebuggerEvents();
}

void Debugger::stepOutOfFunction()
{
    if (!m_isPaused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->callerFrameSkippingVMEntrySentinel() : 0;
    notifyDoneProcessingDebuggerEvents();
}

void Debugger::updateCallFrame(CallFrame* callFrame)
{
    m_currentCallFrame = callFrame;
    SourceID sourceID = DebuggerCallFrame::sourceIDForCallFrame(callFrame);
    if (m_lastExecutedSourceID != sourceID) {
        m_lastExecutedLine = UINT_MAX;
        m_lastExecutedSourceID = sourceID;
    }
}

void Debugger::updateCallFrameAndPauseIfNeeded(CallFrame* callFrame)
{
    updateCallFrame(callFrame);
    pauseIfNeeded(callFrame);
    if (!needsOpDebugCallbacks())
        m_currentCallFrame = 0;
}

void Debugger::pauseIfNeeded(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    JSGlobalObject* vmEntryGlobalObject = callFrame->vmEntryGlobalObject();
    if (!needPauseHandling(vmEntryGlobalObject))
        return;

    Breakpoint breakpoint;
    bool didHitBreakpoint = false;
    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);

    intptr_t sourceID = DebuggerCallFrame::sourceIDForCallFrame(m_currentCallFrame);
    TextPosition position = DebuggerCallFrame::positionForCallFrame(m_currentCallFrame);
    pauseNow |= didHitBreakpoint = hasBreakpoint(sourceID, position, &breakpoint);
    m_lastExecutedLine = position.m_line.zeroBasedInt();
    if (!pauseNow)
        return;

    DebuggerCallFrameScope debuggerCallFrameScope(*this);

    // Make sure we are not going to pause again on breakpoint actions by
    // reseting the pause state before executing any breakpoint actions.
    TemporaryPausedState pausedState(*this);
    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;

    if (didHitBreakpoint) {
        handleBreakpointHit(breakpoint);
        // Note that the actions can potentially stop the debugger, so we need to check that
        // we still have a current call frame when we get back.
        if (breakpoint.autoContinue || !m_currentCallFrame)
            return;
    }

    handlePause(m_reasonForPause, vmEntryGlobalObject);

    if (!m_pauseOnNextStatement && !m_pauseOnCallFrame) {
        setShouldPause(false);
        if (!needsOpDebugCallbacks())
            m_currentCallFrame = 0;
    }
}

void Debugger::exception(CallFrame* callFrame, JSValue exception, bool hasHandler)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedForException);
    if (m_pauseOnExceptionsState == PauseOnAllExceptions || (m_pauseOnExceptionsState == PauseOnUncaughtExceptions && !hasHandler)) {
        m_pauseOnNextStatement = true;
        setShouldPause(true);
    }

    m_hasHandlerForExceptionCallback = true;
    m_currentException = exception;
    updateCallFrameAndPauseIfNeeded(callFrame);
    m_currentException = JSValue();
    m_hasHandlerForExceptionCallback = false;
}

void Debugger::atStatement(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAtStatement);
    updateCallFrameAndPauseIfNeeded(callFrame);
}

void Debugger::callEvent(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAfterCall);
    updateCallFrameAndPauseIfNeeded(callFrame);
}

void Debugger::returnEvent(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedBeforeReturn);
    updateCallFrameAndPauseIfNeeded(callFrame);

    // detach may have been called during pauseIfNeeded
    if (!m_currentCallFrame)
        return;

    // Treat stepping over a return statement like stepping out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = m_currentCallFrame->callerFrameSkippingVMEntrySentinel();

    m_currentCallFrame = m_currentCallFrame->callerFrameSkippingVMEntrySentinel();
}

void Debugger::willExecuteProgram(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAtStartOfProgram);
    // FIXME: This check for whether we're debugging a worker thread is a workaround
    // for https://bugs.webkit.org/show_bug.cgi?id=102637. Remove it when we rework
    // the debugger implementation to not require callbacks.
    if (!m_isInWorkerThread)
        updateCallFrameAndPauseIfNeeded(callFrame);
    else if (needsOpDebugCallbacks())
        updateCallFrame(callFrame);
}

void Debugger::didExecuteProgram(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAtEndOfProgram);
    updateCallFrameAndPauseIfNeeded(callFrame);

    // Treat stepping over the end of a program like stepping out.
    if (!m_currentCallFrame)
        return;
    if (m_currentCallFrame == m_pauseOnCallFrame) {
        m_pauseOnCallFrame = m_currentCallFrame->callerFrameSkippingVMEntrySentinel();
        if (!m_currentCallFrame)
            return;
    }
    m_currentCallFrame = m_currentCallFrame->callerFrameSkippingVMEntrySentinel();
}

void Debugger::didReachBreakpoint(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedForBreakpoint);
    m_pauseOnNextStatement = true;
    setShouldPause(true);
    updateCallFrameAndPauseIfNeeded(callFrame);
}

DebuggerCallFrame* Debugger::currentDebuggerCallFrame() const
{
    ASSERT(m_currentDebuggerCallFrame);
    return m_currentDebuggerCallFrame.get();
}

} // namespace JSC

#endif // ENABLE(JAVASCRIPT_DEBUGGER)
