/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "HandleTypes.h"
#include "IterationStatus.h"
#include "MarkStack.h"
#include "VisitRaceKey.h"
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/SharedTask.h>
#include <wtf/text/CString.h>

namespace JSC {

class ConservativeRoots;
class GCThreadSharedData;
class Heap;
class HeapCell;
class HeapSnapshotBuilder;
class MarkedBlock;
class MarkingConstraint;
class MarkingConstraintSolver;
template<typename T> class Weak;
template<typename T, typename Traits> class WriteBarrierBase;

typedef uint32_t HeapVersion;

class SlotVisitor {
    WTF_MAKE_NONCOPYABLE(SlotVisitor);
    WTF_MAKE_FAST_ALLOCATED;

    friend class SetCurrentCellScope;
    friend class Heap;

public:
    enum RootMarkReason {
        None,
        ConservativeScan,
        StrongReferences,
        ProtectedValues,
        MarkListSet,
        VMExceptions,
        StrongHandles,
        Debugger,
        JITStubRoutines,
        WeakSets,
        Output,
        DFGWorkLists,
        CodeBlocks,
        DOMGCOutput,
    };

    SlotVisitor(Heap&, CString codeName);
    ~SlotVisitor();

    MarkStackArray& collectorMarkStack() { return m_collectorStack; }
    MarkStackArray& mutatorMarkStack() { return m_mutatorStack; }
    const MarkStackArray& collectorMarkStack() const { return m_collectorStack; }
    const MarkStackArray& mutatorMarkStack() const { return m_mutatorStack; }
    
    VM& vm();
    const VM& vm() const;
    Heap* heap() const;

    void append(const ConservativeRoots&);
    
    template<typename T, typename Traits> void append(const WriteBarrierBase<T, Traits>&);
    template<typename T, typename Traits> void appendHidden(const WriteBarrierBase<T, Traits>&);
    template<typename Iterator> void append(Iterator begin , Iterator end);
    void appendValues(const WriteBarrierBase<Unknown, DumbValueTraits<Unknown>>*, size_t count);
    void appendValuesHidden(const WriteBarrierBase<Unknown, DumbValueTraits<Unknown>>*, size_t count);
    
    // These don't require you to prove that you have a WriteBarrier<>. That makes sense
    // for:
    //
    // - roots.
    // - sophisticated data structures that barrier through other means (like DFG::Plan and
    //   friends).
    //
    // If you are not a root and you don't know what kind of barrier you have, then you
    // shouldn't call these methods.
    void appendUnbarriered(JSValue);
    void appendUnbarriered(JSValue*, size_t);
    void appendUnbarriered(JSCell*);
    
    template<typename T>
    void append(const Weak<T>& weak);
    
    void appendHiddenUnbarriered(JSValue);
    void appendHiddenUnbarriered(JSCell*);

    bool addOpaqueRoot(void*); // Returns true if the root was new.
    
    bool containsOpaqueRoot(void*) const;

    bool isEmpty() { return m_collectorStack.isEmpty() && m_mutatorStack.isEmpty(); }

    void didStartMarking();
    void reset();
    void clearMarkStacks();

    size_t bytesVisited() const { return m_bytesVisited; }
    size_t visitCount() const { return m_visitCount; }
    
    void addToVisitCount(size_t value) { m_visitCount += value; }

    void donate();
    void drain(MonotonicTime timeout = MonotonicTime::infinity());
    void donateAndDrain(MonotonicTime timeout = MonotonicTime::infinity());
    
    enum SharedDrainMode { SlaveDrain, MasterDrain };
    enum class SharedDrainResult { Done, TimedOut };
    SharedDrainResult drainFromShared(SharedDrainMode, MonotonicTime timeout = MonotonicTime::infinity());

    SharedDrainResult drainInParallel(MonotonicTime timeout = MonotonicTime::infinity());
    SharedDrainResult drainInParallelPassively(MonotonicTime timeout = MonotonicTime::infinity());
    
    SharedDrainResult waitForTermination(MonotonicTime timeout = MonotonicTime::infinity());

    // Attempts to perform an increment of draining that involves only walking `bytes` worth of data. This
    // is likely to accidentally walk more or less than that. It will usually mark more than bytes. It may
    // mark less than bytes if we're reaching termination or if the global worklist is empty (which may in
    // rare cases happen temporarily even if we're not reaching termination).
    size_t performIncrementOfDraining(size_t bytes);
    
    // This informs the GC about auxiliary of some size that we are keeping alive. If you don't do
    // this then the space will be freed at end of GC.
    void markAuxiliary(const void* base);

    void reportExtraMemoryVisited(size_t);
#if ENABLE(RESOURCE_USAGE)
    void reportExternalMemoryVisited(size_t);
#endif
    
    void dump(PrintStream&) const;

    bool isBuildingHeapSnapshot() const { return !!m_heapSnapshotBuilder; }
    HeapSnapshotBuilder* heapSnapshotBuilder() const { return m_heapSnapshotBuilder; }
    
    RootMarkReason rootMarkReason() const { return m_rootMarkReason; }
    void setRootMarkReason(RootMarkReason reason) { m_rootMarkReason = reason; }

    HeapVersion markingVersion() const { return m_markingVersion; }

    bool mutatorIsStopped() const { return m_mutatorIsStopped; }
    
    Lock& rightToRun() { return m_rightToRun; }
    
    void updateMutatorIsStopped(const AbstractLocker&);
    void updateMutatorIsStopped();
    
    bool hasAcknowledgedThatTheMutatorIsResumed() const;
    bool mutatorIsStoppedIsUpToDate() const;
    
    void optimizeForStoppedMutator();
    
    void didRace(const VisitRaceKey&);
    void didRace(JSCell* cell, const char* reason) { didRace(VisitRaceKey(cell, reason)); }
    
    void visitAsConstraint(const JSCell*);
    
    bool didReachTermination();
    
    void setIgnoreNewOpaqueRoots(bool value) { m_ignoreNewOpaqueRoots = value; }

    void donateAll();
    
    const char* codeName() const { return m_codeName.data(); }
    
    JS_EXPORT_PRIVATE void addParallelConstraintTask(RefPtr<SharedTask<void(SlotVisitor&)>>);

private:
    friend class ParallelModeEnabler;
    friend class MarkingConstraintSolver;
    
    void appendJSCellOrAuxiliary(HeapCell*);

    JS_EXPORT_PRIVATE void appendSlow(JSCell*, Dependency);
    JS_EXPORT_PRIVATE void appendHiddenSlow(JSCell*, Dependency);
    void appendHiddenSlowImpl(JSCell*, Dependency);
    
    template<typename ContainerType>
    void setMarkedAndAppendToMarkStack(ContainerType&, JSCell*, Dependency);
    
    void appendToMarkStack(JSCell*);
    
    template<typename ContainerType>
    void appendToMarkStack(ContainerType&, JSCell*);
    
    void appendToMutatorMarkStack(const JSCell*);
    
    void noteLiveAuxiliaryCell(HeapCell*);
    
    void visitChildren(const JSCell*);

    void propagateExternalMemoryVisitedIfNecessary();
    
    void donateKnownParallel();
    void donateKnownParallel(MarkStackArray& from, MarkStackArray& to);

    void donateAll(const AbstractLocker&);

    bool hasWork(const AbstractLocker&);
    bool didReachTermination(const AbstractLocker&);

    template<typename Func>
    IterationStatus forEachMarkStack(const Func&);

    MarkStackArray& correspondingGlobalStack(MarkStackArray&);

    MarkStackArray m_collectorStack;
    MarkStackArray m_mutatorStack;
    
    size_t m_bytesVisited;
    size_t m_visitCount;
    size_t m_nonCellVisitCount { 0 }; // Used for incremental draining, ignored otherwise.
    Checked<size_t, RecordOverflow> m_extraMemorySize { 0 };
    bool m_isInParallelMode;
    bool m_ignoreNewOpaqueRoots { false }; // Useful as a debugging mode.

    HeapVersion m_markingVersion;
    
    Heap& m_heap;

    HeapSnapshotBuilder* m_heapSnapshotBuilder { nullptr };
    JSCell* m_currentCell { nullptr };
    RootMarkReason m_rootMarkReason { RootMarkReason::None };
    bool m_isFirstVisit { false };
    bool m_mutatorIsStopped { false };
    bool m_canOptimizeForStoppedMutator { false };
    Lock m_rightToRun;
    
    CString m_codeName;
    
    MarkingConstraint* m_currentConstraint { nullptr };
    MarkingConstraintSolver* m_currentSolver { nullptr };
    
public:
#if !ASSERT_DISABLED
    bool m_isCheckingForDefaultMarkViolation;
    bool m_isDraining;
#endif
};

class ParallelModeEnabler {
public:
    ParallelModeEnabler(SlotVisitor& stack)
        : m_stack(stack)
    {
        ASSERT(!m_stack.m_isInParallelMode);
        m_stack.m_isInParallelMode = true;
    }
    
    ~ParallelModeEnabler()
    {
        ASSERT(m_stack.m_isInParallelMode);
        m_stack.m_isInParallelMode = false;
    }
    
private:
    SlotVisitor& m_stack;
};

class SetRootMarkReasonScope {
public:
    SetRootMarkReasonScope(SlotVisitor& visitor, SlotVisitor::RootMarkReason reason)
        : m_visitor(visitor)
        , m_previousReason(visitor.rootMarkReason())
    {
        m_visitor.setRootMarkReason(reason);
    }

    ~SetRootMarkReasonScope()
    {
        m_visitor.setRootMarkReason(m_previousReason);
    }

private:
    SlotVisitor& m_visitor;
    SlotVisitor::RootMarkReason m_previousReason;
};

} // namespace JSC
