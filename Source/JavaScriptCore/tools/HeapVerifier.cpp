/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HeapVerifier.h"

#include "ButterflyInlines.h"
#include "HeapIterationScope.h"
#include "JSCInlines.h"
#include "JSObject.h"
#include "MarkedSpaceInlines.h"

namespace JSC {

HeapVerifier::HeapVerifier(Heap* heap, unsigned numberOfGCCyclesToRecord)
    : m_heap(heap)
    , m_currentCycle(0)
    , m_numberOfCycles(numberOfGCCyclesToRecord)
{
    RELEASE_ASSERT(m_numberOfCycles > 0);
    m_cycles = std::make_unique<GCCycle[]>(m_numberOfCycles);
}

const char* HeapVerifier::phaseName(HeapVerifier::Phase phase)
{
    switch (phase) {
    case Phase::BeforeGC:
        return "BeforeGC";
    case Phase::BeforeMarking:
        return "BeforeMarking";
    case Phase::AfterMarking:
        return "AfterMarking";
    case Phase::AfterGC:
        return "AfterGC";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr; // Silencing a compiler warning.
}

void HeapVerifier::initializeGCCycle()
{
    Heap* heap = m_heap;
    incrementCycle();
    currentCycle().scope = *heap->collectionScope();
}

struct GatherCellFunctor : MarkedBlock::CountFunctor {
    GatherCellFunctor(CellList& list)
        : m_list(list)
    {
        ASSERT(!list.liveCells.size());
    }

    inline void visit(JSCell* cell)
    {
        CellProfile profile(cell);
        m_list.liveCells.append(profile);
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind kind) const
    {
        if (kind == HeapCell::JSCell) {
            // FIXME: This const_cast exists because this isn't a C++ lambda.
            // https://bugs.webkit.org/show_bug.cgi?id=159644
            const_cast<GatherCellFunctor*>(this)->visit(static_cast<JSCell*>(cell));
        }
        return IterationStatus::Continue;
    }

    CellList& m_list;
};

void HeapVerifier::gatherLiveCells(HeapVerifier::Phase phase)
{
    Heap* heap = m_heap;
    CellList& list = *cellListForGathering(phase);

    HeapIterationScope iterationScope(*heap);
    list.reset();
    GatherCellFunctor functor(list);
    heap->m_objectSpace.forEachLiveCell(iterationScope, functor);
}

CellList* HeapVerifier::cellListForGathering(HeapVerifier::Phase phase)
{
    switch (phase) {
    case Phase::BeforeMarking:
        return &currentCycle().before;
    case Phase::AfterMarking:
        return &currentCycle().after;
    case Phase::BeforeGC:
    case Phase::AfterGC:
        // We should not be gathering live cells during these phases.
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr; // Silencing a compiler warning.
}

static void trimDeadCellsFromList(HashSet<JSCell*>& knownLiveSet, CellList& list)
{
    if (!list.hasLiveCells)
        return;

    size_t liveCellsFound = 0;
    for (auto& cellProfile : list.liveCells) {
        if (cellProfile.isConfirmedDead)
            continue; // Don't "resurrect" known dead cells.
        if (!knownLiveSet.contains(cellProfile.cell)) {
            cellProfile.isConfirmedDead = true;
            continue;
        }
        liveCellsFound++;
    }
    list.hasLiveCells = !!liveCellsFound;
}

void HeapVerifier::trimDeadCells()
{
    HashSet<JSCell*> knownLiveSet;

    CellList& after = currentCycle().after;
    for (auto& cellProfile : after.liveCells)
        knownLiveSet.add(cellProfile.cell);

    trimDeadCellsFromList(knownLiveSet, currentCycle().before);

    for (int i = -1; i > -m_numberOfCycles; i--) {
        trimDeadCellsFromList(knownLiveSet, cycleForIndex(i).before);
        trimDeadCellsFromList(knownLiveSet, cycleForIndex(i).after);
    }
}

bool HeapVerifier::verifyButterflyIsInStorageSpace(Phase, CellList&)
{
    // FIXME: Make this work again. https://bugs.webkit.org/show_bug.cgi?id=161752
    return true;
}

void HeapVerifier::verify(HeapVerifier::Phase phase)
{
    bool beforeVerified = verifyButterflyIsInStorageSpace(phase, currentCycle().before);
    bool afterVerified = verifyButterflyIsInStorageSpace(phase, currentCycle().after);
    RELEASE_ASSERT(beforeVerified && afterVerified);
}

void HeapVerifier::reportCell(CellProfile& cellProfile, int cycleIndex, HeapVerifier::GCCycle& cycle, CellList& list)
{
    JSCell* cell = cellProfile.cell;

    if (cellProfile.isConfirmedDead) {
        dataLogF("FOUND dead cell %p in GC[%d] %s list '%s'\n",
            cell, cycleIndex, collectionScopeName(cycle.scope), list.name);
        return;
    }

    if (cell->isObject()) {
        JSObject* object = static_cast<JSObject*>(cell);
        Structure* structure = object->structure();
        Butterfly* butterfly = object->butterfly();
        void* butterflyBase = butterfly->base(structure);

        dataLogF("FOUND object %p type '%s' butterfly %p (base %p) in GC[%d] %s list '%s'\n",
            object, structure->classInfo()->className,
            butterfly, butterflyBase,
            cycleIndex, collectionScopeName(cycle.scope), list.name);
    } else {
        Structure* structure = cell->structure();
        dataLogF("FOUND cell %p type '%s' in GC[%d] %s list '%s'\n",
            cell, structure->classInfo()->className,
            cycleIndex, collectionScopeName(cycle.scope), list.name);
    }
}

void HeapVerifier::checkIfRecorded(JSCell* cell)
{
    bool found = false;

    for (int cycleIndex = 0; cycleIndex > -m_numberOfCycles; cycleIndex--) {
        GCCycle& cycle = cycleForIndex(cycleIndex);
        CellList& beforeList = cycle.before;
        CellList& afterList = cycle.after;

        CellProfile* profile;
        profile = beforeList.findCell(cell);
        if (profile) {
            reportCell(*profile, cycleIndex, cycle, beforeList);
            found = true;
        }
        profile = afterList.findCell(cell);
        if (profile) {
            reportCell(*profile, cycleIndex, cycle, afterList);
            found = true;
        }
    }

    if (!found)
        dataLogF("cell %p NOT FOUND\n", cell);
}

} // namespace JSC
