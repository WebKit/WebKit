/*
 *  Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "DebuggerScope.h"
#include "HeapIterationScope.h"
#include "JSCInlines.h"
#include "MarkedSpaceInlines.h"
#include "VMEntryScope.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace JSC {

class DebuggerPausedScope {
public:
    DebuggerPausedScope(Debugger& debugger)
        : m_debugger(debugger)
    {
        ASSERT(!m_debugger.m_currentDebuggerCallFrame);
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

// This is very similar to SetForScope<bool>, but that cannot be used
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

Debugger::TemporarilyDisableExceptionBreakpoints::TemporarilyDisableExceptionBreakpoints(Debugger& debugger)
    : m_debugger(debugger)
{
}

Debugger::TemporarilyDisableExceptionBreakpoints::~TemporarilyDisableExceptionBreakpoints()
{
    restore();
}

void Debugger::TemporarilyDisableExceptionBreakpoints::replace()
{
    if (m_debugger.m_pauseOnAllExceptionsBreakpoint)
        m_pauseOnAllExceptionsBreakpoint = WTFMove(m_debugger.m_pauseOnAllExceptionsBreakpoint);

    if (m_debugger.m_pauseOnUncaughtExceptionsBreakpoint)
        m_pauseOnUncaughtExceptionsBreakpoint = WTFMove(m_debugger.m_pauseOnUncaughtExceptionsBreakpoint);
}

void Debugger::TemporarilyDisableExceptionBreakpoints::restore()
{
    if (m_pauseOnAllExceptionsBreakpoint)
        m_debugger.m_pauseOnAllExceptionsBreakpoint = WTFMove(m_pauseOnAllExceptionsBreakpoint);

    if (m_pauseOnUncaughtExceptionsBreakpoint)
        m_debugger.m_pauseOnUncaughtExceptionsBreakpoint = WTFMove(m_pauseOnUncaughtExceptionsBreakpoint);
}


Debugger::ProfilingClient::~ProfilingClient()
{
}

Debugger::Debugger(VM& vm)
    : m_vm(vm)
    , m_pauseAtNextOpportunity(false)
    , m_pastFirstExpressionInStatement(false)
    , m_isPaused(false)
    , m_breakpointsActivated(false)
    , m_hasHandlerForExceptionCallback(false)
    , m_suppressAllPauses(false)
    , m_steppingMode(SteppingModeDisabled)
    , m_reasonForPause(NotPaused)
    , m_lastExecutedLine(UINT_MAX)
    , m_lastExecutedSourceID(noSourceID)
    , m_pausingBreakpointID(noBreakpointID)
{
}

Debugger::~Debugger()
{
    HashSet<JSGlobalObject*>::iterator end = m_globalObjects.end();
    for (HashSet<JSGlobalObject*>::iterator it = m_globalObjects.begin(); it != end; ++it)
        (*it)->setDebugger(nullptr);
}

void Debugger::attach(JSGlobalObject* globalObject)
{
    ASSERT(!globalObject->debugger());
    globalObject->setDebugger(this);
    m_globalObjects.add(globalObject);

    m_vm.setShouldBuildPCToCodeOriginMapping();

    // Call `sourceParsed` after iterating because it will execute JavaScript in Web Inspector.
    HashSet<RefPtr<SourceProvider>> sourceProviders;
    {
        HeapIterationScope iterationScope(m_vm.heap);
        m_vm.heap.objectSpace().forEachLiveCell(iterationScope, [&] (HeapCell* heapCell, HeapCell::Kind kind) {
            if (isJSCellKind(kind)) {
                auto* cell = static_cast<JSCell*>(heapCell);
                if (auto* function = jsDynamicCast<JSFunction*>(cell->vm(), cell)) {
                    if (function->scope()->globalObject() == globalObject && function->executable()->isFunctionExecutable() && !function->isHostOrBuiltinFunction())
                        sourceProviders.add(jsCast<FunctionExecutable*>(function->executable())->source().provider());
                }
            }
            return IterationStatus::Continue;
        });
    }
    for (auto& sourceProvider : sourceProviders)
        sourceParsed(globalObject, sourceProvider.get(), -1, nullString());
}

void Debugger::detach(JSGlobalObject* globalObject, ReasonForDetach reason)
{
    // If we're detaching from the currently executing global object, manually tear down our
    // stack, since we won't get further debugger callbacks to do so. Also, resume execution,
    // since there's no point in staying paused once a window closes.
    // We know there is an entry scope, otherwise, m_currentCallFrame would be null.
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);

    if (m_isPaused && m_currentCallFrame && vm.entryScope->globalObject() == globalObject) {
        m_currentCallFrame = nullptr;
        m_pauseOnCallFrame = nullptr;
        continueProgram();
    }

    ASSERT(m_globalObjects.contains(globalObject));
    m_globalObjects.remove(globalObject);

    // If the globalObject is destructing, then its CodeBlocks will also be
    // destructed. There is no need to do the debugger requests clean up, and
    // it is not safe to access those CodeBlocks at this time anyway.
    if (reason != GlobalObjectIsDestructing)
        clearDebuggerRequests(globalObject);

    globalObject->setDebugger(nullptr);

    if (m_globalObjects.isEmpty())
        clearParsedData();
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

    void operator()(CodeBlock* codeBlock) const
    {
        if (m_debugger == codeBlock->globalObject()->debugger()) {
            if (m_mode == SteppingModeEnabled)
                codeBlock->setSteppingMode(CodeBlock::SteppingModeEnabled);
            else
                codeBlock->setSteppingMode(CodeBlock::SteppingModeDisabled);
        }
    }

private:
    Debugger* m_debugger;
    SteppingMode m_mode;
};

void Debugger::setSteppingMode(SteppingMode mode)
{
    if (mode == m_steppingMode)
        return;

    m_vm.heap.completeAllJITPlans();

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

void Debugger::addObserver(Observer& observer)
{
    bool wasEmpty = m_observers.isEmpty();

    m_observers.add(&observer);

    if (wasEmpty)
        attachDebugger();
}

void Debugger::removeObserver(Observer& observer, bool isBeingDestroyed)
{
    m_observers.remove(&observer);

    if (m_observers.isEmpty())
        detachDebugger(isBeingDestroyed);
}

bool Debugger::canDispatchFunctionToObservers() const
{
    return !m_dispatchingFunctionToObservers && !m_observers.isEmpty();
}

void Debugger::dispatchFunctionToObservers(Function<void(Observer&)> func)
{
    if (!canDispatchFunctionToObservers())
        return;

    SetForScope<bool> change(m_dispatchingFunctionToObservers, true);

    for (auto* observer : copyToVector(m_observers))
        func(*observer);
}

void Debugger::setProfilingClient(ProfilingClient* client)
{
    ASSERT(!!m_profilingClient != !!client);
    m_profilingClient = client;
}

void Debugger::sourceParsed(JSGlobalObject* globalObject, SourceProvider* sourceProvider, int errorLine, const String& errorMessage)
{
    // Preemptively check whether we can dispatch so that we don't do any unnecessary allocations.
    if (!canDispatchFunctionToObservers())
        return;

    if (errorLine != -1) {
        auto sourceURL = sourceProvider->sourceURL();
        auto data = sourceProvider->source().toString();
        auto firstLine = sourceProvider->startPosition().m_line.oneBasedInt();
        dispatchFunctionToObservers([&] (Observer& observer) {
            observer.failedToParseSource(sourceURL, data, firstLine, errorLine, errorMessage);
        });
        return;
    }

    JSC::SourceID sourceID = sourceProvider->asID();

    // FIXME: <https://webkit.org/b/162773> Web Inspector: Simplify Debugger::Script to use SourceProvider
    Debugger::Script script;
    script.sourceProvider = sourceProvider;
    script.url = sourceProvider->sourceURL();
    script.source = sourceProvider->source().toString();
    script.startLine = sourceProvider->startPosition().m_line.zeroBasedInt();
    script.startColumn = sourceProvider->startPosition().m_column.zeroBasedInt();
    script.isContentScript = isContentScript(globalObject);
    script.sourceURL = sourceProvider->sourceURLDirective();
    script.sourceMappingURL = sourceProvider->sourceMappingURLDirective();

    int sourceLength = script.source.length();
    int lineCount = 1;
    int lastLineStart = 0;
    for (int i = 0; i < sourceLength; ++i) {
        if (script.source[i] == '\n') {
            lineCount += 1;
            lastLineStart = i + 1;
        }
    }

    script.endLine = script.startLine + lineCount - 1;
    if (lineCount == 1)
        script.endColumn = script.startColumn + sourceLength;
    else
        script.endColumn = sourceLength - lastLineStart;

    dispatchFunctionToObservers([&] (Observer& observer) {
        observer.didParseSource(sourceID, script);
    });
}

Seconds Debugger::willEvaluateScript()
{
    return m_profilingClient->willEvaluateScript();
}

void Debugger::didEvaluateScript(Seconds startTime, ProfilingReason reason)
{
    m_profilingClient->didEvaluateScript(startTime, reason);
}

void Debugger::toggleBreakpoint(CodeBlock* codeBlock, Breakpoint& breakpoint, BreakpointState enabledOrNot)
{
    ASSERT(breakpoint.isResolved());

    ScriptExecutable* executable = codeBlock->ownerExecutable();

    SourceID sourceID = static_cast<SourceID>(executable->sourceID());
    if (breakpoint.sourceID() != sourceID)
        return;

    unsigned startLine = executable->firstLine();
    unsigned startColumn = executable->startColumn();
    unsigned endLine = executable->lastLine();
    unsigned endColumn = executable->endColumn();

    // Inspector breakpoint line and column values are zero-based but the executable
    // and CodeBlock line and column values are one-based.
    unsigned line = breakpoint.lineNumber() + 1;
    Optional<unsigned> column;
    if (breakpoint.columnNumber())
        column = breakpoint.columnNumber() + 1;

    if (line < startLine || line > endLine)
        return;
    if (column) {
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
    for (auto& breakpoint : m_breakpoints)
        toggleBreakpoint(codeBlock, breakpoint, BreakpointEnabled);
}

class Debugger::ToggleBreakpointFunctor {
public:
    ToggleBreakpointFunctor(Debugger* debugger, Breakpoint& breakpoint, BreakpointState enabledOrNot)
        : m_debugger(debugger)
        , m_breakpoint(breakpoint)
        , m_enabledOrNot(enabledOrNot)
    {
    }

    void operator()(CodeBlock* codeBlock) const
    {
        if (m_debugger == codeBlock->globalObject()->debugger())
            m_debugger->toggleBreakpoint(codeBlock, m_breakpoint, m_enabledOrNot);
    }

private:
    Debugger* m_debugger;
    Breakpoint& m_breakpoint;
    BreakpointState m_enabledOrNot;
};

void Debugger::toggleBreakpoint(Breakpoint& breakpoint, Debugger::BreakpointState enabledOrNot)
{
    m_vm.heap.completeAllJITPlans();

    ToggleBreakpointFunctor functor(this, breakpoint, enabledOrNot);
    m_vm.heap.forEachCodeBlock(functor);
}

void Debugger::recompileAllJSFunctions()
{
    m_vm.deleteAllCode(PreventCollectionAndDeleteAllCode);
}

DebuggerParseData& Debugger::debuggerParseData(SourceID sourceID, SourceProvider* provider)
{
    auto iter = m_parseDataMap.find(sourceID);
    if (iter != m_parseDataMap.end())
        return iter->value;

    DebuggerParseData parseData;
    gatherDebuggerParseDataForSource(m_vm, provider, parseData);
    auto result = m_parseDataMap.add(sourceID, parseData);
    return result.iterator->value;
}

bool Debugger::resolveBreakpoint(Breakpoint& breakpoint, SourceProvider* sourceProvider)
{
    RELEASE_ASSERT(!breakpoint.isResolved());
    ASSERT(breakpoint.isLinked());

    // FIXME: <https://webkit.org/b/162771> Web Inspector: Adopt TextPosition in Inspector to avoid oneBasedInt/zeroBasedInt ambiguity
    // Inspector breakpoint line and column values are zero-based but the executable
    // and CodeBlock line values are one-based while column is zero-based.
    int line = breakpoint.lineNumber() + 1;
    int column = breakpoint.columnNumber();

    // Account for a <script>'s start position on the first line only.
    int providerStartLine = sourceProvider->startPosition().m_line.oneBasedInt(); // One based to match the already adjusted line.
    int providerStartColumn = sourceProvider->startPosition().m_column.zeroBasedInt(); // Zero based so column zero is zero.
    if (line == providerStartLine && breakpoint.columnNumber()) {
        ASSERT(providerStartColumn <= column);
        if (providerStartColumn)
            column -= providerStartColumn;
    }

    DebuggerParseData& parseData = debuggerParseData(breakpoint.sourceID(), sourceProvider);
    Optional<JSTextPosition> resolvedPosition = parseData.pausePositions.breakpointLocationForLineColumn(line, column);
    if (!resolvedPosition)
        return false;

    int resolvedLine = resolvedPosition->line;
    int resolvedColumn = resolvedPosition->column();

    // Re-account for a <script>'s start position on the first line only.
    if (resolvedLine == providerStartLine && breakpoint.columnNumber()) {
        if (providerStartColumn)
            resolvedColumn += providerStartColumn;
    }

    return breakpoint.resolve(resolvedLine - 1, resolvedColumn);
}

bool Debugger::setBreakpoint(Breakpoint& breakpoint)
{
    ASSERT(breakpoint.isResolved());

    auto& breakpointsForLine = m_breakpointsForSourceID.ensure(breakpoint.sourceID(), [] {
        return LineToBreakpointsMap();
    }).iterator->value;

    auto& breakpoints = breakpointsForLine.ensure(breakpoint.lineNumber(), [] {
        return BreakpointsVector();
    }).iterator->value;

    for (auto& existingBreakpoint : breakpoints) {
        if (existingBreakpoint->columnNumber() == breakpoint.columnNumber()) {
            ASSERT(existingBreakpoint->id() != breakpoint.id());
            // Found existing breakpoint. Do not create a duplicate at this location.
            return false;
        }
    }

    breakpoints.append(makeRef(breakpoint));

    m_breakpoints.add(makeRef(breakpoint));

    toggleBreakpoint(breakpoint, BreakpointEnabled);

    return true;
}

bool Debugger::removeBreakpoint(Breakpoint& breakpoint)
{
    ASSERT(breakpoint.isResolved());

    auto breakpointsForLineIterator = m_breakpointsForSourceID.find(breakpoint.sourceID());
    if (breakpointsForLineIterator == m_breakpointsForSourceID.end())
        return false;

    auto breakpointsIterator = breakpointsForLineIterator->value.find(breakpoint.lineNumber());
    if (breakpointsIterator == breakpointsForLineIterator->value.end())
        return false;

    toggleBreakpoint(breakpoint, BreakpointDisabled);

    auto& breakpoints = breakpointsIterator->value;

#if ASSERT_ENABLED
    bool found = false;
    for (auto& existingBreakpoint : breakpoints) {
        if (existingBreakpoint->columnNumber() == breakpoint.columnNumber()) {
            ASSERT(existingBreakpoint->id() == breakpoint.id());
            ASSERT(!found);
            found = true;
        }
    }
#endif // ASSERT_ENABLED

    auto removed = m_breakpoints.remove(breakpoint);
    removed |= !breakpoints.removeAllMatching([&] (const Ref<Breakpoint>& existingBreakpoint) {
        return &breakpoint == existingBreakpoint.ptr();
    });

    if (breakpoints.isEmpty()) {
        breakpointsForLineIterator->value.remove(breakpointsIterator);
        if (breakpointsForLineIterator->value.isEmpty())
            m_breakpointsForSourceID.remove(breakpointsForLineIterator);
    }

    return removed;
}

RefPtr<Breakpoint> Debugger::didHitBreakpoint(JSGlobalObject* globalObject, SourceID sourceID, const TextPosition& position)
{
    if (!m_breakpointsActivated)
        return nullptr;

    auto breakpointsForLineIterator = m_breakpointsForSourceID.find(sourceID);
    if (breakpointsForLineIterator == m_breakpointsForSourceID.end())
        return nullptr;

    unsigned line = position.m_line.zeroBasedInt();
    unsigned column = position.m_column.zeroBasedInt();
    
    auto breakpointsIterator = breakpointsForLineIterator->value.find(line);
    if (breakpointsIterator == breakpointsForLineIterator->value.end())
        return nullptr;

    for (auto& breakpoint : breakpointsIterator->value) {
        unsigned breakLine = breakpoint->lineNumber();
        unsigned breakColumn = breakpoint->columnNumber();

        // Since frontend truncates the indent, the first statement in a line must match the breakpoint (line,0).
        ASSERT(this == m_currentCallFrame->codeBlock()->globalObject()->debugger());
        if ((line != m_lastExecutedLine && line == breakLine && !breakColumn) || (line == breakLine && column == breakColumn)) {
            if (breakpoint->shouldPause(*this, globalObject))
                return breakpoint.copyRef();
            break;
        }
    }

    return nullptr;
}

class Debugger::ClearCodeBlockDebuggerRequestsFunctor {
public:
    ClearCodeBlockDebuggerRequestsFunctor(Debugger* debugger)
        : m_debugger(debugger)
    {
    }

    void operator()(CodeBlock* codeBlock) const
    {
        if (codeBlock->hasDebuggerRequests() && m_debugger == codeBlock->globalObject()->debugger())
            codeBlock->clearDebuggerRequests();
    }

private:
    Debugger* m_debugger;
};

void Debugger::clearBreakpoints()
{
    m_vm.heap.completeAllJITPlans();

    m_breakpointsForSourceID.clear();
    m_breakpoints.clear();
    m_specialBreakpoint = nullptr;

    ClearCodeBlockDebuggerRequestsFunctor functor(this);
    m_vm.heap.forEachCodeBlock(functor);
}

bool Debugger::evaluateBreakpointCondition(Breakpoint& breakpoint, JSGlobalObject* globalObject)
{
    const String& condition = breakpoint.condition();
    if (condition.isEmpty())
        return true;

    // We cannot stop in the debugger while executing condition code,
    // so make it looks like the debugger is already paused.
    TemporaryPausedState pausedState(*this);

    NakedPtr<Exception> exception;
    DebuggerCallFrame& debuggerCallFrame = currentDebuggerCallFrame();
    JSObject* scopeExtensionObject = nullptr;
    JSValue result = debuggerCallFrame.evaluateWithScopeExtension(condition, scopeExtensionObject, exception);

    // We can lose the debugger while executing JavaScript.
    if (!m_currentCallFrame)
        return false;

    if (exception) {
        reportException(globalObject, exception);
        return false;
    }

    return result.toBoolean(globalObject);
}

void Debugger::evaluateBreakpointActions(Breakpoint& breakpoint, JSGlobalObject* globalObject)
{
    ASSERT(m_isPaused);
    ASSERT(isAttached(globalObject));

    m_currentProbeBatchId++;

    for (auto& action : breakpoint.actions()) {
        auto& debuggerCallFrame = currentDebuggerCallFrame();

        switch (action.type) {
        case Breakpoint::Action::Type::Log:
            dispatchFunctionToObservers([&] (Observer& observer) {
                observer.breakpointActionLog(debuggerCallFrame.globalObject(), action.data);
            });
            break;

        case Breakpoint::Action::Type::Evaluate: {
            NakedPtr<Exception> exception;
            JSObject* scopeExtensionObject = nullptr;
            debuggerCallFrame.evaluateWithScopeExtension(action.data, scopeExtensionObject, exception);
            if (exception)
                reportException(debuggerCallFrame.globalObject(), exception);
            break;
        }

        case Breakpoint::Action::Type::Sound:
            dispatchFunctionToObservers([&] (Observer& observer) {
                observer.breakpointActionSound(action.id);
            });
            break;

        case Breakpoint::Action::Type::Probe: {
            NakedPtr<Exception> exception;
            JSObject* scopeExtensionObject = nullptr;
            JSValue result = debuggerCallFrame.evaluateWithScopeExtension(action.data, scopeExtensionObject, exception);
            JSC::JSGlobalObject* debuggerGlobalObject = debuggerCallFrame.globalObject();
            if (exception)
                reportException(debuggerGlobalObject, exception);

            dispatchFunctionToObservers([&] (Observer& observer) {
                observer.breakpointActionProbe(debuggerGlobalObject, action.id, m_currentProbeBatchId, m_nextProbeSampleId++, exception ? exception->value() : result);
            });
            break;
        }
        }

        if (!isAttached(globalObject))
            return;
    }
}

class Debugger::ClearDebuggerRequestsFunctor {
public:
    ClearDebuggerRequestsFunctor(JSGlobalObject* globalObject)
        : m_globalObject(globalObject)
    {
    }

    void operator()(CodeBlock* codeBlock) const
    {
        if (codeBlock->hasDebuggerRequests() && m_globalObject == codeBlock->globalObject())
            codeBlock->clearDebuggerRequests();
    }

private:
    JSGlobalObject* m_globalObject;
};

void Debugger::clearDebuggerRequests(JSGlobalObject* globalObject)
{
    m_vm.heap.completeAllJITPlans();

    ClearDebuggerRequestsFunctor functor(globalObject);
    m_vm.heap.forEachCodeBlock(functor);
}

void Debugger::clearParsedData()
{
    m_parseDataMap.clear();
}

void Debugger::setBreakpointsActivated(bool activated)
{
    if (activated == m_breakpointsActivated)
        return;

    m_breakpointsActivated = activated;
    recompileAllJSFunctions();
}

void Debugger::schedulePauseAtNextOpportunity()
{
    m_pauseAtNextOpportunity = true;

    setSteppingMode(SteppingModeEnabled);
}

void Debugger::cancelPauseAtNextOpportunity()
{
    m_pauseAtNextOpportunity = false;
}

bool Debugger::schedulePauseForSpecialBreakpoint(Breakpoint& breakpoint)
{
    if (m_specialBreakpoint)
        return false;

    m_specialBreakpoint = makeRef(breakpoint);
    setSteppingMode(SteppingModeEnabled);
    return true;
}

bool Debugger::cancelPauseForSpecialBreakpoint(Breakpoint& breakpoint)
{
    if (&breakpoint != m_specialBreakpoint)
        return false;

    m_specialBreakpoint = nullptr;
    return true;
}

void Debugger::breakProgram(RefPtr<Breakpoint>&& specialBreakpoint)
{
    if (m_isPaused)
        return;

    if (!m_vm.topCallFrame)
        return;

    if (specialBreakpoint) {
        ASSERT(!m_specialBreakpoint);
        m_specialBreakpoint = WTFMove(specialBreakpoint);
    } else
        m_pauseAtNextOpportunity = true;

    setSteppingMode(SteppingModeEnabled);
    m_currentCallFrame = m_vm.topCallFrame;
    pauseIfNeeded(m_currentCallFrame->lexicalGlobalObject(m_vm));
}

void Debugger::continueProgram()
{
    clearNextPauseState();

    if (!m_isPaused)
        return;

    m_doneProcessingDebuggerEvents = true;
}

void Debugger::stepNextExpression()
{
    if (!m_isPaused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame;
    m_pauseOnStepNext = true;
    setSteppingMode(SteppingModeEnabled);
    m_doneProcessingDebuggerEvents = true;
}

void Debugger::stepIntoStatement()
{
    if (!m_isPaused)
        return;

    m_pauseAtNextOpportunity = true;
    setSteppingMode(SteppingModeEnabled);
    m_doneProcessingDebuggerEvents = true;
}

void Debugger::stepOverStatement()
{
    if (!m_isPaused)
        return;

    m_pauseOnCallFrame = m_currentCallFrame;
    setSteppingMode(SteppingModeEnabled);
    m_doneProcessingDebuggerEvents = true;
}

void Debugger::stepOutOfFunction()
{
    if (!m_isPaused)
        return;

    EntryFrame* topEntryFrame = m_vm.topEntryFrame;
    m_pauseOnCallFrame = m_currentCallFrame ? m_currentCallFrame->callerFrame(topEntryFrame) : nullptr;
    m_pauseOnStepOut = true;
    setSteppingMode(SteppingModeEnabled);
    m_doneProcessingDebuggerEvents = true;
}

static inline JSGlobalObject* lexicalGlobalObjectForCallFrame(VM& vm, CallFrame* callFrame)
{
    if (!callFrame)
        return nullptr;
    return callFrame->lexicalGlobalObject(vm);
}

void Debugger::updateCallFrame(JSGlobalObject* globalObject, CallFrame* callFrame, CallFrameUpdateAction action)
{
    if (!callFrame) {
        m_currentCallFrame = nullptr;
        return;
    }
    updateCallFrameInternal(callFrame);

    if (action == AttemptPause)
        pauseIfNeeded(globalObject);

    if (!isStepping())
        m_currentCallFrame = nullptr;
}

void Debugger::updateCallFrameInternal(CallFrame* callFrame)
{
    m_currentCallFrame = callFrame;
    SourceID sourceID = DebuggerCallFrame::sourceIDForCallFrame(callFrame);
    if (m_lastExecutedSourceID != sourceID) {
        m_lastExecutedLine = UINT_MAX;
        m_lastExecutedSourceID = sourceID;
    }
}

void Debugger::pauseIfNeeded(JSGlobalObject* globalObject)
{
    VM& vm = m_vm;
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_isPaused)
        return;

    if (m_suppressAllPauses)
        return;

    intptr_t sourceID = DebuggerCallFrame::sourceIDForCallFrame(m_currentCallFrame);

    auto blackboxTypeIterator = m_blackboxedScripts.find(sourceID);
    if (blackboxTypeIterator != m_blackboxedScripts.end() && blackboxTypeIterator->value == BlackboxType::Ignored)
        return;

    DebuggerPausedScope debuggerPausedScope(*this);

    bool pauseNow = m_pauseAtNextOpportunity;
    pauseNow |= (m_pauseOnCallFrame == m_currentCallFrame);

    bool didPauseForStep = pauseNow;

    TextPosition position = DebuggerCallFrame::positionForCallFrame(vm, m_currentCallFrame);

    auto breakpoint = didHitBreakpoint(globalObject, sourceID, position);
    if (breakpoint)
        pauseNow = true;

    // Special breakpoints are only given one opportunity to pause.
    auto specialBreakpoint = WTFMove(m_specialBreakpoint);
    if (specialBreakpoint && specialBreakpoint->shouldPause(*this, globalObject))
        pauseNow = true;

    m_lastExecutedLine = position.m_line.zeroBasedInt();
    if (!pauseNow)
        return;

    bool afterBlackboxedScript = m_afterBlackboxedScript;
    clearNextPauseState();

    // Make sure we are not going to pause again on breakpoint actions by
    // reseting the pause state before executing any breakpoint actions.
    TemporaryPausedState pausedState(*this);

    if (breakpoint || specialBreakpoint) {
        // Note that the actions can potentially stop the debugger, so we need to check that
        // we still have a current call frame when we get back.

        bool autoContinue = false;

        if (breakpoint) {
            evaluateBreakpointActions(*breakpoint, globalObject);

            if (!m_currentCallFrame)
                return;

            if (breakpoint->isAutoContinue())
                autoContinue = true;
        }

        if (specialBreakpoint) {
            evaluateBreakpointActions(*specialBreakpoint, globalObject);

            if (!m_currentCallFrame)
                return;

            if (specialBreakpoint->isAutoContinue())
                autoContinue = true;
        }

        if (autoContinue) {
            if (!didPauseForStep)
                return;

            breakpoint = nullptr;
            specialBreakpoint = nullptr;
        } else if (breakpoint)
            m_pausingBreakpointID = breakpoint->id();
    }

    if (blackboxTypeIterator != m_blackboxedScripts.end() && blackboxTypeIterator->value == BlackboxType::Deferred) {
        m_afterBlackboxedScript = true;
        schedulePauseAtNextOpportunity();
        return;
    }

    {
        auto reason = m_reasonForPause;
        if (afterBlackboxedScript)
            reason = PausedAfterBlackboxedScript;
        else if (breakpoint)
            reason = PausedForBreakpoint;
        PauseReasonDeclaration rauseReasonDeclaration(*this, reason);

        handlePause(globalObject, m_reasonForPause);
        scope.releaseAssertNoException();
    }

    m_pausingBreakpointID = noBreakpointID;

    if (!m_pauseAtNextOpportunity && !m_pauseOnCallFrame && !m_specialBreakpoint) {
        setSteppingMode(SteppingModeDisabled);
        m_currentCallFrame = nullptr;
    }
}

void Debugger::handlePause(JSGlobalObject* globalObject, ReasonForPause)
{
    dispatchFunctionToObservers([&] (Observer& observer) {
        ASSERT(isPaused());
        observer.didPause(globalObject, currentDebuggerCallFrame(), exceptionOrCaughtValue(globalObject));
    });

    didPause(globalObject);

    m_doneProcessingDebuggerEvents = false;
    runEventLoopWhilePaused();

    didContinue(globalObject);

    dispatchFunctionToObservers([&] (Observer& observer) {
        observer.didContinue();
    });
}

JSC::JSValue Debugger::exceptionOrCaughtValue(JSC::JSGlobalObject* globalObject)
{
    if (reasonForPause() == PausedForException)
        return currentException();

    for (RefPtr<DebuggerCallFrame> frame = &currentDebuggerCallFrame(); frame; frame = frame->callerFrame()) {
        DebuggerScope& scope = *frame->scope();
        if (scope.isCatchScope())
            return scope.caughtValue(globalObject);
    }

    return { };
}

void Debugger::exception(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue exception, bool hasCatchHandler)
{
    if (m_isPaused)
        return;

    if (JSObject* object = jsDynamicCast<JSObject*>(m_vm, exception)) {
        if (object->isErrorInstance()) {
            ErrorInstance* error = static_cast<ErrorInstance*>(object);
            // FIXME: <https://webkit.org/b/173625> Web Inspector: Should be able to pause and debug a StackOverflow Exception
            // FIXME: <https://webkit.org/b/173627> Web Inspector: Should be able to pause and debug an OutOfMemory Exception
            if (error->isStackOverflowError() || error->isOutOfMemoryError())
                return;
        }
    }

    PauseReasonDeclaration reason(*this, PausedForException);
    if (m_pauseOnAllExceptionsBreakpoint || (m_pauseOnUncaughtExceptionsBreakpoint && !hasCatchHandler)) {
        m_specialBreakpoint = m_pauseOnAllExceptionsBreakpoint ? m_pauseOnAllExceptionsBreakpoint.copyRef() : m_pauseOnUncaughtExceptionsBreakpoint.copyRef();
        setSteppingMode(SteppingModeEnabled);
    }

    m_hasHandlerForExceptionCallback = true;
    m_currentException = exception;
    updateCallFrame(globalObject, callFrame, AttemptPause);
    m_currentException = JSValue();
    m_hasHandlerForExceptionCallback = false;
}

void Debugger::atStatement(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    m_pastFirstExpressionInStatement = false;

    PauseReasonDeclaration reason(*this, PausedAtStatement);
    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, AttemptPause);
}

void Debugger::atExpression(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    // If this is the first call in a statement, then we would have paused at the statement.
    if (!m_pastFirstExpressionInStatement) {
        m_pastFirstExpressionInStatement = true;
        return;
    }

    // Only pause at the next expression for step-in, step-next, step-out, or special breakpoints.
    bool shouldAttemptPause = m_pauseAtNextOpportunity || m_pauseOnStepNext || m_pauseOnStepOut || m_specialBreakpoint;

    PauseReasonDeclaration reason(*this, PausedAtExpression);
    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, shouldAttemptPause ? AttemptPause : NoPause);
}

void Debugger::callEvent(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, NoPause);
}

void Debugger::returnEvent(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    {
        PauseReasonDeclaration reason(*this, PausedBeforeReturn);
        updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, AttemptPause);
    }

    // Detach may have been called during pauseIfNeeded.
    if (!m_currentCallFrame)
        return;

    EntryFrame* topEntryFrame = m_vm.topEntryFrame;
    CallFrame* callerFrame = m_currentCallFrame->callerFrame(topEntryFrame);

    // Returning from a call, there was at least one expression on the statement we are returning to.
    m_pastFirstExpressionInStatement = true;

    // Treat stepping over a return statement like a step-out.
    if (m_currentCallFrame == m_pauseOnCallFrame) {
        m_pauseOnCallFrame = callerFrame;
        m_pauseOnStepOut = true;
    }

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callerFrame), callerFrame, NoPause);
}

void Debugger::unwindEvent(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, NoPause);

    if (!m_currentCallFrame)
        return;

    EntryFrame* topEntryFrame = m_vm.topEntryFrame;
    CallFrame* callerFrame = m_currentCallFrame->callerFrame(topEntryFrame);

    // Treat stepping over an exception location like a step-out.
    if (m_currentCallFrame == m_pauseOnCallFrame)
        m_pauseOnCallFrame = callerFrame;

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callerFrame), callerFrame, NoPause);
}

void Debugger::willExecuteProgram(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, NoPause);
}

void Debugger::didExecuteProgram(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    PauseReasonDeclaration reason(*this, PausedAtEndOfProgram);
    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, AttemptPause);

    // Detach may have been called during pauseIfNeeded.
    if (!m_currentCallFrame)
        return;

    EntryFrame* topEntryFrame = m_vm.topEntryFrame;
    CallFrame* callerFrame = m_currentCallFrame->callerFrame(topEntryFrame);

    // Returning from a program, could be eval(), there was at least one expression on the statement we are returning to.
    m_pastFirstExpressionInStatement = true;

    // Treat stepping over the end of a program like a step-out.
    if (m_currentCallFrame == m_pauseOnCallFrame) {
        m_pauseOnCallFrame = callerFrame;
        m_pauseAtNextOpportunity = true;
    }

    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callerFrame), callerFrame, NoPause);

    // Do not continue stepping into an unknown future program.
    if (!m_currentCallFrame)
        clearNextPauseState();
}

void Debugger::clearNextPauseState()
{
    m_pauseOnCallFrame = nullptr;
    m_pauseAtNextOpportunity = false;
    m_pauseOnStepNext = false;
    m_pauseOnStepOut = false;
    m_afterBlackboxedScript = false;
    m_specialBreakpoint = nullptr;
}

void Debugger::didReachDebuggerStatement(CallFrame* callFrame)
{
    if (m_isPaused)
        return;

    if (!m_pauseOnDebuggerStatementsBreakpoint)
        return;

    PauseReasonDeclaration reason(*this, PausedForDebuggerStatement);
    m_specialBreakpoint = m_pauseOnDebuggerStatementsBreakpoint.copyRef();
    setSteppingMode(SteppingModeEnabled);
    updateCallFrame(lexicalGlobalObjectForCallFrame(m_vm, callFrame), callFrame, AttemptPause);
}

void Debugger::willRunMicrotask()
{
    dispatchFunctionToObservers([&] (Observer& observer) {
        observer.willRunMicrotask();
    });
}

void Debugger::didRunMicrotask()
{
    dispatchFunctionToObservers([&] (Observer& observer) {
        observer.didRunMicrotask();
    });
}

DebuggerCallFrame& Debugger::currentDebuggerCallFrame()
{
    if (!m_currentDebuggerCallFrame)
        m_currentDebuggerCallFrame = DebuggerCallFrame::create(m_vm, m_currentCallFrame);
    return *m_currentDebuggerCallFrame;
}

void Debugger::setBlackboxType(SourceID sourceID, Optional<BlackboxType> type)
{
    if (type)
        m_blackboxedScripts.set(sourceID, type.value());
    else
        m_blackboxedScripts.remove(sourceID);
}

void Debugger::clearBlackbox()
{
    m_blackboxedScripts.clear();
}

} // namespace JSC
