/*
 * Copyright (C) 2011-2013, 2015-2016 Apple Inc. All rights reserved.
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

#include "CellState.h"
#include "HandleTypes.h"
#include "MarkStack.h"
#include "OpaqueRootSet.h"
#include "VisitRaceKey.h"
#include <wtf/MonotonicTime.h>

namespace JSC {

class ConservativeRoots;
class GCThreadSharedData;
class Heap;
class HeapCell;
class HeapSnapshotBuilder;
class MarkedBlock;
class UnconditionalFinalizer;
template<typename T> class Weak;
class WeakReferenceHarvester;
template<typename T> class WriteBarrierBase;

typedef uint32_t HeapVersion;

class SlotVisitor {
    WTF_MAKE_NONCOPYABLE(SlotVisitor);
    WTF_MAKE_FAST_ALLOCATED;

    friend class SetCurrentCellScope;
    friend class HeapRootVisitor; // Allowed to mark a JSValue* or JSCell** directly.
    friend class Heap;

public:
    SlotVisitor(Heap&);
    ~SlotVisitor();

    MarkStackArray& collectorMarkStack() { return m_collectorStack; }
    MarkStackArray& mutatorMarkStack() { return m_mutatorStack; }
    const MarkStackArray& collectorMarkStack() const { return m_collectorStack; }
    const MarkStackArray& mutatorMarkStack() const { return m_mutatorStack; }
    
    VM& vm();
    const VM& vm() const;
    Heap* heap() const;

    void append(ConservativeRoots&);
    
    template<typename T> void append(WriteBarrierBase<T>*);
    template<typename T> void appendHidden(WriteBarrierBase<T>*);
    template<typename Iterator> void append(Iterator begin , Iterator end);
    void appendValues(WriteBarrierBase<Unknown>*, size_t count);
    void appendValuesHidden(WriteBarrierBase<Unknown>*, size_t count);
    
    template<typename T>
    void appendUnbarrieredPointer(T**);
    void appendUnbarrieredValue(JSValue*);
    template<typename T>
    void appendUnbarrieredWeak(Weak<T>*);
    template<typename T>
    void appendUnbarrieredReadOnlyPointer(T*);
    void appendUnbarrieredReadOnlyValue(JSValue);
    
    void visitSubsequently(JSCell*);
    
    // Does your visitChildren do logic that depends on non-JS-object state that can
    // change during the course of a GC, or in between GCs? Then you should call this
    // method! It will cause the GC to invoke your visitChildren method again just before
    // terminating with the world stopped.
    JS_EXPORT_PRIVATE void rescanAsConstraint();
    
    // Implies rescanAsConstraint, so you don't have to call rescanAsConstraint() if you
    // call this unconditionally.
    JS_EXPORT_PRIVATE void addOpaqueRoot(void*);
    
    JS_EXPORT_PRIVATE bool containsOpaqueRoot(void*) const;
    TriState containsOpaqueRootTriState(void*) const;

    bool isEmpty() { return m_collectorStack.isEmpty() && m_mutatorStack.isEmpty(); }

    void didStartMarking();
    void reset();
    void clearMarkStacks();

    size_t bytesVisited() const { return m_bytesVisited; }
    size_t visitCount() const { return m_visitCount; }

    void donate();
    void drain(MonotonicTime timeout = MonotonicTime::infinity());
    void donateAndDrain(MonotonicTime timeout = MonotonicTime::infinity());
    
    enum SharedDrainMode { SlaveDrain, MasterDrain };
    enum class SharedDrainResult { Done, TimedOut };
    SharedDrainResult drainFromShared(SharedDrainMode, MonotonicTime timeout = MonotonicTime::infinity());

    SharedDrainResult drainInParallel(MonotonicTime timeout = MonotonicTime::infinity());
    SharedDrainResult drainInParallelPassively(MonotonicTime timeout = MonotonicTime::infinity());
    
    void mergeIfNecessary();

    // This informs the GC about auxiliary of some size that we are keeping alive. If you don't do
    // this then the space will be freed at end of GC.
    void markAuxiliary(const void* base);

    void reportExtraMemoryVisited(size_t);
#if ENABLE(RESOURCE_USAGE)
    void reportExternalMemoryVisited(size_t);
#endif
    
    void addWeakReferenceHarvester(WeakReferenceHarvester*);
    void addUnconditionalFinalizer(UnconditionalFinalizer*);

    void dump(PrintStream&) const;

    bool isBuildingHeapSnapshot() const { return !!m_heapSnapshotBuilder; }
    
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
    void didNotRace(const VisitRaceKey&);
    void didNotRace(JSCell* cell, const char* reason) { didNotRace(VisitRaceKey(cell, reason)); }

private:
    friend class ParallelModeEnabler;
    
    JS_EXPORT_PRIVATE void append(JSValue); // This is private to encourage clients to use WriteBarrier<T>.
    void appendJSCellOrAuxiliary(HeapCell*);
    void appendHidden(JSValue);

    JS_EXPORT_PRIVATE void setMarkedAndAppendToMarkStack(JSCell*);
    
    template<typename ContainerType>
    void setMarkedAndAppendToMarkStack(ContainerType&, JSCell*);
    
    void appendToMarkStack(JSCell*);
    
    template<typename ContainerType>
    void appendToMarkStack(ContainerType&, JSCell*);
    
    void appendToMutatorMarkStack(const JSCell*);
    
    void noteLiveAuxiliaryCell(HeapCell*);
    
    void mergeOpaqueRoots();
    void mergeOpaqueRootsAndConstraints();

    void mergeOpaqueRootsIfProfitable();

    void visitChildren(const JSCell*);
    
    void donateKnownParallel();
    void donateKnownParallel(MarkStackArray& from, MarkStackArray& to);
    
    bool hasWork();
    bool didReachTermination();

    MarkStackArray m_collectorStack;
    MarkStackArray m_mutatorStack;
    OpaqueRootSet m_opaqueRoots; // Handle-owning data structures not visible to the garbage collector.
    HashSet<JSCell*> m_constraints;
    
    size_t m_bytesVisited;
    size_t m_visitCount;
    bool m_isInParallelMode;
    
    HeapVersion m_markingVersion;
    
    Heap& m_heap;

    HeapSnapshotBuilder* m_heapSnapshotBuilder { nullptr };
    JSCell* m_currentCell { nullptr };
    bool m_isFirstVisit { false };
    bool m_mutatorIsStopped { false };
    bool m_canOptimizeForStoppedMutator { false };
    Lock m_rightToRun;
    
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

} // namespace JSC
