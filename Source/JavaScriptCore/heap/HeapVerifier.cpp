/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "CopiedSpaceInlines.h"
#include "HeapIterationScope.h"
#include "JSCInlines.h"
#include "JSObject.h"

namespace JSC {

HeapVerifier::HeapVerifier(Heap* heap, unsigned numberOfGCCyclesToRecord)
    : m_heap(heap)
    , m_currentCycle(0)
    , m_numberOfCycles(numberOfGCCyclesToRecord)
{
    RELEASE_ASSERT(m_numberOfCycles > 0);
    m_cycles = std::make_unique<GCCycle[]>(m_numberOfCycles);
}

const char* HeapVerifier::collectionTypeName(HeapOperation type)
{
    switch (type) {
    case NoOperation:
        return "NoOperation";
    case AnyCollection:
        return "AnyCollection";
    case Allocation:
        return "Allocation";
    case EdenCollection:
        return "EdenCollection";
    case FullCollection:
        return "FullCollection";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr; // Silencing a compiler warning.
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

static void getButterflyDetails(JSObject* obj, void*& butterflyBase, size_t& butterflyCapacityInBytes, CopiedBlock*& butterflyBlock)
{
    Structure* structure = obj->structure();
    Butterfly* butterfly = obj->butterfly();
    butterflyBase = butterfly->base(structure);
    butterflyBlock = CopiedSpace::blockFor(butterflyBase);

    size_t propertyCapacity = structure->outOfLineCapacity();
    size_t preCapacity;
    size_t indexingPayloadSizeInBytes;
    bool hasIndexingHeader = obj->hasIndexingHeader();
    if (UNLIKELY(hasIndexingHeader)) {
        preCapacity = butterfly->indexingHeader()->preCapacity(structure);
        indexingPayloadSizeInBytes = butterfly->indexingHeader()->indexingPayloadSizeInBytes(structure);
    } else {
        preCapacity = 0;
        indexingPayloadSizeInBytes = 0;
    }
    butterflyCapacityInBytes = Butterfly::totalSize(preCapacity, propertyCapacity, hasIndexingHeader, indexingPayloadSizeInBytes);
}

void HeapVerifier::initializeGCCycle()
{
    Heap* heap = m_heap;
    incrementCycle();
    currentCycle().collectionType = heap->operationInProgress();
}

struct GatherLiveObjFunctor : MarkedBlock::CountFunctor {
    GatherLiveObjFunctor(LiveObjectList& list)
        : m_list(list)
    {
        ASSERT(!list.liveObjects.size());
    }

    inline void visit(JSCell* cell)
    {
        if (!cell->isObject())
            return;        
        LiveObjectData data(asObject(cell));
        m_list.liveObjects.append(data);
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind kind) const
    {
        if (kind == HeapCell::JSCell) {
            // FIXME: This const_cast exists because this isn't a C++ lambda.
            // https://bugs.webkit.org/show_bug.cgi?id=159644
            const_cast<GatherLiveObjFunctor*>(this)->visit(static_cast<JSCell*>(cell));
        }
        return IterationStatus::Continue;
    }

    LiveObjectList& m_list;
};

void HeapVerifier::gatherLiveObjects(HeapVerifier::Phase phase)
{
    Heap* heap = m_heap;
    LiveObjectList& list = *liveObjectListForGathering(phase);

    HeapIterationScope iterationScope(*heap);
    list.reset();
    GatherLiveObjFunctor functor(list);
    heap->m_objectSpace.forEachLiveCell(iterationScope, functor);
}

LiveObjectList* HeapVerifier::liveObjectListForGathering(HeapVerifier::Phase phase)
{
    switch (phase) {
    case Phase::BeforeMarking:
        return &currentCycle().before;
    case Phase::AfterMarking:
        return &currentCycle().after;
    case Phase::BeforeGC:
    case Phase::AfterGC:
        // We should not be gathering live objects during these phases.
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr; // Silencing a compiler warning.
}

static void trimDeadObjectsFromList(HashSet<JSObject*>& knownLiveSet, LiveObjectList& list)
{
    if (!list.hasLiveObjects)
        return;

    size_t liveObjectsFound = 0;
    for (size_t i = 0; i < list.liveObjects.size(); i++) {
        LiveObjectData& objData = list.liveObjects[i];
        if (objData.isConfirmedDead)
            continue; // Don't "resurrect" known dead objects.
        if (!knownLiveSet.contains(objData.obj)) {
            objData.isConfirmedDead = true;
            continue;
        }
        liveObjectsFound++;
    }
    list.hasLiveObjects = !!liveObjectsFound;
}

void HeapVerifier::trimDeadObjects()
{
    HashSet<JSObject*> knownLiveSet;

    LiveObjectList& after = currentCycle().after;
    for (size_t i = 0; i < after.liveObjects.size(); i++) {
        LiveObjectData& objData = after.liveObjects[i];
        knownLiveSet.add(objData.obj);
    }

    trimDeadObjectsFromList(knownLiveSet, currentCycle().before);

    for (int i = -1; i > -m_numberOfCycles; i--) {
        trimDeadObjectsFromList(knownLiveSet, cycleForIndex(i).before);
        trimDeadObjectsFromList(knownLiveSet, cycleForIndex(i).after);
    }
}

bool HeapVerifier::verifyButterflyIsInStorageSpace(Phase phase, LiveObjectList& list)
{
    auto& liveObjects = list.liveObjects;

    CopiedSpace& storageSpace = m_heap->m_storageSpace;
    bool listNamePrinted = false;
    bool success = true;
    for (size_t i = 0; i < liveObjects.size(); i++) {
        LiveObjectData& objectData = liveObjects[i];
        if (objectData.isConfirmedDead)
            continue;

        JSObject* obj = objectData.obj;
        Butterfly* butterfly = obj->butterfly();
        if (butterfly) {
            void* butterflyBase;
            size_t butterflyCapacityInBytes;
            CopiedBlock* butterflyBlock;
            getButterflyDetails(obj, butterflyBase, butterflyCapacityInBytes, butterflyBlock);

            if (!storageSpace.contains(butterflyBlock)) {
                if (!listNamePrinted) {
                    dataLogF("Verification @ phase %s FAILED in object list '%s' (size %zu)\n",
                        phaseName(phase), list.name, liveObjects.size());
                    listNamePrinted = true;
                }

                Structure* structure = obj->structure();
                const char* structureClassName = structure->classInfo()->className;
                dataLogF("    butterfly %p (base %p size %zu block %p) NOT in StorageSpace | obj %p type '%s'\n",
                    butterfly, butterflyBase, butterflyCapacityInBytes, butterflyBlock, obj, structureClassName);
                success = false;
            }
        }
    }
    return success;
}

void HeapVerifier::verify(HeapVerifier::Phase phase)
{
    bool beforeVerified = verifyButterflyIsInStorageSpace(phase, currentCycle().before);
    bool afterVerified = verifyButterflyIsInStorageSpace(phase, currentCycle().after);
    RELEASE_ASSERT(beforeVerified && afterVerified);
}

void HeapVerifier::reportObject(LiveObjectData& objData, int cycleIndex, HeapVerifier::GCCycle& cycle, LiveObjectList& list)
{
    JSObject* obj = objData.obj;

    if (objData.isConfirmedDead) {
        dataLogF("FOUND dead obj %p in GC[%d] %s list '%s'\n",
            obj, cycleIndex, cycle.collectionTypeName(), list.name);
        return;
    }

    Structure* structure = obj->structure();
    Butterfly* butterfly = obj->butterfly();
    void* butterflyBase;
    size_t butterflyCapacityInBytes;
    CopiedBlock* butterflyBlock;
    getButterflyDetails(obj, butterflyBase, butterflyCapacityInBytes, butterflyBlock);

    dataLogF("FOUND obj %p type '%s' butterfly %p (base %p size %zu block %p) in GC[%d] %s list '%s'\n",
        obj, structure->classInfo()->className,
        butterfly, butterflyBase, butterflyCapacityInBytes, butterflyBlock,
        cycleIndex, cycle.collectionTypeName(), list.name);
}

void HeapVerifier::checkIfRecorded(JSObject* obj)
{
    bool found = false;

    for (int cycleIndex = 0; cycleIndex > -m_numberOfCycles; cycleIndex--) {
        GCCycle& cycle = cycleForIndex(cycleIndex);
        LiveObjectList& beforeList = cycle.before; 
        LiveObjectList& afterList = cycle.after; 

        LiveObjectData* objData;
        objData = beforeList.findObject(obj);
        if (objData) {
            reportObject(*objData, cycleIndex, cycle, beforeList);
            found = true;
        }
        objData = afterList.findObject(obj);
        if (objData) {
            reportObject(*objData, cycleIndex, cycle, afterList);
            found = true;
        }
    }

    if (!found)
        dataLogF("obj %p NOT FOUND\n", obj);
}

} // namespace JSC
