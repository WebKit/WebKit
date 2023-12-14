/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "VerifierSlotVisitor.h"

#include "BlockDirectoryInlines.h"
#include "ConservativeRoots.h"
#include "GCSegmentedArrayInlines.h"
#include "HeapCell.h"
#include "JSCInlines.h"
#include "VM.h"
#include "VerifierSlotVisitorInlines.h"
#include <wtf/StackTrace.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VerifierSlotVisitor);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(VerifierSlotVisitorMarkedBlockData, VerifierSlotVisitor::MarkedBlockData);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(VerifierSlotVisitorOpaqueRootData, VerifierSlotVisitor::OpaqueRootData);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(VerifierSlotVisitorPreciseAllocationData, VerifierSlotVisitor::PreciseAllocationData);

using MarkerData = VerifierSlotVisitor::MarkerData;

constexpr int maxMarkingStackFramesToCapture = 100;

MarkerData::MarkerData(ReferrerToken referrer, std::unique_ptr<StackTrace>&& stack)
    : m_referrer(referrer)
    , m_stack(WTFMove(stack))
{
}

VerifierSlotVisitor::MarkedBlockData::MarkedBlockData(MarkedBlock* block)
    : m_block(block)
{
}

void VerifierSlotVisitor::MarkedBlockData::addMarkerData(unsigned atomNumber, MarkerData&& marker)
{
    if (m_markers.isEmpty())
        m_markers.grow(MarkedBlock::atomsPerBlock);
    m_markers[atomNumber] = WTFMove(marker);
}

const MarkerData* VerifierSlotVisitor::MarkedBlockData::markerData(unsigned atomNumber) const
{
    auto& marker = m_markers[atomNumber];
    if (marker.stack())
        return &marker;
    return nullptr;
}

VerifierSlotVisitor::PreciseAllocationData::PreciseAllocationData(PreciseAllocation* allocation)
    : m_allocation(allocation)
{
}

const MarkerData* VerifierSlotVisitor::PreciseAllocationData::markerData() const
{
    if (m_marker.stack())
        return &m_marker;
    return nullptr;
}

void VerifierSlotVisitor::PreciseAllocationData::addMarkerData(MarkerData&& marker)
{
    m_marker = WTFMove(marker);
}

const MarkerData* VerifierSlotVisitor::OpaqueRootData::markerData() const
{
    if (m_marker.stack())
        return &m_marker;
    return nullptr;
}

void VerifierSlotVisitor::OpaqueRootData::addMarkerData(MarkerData&& marker)
{
    m_marker = WTFMove(marker);
}

VerifierSlotVisitor::VerifierSlotVisitor(JSC::Heap& heap)
    : Base(heap, "Verifier", m_opaqueRootStorage)
{
    m_needsExtraOpaqueRootHandling = true;
}

VerifierSlotVisitor::~VerifierSlotVisitor()
{
    heap()->objectSpace().forEachBlock(
        [&] (MarkedBlock::Handle* handle) {
            handle->block().setVerifierMemo(nullptr);
        });
}

void VerifierSlotVisitor::addParallelConstraintTask(RefPtr<SharedTask<void(AbstractSlotVisitor&)>> task)
{
    m_constraintTasks.append(WTFMove(task));
}

NO_RETURN_DUE_TO_CRASH void VerifierSlotVisitor::addParallelConstraintTask(RefPtr<SharedTask<void(SlotVisitor&)>>)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void VerifierSlotVisitor::executeConstraintTasks()
{
    while (!m_constraintTasks.isEmpty())
        m_constraintTasks.takeFirst()->run(*this);
}

void VerifierSlotVisitor::append(const ConservativeRoots& conservativeRoots)
{
    auto appendJSCellOrAuxiliary = [&] (HeapCell* heapCell) {
        if (!heapCell)
            return;

        if (testAndSetMarked(heapCell))
            return;

        switch (heapCell->cellKind()) {
        case HeapCell::JSCell:
        case HeapCell::JSCellWithIndexingHeader: {
            JSCell* jsCell = static_cast<JSCell*>(heapCell);
            appendToMarkStack(jsCell);
            return;
        }

        case HeapCell::Auxiliary:
            return;
        }
    };

    HeapCell** roots = conservativeRoots.roots();
    size_t size = conservativeRoots.size();
    for (size_t i = 0; i < size; ++i)
        appendJSCellOrAuxiliary(roots[i]);
}

void VerifierSlotVisitor::appendToMarkStack(JSCell* cell)
{
    ASSERT(!cell->isZapped());
    m_collectorStack.append(cell);
}

void VerifierSlotVisitor::appendUnbarriered(JSCell* cell)
{
    if (!cell)
        return;

    if (UNLIKELY(cell->isPreciseAllocation())) {
        if (LIKELY(isMarked(cell->preciseAllocation(), cell)))
            return;
    } else {
        MarkedBlock& block = cell->markedBlock();
        if (LIKELY(isMarked(block, cell)))
            return;
    }

    appendSlow(cell);
}

void VerifierSlotVisitor::appendHiddenUnbarriered(JSCell* cell)
{
    appendUnbarriered(cell);
}

void VerifierSlotVisitor::didAddOpaqueRoot(void* opaqueRoot)
{
    if (!Options::verboseVerifyGC())
        return;

    std::unique_ptr<OpaqueRootData>& data = m_opaqueRootMap.add(opaqueRoot, nullptr).iterator->value;
    if (!data)
        data = makeUnique<OpaqueRootData>();
    data->addMarkerData({ referrer(), StackTrace::captureStackTrace(maxMarkingStackFramesToCapture, 1) });
}

void VerifierSlotVisitor::didFindOpaqueRoot(void* opaqueRoot)
{
    RELEASE_ASSERT(m_context && m_context->isOpaqueRootContext());
    RELEASE_ASSERT(!m_context->referrer());
    m_context->setReferrer(ReferrerToken(OpaqueRoot, opaqueRoot));
}

void VerifierSlotVisitor::drain()
{
    RELEASE_ASSERT(m_mutatorStack.isEmpty());

    MarkStackArray& stack = m_collectorStack;
    if (stack.isEmpty())
        return;

    stack.refill();
    while (stack.canRemoveLast())
        visitChildren(stack.removeLast());
}

void VerifierSlotVisitor::dump(PrintStream& out) const
{
    RELEASE_ASSERT(m_mutatorStack.isEmpty());
    out.print("Verifier collector stack: ", m_collectorStack.size());
}

void VerifierSlotVisitor::dumpMarkerData(HeapCell* cell)
{
    auto markerDataForPreciseAllocation = [&] (PreciseAllocation& allocation) -> const MarkerData* {
        auto iterator = m_preciseAllocationMap.find(&allocation);
        if (iterator == m_preciseAllocationMap.end())
            return nullptr;
        return iterator->value->markerData();
    };

    auto markerDataForMarkedBlockCell = [&] (MarkedBlock& block, HeapCell* cell) -> const MarkerData* {
        auto iterator = m_markedBlockMap.find(&block);
        if (iterator == m_markedBlockMap.end())
            return nullptr;
        unsigned atomNumber = block.atomNumber(cell);
        return iterator->value->markerData(atomNumber);
    };

    auto markerDataForOpaqueRoot = [&] (void* opaqueRoot) -> const MarkerData* {
        auto iterator = m_opaqueRootMap.find(opaqueRoot);
        if (iterator == m_opaqueRootMap.end())
            return nullptr;
        return iterator->value->markerData();
    };

    WTF::dataFile().flush();

    void* opaqueRoot = nullptr;
    do {
        const MarkerData* markerData = nullptr;

        if (cell) {
            if (isJSCellKind(cell->cellKind()))
                dataLogLn(JSValue(static_cast<JSCell*>(cell)));

            bool isMarked = heap()->isMarked(cell);
            const char* wasOrWasNot = isMarked ? "was" : "was NOT";
            dataLogLn("In the real GC, cell ", RawPointer(cell), " ", wasOrWasNot, " marked.");

            if (cell->isPreciseAllocation())
                markerData = markerDataForPreciseAllocation(cell->preciseAllocation());
            else
                markerData = markerDataForMarkedBlockCell(cell->markedBlock(), cell);
            if (!markerData) {
                dataLogLn("Marker data is not available for cell ", RawPointer(cell));
                break;
            }
            dataLog("In the verifier GC, cell ", RawPointer(cell), " was visited");

        } else {
            RELEASE_ASSERT(opaqueRoot);

            bool containsOpaqueRoot = heap()->m_opaqueRoots.contains(opaqueRoot);
            const char* wasOrWasNot = containsOpaqueRoot ? "was" : "was NOT";
            dataLogLn("In the real GC, opaque root ", RawPointer(opaqueRoot), " ", wasOrWasNot, " added to the heap's opaque roots.");

            markerData = markerDataForOpaqueRoot(opaqueRoot);
            if (!markerData) {
                dataLogLn("Marker data is not available for opaque root ", RawPointer(opaqueRoot));
                break;
            }
            dataLog("In the verifier GC, opaque root ", RawPointer(opaqueRoot), " was added");
        }

        ReferrerToken referrer = markerData->referrer();
        if (auto* referrerCell = referrer.asCell()) {
            dataLogLn(" via cell ", RawPointer(referrerCell), " at:");
            cell = referrerCell;
            opaqueRoot = nullptr;
        } else if (auto* referrerOpaqueRoot = referrer.asOpaqueRoot()) {
            dataLogLn(" via opaque root ", RawPointer(referrerOpaqueRoot), " at:");
            cell = nullptr;
            opaqueRoot = referrerOpaqueRoot;
        } else {
            auto reason = referrer.asRootMarkReason();
            if (reason != RootMarkReason::None)
                dataLogLn(" from scan of ", reason, " roots at:");
            else
                dataLogLn(" at:");
            cell = nullptr;
            opaqueRoot = nullptr;
        }

        dataLogLn(StackTracePrinter { *markerData->stack(), "    " });
    } while (cell || opaqueRoot);
}

bool VerifierSlotVisitor::isFirstVisit() const
{
    // In the regular GC, this return value is only used to control whether
    // UnlinkedCodeBlocks will be aged (see UnlinkedCodeBlock::visitChildrenImpl()).
    // For the verifier GC pass, we should always return false because we don't
    // want to do the aging action again.
    return false;
}

bool VerifierSlotVisitor::isMarked(const void* rawCell) const
{
    HeapCell* cell = bitwise_cast<HeapCell*>(rawCell);
    if (cell->isPreciseAllocation())
        return isMarked(cell->preciseAllocation(), cell);
    return isMarked(cell->markedBlock(), cell);
}

bool VerifierSlotVisitor::isMarked(PreciseAllocation& allocation, HeapCell*) const
{
    return m_preciseAllocationMap.contains(&allocation);
}

bool VerifierSlotVisitor::isMarked(MarkedBlock& block, HeapCell* cell) const
{
    auto entry = m_markedBlockMap.find(&block);
    if (entry == m_markedBlockMap.end())
        return false;

    auto& data = entry->value;
    unsigned atomNumber = block.atomNumber(cell);
    return data->isMarked(atomNumber);
}

void VerifierSlotVisitor::markAuxiliary(const void* base)
{
    HeapCell* cell = bitwise_cast<HeapCell*>(base);

    ASSERT(cell->heap() == heap());
    testAndSetMarked(cell);
}

bool VerifierSlotVisitor::mutatorIsStopped() const
{
    return true;
}

bool VerifierSlotVisitor::testAndSetMarked(const void* rawCell)
{
    HeapCell* cell = bitwise_cast<HeapCell*>(rawCell);
    if (cell->isPreciseAllocation())
        return testAndSetMarked(cell->preciseAllocation());
    return testAndSetMarked(cell->markedBlock(), cell);
}

bool VerifierSlotVisitor::testAndSetMarked(PreciseAllocation& allocation)
{
    std::unique_ptr<PreciseAllocationData>& data = m_preciseAllocationMap.add(&allocation, nullptr).iterator->value;
    if (!data) {
        data = makeUnique<PreciseAllocationData>(&allocation);
        if (UNLIKELY(Options::verboseVerifyGC()))
            data->addMarkerData({ referrer(), StackTrace::captureStackTrace(maxMarkingStackFramesToCapture, 2) });
        return false;
    }
    return true;
}

bool VerifierSlotVisitor::testAndSetMarked(MarkedBlock& block, HeapCell* cell)
{
    MarkedBlockData* data = block.verifierMemo<MarkedBlockData*>();
    if (UNLIKELY(!data)) {
        std::unique_ptr<MarkedBlockData>& entryData = m_markedBlockMap.add(&block, nullptr).iterator->value;
        RELEASE_ASSERT(!entryData);
        entryData = makeUnique<MarkedBlockData>(&block);
        data = entryData.get();
        block.setVerifierMemo(data);
    }

    unsigned atomNumber = block.atomNumber(cell);
    bool alreadySet = data->testAndSetMarked(atomNumber);
    if (!alreadySet && UNLIKELY(Options::verboseVerifyGC()))
        data->addMarkerData(atomNumber, { referrer(), StackTrace::captureStackTrace(maxMarkingStackFramesToCapture, 2) });
    return alreadySet;
}

void VerifierSlotVisitor::setMarkedAndAppendToMarkStack(JSCell* cell)
{
    if (m_suppressVerifier)
        return;
    if (testAndSetMarked(cell))
        return;
    appendToMarkStack(cell);
}

void VerifierSlotVisitor::visitAsConstraint(const JSCell* cell)
{
    visitChildren(cell);
}

void VerifierSlotVisitor::visitChildren(const JSCell* cell)
{
    RELEASE_ASSERT(isMarked(cell));
    cell->methodTable()->visitChildren(const_cast<JSCell*>(cell), *this);
}

} // namespace JSC
