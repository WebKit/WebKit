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

namespace JSC {

using MarkerData = VerifierSlotVisitor::MarkerData;

constexpr int maxMarkingStackFramesToCapture = 100;

MarkerData::MarkerData(HeapCell* parent, std::unique_ptr<StackTrace>&& stack)
    : parent(parent)
    , stack(WTFMove(stack))
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
    if (marker.stack)
        return &marker;
    return nullptr;
}

VerifierSlotVisitor::PreciseAllocationData::PreciseAllocationData(PreciseAllocation* allocation)
    : m_allocation(allocation)
{
}

const MarkerData* VerifierSlotVisitor::PreciseAllocationData::markerData() const
{
    if (m_marker.stack)
        return &m_marker;
    return nullptr;
}

void VerifierSlotVisitor::PreciseAllocationData::addMarkerData(MarkerData&& marker)
{
    m_marker = WTFMove(marker);
}

VerifierSlotVisitor::VerifierSlotVisitor(Heap& heap)
    : Base(heap, "Verifier", m_opaqueRootStorage)
{
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

void VerifierSlotVisitor::dumpMarkerData(HeapCell* cell)
{
    if (cell->isPreciseAllocation())
        return dumpMarkerData(cell->preciseAllocation(), cell);
    return dumpMarkerData(cell->markedBlock(), cell);
}

void VerifierSlotVisitor::dumpMarkerData(PreciseAllocation& allocation, HeapCell* cell)
{
    auto iterator = m_preciseAllocationMap.find(&allocation);
    if (iterator != m_preciseAllocationMap.end()) {
        dumpMarkerData(cell, iterator->value->markerData());
        return;
    }
    dataLogLn("Cell ", RawPointer(cell), " not found");
}

void VerifierSlotVisitor::dumpMarkerData(MarkedBlock& block, HeapCell* cell)
{
    auto iterator = m_markedBlockMap.find(&block);
    if (iterator != m_markedBlockMap.end()) {
        unsigned atomNumber = block.atomNumber(cell);
        dumpMarkerData(cell, iterator->value->markerData(atomNumber));
        return;
    }
    dataLogLn("Cell ", RawPointer(cell), " not found");
}

void VerifierSlotVisitor::dumpMarkerData(HeapCell* cell, const MarkerData* marker)
{
    if (!marker) {
        dataLogLn("Marker data is not available for cell ", RawPointer(cell));
        return;
    }

    dataLogLn("Cell ", RawPointer(cell), " was reachable via cell ", RawPointer(marker->parent), " at:");
    marker->stack->dump(WTF::dataFile(), "    ");
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
            data->addMarkerData({ parentCell(), StackTrace::captureStackTrace(maxMarkingStackFramesToCapture, 2) });
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
        data->addMarkerData(atomNumber, { parentCell(), StackTrace::captureStackTrace(maxMarkingStackFramesToCapture, 2) });
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
    cell->methodTable(vm())->visitChildren(const_cast<JSCell*>(cell), *this);
}

} // namespace JSC
