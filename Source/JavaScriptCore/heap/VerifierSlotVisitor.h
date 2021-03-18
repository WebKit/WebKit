/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AbstractSlotVisitor.h"
#include "JSCJSValue.h"
#include "MarkedBlock.h"
#include "VisitRaceKey.h"
#include "Weak.h"
#include <memory>
#include <wtf/Bitmap.h>
#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/SharedTask.h>

namespace WTF {

class StackTrace;

} // namespace WTF

namespace JSC {

class ConservativeRoots;
class Heap;
class HeapCell;
class PreciseAllocation;

using WTF::StackTrace;

class VerifierSlotVisitor : public AbstractSlotVisitor {
    WTF_MAKE_NONCOPYABLE(VerifierSlotVisitor);
    WTF_MAKE_FAST_ALLOCATED;
    using Base = AbstractSlotVisitor;
public:
    using ReferrerToken = AbstractSlotVisitor::ReferrerToken;

    struct MarkerData {
        MarkerData() = default;
        MarkerData(MarkerData&&) = default;
        MarkerData(ReferrerToken, std::unique_ptr<StackTrace>&&);
        MarkerData& operator=(MarkerData&&) = default;

        ReferrerToken referrer() const { return m_referrer; }
        StackTrace* stack() const { return m_stack.get(); }

    private:
        ReferrerToken m_referrer;
        std::unique_ptr<StackTrace> m_stack;
    };

    VerifierSlotVisitor(Heap&);
    ~VerifierSlotVisitor();

    void append(const ConservativeRoots&) final;
    void appendUnbarriered(JSCell*) final;
    void appendHiddenUnbarriered(JSCell*) final;

    bool isFirstVisit() const final;
    bool isMarked(const void*) const final;
    bool isMarked(MarkedBlock&, HeapCell*) const final;
    bool isMarked(PreciseAllocation&, HeapCell*) const final;

    void markAuxiliary(const void*) final;

    void reportExtraMemoryVisited(size_t) final { }
#if ENABLE(RESOURCE_USAGE)
    void reportExternalMemoryVisited(size_t) final { }
#endif

    bool mutatorIsStopped() const final;

    void didAddOpaqueRoot(void*) final;
    void didFindOpaqueRoot(void*) final;

    void didRace(const VisitRaceKey&) final { }
    void dump(PrintStream&) const final;

    void visitAsConstraint(const JSCell*) final;

    void drain();

    void addParallelConstraintTask(RefPtr<SharedTask<void(AbstractSlotVisitor&)>>) final;
    NO_RETURN_DUE_TO_CRASH void addParallelConstraintTask(RefPtr<SharedTask<void(SlotVisitor&)>>) final;
    void executeConstraintTasks();

    template<typename Functor> void forEachLiveCell(const Functor&);
    template<typename Functor> void forEachLivePreciseAllocation(const Functor&);
    template<typename Functor> void forEachLiveMarkedBlockCell(const Functor&);

    JS_EXPORT_PRIVATE void dumpMarkerData(HeapCell*);

private:
    class MarkedBlockData {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(MarkedBlockData);
    public:
        using AtomsBitmap = Bitmap<MarkedBlock::atomsPerBlock>;

        MarkedBlockData(MarkedBlock*);

        MarkedBlock* block() const { return m_block; }
        const AtomsBitmap& atoms() const { return m_atoms; }

        bool isMarked(unsigned atomNumber) { return m_atoms.get(atomNumber); }
        bool testAndSetMarked(unsigned atomNumber) { return m_atoms.testAndSet(atomNumber); }

        void addMarkerData(unsigned atomNumber, MarkerData&&);
        const MarkerData* markerData(unsigned atomNumber) const;

    private:
        MarkedBlock* m_block { nullptr };
        AtomsBitmap m_atoms;
        Vector<MarkerData> m_markers;
    };

    class PreciseAllocationData {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(PreciseAllocationData);
    public:
        PreciseAllocationData(PreciseAllocation*);
        PreciseAllocation* allocation() const { return m_allocation; }

        void addMarkerData(MarkerData&&);
        const MarkerData* markerData() const;

    private:
        PreciseAllocation* m_allocation { nullptr };
        MarkerData m_marker;
    };

    class OpaqueRootData {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(OpaqueRootData);
    public:
        OpaqueRootData() = default;

        void addMarkerData(MarkerData&&);
        const MarkerData* markerData() const;

    private:
        MarkerData m_marker;
    };

    using MarkedBlockMap = HashMap<MarkedBlock*, std::unique_ptr<MarkedBlockData>>;
    using PreciseAllocationMap = HashMap<PreciseAllocation*, std::unique_ptr<PreciseAllocationData>>;
    using OpaqueRootMap = HashMap<void*, std::unique_ptr<OpaqueRootData>>;

    void appendToMarkStack(JSCell*);
    void appendSlow(JSCell* cell) { setMarkedAndAppendToMarkStack(cell); }

    bool testAndSetMarked(const void* rawCell);
    bool testAndSetMarked(PreciseAllocation&);
    bool testAndSetMarked(MarkedBlock&, HeapCell*);

    void setMarkedAndAppendToMarkStack(JSCell*);

    void visitChildren(const JSCell*);

    OpaqueRootMap m_opaqueRootMap;
    PreciseAllocationMap m_preciseAllocationMap;
    MarkedBlockMap m_markedBlockMap;
    ConcurrentPtrHashSet m_opaqueRootStorage;
    Deque<RefPtr<SharedTask<void(AbstractSlotVisitor&)>>, 32> m_constraintTasks;
};

} // namespace JSC
