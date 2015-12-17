/*
 *  Copyright (C) 2008, 2013, 2014 Apple Inc. All rights reserved.
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
#include "Debugger.h"

#include "CodeBlock.h"
#include "DebuggerCallFrame.h"
#include "Error.h"
#include "HeapIterationScope.h"
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "Protect.h"
#include "VMEntryScope.h"

namespace {

using namespace JSC;

struct GatherSourceProviders : public MarkedBlock::VoidFunctor {
    HashSet<SourceProvider*> sourceProviders;
    JSGlobalObject* m_globalObject;

    GatherSourceProviders(JSGlobalObject* globalObject)
        : m_globalObject(globalObject) { }

    IterationStatus operator()(JSCell* cell)
    {
        JSFunction* function = jsDynamicCast<JSFunction*>(cell);
        if (!function)
            return IterationStatus::Continue;

        if (function->scope()->globalObject() != m_globalObject)
            return IterationStatus::Continue;

        if (!function->executable()->isFunctionExecutable())
            return IterationStatus::Continue;

        if (function->isHostOrBuiltinFunction())
            return IterationStatus::Continue;

        sourceProviders.add(
            jsCast<FunctionExecutable*>(function->executable())->source().provider());
        return IterationStatus::Continue;
    }
};

} // namespace

namespace JSC {

class DebuggerPausedScope {
public:
    DebuggerPausedScope(Debugger& debugger)
        : m_debugger(debugger)
    {
        ASSERT(!m_debugger.m_currentDebuggerCallFrame);
        if (m_debugger.m_currentCallFrame)
            m_debugger.m_currentDebuggerCallFrame = DebuggerCallFrame::create(debugger.m_currentCallFrame);
    }

    ~DebuggerPausedScope()
    {
        if (m_debugger.m_currentDebuggerCallFrame) {
            m_debugger.m_currentDebuggerCallFrame->invalidate();
            m_debugger.m_currentDebuggerCallFrame = nullptr;
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

Debugger::Debugger(VM& vm)
    : m_vm(vm)
    , m_pauseOnExceptionsState(DontPauseOnExceptions)
    , m_pauseOnNextStatement(false)
    , m_isPaused(false)
    , m_breakpointsActivated(true)
    , m_hasHandlerForExceptionCallback(false)
    , m_suppressAllPauses(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_reasonForPause(NotPaused)
    , m_pauseOnCallFrame(0)
    , m_currentCallFrame(0)
    , m_lastExecutedLine(UINT_MAX)
    , m_lastExecutedSourceID(noSourceID)
    , m_topBreakpointID(noBreakpointID)
    , m_pausingBreakpointID(noBreakpointID)
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

    // Call sourceParsed because it will execute JavaScript in the inspector.
    GatherSourceProviders gatherSourceProviders(globalObject);
    {
        HeapIterationScope iterationScope(m_vm.heap);
        m_vm.heap.objectSpace().forEachLiveCell(iterationScope, gatherSourceProviders);
    }
    for (auto* sourceProvider : gatherSourceProviders.sourceProviders)
        sourceParsed(globalObject->globalExec(), sourceProvider, -1, String());
}

void Debugger::detach(JSGlobalObject* globalObject, ReasonForDetach reason)
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

    // If the globalObject is destructing, then its CodeBlocks will also be
    // destructed. There is no need to do the debugger requests clean up, and
    // it is not safe to access those CodeBlocks at this time anyway.
    if (reason != GlobalObjectIsDestructing)
        clearDebuggerRequests(globalObject);

    globalObject->setDebugger(0);
}

bool Debugger::isAttached(JSGlobalObject* globalObject)
{
    return globalObject->debugger() == this;
}

class Debugger::SetSteppingModeFunctor {
public:
    SetSteppingModeFunctor(Debugger* debugger, SteppingMode mode)
        : m_debugger(debugger)
        , m_mode(mode)
    {
    }

    bool operator()(CodeBlock* codeBlock)
    {
        if (m_debugger == codeBlock->globalObject()->debugger()) {
            if (m_mode == SteppingModeEnabled)
                codeBlock->setSteppingMode(CodeBlock::SteppingModeEnabled);
            else
                codeBlock->setSteppingMode(CodeBlock::SteppingModeDisabled);
        }
        return false;
    }

private:
    Debugger* m_debugger;
    SteppingMode m_mode;
};

void Debugger::setSteppingMode(SteppingMode mode)
{
    if (mode == m_steppingMode)
        return;

    m_vm.heap.completeAllDFGPlans();

    m_steppingMode = mode;
    SetSteppingModeFunctor functor(this, mode);
    m_vm.heap.forEachCodeBlock(functor);
}

void Debugger::registerCodeBlock(CodeBlock* codeBlock)
{
    applyBreakpoints(codeBlock);
    if (isStepping())
        codeBlock->setSteppingMode(CodeBlock::SteppingModeEnabled);
}

void Debugger::setProfilingClient(ProfilingClient* client)
{
    ASSERT(!!m_profilingClient != !!client);
    m_profilingClient = client;

    recompileAllJSFunctions();
}

double Debugger::willEvaluateScript(JSGlobalObject& globalObject)
{
    return m_profilingClient->willEvaluateScript(globalObject);
}

void Debugger::didEvaluateScript(JSGlobalObject& globalObject, double startTime, ProfilingReason reason)
{
    m_profilingClient->didEvaluateScript(globalObject, startTime, reason);
}

void Debugger::toggleBreakpoint(CodeBlock* codeBlock, Breakpoint& breakpoint, BreakpointState enabledOrNot)
{
    ScriptExecutable* executable = codeBlock->ownerScriptExecutable();

    SourceID sourceID = static_cast<SourceID>(executable->sourceID());
    if (breakpoint.sourceID != sourceID)
        return;

    unsigned line = breakpoint.line;
    unsigned column = breakpoint.column;

    unsigned startLine = executable->firstLine();
    unsigned startColumn = executable->startColumn();
    unsigned endLine = executable->lastLine();
    unsigned endColumn = executable->endColumn();

    // Inspector breakpoint line and column values are zero-based but the executable
    // and CodeBlock line and column values are one-based.
    line += 1;
    column = column ? column + 1 : Breakpoint::unspecifiedColumn;

    if (line < startLine || line > endLine)
        return;
    if (column != Breakpoint::unspecifiedColumn) {
        if (line == startLine && column < startColumn)
            return;
        if (line == endLine && column > endColumn)
            return;
    }
    if (!codeBlock->hasOpDebugForLineAndColumn(line, column))
        return;

    if (enabledOrNot == BreakpointEnabled)
        codeBlock->addBreakpoint(1);
    else
        codeBlock->removeBreakpoint(1);
}

void Debugger::applyBreakpoints(CodeBlock* codeBlock)
{
    BreakpointIDToBreakpointMap& breakpoints = m_breakpointIDToBreakpoint;
    for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it) {
        Breakpoint& breakpoint = *it->value;
        toggleBreakpoint(codeBlock, breakpoint, BreakpointEnabled);
    }
}

class Debugger::ToggleBreakpointFunctor {
public:
    ToggleBreakpointFunctor(Debugger* debugger, Breakpoint& breakpoint, BreakpointState enabledOrNot)
        : m_debugger(debugger)
        , m_breakpoint(breakpoint)
        , m_enabledOrNot(enabledOrNot)
    {
    }

    bool operator()(CodeBlock* codeBlock)
    {
        if (m_debugger == codeBlock->globalObject()->debugger())
            m_debugger->toggleBreakpoint(codeBlock, m_breakpoint, m_enabledOrNot);
        return false;
    }

private:
    Debugger* m_debugger;
    Breakpoint& m_breakpoint;
    BreakpointState m_enabledOrNot;
};

void Debugger::toggleBreakpoint(Breakpoint& breakpoint, Debugger::BreakpointState enabledOrNot)
{
    m_vm.heap.completeAllDFGPlans();

    ToggleBreakpointFunctor functor(this, breakpoint, enabledOrNot);
    m_vm.heap.forEachCodeBlock(functor);
}

void Debugger::recompileAllJSFunctions()
{
    m_vm.deleteAllCode();
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
        breaksIt = it->value.set(line, adoptRef(new BreakpointsList)).iterator;

    BreakpointsList& breakpoints = *breaksIt->value;
    for (Breakpoint* current = breakpoints.head(); current; current = current->next()) {
        if (current->column == column) {
            // The breakpoint already exists. We're not allowed to create a new
            // breakpoint at this location. Rather than returning the breakpointID
            // of the pre-existing breakpoint, we need to return noBreakpointID
            // to indicate that we're not creating a new one.
            return noBreakpointID;
        }
    }

    BreakpointID id = ++m_topBreakpointID;
    RELEASE_ASSERT(id != noBreakpointID);

    breakpoint.id = id;
    actualLine = line;
    actualColumn = column;

    Breakpoint* newBreakpoint = new Breakpoint(breakpoint);
    breakpoints.append(newBreakpoint);
    m_breakpointIDToBreakpoint.set(id, newBreakpoint);

    toggleBreakpoint(breakpoint, BreakpointEnabled);

    return id;
}

void Debugger::removeBreakpoint(BreakpointID id)
{
    ASSERT(id != noBreakpointID);

    BreakpointIDToBreakpointMap::iterator idIt = m_breakpointIDToBreakpoint.find(id);
    ASSERT(idIt != m_breakpointIDToBreakpoint.end());
    Breakpoint* breakpoint = idIt->value;

    SourceID sourceID = breakpoint->sourceID;
    ASSERT(sourceID);
    SourceIDToBreakpointsMap::iterator it = m_sourceIDToBreakpoints.find(sourceID);
    ASSERT(it != m_sourceIDToBreakpoints.end());
    LineToBreakpointsMap::iterator breaksIt = it->value.find(breakpoint->line);
    ASSERT(breaksIt != it->value.end());

    toggleBreakpoint(*breakpoint, BreakpointDisabled);

    BreakpointsList& breakpoints = *breaksIt->value;
#if !ASSERT_DISABLED
    bool found = false;
    for (Breakpoint* current = breakpoints.head(); current && !found; current = current->next()) {
        if (current->id == breakpoint->id)
            found = true;
    }
    ASSERT(found);
#endif

    m_breakpointIDToBreakpoint.remove(idIt);
    breakpoints.remove(breakpoint);
    delete breakpoint;

    if (breakpoints.isEmpty()) {
        it->value.remove(breaksIt);
        if (it->value.isEmpty())
            m_sourceIDToBreakpoints.remove(it);
    }
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
    const BreakpointsList& breakpoints = *breaksIt->value;
    Breakpoint* breakpoint;
    for (breakpoint = breakpoints.head(); breakpoint; breakpoint = breakpoint->next()) {
        unsigned breakLine = breakpoint->line;
        unsigned breakColumn = breakpoint->column;
        // Since frontend truncates the indent, the first statement in a line must match the breakpoint (line,0).
        ASSERT(this == m_currentCallFrame->codeBlock()->globalObject()->debugger());
        if ((line != m_lastExecutedLine && line == breakLine && !breakColumn)
            || (line == breakLine && column == breakColumn)) {
            hit = true;
            break;
        }
    }
    if (!hit)
        return false;

    if (hitBreakpoint)
        *hitBreakpoint = *breakpoint;

    breakpoint->hitCount++;
    if (breakpoint->ignoreCount >= breakpoint->hitCount)
        return false;

    if (breakpoint->condition.isEmpty())
        return true;

    // We cannot stop in the debugger while executing condition code,
    // so make it looks like the debugger is already paused.
    TemporaryPausedState pausedState(*this);

    NakedPtr<Exception> exception;
    DebuggerCallFrame* debuggerCallFrame = currentDebuggerCallFrame();
    JSValue result = debuggerCallFrame->evaluate(breakpoint->condition, exception);

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

class Debugger::ClearCodeBlockDebuggerRequestsFunctor {
public:
    ClearCodeBlockDebuggerRequestsFunctor(Debugger* debugger)
        : m_debugger(debugger)
    {
    }

    bool operator()(CodeBlock* codeBlock)
    {
        if (codeBlock->hasDebuggerRequests() && m_debugger == codeBlock->globalObject()->debugger())
            codeBlock->clearDebuggerRequests();
        return false;
    }

private:
    Debugger* m_debugger;
};

void Debugger::clearBreakpoints()
{
    m_vm.heap.completeAllDFGPlans();

    m_topBreakpointID = noBreakpointID;
    m_breakpointIDToBreakpoint.clear();
    m_sourceIDToBreakpoints.clear();

    ClearCodeBlockDebuggerRequestsFunctor functor(this);
    m_vm.heap.forEachCodeBlock(functor);
}

class Debugger::ClearDebuggerRequestsFunctor {
public:
    ClearDebuggerRequestsFunctor(JSGlobalObject* globalObject)
        : m_globalObject(globalObject)
    {
    }

    bool operator()(CodeBlock* codeBlock)
    {
        if (codeBlock->hasDebuggerRequests() && m_globalObject == codeBlock->globalObject())
            codeBlock->clearDebuggerRequests();
        return false;
    }

private:
    JSGlobalObject* m_globalObject;
};

void Debugger::clearDebuggerRequests(JSGlobalObject* globalObject)
{
    m_vm.heap.completeAllDFGPlans();

    ClearDebuggerRequestsFunctor functor(globalObject);
    m_vm.heap.forEachCodeBlock(functor);
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
        setSteppingMode(SteppingModeEnabled);
}

void Debugger::breakProgram()
{
    if (m_isPaused)
        return;

    m_pauseOnNextStatement = true;
    setSteppingMode(SteppingModeEnabled);
    m_currentCallFrame = m_vm.topCallFrame;
    ASSERT(m_currentCallFrame);
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
    setSteppingMode(SteppingModeEnabled);
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

    VMEntryFrame* topVMEntryFrame = m_vm.topVMEntryFrame;
    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->callerFrame(topVMEntryFrame) : 0;
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
    if (!isStepping())
        m_currentCallFrame = 0;
}

void Debugger::pauseIfNeeded(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    if (m_suppressAllPauses)
        return;

    JSGlobalObject* vmEntryGlobalObject = callFrame->vmEntryGlobalObject();
    if (!needPauseHandling(vmEntryGlobalObject))
        return;

    Breakpoint breakpoint;
    bool didHitBreakpoint = false;
    bool pauseNow = m_pauseOnNextStatement;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);

    DebuggerPausedScope debuggerPausedScope(*this);

    intptr_t sourceID = DebuggerCallFrame::sourceIDForCallFrame(m_currentCallFrame);
    TextPosition position = DebuggerCallFrame::positionForCallFrame(m_currentCallFrame);
    pauseNow |= didHitBreakpoint = hasBreakpoint(sourceID, position, &breakpoint);
    m_lastExecutedLine = position.m_line.zeroBasedInt();
    if (!pauseNow)
        return;

    // Make sure we are not going to pause again on breakpoint actions by
    // reseting the pause state before executing any breakpoint actions.
    TemporaryPausedState pausedState(*this);
    m_pauseOnCallFrame = 0;
    m_pauseOnNextStatement = false;

    if (didHitBreakpoint) {
        handleBreakpointHit(vmEntryGlobalObject, breakpoint);
        // Note that the actions can potentially stop the debugger, so we need to check that
        // we still have a current call frame when we get back.
        if (breakpoint.autoContinue || !m_currentCallFrame)
            return;
        m_pausingBreakpointID = breakpoint.id;
    }

    {
        PauseReasonDeclaration reason(*this, didHitBreakpoint ? PausedForBreakpoint : m_reasonForPause);
        handlePause(vmEntryGlobalObject, m_reasonForPause);
        RELEASE_ASSERT(!callFrame->hadException());
    }

    m_pausingBreakpointID = noBreakpointID;

    if (!m_pauseOnNextStatement && !m_pauseOnCallFrame) {
        setSteppingMode(SteppingModeDisabled);
        m_currentCallFrame = nullptr;
    }
}

void Debugger::exception(CallFrame* callFrame, JSValue exception, bool hasCatchHandler)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedForException);
    if (m_pauseOnExceptionsState == PauseOnAllExceptions || (m_pauseOnExceptionsState == PauseOnUncaughtExceptions && !hasCatchHandler)) {
        m_pauseOnNextStatement = true;
        setSteppingMode(SteppingModeEnabled);
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
    if (m_currentCallFrame == m_pauseOnCallFrame) {
        VMEntryFrame* topVMEntryFrame = m_vm.topVMEntryFrame;
        m_pauseOnCallFrame = m_currentCallFrame->callerFrame(topVMEntryFrame);
    }

    VMEntryFrame* topVMEntryFrame = m_vm.topVMEntryFrame;
    m_currentCallFrame = m_currentCallFrame->callerFrame(topVMEntryFrame);
}

void Debugger::willExecuteProgram(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAtStartOfProgram);
    updateCallFrameAndPauseIfNeeded(callFrame);
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
        VMEntryFrame* topVMEntryFrame = m_vm.topVMEntryFrame;
        m_pauseOnCallFrame = m_currentCallFrame->callerFrame(topVMEntryFrame);
        if (!m_currentCallFrame)
            return;
    }
    VMEntryFrame* topVMEntryFrame = m_vm.topVMEntryFrame;
    m_currentCallFrame = m_currentCallFrame->callerFrame(topVMEntryFrame);
}

void Debugger::didReachBreakpoint(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedForDebuggerStatement);
    m_pauseOnNextStatement = true;
    setSteppingMode(SteppingModeEnabled);
    updateCallFrameAndPauseIfNeeded(callFrame);
}

DebuggerCallFrame* Debugger::currentDebuggerCallFrame() const
{
    ASSERT(m_currentDebuggerCallFrame);
    return m_currentDebuggerCallFrame.get();
}

} // namespace JSC
