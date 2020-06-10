/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "JSObject.h"

#include "ArrayConstructor.h"
#include "CatchScope.h"
#include "CustomGetterSetter.h"
#include "Exception.h"
#include "GCDeferralContextInlines.h"
#include "GetterSetter.h"
#include "HeapAnalyzer.h"
#include "IndexingHeaderInlines.h"
#include "JSCInlines.h"
#include "JSCustomGetterSetterFunction.h"
#include "JSFunction.h"
#include "JSImmutableButterfly.h"
#include "Lookup.h"
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "ProxyObject.h"
#include "TypeError.h"
#include "VMInlines.h"
#include <wtf/Assertions.h>

namespace JSC {

// We keep track of the size of the last array after it was grown. We use this
// as a simple heuristic for as the value to grow the next array from size 0.
// This value is capped by the constant FIRST_VECTOR_GROW defined in
// ArrayConventions.h.
static unsigned lastArraySize = 0;

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSObject);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSFinalObject);

const ASCIILiteral NonExtensibleObjectPropertyDefineError { "Attempting to define property on object that is not extensible."_s };
const ASCIILiteral ReadonlyPropertyWriteError { "Attempted to assign to readonly property."_s };
const ASCIILiteral ReadonlyPropertyChangeError { "Attempting to change value of a readonly property."_s };
const ASCIILiteral UnableToDeletePropertyError { "Unable to delete property."_s };
const ASCIILiteral UnconfigurablePropertyChangeAccessMechanismError { "Attempting to change access mechanism for an unconfigurable property."_s };
const ASCIILiteral UnconfigurablePropertyChangeConfigurabilityError { "Attempting to change configurable attribute of unconfigurable property."_s };
const ASCIILiteral UnconfigurablePropertyChangeEnumerabilityError { "Attempting to change enumerable attribute of unconfigurable property."_s };
const ASCIILiteral UnconfigurablePropertyChangeWritabilityError { "Attempting to change writable attribute of unconfigurable property."_s };

const ClassInfo JSObject::s_info = { "Object", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSObject) };

const ClassInfo JSFinalObject::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFinalObject) };

static inline void getClassPropertyNames(JSGlobalObject* globalObject, const ClassInfo* classInfo, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();

    // Add properties from the static hashtables of properties
    for (; classInfo; classInfo = classInfo->parentClass) {
        const HashTable* table = classInfo->staticPropHashTable;
        if (!table)
            continue;

        for (auto iter = table->begin(); iter != table->end(); ++iter) {
            if (!(iter->attributes() & PropertyAttribute::DontEnum) || mode.includeDontEnumProperties())
                propertyNames.add(Identifier::fromString(vm, iter.key()));
        }
    }
}

ALWAYS_INLINE void JSObject::markAuxiliaryAndVisitOutOfLineProperties(SlotVisitor& visitor, Butterfly* butterfly, Structure* structure, PropertyOffset maxOffset)
{
    // We call this when we found everything without races.
    ASSERT(structure);
    
    if (!butterfly)
        return;

    if (isCopyOnWrite(structure->indexingMode())) {
        visitor.append(bitwise_cast<WriteBarrier<JSCell>>(JSImmutableButterfly::fromButterfly(butterfly)));
        return;
    }

    bool hasIndexingHeader = structure->hasIndexingHeader(this);
    size_t preCapacity;
    if (hasIndexingHeader)
        preCapacity = butterfly->indexingHeader()->preCapacity(structure);
    else
        preCapacity = 0;
    
    HeapCell* base = bitwise_cast<HeapCell*>(
        butterfly->base(preCapacity, Structure::outOfLineCapacity(maxOffset)));
    
    ASSERT(Heap::heap(base) == visitor.heap());
    
    visitor.markAuxiliary(base);
    
    unsigned outOfLineSize = Structure::outOfLineSize(maxOffset);
    visitor.appendValuesHidden(butterfly->propertyStorage() - outOfLineSize, outOfLineSize);
}

ALWAYS_INLINE Structure* JSObject::visitButterfly(SlotVisitor& visitor)
{
    static const char* const raceReason = "JSObject::visitButterfly";
    Structure* result = visitButterflyImpl(visitor);
    if (!result)
        visitor.didRace(this, raceReason);
    return result;
}

ALWAYS_INLINE Structure* JSObject::visitButterflyImpl(SlotVisitor& visitor)
{
    VM& vm = visitor.vm();
    
    Butterfly* butterfly;
    Structure* structure;
    PropertyOffset maxOffset;

    auto visitElements = [&] (IndexingType indexingMode) {
        switch (indexingMode) {
        // We don't need to visit the elements for CopyOnWrite butterflies since they we marked the JSImmutableButterfly acting as out butterfly.
        case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES:
            visitor.appendValuesHidden(butterfly->contiguous().data(), butterfly->publicLength());
            break;
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            visitor.appendValuesHidden(butterfly->arrayStorage()->m_vector, butterfly->arrayStorage()->vectorLength());
            if (butterfly->arrayStorage()->m_sparseMap)
                visitor.append(butterfly->arrayStorage()->m_sparseMap);
            break;
        default:
            break;
        }
    };

    if (visitor.mutatorIsStopped()) {
        butterfly = this->butterfly();
        structure = this->structure(vm);
        maxOffset = structure->maxOffset();
        
        markAuxiliaryAndVisitOutOfLineProperties(visitor, butterfly, structure, maxOffset);
        visitElements(structure->indexingMode());

        return structure;
    }
    
    // We want to ensure that we only scan the butterfly if we have an exactly matched structure and an
    // exactly matched size. The mutator is required to perform the following shenanigans when
    // reallocating the butterfly with a concurrent collector, with all fencing necessary to ensure
    // that this executes as if under sequential consistency:
    //
    //     object->structure = nuke(object->structure)
    //     object->butterfly = newButterfly
    //     structure->m_offset = newMaxOffset
    //     object->structure = newStructure
    //
    // It's OK to skip this when reallocating the butterfly in a way that does not affect the m_offset.
    // We have other protocols in place for that.
    //
    // Note that the m_offset can change without the structure changing, but in that case the mutator
    // will still store null to the structure.
    //
    // The collector will ensure that it always sees a matched butterfly/structure by reading the
    // structure before and after reading the butterfly. For simplicity, let's first consider the case
    // where the only way to change the outOfLineCapacity is to change the structure. This works
    // because the mutator performs the following steps sequentially:
    //
    //     NukeStructure ChangeButterfly PutNewStructure
    //
    // Meanwhile the collector performs the following steps sequentially:
    //
    //     ReadStructureEarly ReadButterfly ReadStructureLate
    //
    // The collector is allowed to do any of these three things:
    //
    // BEFORE: Scan the object with the structure and butterfly *before* the mutator's transition.
    // AFTER: Scan the object with the structure and butterfly *after* the mutator's transition.
    // IGNORE: Ignore the butterfly and call didRace to schedule us to be revisted again in the future.
    //
    // In other words, the collector will never see any torn structure/butterfly mix. It will
    // always see the structure/butterfly before the transition or after but not in between.
    //
    // We can prove that this is correct by exhaustively considering all interleavings:
    //
    // NukeStructure ChangeButterfly PutNewStructure ReadStructureEarly ReadButterfly ReadStructureLate: AFTER, trivially.
    // NukeStructure ChangeButterfly ReadStructureEarly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because nuked structure read early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ChangeButterfly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ReadButterfly ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ReadButterfly ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // NukeStructure ReadStructureEarly ReadButterfly ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read early
    // ReadStructureEarly NukeStructure ChangeButterfly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly NukeStructure ChangeButterfly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly NukeStructure ReadButterfly ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly ReadButterfly NukeStructure ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly PutNewStructure: BEFORE, trivially.
    //
    // But we additionally have to worry about the size changing. We make this work by requiring that
    // the collector reads the size early and late as well. Lets consider the interleaving of the
    // mutator changing the size without changing the structure:
    //
    //     NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure
    //
    // Meanwhile the collector does:
    //
    //     ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate
    //
    // The collector can detect races by not only comparing the early structure to the late structure
    // (which will be the same before and after the algorithm runs) but also by comparing the early and
    // late maxOffsets. Note: the IGNORE proofs do not cite all of the reasons why the collector will
    // ignore the case, since we only need to identify one to say that we're in the ignore case.
    //
    // NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate: AFTER, trivially
    // NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureEarly RestoreStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureEarly ReadMaxOffsetEarly RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureEarly ReadMaxOffsetEarly ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ChangeMaxOffset RestoreStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ChangeMaxOffset ReadMaxOffsetEarly RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ChangeButterfly ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // NukeStructure ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeButterfly ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure early
    // ReadStructureEarly NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate: AFTER, the ReadStructureEarly sees the same structure as after and everything else runs after.
    // ReadStructureEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: AFTER, as above and the ReadMaxOffsetEarly sees the maxOffset after.
    // ReadStructureEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: AFTER, as above and the ReadButterfly sees the right butterfly after.
    // ReadStructureEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ChangeButterfly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ChangeButterfly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ChangeButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ChangeButterfly ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly NukeStructure ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeButterfly ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadButterfly RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ChangeMaxOffset ReadButterfly ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly NukeStructure ReadButterfly ReadStructureLate ReadMaxOffsetLate ChangeButterfly ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure ReadStructureLate ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureLate RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ChangeMaxOffset ReadStructureLate ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly NukeStructure ReadStructureLate ReadMaxOffsetLate ChangeButterfly ChangeMaxOffset RestoreStructure: IGNORE, read nuked structure late
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure ReadMaxOffsetLate: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly ChangeMaxOffset ReadMaxOffsetLate RestoreStructure: IGNORE, read different offsets
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly ReadMaxOffsetLate ChangeMaxOffset RestoreStructure: BEFORE, reads the offset before, everything else happens before
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate NukeStructure ReadMaxOffsetLate ChangeButterfly ChangeMaxOffset RestoreStructure: BEFORE, reads the offset before, everything else happens before
    // ReadStructureEarly ReadMaxOffsetEarly ReadButterfly ReadStructureLate ReadMaxOffsetLate NukeStructure ChangeButterfly ChangeMaxOffset RestoreStructure: BEFORE, trivially
    //
    // Whew.
    //
    // What the collector is doing is just the "double collect" snapshot from "The Unbounded Single-
    // Writer Algorithm" from Yehuda Afek et al's "Atomic Snapshots of Shared Memory" in JACM 1993,
    // also available here:
    //
    // http://people.csail.mit.edu/shanir/publications/AADGMS.pdf
    //
    // Unlike Afek et al's algorithm, ours does not require extra hacks to force wait-freedom (see
    // "Observation 2" in the paper). This simplifies the whole algorithm. Instead we are happy with
    // obstruction-freedom, and like any good obstruction-free algorithm, we ensure progress using
    // scheduling. We also only collect the butterfly once instead of twice; this optimization seems
    // to hold up in my proofs above and I'm not sure it's part of Afek et al's algos.
    //
    // For more background on this kind of madness, I like this paper; it's where I learned about
    // both the snapshot algorithm and obstruction-freedom:
    //
    // Lunchangco, Moir, Shavit. "Nonblocking k-compare-single-swap." SPAA '03
    // https://pdfs.semanticscholar.org/343f/7182cde7669ca2a7de3dc01127927f384ef7.pdf
    
    StructureID structureID = this->structureID();
    if (isNuked(structureID))
        return nullptr;
    structure = vm.getStructure(structureID);
    maxOffset = structure->maxOffset();
    IndexingType indexingMode = structure->indexingMode();
    Dependency indexingModeDependency = Dependency::fence(indexingMode);
    Locker<JSCellLock> locker(NoLockingNecessary);
    switch (indexingMode) {
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        // We need to hold this lock to protect against changes to the innards of the butterfly
        // that can happen when the butterfly is used for array storage.
        // We do not need to hold this lock for contiguous butterflies. We do not reuse the existing
        // butterfly with contiguous shape for new array storage butterfly. When converting the butterfly
        // with contiguous shape to array storage, we always allocate a new one. Holding this lock for contiguous
        // butterflies is unnecessary since contiguous shaped butterfly never becomes broken state.
        locker = holdLock(cellLock());
        break;
    default:
        break;
    }
    butterfly = indexingModeDependency.consume(this)->butterfly();
    Dependency butterflyDependency = Dependency::fence(butterfly);
    if (!butterfly)
        return structure;
    if (butterflyDependency.consume(this)->structureID() != structureID)
        return nullptr;
    if (butterflyDependency.consume(structure)->maxOffset() != maxOffset)
        return nullptr;
    
    markAuxiliaryAndVisitOutOfLineProperties(visitor, butterfly, structure, maxOffset);
    ASSERT(indexingMode == structure->indexingMode());
    visitElements(indexingMode);
    
    return structure;
}

size_t JSObject::estimatedSize(JSCell* cell, VM& vm)
{
    JSObject* thisObject = jsCast<JSObject*>(cell);
    size_t butterflyOutOfLineSize = thisObject->m_butterfly ? thisObject->structure(vm)->outOfLineSize() : 0;
    return Base::estimatedSize(cell, vm) + butterflyOutOfLineSize;
}

void JSObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSObject* thisObject = jsCast<JSObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
#if ASSERT_ENABLED
    bool wasCheckingForDefaultMarkViolation = visitor.m_isCheckingForDefaultMarkViolation;
    visitor.m_isCheckingForDefaultMarkViolation = false;
#endif
    
    JSCell::visitChildren(thisObject, visitor);
    
    thisObject->visitButterfly(visitor);
    
#if ASSERT_ENABLED
    visitor.m_isCheckingForDefaultMarkViolation = wasCheckingForDefaultMarkViolation;
#endif
}

void JSObject::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    JSObject* thisObject = jsCast<JSObject*>(cell);
    Base::analyzeHeap(cell, analyzer);

    Structure* structure = thisObject->structure();
    for (auto& entry : structure->getPropertiesConcurrently()) {
        JSValue toValue = thisObject->getDirect(entry.offset);
        if (toValue && toValue.isCell())
            analyzer.analyzePropertyNameEdge(thisObject, toValue.asCell(), entry.key);
    }

    Butterfly* butterfly = thisObject->butterfly();
    if (butterfly) {
        WriteBarrier<Unknown>* data = nullptr;
        uint32_t count = 0;

        switch (thisObject->indexingType()) {
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            data = butterfly->contiguous().data();
            count = butterfly->publicLength();
            break;
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            data = butterfly->arrayStorage()->m_vector;
            count = butterfly->arrayStorage()->vectorLength();
            break;
        default:
            break;
        }

        for (uint32_t i = 0; i < count; ++i) {
            JSValue toValue = data[i].get();
            if (toValue && toValue.isCell())
                analyzer.analyzeIndexEdge(thisObject, toValue.asCell(), i);
        }
    }
}

void JSFinalObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSFinalObject* thisObject = jsCast<JSFinalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
#if ASSERT_ENABLED
    bool wasCheckingForDefaultMarkViolation = visitor.m_isCheckingForDefaultMarkViolation;
    visitor.m_isCheckingForDefaultMarkViolation = false;
#endif
    
    JSCell::visitChildren(thisObject, visitor);
    
    if (Structure* structure = thisObject->visitButterfly(visitor)) {
        if (unsigned storageSize = structure->inlineSize())
            visitor.appendValuesHidden(thisObject->inlineStorage(), storageSize);
    }
    
#if ASSERT_ENABLED
    visitor.m_isCheckingForDefaultMarkViolation = wasCheckingForDefaultMarkViolation;
#endif
}

String JSObject::className(const JSObject* object, VM& vm)
{
    const ClassInfo* info = object->classInfo(vm);
    ASSERT(info);
    return info->className;
}

String JSObject::toStringName(const JSObject* object, JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool objectIsArray = isArray(globalObject, object);
    RETURN_IF_EXCEPTION(scope, String());
    if (objectIsArray)
        return "Array"_s;
    if (TypeInfo::isArgumentsType(object->type()))
        return "Arguments"_s;
    if (const_cast<JSObject*>(object)->isCallable(vm))
        return "Function"_s;
    return "Object"_s;
}

String JSObject::calculatedClassName(JSObject* object)
{
    String constructorFunctionName;
    auto* structure = object->structure();
    auto* globalObject = structure->globalObject();
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    // Check for a display name of obj.constructor.
    // This is useful to get `Foo` for the `(class Foo).prototype` object.
    PropertySlot slot(object, PropertySlot::InternalMethodType::VMInquiry);
    if (object->getOwnPropertySlot(object, globalObject, vm.propertyNames->constructor, slot)) {
        EXCEPTION_ASSERT(!scope.exception());
        if (slot.isValue()) {
            if (JSObject* ctorObject = jsDynamicCast<JSObject*>(vm, slot.getValue(globalObject, vm.propertyNames->constructor))) {
                if (JSFunction* constructorFunction = jsDynamicCast<JSFunction*>(vm, ctorObject))
                    constructorFunctionName = constructorFunction->calculatedDisplayName(vm);
                else if (InternalFunction* constructorFunction = jsDynamicCast<InternalFunction*>(vm, ctorObject))
                    constructorFunctionName = constructorFunction->calculatedDisplayName(vm);
            }
        }
    }

    EXCEPTION_ASSERT(!scope.exception() || constructorFunctionName.isNull());
    if (UNLIKELY(scope.exception()))
        scope.clearException();

    // Get the display name of obj.__proto__.constructor.
    // This is useful to get `Foo` for a `new Foo` object.
    if (constructorFunctionName.isNull()) {
        MethodTable::GetPrototypeFunctionPtr defaultGetPrototype = JSObject::getPrototype;
        if (LIKELY(structure->classInfo()->methodTable.getPrototype == defaultGetPrototype)) {
            JSValue protoValue = object->getPrototypeDirect(vm);
            if (protoValue.isObject()) {
                JSObject* protoObject = asObject(protoValue);
                PropertySlot slot(protoValue, PropertySlot::InternalMethodType::VMInquiry);
                if (protoObject->getPropertySlot(globalObject, vm.propertyNames->constructor, slot)) {
                    EXCEPTION_ASSERT(!scope.exception());
                    if (slot.isValue()) {
                        if (JSObject* ctorObject = jsDynamicCast<JSObject*>(vm, slot.getValue(globalObject, vm.propertyNames->constructor))) {
                            if (JSFunction* constructorFunction = jsDynamicCast<JSFunction*>(vm, ctorObject))
                                constructorFunctionName = constructorFunction->calculatedDisplayName(vm);
                            else if (InternalFunction* constructorFunction = jsDynamicCast<InternalFunction*>(vm, ctorObject))
                                constructorFunctionName = constructorFunction->calculatedDisplayName(vm);
                        }
                    }
                }
            }
        }
    }

    EXCEPTION_ASSERT(!scope.exception() || constructorFunctionName.isNull());
    if (UNLIKELY(scope.exception()))
        scope.clearException();

    if (constructorFunctionName.isNull() || constructorFunctionName == "Object") {
        String tableClassName = object->methodTable(vm)->className(object, vm);
        if (!tableClassName.isNull() && tableClassName != "Object")
            return tableClassName;

        String classInfoName = object->classInfo(vm)->className;
        if (!classInfoName.isNull())
            return classInfoName;

        if (constructorFunctionName.isNull())
            return "Object"_s;
    }

    return constructorFunctionName;
}

bool JSObject::getOwnPropertySlotByIndex(JSObject* thisObject, JSGlobalObject* globalObject, unsigned i, PropertySlot& slot)
{
    VM& vm = globalObject->vm();

    // NB. The fact that we're directly consulting our indexed storage implies that it is not
    // legal for anyone to override getOwnPropertySlot() without also overriding
    // getOwnPropertySlotByIndex().
    
    if (i > MAX_ARRAY_INDEX)
        return thisObject->methodTable(vm)->getOwnPropertySlot(thisObject, globalObject, Identifier::from(vm, i), slot);
    
    switch (thisObject->indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        break;
        
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return false;
        
        JSValue value = butterfly->contiguous().at(thisObject, i).get();
        if (value) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::None), value);
            return true;
        }
        
        return false;
    }
        
    case ALL_DOUBLE_INDEXING_TYPES: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return false;
        
        double value = butterfly->contiguousDouble().at(thisObject, i);
        if (value == value) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::None), JSValue(JSValue::EncodeAsDouble, value));
            return true;
        }
        
        return false;
    }
        
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = thisObject->m_butterfly->arrayStorage();
        if (i >= storage->length())
            return false;
        
        if (i < storage->vectorLength()) {
            JSValue value = storage->m_vector[i].get();
            if (value) {
                slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::None), value);
                return true;
            }
        } else if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->notFound()) {
                it->value.get(thisObject, slot);
                return true;
            }
        }
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    return false;
}

#if ASSERT_ENABLED
// These needs to be unique (not inlined) for ASSERT_ENABLED builds to enable
// Structure::validateFlags() to do checks using function pointer comparisons.

bool JSObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    return getOwnPropertySlotImpl(object, globalObject, propertyName, slot);
}

void JSObject::doPutPropertySecurityCheck(JSObject*, JSGlobalObject*, PropertyName, PutPropertySlot&)
{
}
#endif // ASSERT_ENABLED

// https://tc39.github.io/ecma262/#sec-ordinaryset
bool ordinarySetSlow(JSGlobalObject* globalObject, JSObject* object, PropertyName propertyName, JSValue value, JSValue receiver, bool shouldThrow)
{
    // If we find the receiver is not the same to the object, we fall to this slow path.
    // Currently, there are 3 candidates.
    // 1. Reflect.set can alter the receiver with an arbitrary value.
    // 2. Window Proxy.
    // 3. ES6 Proxy.

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* current = object;
    PropertyDescriptor ownDescriptor;
    while (true) {
        if (current->type() == ProxyObjectType) {
            ProxyObject* proxy = jsCast<ProxyObject*>(current);
            PutPropertySlot slot(receiver, shouldThrow);
            RELEASE_AND_RETURN(scope, proxy->ProxyObject::put(proxy, globalObject, propertyName, value, slot));
        }

        // 9.1.9.1-2 Let ownDesc be ? O.[[GetOwnProperty]](P).
        bool ownDescriptorFound = current->getOwnPropertyDescriptor(globalObject, propertyName, ownDescriptor);
        RETURN_IF_EXCEPTION(scope, false);

        if (!ownDescriptorFound) {
            // 9.1.9.1-3-a Let parent be ? O.[[GetPrototypeOf]]().
            JSValue prototype = current->getPrototype(vm, globalObject);
            RETURN_IF_EXCEPTION(scope, false);

            // 9.1.9.1-3-b If parent is not null, then
            if (!prototype.isNull()) {
                // 9.1.9.1-3-b-i Return ? parent.[[Set]](P, V, Receiver).
                current = asObject(prototype);
                continue;
            }
            // 9.1.9.1-3-c-i Let ownDesc be the PropertyDescriptor{[[Value]]: undefined, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}.
            ownDescriptor = PropertyDescriptor(jsUndefined(), static_cast<unsigned>(PropertyAttribute::None));
        }
        break;
    }

    // 9.1.9.1-4 If IsDataDescriptor(ownDesc) is true, then
    if (ownDescriptor.isDataDescriptor()) {
        // 9.1.9.1-4-a If ownDesc.[[Writable]] is false, return false.
        if (!ownDescriptor.writable())
            return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

        // 9.1.9.1-4-b If Type(Receiver) is not Object, return false.
        if (!receiver.isObject())
            return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

        // In OrdinarySet, the receiver may not be the same to the object.
        // So, we perform [[GetOwnProperty]] onto the receiver while we already perform [[GetOwnProperty]] onto the object.

        // 9.1.9.1-4-c Let existingDescriptor be ? Receiver.[[GetOwnProperty]](P).
        JSObject* receiverObject = asObject(receiver);
        PropertyDescriptor existingDescriptor;
        bool existingDescriptorFound = receiverObject->getOwnPropertyDescriptor(globalObject, propertyName, existingDescriptor);
        RETURN_IF_EXCEPTION(scope, false);

        // 9.1.9.1-4-d If existingDescriptor is not undefined, then
        if (existingDescriptorFound) {
            // 9.1.9.1-4-d-i If IsAccessorDescriptor(existingDescriptor) is true, return false.
            if (existingDescriptor.isAccessorDescriptor())
                return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

            // 9.1.9.1-4-d-ii If existingDescriptor.[[Writable]] is false, return false.
            if (!existingDescriptor.writable())
                return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

            // 9.1.9.1-4-d-iii Let valueDesc be the PropertyDescriptor{[[Value]]: V}.
            PropertyDescriptor valueDescriptor;
            valueDescriptor.setValue(value);

            // 9.1.9.1-4-d-iv Return ? Receiver.[[DefineOwnProperty]](P, valueDesc).
            RELEASE_AND_RETURN(scope, receiverObject->methodTable(vm)->defineOwnProperty(receiverObject, globalObject, propertyName, valueDescriptor, shouldThrow));
        }

        // 9.1.9.1-4-e Else Receiver does not currently have a property P,
        // 9.1.9.1-4-e-i Return ? CreateDataProperty(Receiver, P, V).
        RELEASE_AND_RETURN(scope, receiverObject->methodTable(vm)->defineOwnProperty(receiverObject, globalObject, propertyName, PropertyDescriptor(value, static_cast<unsigned>(PropertyAttribute::None)), shouldThrow));
    }

    // 9.1.9.1-5 Assert: IsAccessorDescriptor(ownDesc) is true.
    ASSERT(ownDescriptor.isAccessorDescriptor());

    // 9.1.9.1-6 Let setter be ownDesc.[[Set]].
    // 9.1.9.1-7 If setter is undefined, return false.
    JSValue setter = ownDescriptor.setter();
    if (!setter.isObject())
        return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);

    // 9.1.9.1-8 Perform ? Call(setter, Receiver, << V >>).
    JSObject* setterObject = asObject(setter);
    MarkedArgumentBuffer args;
    args.append(value);
    ASSERT(!args.hasOverflowed());

    auto callData = getCallData(vm, setterObject);
    scope.release();
    call(globalObject, setterObject, callData, receiver, args);

    // 9.1.9.1-9 Return true.
    return true;
}

// ECMA 8.6.2.2
bool JSObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    return putInlineForJSObject(cell, globalObject, propertyName, value, slot);
}

bool JSObject::putInlineSlow(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!isThisValueAltered(slot, this));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* obj = this;
    for (;;) {
        Structure* structure = obj->structure(vm);
        if (UNLIKELY(structure->typeInfo().hasPutPropertySecurityCheck())) {
            obj->methodTable(vm)->doPutPropertySecurityCheck(obj, globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, false);
        }
        unsigned attributes;
        PropertyOffset offset = structure->get(vm, propertyName, attributes);
        if (isValidOffset(offset)) {
            if (attributes & PropertyAttribute::ReadOnly) {
                ASSERT(this->prototypeChainMayInterceptStoreTo(vm, propertyName) || obj == this);
                return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
            }

            JSValue gs = obj->getDirect(offset);
            if (gs.isGetterSetter()) {
                // We need to make sure that we decide to cache this property before we potentially execute aribitrary JS.
                if (!this->structure(vm)->isDictionary())
                    slot.setCacheableSetter(obj, offset);

                bool result = callSetter(globalObject, slot.thisValue(), gs, value, slot.isStrictMode() ? ECMAMode::strict() : ECMAMode::sloppy());
                RETURN_IF_EXCEPTION(scope, false);
                return result;
            }
            if (gs.isCustomGetterSetter()) {
                // We need to make sure that we decide to cache this property before we potentially execute aribitrary JS.
                if (attributes & PropertyAttribute::CustomAccessor)
                    slot.setCustomAccessor(obj, jsCast<CustomGetterSetter*>(gs.asCell())->setter());
                else
                    slot.setCustomValue(obj, jsCast<CustomGetterSetter*>(gs.asCell())->setter());

                bool result = callCustomSetter(globalObject, gs, attributes & PropertyAttribute::CustomAccessor, obj, slot.thisValue(), value);
                RETURN_IF_EXCEPTION(scope, false);
                return result;
            }
            ASSERT(!(attributes & PropertyAttribute::Accessor));

            // If there's an existing property on the base object, or on one of its 
            // prototypes, we should store the property on the *base* object.
            break;
        }
        if (!obj->staticPropertiesReified(vm)) {
            if (obj->classInfo(vm)->hasStaticSetterOrReadonlyProperties()) {
                if (auto entry = obj->findPropertyHashEntry(vm, propertyName))
                    RELEASE_AND_RETURN(scope, putEntry(globalObject, entry->table->classForThis, entry->value, obj, this, propertyName, value, slot));
            }
        }
        if (obj->type() == ProxyObjectType) {
            ProxyObject* proxy = jsCast<ProxyObject*>(obj);
            RELEASE_AND_RETURN(scope, proxy->ProxyObject::put(proxy, globalObject, propertyName, value, slot));
        }
        JSValue prototype = obj->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (prototype.isNull())
            break;
        obj = asObject(prototype);
    }

    if (!putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot))
        return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
    return true;
}

bool JSObject::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    JSObject* thisObject = jsCast<JSObject*>(cell);

    if (propertyName > MAX_ARRAY_INDEX) {
        PutPropertySlot slot(cell, shouldThrow);
        return thisObject->methodTable(vm)->put(thisObject, globalObject, Identifier::from(vm, propertyName), value, slot);
    }

    thisObject->ensureWritable(vm);

    switch (thisObject->indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        break;
        
    case ALL_UNDECIDED_INDEXING_TYPES: {
        thisObject->convertUndecidedForValue(vm, value);
        // Reloop.
        return putByIndex(cell, globalObject, propertyName, value, shouldThrow);
    }
        
    case ALL_INT32_INDEXING_TYPES: {
        if (!value.isInt32()) {
            thisObject->convertInt32ForValue(vm, value);
            return putByIndex(cell, globalObject, propertyName, value, shouldThrow);
        }
        FALLTHROUGH;
    }
        
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        Butterfly* butterfly = thisObject->butterfly();
        if (propertyName >= butterfly->vectorLength())
            break;
        butterfly->contiguous().at(thisObject, propertyName).setWithoutWriteBarrier(value);
        if (propertyName >= butterfly->publicLength())
            butterfly->setPublicLength(propertyName + 1);
        vm.heap.writeBarrier(thisObject, value);
        return true;
    }
        
    case ALL_DOUBLE_INDEXING_TYPES: {
        if (!value.isNumber()) {
            thisObject->convertDoubleToContiguous(vm);
            // Reloop.
            return putByIndex(cell, globalObject, propertyName, value, shouldThrow);
        }

        double valueAsDouble = value.asNumber();
        if (valueAsDouble != valueAsDouble) {
            thisObject->convertDoubleToContiguous(vm);
            // Reloop.
            return putByIndex(cell, globalObject, propertyName, value, shouldThrow);
        }
        Butterfly* butterfly = thisObject->butterfly();
        if (propertyName >= butterfly->vectorLength())
            break;
        butterfly->contiguousDouble().at(thisObject, propertyName) = valueAsDouble;
        if (propertyName >= butterfly->publicLength())
            butterfly->setPublicLength(propertyName + 1);
        return true;
    }
        
    case NonArrayWithArrayStorage:
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = thisObject->m_butterfly->arrayStorage();
        
        if (propertyName >= storage->vectorLength())
            break;
        
        WriteBarrier<Unknown>& valueSlot = storage->m_vector[propertyName];
        unsigned length = storage->length();
        
        // Update length & m_numValuesInVector as necessary.
        if (propertyName >= length) {
            length = propertyName + 1;
            storage->setLength(length);
            ++storage->m_numValuesInVector;
        } else if (!valueSlot)
            ++storage->m_numValuesInVector;
        
        valueSlot.set(vm, thisObject, value);
        return true;
    }
        
    case NonArrayWithSlowPutArrayStorage:
    case ArrayWithSlowPutArrayStorage: {
        ArrayStorage* storage = thisObject->m_butterfly->arrayStorage();
        
        if (propertyName >= storage->vectorLength())
            break;
        
        WriteBarrier<Unknown>& valueSlot = storage->m_vector[propertyName];
        unsigned length = storage->length();

        auto scope = DECLARE_THROW_SCOPE(vm);
        
        // Update length & m_numValuesInVector as necessary.
        if (propertyName >= length) {
            bool putResult = false;
            bool result = thisObject->attemptToInterceptPutByIndexOnHole(globalObject, propertyName, value, shouldThrow, putResult);
            RETURN_IF_EXCEPTION(scope, false);
            if (result)
                return putResult;
            length = propertyName + 1;
            storage->setLength(length);
            ++storage->m_numValuesInVector;
        } else if (!valueSlot) {
            bool putResult = false;
            bool result = thisObject->attemptToInterceptPutByIndexOnHole(globalObject, propertyName, value, shouldThrow, putResult);
            RETURN_IF_EXCEPTION(scope, false);
            if (result)
                return putResult;
            ++storage->m_numValuesInVector;
        }
        
        valueSlot.set(vm, thisObject, value);
        return true;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    return thisObject->putByIndexBeyondVectorLength(globalObject, propertyName, value, shouldThrow);
}

ArrayStorage* JSObject::enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(VM& vm, ArrayStorage* storage)
{
    SparseArrayValueMap* map = storage->m_sparseMap.get();

    if (!map)
        map = allocateSparseIndexMap(vm);

    if (map->sparseMode())
        return storage;

    map->setSparseMode();

    unsigned usedVectorLength = std::min(storage->length(), storage->vectorLength());
    for (unsigned i = 0; i < usedVectorLength; ++i) {
        JSValue value = storage->m_vector[i].get();
        // This will always be a new entry in the map, so no need to check we can write,
        // and attributes are default so no need to set them.
        if (value)
            map->add(this, i).iterator->value.forceSet(vm, map, value, 0);
    }

    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = storage->butterfly()->resizeArray(vm, this, structure(vm), 0, ArrayStorage::sizeFor(0));
    RELEASE_ASSERT(newButterfly);
    newButterfly->arrayStorage()->m_indexBias = 0;
    newButterfly->arrayStorage()->setVectorLength(0);
    newButterfly->arrayStorage()->m_sparseMap.set(vm, this, map);
    setButterfly(vm, newButterfly);
    
    return newButterfly->arrayStorage();
}

void JSObject::enterDictionaryIndexingMode(VM& vm)
{
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        // NOTE: this is horribly inefficient, as it will perform two conversions. We could optimize
        // this case if we ever cared. Note that ensureArrayStorage() can return null if the object
        // doesn't support traditional indexed properties. At the time of writing, this just affects
        // typed arrays.
        if (ArrayStorage* storage = ensureArrayStorageSlow(vm))
            enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, storage);
        break;
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, m_butterfly->arrayStorage());
        break;
        
    default:
        break;
    }
}

void JSObject::notifyPresenceOfIndexedAccessors(VM& vm)
{
    if (mayInterceptIndexedAccesses(vm))
        return;
    
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AddIndexedAccessors));
    
    if (!mayBePrototype())
        return;
    
    globalObject(vm)->haveABadTime(vm);
}

Butterfly* JSObject::createInitialIndexedStorage(VM& vm, unsigned length)
{
    ASSERT(length <= MAX_STORAGE_VECTOR_LENGTH);
    IndexingType oldType = indexingType();
    ASSERT_UNUSED(oldType, !hasIndexedProperties(oldType));
    ASSERT(!needsSlowPutIndexing(vm));
    ASSERT(!indexingShouldBeSparse(vm));
    Structure* structure = this->structure(vm);
    unsigned propertyCapacity = structure->outOfLineCapacity();
    unsigned vectorLength = Butterfly::optimalContiguousVectorLength(propertyCapacity, length);
    Butterfly* newButterfly = Butterfly::createOrGrowArrayRight(
        butterfly(), vm, this, structure, propertyCapacity, false, 0,
        sizeof(EncodedJSValue) * vectorLength);
    newButterfly->setPublicLength(length);
    newButterfly->setVectorLength(vectorLength);
    return newButterfly;
}

Butterfly* JSObject::createInitialUndecided(VM& vm, unsigned length)
{
    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = createInitialIndexedStorage(vm, length);
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, NonPropertyTransition::AllocateUndecided);
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
    return newButterfly;
}

ContiguousJSValues JSObject::createInitialInt32(VM& vm, unsigned length)
{
    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = createInitialIndexedStorage(vm, length);
    for (unsigned i = newButterfly->vectorLength(); i--;)
        newButterfly->contiguous().at(this, i).setWithoutWriteBarrier(JSValue());
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, NonPropertyTransition::AllocateInt32);
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
    return newButterfly->contiguousInt32();
}

ContiguousDoubles JSObject::createInitialDouble(VM& vm, unsigned length)
{
    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = createInitialIndexedStorage(vm, length);
    for (unsigned i = newButterfly->vectorLength(); i--;)
        newButterfly->contiguousDouble().at(this, i) = PNaN;
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, NonPropertyTransition::AllocateDouble);
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
    return newButterfly->contiguousDouble();
}

ContiguousJSValues JSObject::createInitialContiguous(VM& vm, unsigned length)
{
    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = createInitialIndexedStorage(vm, length);
    for (unsigned i = newButterfly->vectorLength(); i--;)
        newButterfly->contiguous().at(this, i).setWithoutWriteBarrier(JSValue());
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, NonPropertyTransition::AllocateContiguous);
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
    return newButterfly->contiguous();
}

Butterfly* JSObject::createArrayStorageButterfly(VM& vm, JSObject* intendedOwner, Structure* structure, unsigned length, unsigned vectorLength, Butterfly* oldButterfly)
{
    Butterfly* newButterfly = Butterfly::createOrGrowArrayRight(
        oldButterfly, vm, intendedOwner, structure, structure->outOfLineCapacity(), false, 0,
        ArrayStorage::sizeFor(vectorLength));
    RELEASE_ASSERT(newButterfly);

    ArrayStorage* result = newButterfly->arrayStorage();
    result->setLength(length);
    result->setVectorLength(vectorLength);
    result->m_sparseMap.clear();
    result->m_numValuesInVector = 0;
    result->m_indexBias = 0;
    for (size_t i = vectorLength; i--;)
        result->m_vector[i].setWithoutWriteBarrier(JSValue());

    return newButterfly;
}

ArrayStorage* JSObject::createArrayStorage(VM& vm, unsigned length, unsigned vectorLength)
{
    DeferGC deferGC(vm.heap);
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    IndexingType oldType = indexingType();
    ASSERT_UNUSED(oldType, !hasIndexedProperties(oldType));

    Butterfly* newButterfly = createArrayStorageButterfly(vm, this, oldStructure, length, vectorLength, butterfly());
    ArrayStorage* result = newButterfly->arrayStorage();
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, suggestedArrayStorageTransition(vm));
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
    return result;
}

ArrayStorage* JSObject::createInitialArrayStorage(VM& vm)
{
    return createArrayStorage(
        vm, 0, ArrayStorage::optimalVectorLength(0, structure(vm)->outOfLineCapacity(), 0));
}

ContiguousJSValues JSObject::convertUndecidedToInt32(VM& vm)
{
    ASSERT(hasUndecided(indexingType()));

    Butterfly* butterfly = this->butterfly();
    for (unsigned i = butterfly->vectorLength(); i--;)
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(JSValue());

    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateInt32));
    return m_butterfly->contiguousInt32();
}

ContiguousDoubles JSObject::convertUndecidedToDouble(VM& vm)
{
    ASSERT(hasUndecided(indexingType()));

    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = butterfly->vectorLength(); i--;)
        butterfly->contiguousDouble().at(this, i) = PNaN;
    
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateDouble));
    return m_butterfly->contiguousDouble();
}

ContiguousJSValues JSObject::convertUndecidedToContiguous(VM& vm)
{
    ASSERT(hasUndecided(indexingType()));

    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = butterfly->vectorLength(); i--;)
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(JSValue());

    WTF::storeStoreFence();
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateContiguous));
    return m_butterfly->contiguous();
}

ArrayStorage* JSObject::constructConvertedArrayStorageWithoutCopyingElements(VM& vm, unsigned neededLength)
{
    Structure* structure = this->structure(vm);
    unsigned publicLength = m_butterfly->publicLength();
    unsigned propertyCapacity = structure->outOfLineCapacity();

    Butterfly* newButterfly = Butterfly::createUninitialized(vm, this, 0, propertyCapacity, true, ArrayStorage::sizeFor(neededLength));
    
    gcSafeMemcpy(
        static_cast<JSValue*>(newButterfly->base(0, propertyCapacity)),
        static_cast<JSValue*>(m_butterfly->base(0, propertyCapacity)),
        propertyCapacity * sizeof(EncodedJSValue));

    ArrayStorage* newStorage = newButterfly->arrayStorage();
    newStorage->setVectorLength(neededLength);
    newStorage->setLength(publicLength);
    newStorage->m_sparseMap.clear();
    newStorage->m_indexBias = 0;
    newStorage->m_numValuesInVector = 0;
    
    return newStorage;
}

ArrayStorage* JSObject::convertUndecidedToArrayStorage(VM& vm, NonPropertyTransition transition)
{
    DeferGC deferGC(vm.heap);
    ASSERT(hasUndecided(indexingType()));

    unsigned vectorLength = m_butterfly->vectorLength();
    ArrayStorage* storage = constructConvertedArrayStorageWithoutCopyingElements(vm, vectorLength);
    
    for (unsigned i = vectorLength; i--;)
        storage->m_vector[i].setWithoutWriteBarrier(JSValue());
    
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, transition);
    nukeStructureAndSetButterfly(vm, oldStructureID, storage->butterfly());
    setStructure(vm, newStructure);
    return storage;
}

ArrayStorage* JSObject::convertUndecidedToArrayStorage(VM& vm)
{
    return convertUndecidedToArrayStorage(vm, suggestedArrayStorageTransition(vm));
}

ContiguousDoubles JSObject::convertInt32ToDouble(VM& vm)
{
    ASSERT(hasInt32(indexingType()));
    ASSERT(!isCopyOnWrite(indexingMode()));

    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = butterfly->vectorLength(); i--;) {
        WriteBarrier<Unknown>* current = &butterfly->contiguous().atUnsafe(i);
        double* currentAsDouble = bitwise_cast<double*>(current);
        JSValue v = current->get();
        // NOTE: Since this may be used during initialization, v could be garbage. If it's garbage,
        // that means it will be overwritten later.
        if (!v.isInt32()) {
            *currentAsDouble = PNaN;
            continue;
        }
        *currentAsDouble = v.asInt32();
    }
    
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateDouble));
    return m_butterfly->contiguousDouble();
}

ContiguousJSValues JSObject::convertInt32ToContiguous(VM& vm)
{
    ASSERT(hasInt32(indexingType()));
    
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateContiguous));
    return m_butterfly->contiguous();
}

ArrayStorage* JSObject::convertInt32ToArrayStorage(VM& vm, NonPropertyTransition transition)
{
    DeferGC deferGC(vm.heap);
    ASSERT(hasInt32(indexingType()));

    unsigned vectorLength = m_butterfly->vectorLength();
    ArrayStorage* newStorage = constructConvertedArrayStorageWithoutCopyingElements(vm, vectorLength);
    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = 0; i < vectorLength; i++) {
        JSValue v = butterfly->contiguous().at(this, i).get();
        newStorage->m_vector[i].setWithoutWriteBarrier(v);
        if (v)
            newStorage->m_numValuesInVector++;
    }
    
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, transition);
    nukeStructureAndSetButterfly(vm, oldStructureID, newStorage->butterfly());
    setStructure(vm, newStructure);
    return newStorage;
}

ArrayStorage* JSObject::convertInt32ToArrayStorage(VM& vm)
{
    return convertInt32ToArrayStorage(vm, suggestedArrayStorageTransition(vm));
}

ContiguousJSValues JSObject::convertDoubleToContiguous(VM& vm)
{
    ASSERT(hasDouble(indexingType()));
    ASSERT(!isCopyOnWrite(indexingMode()));

    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = butterfly->vectorLength(); i--;) {
        double* current = &butterfly->contiguousDouble().atUnsafe(i);
        WriteBarrier<Unknown>* currentAsValue = bitwise_cast<WriteBarrier<Unknown>*>(current);
        double value = *current;
        if (value != value) {
            currentAsValue->clear();
            continue;
        }
        JSValue v = JSValue(JSValue::EncodeAsDouble, value);
        currentAsValue->setWithoutWriteBarrier(v);
    }
    
    WTF::storeStoreFence();
    setStructure(vm, Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::AllocateContiguous));
    return m_butterfly->contiguous();
}

ArrayStorage* JSObject::convertDoubleToArrayStorage(VM& vm, NonPropertyTransition transition)
{
    DeferGC deferGC(vm.heap);
    ASSERT(hasDouble(indexingType()));

    unsigned vectorLength = m_butterfly->vectorLength();
    ArrayStorage* newStorage = constructConvertedArrayStorageWithoutCopyingElements(vm, vectorLength);
    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = 0; i < vectorLength; i++) {
        double value = butterfly->contiguousDouble().at(this, i);
        if (value != value) {
            newStorage->m_vector[i].clear();
            continue;
        }
        newStorage->m_vector[i].setWithoutWriteBarrier(JSValue(JSValue::EncodeAsDouble, value));
        newStorage->m_numValuesInVector++;
    }
    
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, transition);
    nukeStructureAndSetButterfly(vm, oldStructureID, newStorage->butterfly());
    setStructure(vm, newStructure);
    return newStorage;
}

ArrayStorage* JSObject::convertDoubleToArrayStorage(VM& vm)
{
    return convertDoubleToArrayStorage(vm, suggestedArrayStorageTransition(vm));
}

ArrayStorage* JSObject::convertContiguousToArrayStorage(VM& vm, NonPropertyTransition transition)
{
    DeferGC deferGC(vm.heap);
    ASSERT(hasContiguous(indexingType()));

    unsigned vectorLength = m_butterfly->vectorLength();
    ArrayStorage* newStorage = constructConvertedArrayStorageWithoutCopyingElements(vm, vectorLength);
    Butterfly* butterfly = m_butterfly.get();
    for (unsigned i = 0; i < vectorLength; i++) {
        JSValue v = butterfly->contiguous().at(this, i).get();
        newStorage->m_vector[i].setWithoutWriteBarrier(v);
        if (v)
            newStorage->m_numValuesInVector++;
    }

    // While we modify the butterfly of Contiguous Array, we do not take any cellLock here. This is because
    // (1) the old butterfly is not changed and (2) new butterfly is not changed after it is exposed to
    // the collector.
    // The mutator performs the following operations are sequentially executed by using storeStoreFence.
    //
    //     CreateNewButterfly NukeStructure ChangeButterfly PutNewStructure
    //
    // Meanwhile the collector performs the following steps sequentially:
    //
    //     ReadStructureEarly ReadButterfly ReadStructureLate
    //
    // We list up all the patterns by writing a tiny script, and ensure all the cases are categorized into BEFORE, AFTER, and IGNORE.
    //
    // CreateNewButterfly NukeStructure ChangeButterfly PutNewStructure ReadStructureEarly ReadButterfly ReadStructureLate: AFTER, trivially
    // CreateNewButterfly NukeStructure ChangeButterfly ReadStructureEarly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ChangeButterfly ReadStructureEarly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ChangeButterfly ReadStructureEarly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ChangeButterfly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ChangeButterfly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ChangeButterfly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ReadButterfly ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ReadButterfly ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read early
    // CreateNewButterfly NukeStructure ReadStructureEarly ReadButterfly ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read early
    // CreateNewButterfly ReadStructureEarly NukeStructure ChangeButterfly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because early and late structures don't match
    // CreateNewButterfly ReadStructureEarly NukeStructure ChangeButterfly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // CreateNewButterfly ReadStructureEarly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // CreateNewButterfly ReadStructureEarly NukeStructure ReadButterfly ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // CreateNewButterfly ReadStructureEarly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // CreateNewButterfly ReadStructureEarly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // CreateNewButterfly ReadStructureEarly ReadButterfly NukeStructure ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // CreateNewButterfly ReadStructureEarly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // CreateNewButterfly ReadStructureEarly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // CreateNewButterfly ReadStructureEarly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly PutNewStructure: BEFORE, trivially.
    // ReadStructureEarly CreateNewButterfly NukeStructure ChangeButterfly PutNewStructure ReadButterfly ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly CreateNewButterfly NukeStructure ChangeButterfly ReadButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly CreateNewButterfly NukeStructure ChangeButterfly ReadButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly CreateNewButterfly NukeStructure ReadButterfly ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly CreateNewButterfly NukeStructure ReadButterfly ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly CreateNewButterfly NukeStructure ReadButterfly ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly CreateNewButterfly ReadButterfly NukeStructure ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly CreateNewButterfly ReadButterfly NukeStructure ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly CreateNewButterfly ReadButterfly NukeStructure ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly CreateNewButterfly ReadButterfly ReadStructureLate NukeStructure ChangeButterfly PutNewStructure: BEFORE, CreateNewButterfly is not visible to collector.
    // ReadStructureEarly ReadButterfly CreateNewButterfly NukeStructure ChangeButterfly PutNewStructure ReadStructureLate: IGNORE, because early and late structures don't match
    // ReadStructureEarly ReadButterfly CreateNewButterfly NukeStructure ChangeButterfly ReadStructureLate PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly ReadButterfly CreateNewButterfly NukeStructure ReadStructureLate ChangeButterfly PutNewStructure: IGNORE, because nuked structure read late
    // ReadStructureEarly ReadButterfly CreateNewButterfly ReadStructureLate NukeStructure ChangeButterfly PutNewStructure: BEFORE, CreateNewButterfly is not visible to collector.
    // ReadStructureEarly ReadButterfly ReadStructureLate CreateNewButterfly NukeStructure ChangeButterfly PutNewStructure: BEFORE, trivially.

    ASSERT(newStorage->butterfly() != butterfly);
    StructureID oldStructureID = this->structureID();
    Structure* oldStructure = vm.getStructure(oldStructureID);
    Structure* newStructure = Structure::nonPropertyTransition(vm, oldStructure, transition);

    // Ensure new Butterfly initialization is correctly done before exposing it to the concurrent threads.
    if (isX86() || vm.heap.mutatorShouldBeFenced())
        WTF::storeStoreFence();
    nukeStructureAndSetButterfly(vm, oldStructureID, newStorage->butterfly());
    setStructure(vm, newStructure);
    
    return newStorage;
}

ArrayStorage* JSObject::convertContiguousToArrayStorage(VM& vm)
{
    return convertContiguousToArrayStorage(vm, suggestedArrayStorageTransition(vm));
}

void JSObject::convertUndecidedForValue(VM& vm, JSValue value)
{
    IndexingType type = indexingTypeForValue(value);
    if (type == Int32Shape) {
        convertUndecidedToInt32(vm);
        return;
    }
    
    if (type == DoubleShape) {
        convertUndecidedToDouble(vm);
        return;
    }

    ASSERT(type == ContiguousShape);
    convertUndecidedToContiguous(vm);
}

void JSObject::createInitialForValueAndSet(VM& vm, unsigned index, JSValue value)
{
    if (value.isInt32()) {
        createInitialInt32(vm, index + 1).at(this, index).set(vm, this, value);
        return;
    }
    
    if (value.isDouble()) {
        double doubleValue = value.asNumber();
        if (doubleValue == doubleValue) {
            createInitialDouble(vm, index + 1).at(this, index) = doubleValue;
            return;
        }
    }
    
    createInitialContiguous(vm, index + 1).at(this, index).set(vm, this, value);
}

void JSObject::convertInt32ForValue(VM& vm, JSValue value)
{
    ASSERT(!value.isInt32());
    
    if (value.isDouble() && !std::isnan(value.asDouble())) {
        convertInt32ToDouble(vm);
        return;
    }

    convertInt32ToContiguous(vm);
}

void JSObject::convertFromCopyOnWrite(VM& vm)
{
    ASSERT(isCopyOnWrite(indexingMode()));
    ASSERT(structure(vm)->indexingMode() == indexingMode());

    const bool hasIndexingHeader = true;
    Butterfly* oldButterfly = butterfly();
    size_t propertyCapacity = 0;
    unsigned newVectorLength = Butterfly::optimalContiguousVectorLength(propertyCapacity, std::min(oldButterfly->vectorLength() * 2, MAX_STORAGE_VECTOR_LENGTH));
    Butterfly* newButterfly = Butterfly::createUninitialized(vm, this, 0, propertyCapacity, hasIndexingHeader, newVectorLength * sizeof(JSValue));

    gcSafeMemcpy(newButterfly->propertyStorage(), oldButterfly->propertyStorage(), oldButterfly->vectorLength() * sizeof(JSValue) + sizeof(IndexingHeader));

    WTF::storeStoreFence();
    NonPropertyTransition transition = ([&] () {
        switch (indexingType()) {
        case ArrayWithInt32:
            return NonPropertyTransition::AllocateInt32;
        case ArrayWithDouble:
            return NonPropertyTransition::AllocateDouble;
        case ArrayWithContiguous:
            return NonPropertyTransition::AllocateContiguous;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return NonPropertyTransition::AllocateContiguous;
        }
    })();
    StructureID oldStructureID = structureID();
    Structure* newStructure = Structure::nonPropertyTransition(vm, structure(vm), transition);
    nukeStructureAndSetButterfly(vm, oldStructureID, newButterfly);
    setStructure(vm, newStructure);
}

void JSObject::setIndexQuicklyToUndecided(VM& vm, unsigned index, JSValue value)
{
    ASSERT(index < m_butterfly->publicLength());
    ASSERT(index < m_butterfly->vectorLength());
    convertUndecidedForValue(vm, value);
    setIndexQuickly(vm, index, value);
}

void JSObject::convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(VM& vm, unsigned index, JSValue value)
{
    ASSERT(!value.isInt32());
    convertInt32ForValue(vm, value);
    setIndexQuickly(vm, index, value);
}

void JSObject::convertDoubleToContiguousWhilePerformingSetIndex(VM& vm, unsigned index, JSValue value)
{
    ASSERT(!value.isNumber() || value.asNumber() != value.asNumber());
    convertDoubleToContiguous(vm);
    setIndexQuickly(vm, index, value);
}

ContiguousJSValues JSObject::tryMakeWritableInt32Slow(VM& vm)
{
    ASSERT(inherits(vm, info()));

    if (isCopyOnWrite(indexingMode())) {
        if (leastUpperBoundOfIndexingTypes(indexingType() & IndexingShapeMask, Int32Shape) == Int32Shape) {
            ASSERT(hasInt32(indexingMode()));
            convertFromCopyOnWrite(vm);
            return butterfly()->contiguousInt32();
        }
        return ContiguousJSValues();
    }

    if (structure(vm)->hijacksIndexingHeader())
        return ContiguousJSValues();
    
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        if (UNLIKELY(indexingShouldBeSparse(vm) || needsSlowPutIndexing(vm)))
            return ContiguousJSValues();
        return createInitialInt32(vm, 0);
        
    case ALL_UNDECIDED_INDEXING_TYPES:
        return convertUndecidedToInt32(vm);
        
    case ALL_DOUBLE_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return ContiguousJSValues();
        
    default:
        CRASH();
        return ContiguousJSValues();
    }
}

ContiguousDoubles JSObject::tryMakeWritableDoubleSlow(VM& vm)
{
    ASSERT(inherits(vm, info()));

    if (isCopyOnWrite(indexingMode())) {
        if (leastUpperBoundOfIndexingTypes(indexingType() & IndexingShapeMask, DoubleShape) == DoubleShape) {
            convertFromCopyOnWrite(vm);
            if (hasDouble(indexingMode()))
                return butterfly()->contiguousDouble();
            ASSERT(hasInt32(indexingMode()));
        } else
            return ContiguousDoubles();
    }

    if (structure(vm)->hijacksIndexingHeader())
        return ContiguousDoubles();
    
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        if (UNLIKELY(indexingShouldBeSparse(vm) || needsSlowPutIndexing(vm)))
            return ContiguousDoubles();
        return createInitialDouble(vm, 0);
        
    case ALL_UNDECIDED_INDEXING_TYPES:
        return convertUndecidedToDouble(vm);
        
    case ALL_INT32_INDEXING_TYPES:
        return convertInt32ToDouble(vm);
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return ContiguousDoubles();
        
    default:
        CRASH();
        return ContiguousDoubles();
    }
}

ContiguousJSValues JSObject::tryMakeWritableContiguousSlow(VM& vm)
{
    ASSERT(inherits(vm, info()));

    if (isCopyOnWrite(indexingMode())) {
        if (leastUpperBoundOfIndexingTypes(indexingType() & IndexingShapeMask, ContiguousShape) == ContiguousShape) {
            convertFromCopyOnWrite(vm);
            if (hasContiguous(indexingMode()))
                return butterfly()->contiguous();
            ASSERT(hasInt32(indexingMode()) || hasDouble(indexingMode()));
        } else
            return ContiguousJSValues();
    }

    if (structure(vm)->hijacksIndexingHeader())
        return ContiguousJSValues();
    
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        if (UNLIKELY(indexingShouldBeSparse(vm) || needsSlowPutIndexing(vm)))
            return ContiguousJSValues();
        return createInitialContiguous(vm, 0);
        
    case ALL_UNDECIDED_INDEXING_TYPES:
        return convertUndecidedToContiguous(vm);
        
    case ALL_INT32_INDEXING_TYPES:
        return convertInt32ToContiguous(vm);
        
    case ALL_DOUBLE_INDEXING_TYPES:
        return convertDoubleToContiguous(vm);
        
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return ContiguousJSValues();
        
    default:
        CRASH();
        return ContiguousJSValues();
    }
}

ArrayStorage* JSObject::ensureArrayStorageSlow(VM& vm)
{
    ASSERT(inherits(vm, info()));

    if (structure(vm)->hijacksIndexingHeader())
        return nullptr;

    ensureWritable(vm);

    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        if (UNLIKELY(indexingShouldBeSparse(vm)))
            return ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm);
        return createInitialArrayStorage(vm);
        
    case ALL_UNDECIDED_INDEXING_TYPES:
        ASSERT(!indexingShouldBeSparse(vm));
        ASSERT(!needsSlowPutIndexing(vm));
        return convertUndecidedToArrayStorage(vm);
        
    case ALL_INT32_INDEXING_TYPES:
        ASSERT(!indexingShouldBeSparse(vm));
        ASSERT(!needsSlowPutIndexing(vm));
        return convertInt32ToArrayStorage(vm);
        
    case ALL_DOUBLE_INDEXING_TYPES:
        ASSERT(!indexingShouldBeSparse(vm));
        ASSERT(!needsSlowPutIndexing(vm));
        return convertDoubleToArrayStorage(vm);
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        ASSERT(!indexingShouldBeSparse(vm));
        ASSERT(!needsSlowPutIndexing(vm));
        return convertContiguousToArrayStorage(vm);
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

ArrayStorage* JSObject::ensureArrayStorageExistsAndEnterDictionaryIndexingMode(VM& vm)
{
    ensureWritable(vm);

    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES: {
        createArrayStorage(vm, 0, 0);
        SparseArrayValueMap* map = allocateSparseIndexMap(vm);
        map->setSparseMode();
        return arrayStorage();
    }
        
    case ALL_UNDECIDED_INDEXING_TYPES:
        return enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, convertUndecidedToArrayStorage(vm));
        
    case ALL_INT32_INDEXING_TYPES:
        return enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, convertInt32ToArrayStorage(vm));
        
    case ALL_DOUBLE_INDEXING_TYPES:
        return enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, convertDoubleToArrayStorage(vm));
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        return enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, convertContiguousToArrayStorage(vm));
        
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return enterDictionaryIndexingModeWhenArrayStorageAlreadyExists(vm, m_butterfly->arrayStorage());
        
    default:
        CRASH();
        return nullptr;
    }
}

void JSObject::switchToSlowPutArrayStorage(VM& vm)
{
    ensureWritable(vm);

    switch (indexingType()) {
    case ArrayClass:
        ensureArrayStorage(vm);
        RELEASE_ASSERT(hasAnyArrayStorage(indexingType()));
        if (hasSlowPutArrayStorage(indexingType()))
            return;
        switchToSlowPutArrayStorage(vm);
        break;

    case ALL_UNDECIDED_INDEXING_TYPES:
        convertUndecidedToArrayStorage(vm, NonPropertyTransition::AllocateSlowPutArrayStorage);
        break;
        
    case ALL_INT32_INDEXING_TYPES:
        convertInt32ToArrayStorage(vm, NonPropertyTransition::AllocateSlowPutArrayStorage);
        break;
        
    case ALL_DOUBLE_INDEXING_TYPES:
        convertDoubleToArrayStorage(vm, NonPropertyTransition::AllocateSlowPutArrayStorage);
        break;
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        convertContiguousToArrayStorage(vm, NonPropertyTransition::AllocateSlowPutArrayStorage);
        break;
        
    case NonArrayWithArrayStorage:
    case ArrayWithArrayStorage: {
        Structure* newStructure = Structure::nonPropertyTransition(vm, structure(vm), NonPropertyTransition::SwitchToSlowPutArrayStorage);
        setStructure(vm, newStructure);
        break;
    }
        
    default:
        CRASH();
        break;
    }
}

void JSObject::setPrototypeDirect(VM& vm, JSValue prototype)
{
    ASSERT(prototype);
    if (prototype.isObject())
        asObject(prototype)->didBecomePrototype();
    
    if (structure(vm)->hasMonoProto()) {
        DeferredStructureTransitionWatchpointFire deferred(vm, structure(vm));
        Structure* newStructure = Structure::changePrototypeTransition(vm, structure(vm), prototype, deferred);
        setStructure(vm, newStructure);
    } else
        putDirect(vm, knownPolyProtoOffset, prototype);

    if (!anyObjectInChainMayInterceptIndexedAccesses(vm))
        return;
    
    if (mayBePrototype()) {
        structure(vm)->globalObject()->haveABadTime(vm);
        return;
    }
    
    if (!hasIndexedProperties(indexingType()))
        return;
    
    if (shouldUseSlowPut(indexingType()))
        return;
    
    switchToSlowPutArrayStorage(vm);
}

bool JSObject::setPrototypeWithCycleCheck(VM& vm, JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (this->structure(vm)->isImmutablePrototypeExoticObject()) {
        // This implements https://tc39.github.io/ecma262/#sec-set-immutable-prototype.
        if (this->getPrototype(vm, globalObject) == prototype)
            return true;

        return typeError(globalObject, scope, shouldThrowIfCantSet, "Cannot set prototype of immutable prototype object"_s);
    }

    ASSERT(methodTable(vm)->toThis(this, globalObject, ECMAMode::sloppy()) == this);

    if (this->getPrototypeDirect(vm) == prototype)
        return true;

    bool isExtensible = this->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, false);

    if (!isExtensible)
        return typeError(globalObject, scope, shouldThrowIfCantSet, ReadonlyPropertyWriteError);

    JSValue nextPrototype = prototype;
    while (nextPrototype && nextPrototype.isObject()) {
        if (nextPrototype == this)
            return typeError(globalObject, scope, shouldThrowIfCantSet, "cyclic __proto__ value"_s);
        // FIXME: The specification currently says we should check if the [[GetPrototypeOf]] internal method of nextPrototype
        // is not the ordinary object internal method. However, we currently restrict this to Proxy objects as it would allow
        // for cycles with certain HTML objects (WindowProxy, Location) otherwise.
        // https://bugs.webkit.org/show_bug.cgi?id=161534
        if (UNLIKELY(asObject(nextPrototype)->type() == ProxyObjectType))
            break; // We're done. Set the prototype.
        nextPrototype = asObject(nextPrototype)->getPrototypeDirect(vm);
    }
    setPrototypeDirect(vm, prototype);
    return true;
}

bool JSObject::setPrototype(JSObject* object, JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    return object->setPrototypeWithCycleCheck(globalObject->vm(), globalObject, prototype, shouldThrowIfCantSet);
}

JSValue JSObject::getPrototype(JSObject* object, JSGlobalObject* globalObject)
{
    return object->getPrototypeDirect(globalObject->vm());
}

bool JSObject::setPrototype(VM& vm, JSGlobalObject* globalObject, JSValue prototype, bool shouldThrowIfCantSet)
{
    return methodTable(vm)->setPrototype(this, globalObject, prototype, shouldThrowIfCantSet);
}

bool JSObject::putGetter(JSGlobalObject* globalObject, PropertyName propertyName, JSValue getter, unsigned attributes)
{
    PropertyDescriptor descriptor;
    descriptor.setGetter(getter);

    ASSERT(attributes & PropertyAttribute::Accessor);
    if (!(attributes & PropertyAttribute::ReadOnly))
        descriptor.setConfigurable(true);
    if (!(attributes & PropertyAttribute::DontEnum))
        descriptor.setEnumerable(true);

    return defineOwnProperty(this, globalObject, propertyName, descriptor, true);
}

bool JSObject::putSetter(JSGlobalObject* globalObject, PropertyName propertyName, JSValue setter, unsigned attributes)
{
    PropertyDescriptor descriptor;
    descriptor.setSetter(setter);

    ASSERT(attributes & PropertyAttribute::Accessor);
    if (!(attributes & PropertyAttribute::ReadOnly))
        descriptor.setConfigurable(true);
    if (!(attributes & PropertyAttribute::DontEnum))
        descriptor.setEnumerable(true);

    return defineOwnProperty(this, globalObject, propertyName, descriptor, true);
}

bool JSObject::putDirectAccessor(JSGlobalObject* globalObject, PropertyName propertyName, GetterSetter* accessor, unsigned attributes)
{
    ASSERT(attributes & PropertyAttribute::Accessor);

    if (Optional<uint32_t> index = parseIndex(propertyName))
        return putDirectIndex(globalObject, index.value(), accessor, attributes, PutDirectIndexLikePutDirect);

    return putDirectNonIndexAccessor(globalObject->vm(), propertyName, accessor, attributes);
}

// FIXME: Introduce a JSObject::putDirectCustomValue() method instead of using
// JSObject::putDirectCustomAccessor() to put CustomValues.
// https://bugs.webkit.org/show_bug.cgi?id=192576
bool JSObject::putDirectCustomAccessor(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!parseIndex(propertyName));
    ASSERT(value.isCustomGetterSetter());
    if (!(attributes & PropertyAttribute::CustomAccessor))
        attributes |= PropertyAttribute::CustomValue;

    PutPropertySlot slot(this);
    bool result = putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, value, attributes, slot);

    ASSERT(slot.type() == PutPropertySlot::NewProperty);

    Structure* structure = this->structure(vm);
    if (attributes & PropertyAttribute::ReadOnly)
        structure->setContainsReadOnlyProperties();
    structure->setHasCustomGetterSetterPropertiesWithProtoCheck(propertyName == vm.propertyNames->underscoreProto);
    return result;
}

bool JSObject::putDirectNonIndexAccessor(VM& vm, PropertyName propertyName, GetterSetter* accessor, unsigned attributes)
{
    ASSERT(attributes & PropertyAttribute::Accessor);
    PutPropertySlot slot(this);
    bool result = putDirectInternal<PutModeDefineOwnProperty>(vm, propertyName, accessor, attributes, slot);

    Structure* structure = this->structure(vm);
    if (attributes & PropertyAttribute::ReadOnly)
        structure->setContainsReadOnlyProperties();

    structure->setHasGetterSetterPropertiesWithProtoCheck(propertyName == vm.propertyNames->underscoreProto);
    return result;
}

void JSObject::putDirectNonIndexAccessorWithoutTransition(VM& vm, PropertyName propertyName, GetterSetter* accessor, unsigned attributes)
{
    ASSERT(attributes & PropertyAttribute::Accessor);
    StructureID structureID = this->structureID();
    Structure* structure = vm.heap.structureIDTable().get(structureID);
    PropertyOffset offset = prepareToPutDirectWithoutTransition(vm, propertyName, attributes, structureID, structure);
    putDirect(vm, offset, accessor);
    if (attributes & PropertyAttribute::ReadOnly)
        structure->setContainsReadOnlyProperties();

    structure->setHasGetterSetterPropertiesWithProtoCheck(propertyName == vm.propertyNames->underscoreProto);
}

// HasProperty(O, P) from Section 7.3.10 of the spec.
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-hasproperty
bool JSObject::hasProperty(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    return hasPropertyGeneric(globalObject, propertyName, PropertySlot::InternalMethodType::HasProperty);
}

bool JSObject::hasProperty(JSGlobalObject* globalObject, unsigned propertyName) const
{
    return hasPropertyGeneric(globalObject, propertyName, PropertySlot::InternalMethodType::HasProperty);
}

bool JSObject::hasProperty(JSGlobalObject* globalObject, uint64_t propertyName) const
{
    if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
        return hasProperty(globalObject, static_cast<uint32_t>(propertyName));
    ASSERT(propertyName <= maxSafeInteger());
    return hasProperty(globalObject, Identifier::from(globalObject->vm(), propertyName));
}

bool JSObject::hasPropertyGeneric(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot::InternalMethodType internalMethodType) const
{
    PropertySlot slot(this, internalMethodType);
    return const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);
}

bool JSObject::hasPropertyGeneric(JSGlobalObject* globalObject, unsigned propertyName, PropertySlot::InternalMethodType internalMethodType) const
{
    PropertySlot slot(this, internalMethodType);
    return const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);
}

// ECMA 8.6.2.5
bool JSObject::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    JSObject* thisObject = jsCast<JSObject*>(cell);
    VM& vm = globalObject->vm();
    
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, globalObject, index.value());

    unsigned attributes;

    if (!thisObject->staticPropertiesReified(vm)) {
        if (auto entry = thisObject->findPropertyHashEntry(vm, propertyName)) {
            // If the static table contains a non-configurable (DontDelete) property then we can return early;
            // if there is a property in the storage array it too must be non-configurable (the language does
            // not allow repacement of a non-configurable property with a configurable one).
            if (entry->value->attributes() & PropertyAttribute::DontDelete && vm.deletePropertyMode() != VM::DeletePropertyMode::IgnoreConfigurable) {
                ASSERT(!isValidOffset(thisObject->structure(vm)->get(vm, propertyName, attributes)) || attributes & PropertyAttribute::DontDelete);
                return false;
            }
            thisObject->reifyAllStaticProperties(globalObject);
        }
    }

    Structure* structure = thisObject->structure(vm);

    bool propertyIsPresent = isValidOffset(structure->get(vm, propertyName, attributes));
    if (propertyIsPresent) {
        if (attributes & PropertyAttribute::DontDelete && vm.deletePropertyMode() != VM::DeletePropertyMode::IgnoreConfigurable) {
            slot.setNonconfigurable();
            return false;
        }
        DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);

        PropertyOffset offset = invalidOffset;
        if (structure->isUncacheableDictionary())
            offset = structure->removePropertyWithoutTransition(vm, propertyName, [] (const GCSafeConcurrentJSLocker&, PropertyOffset, PropertyOffset) { });
        else {
            structure = Structure::removePropertyTransition(vm, structure, propertyName, offset, &deferredWatchpointFire);
            slot.setHit(offset);
            ASSERT(structure->outOfLineCapacity() || !thisObject->structure(vm)->outOfLineCapacity());
            thisObject->setStructure(vm, structure);
        }

        ASSERT(!isValidOffset(structure->get(vm, propertyName, attributes)));

        if (offset != invalidOffset)
            thisObject->locationForOffset(offset)->clear();
    } else
        slot.setConfigurableMiss();

    return true;
}

bool JSObject::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned i)
{
    VM& vm = globalObject->vm();
    JSObject* thisObject = jsCast<JSObject*>(cell);
    
    if (i > MAX_ARRAY_INDEX)
        return JSCell::deleteProperty(thisObject, globalObject, Identifier::from(vm, i));
    
    switch (thisObject->indexingMode()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        return true;

    case CopyOnWriteArrayWithInt32:
    case CopyOnWriteArrayWithContiguous: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return true;
        thisObject->convertFromCopyOnWrite(vm);
        FALLTHROUGH;
    }

    case ALL_WRITABLE_INT32_INDEXING_TYPES:
    case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return true;
        butterfly->contiguous().at(thisObject, i).clear();
        return true;
    }

    case CopyOnWriteArrayWithDouble: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return true;
        thisObject->convertFromCopyOnWrite(vm);
        FALLTHROUGH;
    }

    case ALL_WRITABLE_DOUBLE_INDEXING_TYPES: {
        Butterfly* butterfly = thisObject->butterfly();
        if (i >= butterfly->vectorLength())
            return true;
        butterfly->contiguousDouble().at(thisObject, i) = PNaN;
        return true;
    }
        
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = thisObject->m_butterfly->arrayStorage();
        
        if (i < storage->vectorLength()) {
            WriteBarrier<Unknown>& valueSlot = storage->m_vector[i];
            if (valueSlot) {
                valueSlot.clear();
                --storage->m_numValuesInVector;
            }
        } else if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
            SparseArrayValueMap::iterator it = map->find(i);
            if (it != map->notFound()) {
                if (it->value.attributes() & PropertyAttribute::DontDelete)
                    return false;
                map->remove(it);
            }
        }
        
        return true;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

enum class TypeHintMode { TakesHint, DoesNotTakeHint };

template<TypeHintMode mode = TypeHintMode::DoesNotTakeHint>
static ALWAYS_INLINE JSValue callToPrimitiveFunction(JSGlobalObject* globalObject, const JSObject* object, PropertyName propertyName, PreferredPrimitiveType hint)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertySlot slot(object, PropertySlot::InternalMethodType::Get);
    // FIXME: Remove this when we have fixed: rdar://problem/33451840
    // https://bugs.webkit.org/show_bug.cgi?id=187109.
    constexpr bool debugNullStructure = mode == TypeHintMode::TakesHint;
    bool hasProperty = const_cast<JSObject*>(object)->getPropertySlot<debugNullStructure>(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, scope.exception());
    JSValue function = hasProperty ? slot.getValue(globalObject, propertyName) : jsUndefined();
    RETURN_IF_EXCEPTION(scope, scope.exception());
    if (function.isUndefinedOrNull() && mode == TypeHintMode::TakesHint)
        return JSValue();
    auto callData = getCallData(vm, function);
    if (callData.type == CallData::Type::None) {
        if (mode == TypeHintMode::TakesHint)
            throwTypeError(globalObject, scope, "Symbol.toPrimitive is not a function, undefined, or null"_s);
        return scope.exception();
    }

    MarkedArgumentBuffer callArgs;
    if (mode == TypeHintMode::TakesHint) {
        JSString* hintString = nullptr;
        switch (hint) {
        case NoPreference:
            hintString = vm.smallStrings.defaultString();
            break;
        case PreferNumber:
            hintString = vm.smallStrings.numberString();
            break;
        case PreferString:
            hintString = vm.smallStrings.stringString();
            break;
        }
        callArgs.append(hintString);
    }
    ASSERT(!callArgs.hasOverflowed());

    JSValue result = call(globalObject, function, callData, const_cast<JSObject*>(object), callArgs);
    RETURN_IF_EXCEPTION(scope, scope.exception());
    ASSERT(!result.isGetterSetter());
    if (result.isObject())
        return mode == TypeHintMode::DoesNotTakeHint ? JSValue() : throwTypeError(globalObject, scope, "Symbol.toPrimitive returned an object"_s);
    return result;
}

// ECMA 7.1.1
JSValue JSObject::ordinaryToPrimitive(JSGlobalObject* globalObject, PreferredPrimitiveType hint) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Make sure that whatever default value methods there are on object's prototype chain are
    // being watched.
    for (const JSObject* object = this; object; object = object->structure(vm)->storedPrototypeObject(object))
        object->structure(vm)->startWatchingInternalPropertiesIfNecessary(vm);

    JSValue value;
    if (hint == PreferString) {
        value = callToPrimitiveFunction(globalObject, this, vm.propertyNames->toString, hint);
        EXCEPTION_ASSERT(!scope.exception() || scope.exception() == value.asCell());
        if (value)
            return value;
        value = callToPrimitiveFunction(globalObject, this, vm.propertyNames->valueOf, hint);
        EXCEPTION_ASSERT(!scope.exception() || scope.exception() == value.asCell());
        if (value)
            return value;
    } else {
        value = callToPrimitiveFunction(globalObject, this, vm.propertyNames->valueOf, hint);
        EXCEPTION_ASSERT(!scope.exception() || scope.exception() == value.asCell());
        if (value)
            return value;
        value = callToPrimitiveFunction(globalObject, this, vm.propertyNames->toString, hint);
        EXCEPTION_ASSERT(!scope.exception() || scope.exception() == value.asCell());
        if (value)
            return value;
    }

    scope.assertNoException();

    return throwTypeError(globalObject, scope, "No default value"_s);
}

JSValue JSObject::defaultValue(const JSObject* object, JSGlobalObject* globalObject, PreferredPrimitiveType hint)
{
    return object->ordinaryToPrimitive(globalObject, hint);
}

JSValue JSObject::toPrimitive(JSGlobalObject* globalObject, PreferredPrimitiveType preferredType) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callToPrimitiveFunction<TypeHintMode::TakesHint>(globalObject, this, vm.propertyNames->toPrimitiveSymbol, preferredType);
    RETURN_IF_EXCEPTION(scope, { });
    if (value)
        return value;

    RELEASE_AND_RETURN(scope, this->methodTable(vm)->defaultValue(this, globalObject, preferredType));
}

bool JSObject::getPrimitiveNumber(JSGlobalObject* globalObject, double& number, JSValue& result) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    result = toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, false);
    scope.release();
    number = result.toNumber(globalObject);
    return !result.isString();
}

bool JSObject::getOwnStaticPropertySlot(VM& vm, PropertyName propertyName, PropertySlot& slot)
{
    for (auto* info = classInfo(vm); info; info = info->parentClass) {
        if (auto* table = info->staticPropHashTable) {
            if (getStaticPropertySlotFromTable(vm, table->classForThis, *table, this, propertyName, slot))
                return true;
        }
    }
    return false;
}

Optional<Structure::PropertyHashEntry> JSObject::findPropertyHashEntry(VM& vm, PropertyName propertyName) const
{
    return structure(vm)->findPropertyHashEntry(propertyName);
}

bool JSObject::hasInstance(JSGlobalObject* globalObject, JSValue value, JSValue hasInstanceValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!hasInstanceValue.isUndefinedOrNull() && hasInstanceValue != globalObject->functionProtoHasInstanceSymbolFunction()) {
        auto callData = JSC::getCallData(vm, hasInstanceValue);
        if (callData.type == CallData::Type::None) {
            throwException(globalObject, scope, createInvalidInstanceofParameterErrorHasInstanceValueNotFunction(globalObject, this));
            return false;
        }

        MarkedArgumentBuffer args;
        args.append(value);
        ASSERT(!args.hasOverflowed());
        JSValue result = call(globalObject, hasInstanceValue, callData, this, args);
        RETURN_IF_EXCEPTION(scope, false);
        return result.toBoolean(globalObject);
    }

    TypeInfo info = structure(vm)->typeInfo();
    if (info.implementsDefaultHasInstance()) {
        JSValue prototype = get(globalObject, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, false);
        RELEASE_AND_RETURN(scope, defaultHasInstance(globalObject, value, prototype));
    }
    if (info.implementsHasInstance()) {
        if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
            throwStackOverflowError(globalObject, scope);
            return false;
        }
        RELEASE_AND_RETURN(scope, methodTable(vm)->customHasInstance(this, globalObject, value));
    }

    throwException(globalObject, scope, createInvalidInstanceofParameterErrorNotFunction(globalObject, this));
    return false;
}

bool JSObject::hasInstance(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue hasInstanceValue = get(globalObject, vm.propertyNames->hasInstanceSymbol);
    RETURN_IF_EXCEPTION(scope, false);

    RELEASE_AND_RETURN(scope, hasInstance(globalObject, value, hasInstanceValue));
}

bool JSObject::defaultHasInstance(JSGlobalObject* globalObject, JSValue value, JSValue proto)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        return false;

    if (!proto.isObject()) {
        throwTypeError(globalObject, scope, "instanceof called on an object with an invalid prototype property."_s);
        return false;
    }

    JSObject* object = asObject(value);
    while (true) {
        JSValue objectValue = object->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (!objectValue.isObject())
            return false;
        object = asObject(objectValue);
        if (proto == object)
            return true;
    }
    ASSERT_NOT_REACHED();
}

EncodedJSValue JSC_HOST_CALL objectPrivateFuncInstanceOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue value = callFrame->uncheckedArgument(0);
    JSValue proto = callFrame->uncheckedArgument(1);

    return JSValue::encode(jsBoolean(JSObject::defaultHasInstance(globalObject, value, proto)));
}

void JSObject::getPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, propertyNames, mode);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue nextProto = object->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    if (nextProto.isNull())
        return;

    JSObject* prototype = asObject(nextProto);
    while(1) {
        if (prototype->structure(vm)->typeInfo().overridesGetPropertyNames()) {
            scope.release();
            prototype->methodTable(vm)->getPropertyNames(prototype, globalObject, propertyNames, mode);
            return;
        }
        prototype->methodTable(vm)->getOwnPropertyNames(prototype, globalObject, propertyNames, mode);
        RETURN_IF_EXCEPTION(scope, void());
        nextProto = prototype->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (nextProto.isNull())
            break;
        prototype = asObject(nextProto);
    }
}

void JSObject::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    if (!mode.includeJSObjectProperties()) {
        // We still have to get non-indexed properties from any subclasses of JSObject that have them.
        object->methodTable(vm)->getOwnNonIndexPropertyNames(object, globalObject, propertyNames, mode);
        return;
    }

    if (propertyNames.includeStringProperties()) {
        // Add numeric properties first. That appears to be the accepted convention.
        // FIXME: Filling PropertyNameArray with an identifier for every integer
        // is incredibly inefficient for large arrays. We need a different approach,
        // which almost certainly means a different structure for PropertyNameArray.
        switch (object->indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            break;
            
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES: {
            Butterfly* butterfly = object->butterfly();
            unsigned usedLength = butterfly->publicLength();
            for (unsigned i = 0; i < usedLength; ++i) {
                if (!butterfly->contiguous().at(object, i))
                    continue;
                propertyNames.add(i);
            }
            break;
        }
            
        case ALL_DOUBLE_INDEXING_TYPES: {
            Butterfly* butterfly = object->butterfly();
            unsigned usedLength = butterfly->publicLength();
            for (unsigned i = 0; i < usedLength; ++i) {
                double value = butterfly->contiguousDouble().at(object, i);
                if (value != value)
                    continue;
                propertyNames.add(i);
            }
            break;
        }
            
        case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
            ArrayStorage* storage = object->m_butterfly->arrayStorage();
            
            unsigned usedVectorLength = std::min(storage->length(), storage->vectorLength());
            for (unsigned i = 0; i < usedVectorLength; ++i) {
                if (storage->m_vector[i])
                    propertyNames.add(i);
            }
            
            if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
                Vector<unsigned, 0, UnsafeVectorOverflow> keys;
                keys.reserveInitialCapacity(map->size());
                
                SparseArrayValueMap::const_iterator end = map->end();
                for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it) {
                    if (mode.includeDontEnumProperties() || !(it->value.attributes() & PropertyAttribute::DontEnum))
                        keys.uncheckedAppend(static_cast<unsigned>(it->key));
                }
                
                std::sort(keys.begin(), keys.end());
                for (unsigned i = 0; i < keys.size(); ++i)
                    propertyNames.add(keys[i]);
            }
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    object->methodTable(vm)->getOwnNonIndexPropertyNames(object, globalObject, propertyNames, mode);
}

void JSObject::getOwnNonIndexPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    if (!object->staticPropertiesReified(vm))
        getClassPropertyNames(globalObject, object->classInfo(vm), propertyNames, mode);

    if (!mode.includeJSObjectProperties())
        return;
    
    object->structure(vm)->getPropertyNamesFromStructure(vm, propertyNames, mode);
}

double JSObject::toNumber(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue primitive = toPrimitive(globalObject, PreferNumber);
    RETURN_IF_EXCEPTION(scope, 0.0); // should be picked up soon in Nodes.cpp
    RELEASE_AND_RETURN(scope, primitive.toNumber(globalObject));
}

JSString* JSObject::toString(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue primitive = toPrimitive(globalObject, PreferString);
    RETURN_IF_EXCEPTION(scope, jsEmptyString(vm));
    RELEASE_AND_RETURN(scope, primitive.toString(globalObject));
}

JSValue JSObject::toThis(JSCell* cell, JSGlobalObject*, ECMAMode)
{
    return jsCast<JSObject*>(cell);
}

void JSObject::seal(VM& vm)
{
    if (isSealed(vm))
        return;
    enterDictionaryIndexingMode(vm);
    setStructure(vm, Structure::sealTransition(vm, structure(vm)));
}

void JSObject::freeze(VM& vm)
{
    if (isFrozen(vm))
        return;
    enterDictionaryIndexingMode(vm);
    setStructure(vm, Structure::freezeTransition(vm, structure(vm)));
}

bool JSObject::preventExtensions(JSObject* object, JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    if (!object->isStructureExtensible(vm)) {
        // We've already set the internal [[PreventExtensions]] field to false.
        // We don't call the methodTable isExtensible here because it's not defined
        // that way in the specification. We are just doing an optimization here.
        return true;
    }

    object->enterDictionaryIndexingMode(vm);
    object->setStructure(vm, Structure::preventExtensionsTransition(vm, object->structure(vm)));
    return true;
}

bool JSObject::isExtensible(JSObject* obj, JSGlobalObject* globalObject)
{
    return obj->isStructureExtensible(globalObject->vm());
}

bool JSObject::isExtensible(JSGlobalObject* globalObject)
{ 
    VM& vm = globalObject->vm();
    return methodTable(vm)->isExtensible(this, globalObject);
}

void JSObject::reifyAllStaticProperties(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    ASSERT(!staticPropertiesReified(vm));

    // If this object's ClassInfo has no static properties, then nothing to reify!
    // We can safely set the flag to avoid the expensive check again in the future.
    if (!TypeInfo::hasStaticPropertyTable(inlineTypeFlags())) {
        structure(vm)->setStaticPropertiesReified(true);
        return;
    }

    if (!structure(vm)->isDictionary())
        setStructure(vm, Structure::toCacheableDictionaryTransition(vm, structure(vm)));

    for (const ClassInfo* info = classInfo(vm); info; info = info->parentClass) {
        const HashTable* hashTable = info->staticPropHashTable;
        if (!hashTable)
            continue;

        for (auto& value : *hashTable) {
            unsigned attributes;
            auto key = Identifier::fromString(vm, value.m_key);
            PropertyOffset offset = getDirectOffset(vm, key, attributes);
            if (!isValidOffset(offset))
                reifyStaticProperty(vm, hashTable->classForThis, key, value, *this);
        }
    }

    structure(vm)->setStaticPropertiesReified(true);
}

NEVER_INLINE void JSObject::fillGetterPropertySlot(VM& vm, PropertySlot& slot, JSCell* getterSetter, unsigned attributes, PropertyOffset offset)
{
    if (structure(vm)->isUncacheableDictionary()) {
        slot.setGetterSlot(this, attributes, jsCast<GetterSetter*>(getterSetter));
        return;
    }

    // This access is cacheable because Structure requires an attributeChangedTransition
    // if this property stops being an accessor.
    slot.setCacheableGetterSlot(this, attributes, jsCast<GetterSetter*>(getterSetter), offset);
}

static bool putIndexedDescriptor(JSGlobalObject* globalObject, SparseArrayValueMap* map, SparseArrayEntry* entryInMap, const PropertyDescriptor& descriptor, PropertyDescriptor& oldDescriptor)
{
    VM& vm = globalObject->vm();

    if (descriptor.isDataDescriptor()) {
        unsigned attributes = descriptor.attributesOverridingCurrent(oldDescriptor) & ~PropertyAttribute::Accessor;
        if (descriptor.value())
            entryInMap->forceSet(vm, map, descriptor.value(), attributes);
        else if (oldDescriptor.isAccessorDescriptor())
            entryInMap->forceSet(vm, map, jsUndefined(), attributes);
        else
            entryInMap->forceSet(attributes);
        return true;
    }

    if (descriptor.isAccessorDescriptor()) {
        JSObject* getter = nullptr;
        if (descriptor.getterPresent())
            getter = descriptor.getterObject();
        else if (oldDescriptor.isAccessorDescriptor())
            getter = oldDescriptor.getterObject();
        JSObject* setter = nullptr;
        if (descriptor.setterPresent())
            setter = descriptor.setterObject();
        else if (oldDescriptor.isAccessorDescriptor())
            setter = oldDescriptor.setterObject();

        GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
        entryInMap->forceSet(vm, map, accessor, descriptor.attributesOverridingCurrent(oldDescriptor) & ~PropertyAttribute::ReadOnly);
        return true;
    }

    ASSERT(descriptor.isGenericDescriptor());
    entryInMap->forceSet(descriptor.attributesOverridingCurrent(oldDescriptor));
    return true;
}

ALWAYS_INLINE static bool canDoFastPutDirectIndex(VM& vm, JSObject* object)
{
    if (TypeInfo::isArgumentsType(object->type()))
        return true;

    if (object->inSparseIndexingMode())
        return false;

    return (isJSArray(object) && !isCopyOnWrite(object->indexingMode()))
        || jsDynamicCast<JSFinalObject*>(vm, object);
}

// Defined in ES5.1 8.12.9
bool JSObject::defineOwnIndexedProperty(JSGlobalObject* globalObject, unsigned index, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(index <= MAX_ARRAY_INDEX);

    ensureWritable(vm);

    if (!inSparseIndexingMode()) {
        // Fast case: we're putting a regular property to a regular array
        // FIXME: this will pessimistically assume that if attributes are missing then they'll default to false
        // however if the property currently exists missing attributes will override from their current 'true'
        // state (i.e. defineOwnProperty could be used to set a value without needing to entering 'SparseMode').
        if (!descriptor.attributes() && descriptor.value() && canDoFastPutDirectIndex(vm, this)) {
            ASSERT(!descriptor.isAccessorDescriptor());
            RELEASE_AND_RETURN(scope, putDirectIndex(globalObject, index, descriptor.value(), 0, throwException ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow));
        }
        
        ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm);
    }

    if (descriptor.attributes() & (PropertyAttribute::ReadOnly | PropertyAttribute::Accessor))
        notifyPresenceOfIndexedAccessors(vm);

    SparseArrayValueMap* map = m_butterfly->arrayStorage()->m_sparseMap.get();
    RELEASE_ASSERT(map);
    
    // 1. Let current be the result of calling the [[GetOwnProperty]] internal method of O with property name P.
    SparseArrayValueMap::AddResult result = map->add(this, index);
    SparseArrayEntry* entryInMap = &result.iterator->value;

    // 2. Let extensible be the value of the [[Extensible]] internal property of O.
    // 3. If current is undefined and extensible is false, then Reject.
    // 4. If current is undefined and extensible is true, then
    if (result.isNewEntry) {
        if (!isStructureExtensible(vm)) {
            map->remove(result.iterator);
            return typeError(globalObject, scope, throwException, NonExtensibleObjectPropertyDefineError);
        }

        // 4.a. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then create an own data property
        // named P of object O whose [[Value]], [[Writable]], [[Enumerable]] and [[Configurable]] attribute values
        // are described by Desc. If the value of an attribute field of Desc is absent, the attribute of the newly
        // created property is set to its default value.
        // 4.b. Else, Desc must be an accessor Property Descriptor so, create an own accessor property named P of
        // object O whose [[Get]], [[Set]], [[Enumerable]] and [[Configurable]] attribute values are described by
        // Desc. If the value of an attribute field of Desc is absent, the attribute of the newly created property
        // is set to its default value.
        // 4.c. Return true.

        PropertyDescriptor defaults(jsUndefined(), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
        putIndexedDescriptor(globalObject, map, entryInMap, descriptor, defaults);
        Butterfly* butterfly = m_butterfly.get();
        if (index >= butterfly->arrayStorage()->length())
            butterfly->arrayStorage()->setLength(index + 1);
        return true;
    }

    // 5. Return true, if every field in Desc is absent.
    // 6. Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value as the corresponding field in current when compared using the SameValue algorithm (9.12).
    PropertyDescriptor current;
    entryInMap->get(current);
    bool isEmptyOrEqual = descriptor.isEmpty() || descriptor.equalTo(globalObject, current);
    RETURN_IF_EXCEPTION(scope, false);
    if (isEmptyOrEqual)
        return true;

    // 7. If the [[Configurable]] field of current is false then
    if (!current.configurable()) {
        // 7.a. Reject, if the [[Configurable]] field of Desc is true.
        if (descriptor.configurablePresent() && descriptor.configurable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeConfigurabilityError);
        // 7.b. Reject, if the [[Enumerable]] field of Desc is present and the [[Enumerable]] fields of current and Desc are the Boolean negation of each other.
        if (descriptor.enumerablePresent() && current.enumerable() != descriptor.enumerable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeEnumerabilityError);
    }

    // 8. If IsGenericDescriptor(Desc) is true, then no further validation is required.
    if (!descriptor.isGenericDescriptor()) {
        // 9. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results, then
        if (current.isDataDescriptor() != descriptor.isDataDescriptor()) {
            // 9.a. Reject, if the [[Configurable]] field of current is false.
            if (!current.configurable())
                return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);
            // 9.b. If IsDataDescriptor(current) is true, then convert the property named P of object O from a
            // data property to an accessor property. Preserve the existing values of the converted property's
            // [[Configurable]] and [[Enumerable]] attributes and set the rest of the property's attributes to
            // their default values.
            // 9.c. Else, convert the property named P of object O from an accessor property to a data property.
            // Preserve the existing values of the converted property's [[Configurable]] and [[Enumerable]]
            // attributes and set the rest of the property's attributes to their default values.
        } else if (current.isDataDescriptor() && descriptor.isDataDescriptor()) {
            // 10. Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true, then
            // 10.a. If the [[Configurable]] field of current is false, then
            if (!current.configurable() && !current.writable()) {
                // 10.a.i. Reject, if the [[Writable]] field of current is false and the [[Writable]] field of Desc is true.
                if (descriptor.writable())
                    return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeWritabilityError);
                // 10.a.ii. If the [[Writable]] field of current is false, then
                // 10.a.ii.1. Reject, if the [[Value]] field of Desc is present and SameValue(Desc.[[Value]], current.[[Value]]) is false.
                if (descriptor.value()) {
                    bool isSame = sameValue(globalObject, descriptor.value(), current.value());
                    RETURN_IF_EXCEPTION(scope, false);
                    if (!isSame)
                        return typeError(globalObject, scope, throwException, ReadonlyPropertyChangeError);
                }
            }
            // 10.b. else, the [[Configurable]] field of current is true, so any change is acceptable.
        } else {
            ASSERT(current.isAccessorDescriptor() && current.getterPresent() && current.setterPresent());
            // 11. Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true so, if the [[Configurable]] field of current is false, then
            if (!current.configurable()) {
                // 11.i. Reject, if the [[Set]] field of Desc is present and SameValue(Desc.[[Set]], current.[[Set]]) is false.
                if (descriptor.setterPresent() && descriptor.setter() != current.setter())
                    return typeError(globalObject, scope, throwException, "Attempting to change the setter of an unconfigurable property."_s);
                // 11.ii. Reject, if the [[Get]] field of Desc is present and SameValue(Desc.[[Get]], current.[[Get]]) is false.
                if (descriptor.getterPresent() && descriptor.getter() != current.getter())
                    return typeError(globalObject, scope, throwException, "Attempting to change the getter of an unconfigurable property."_s);
            }
        }
    }

    // 12. For each attribute field of Desc that is present, set the correspondingly named attribute of the property named P of object O to the value of the field.
    putIndexedDescriptor(globalObject, map, entryInMap, descriptor, current);
    // 13. Return true.
    return true;
}

SparseArrayValueMap* JSObject::allocateSparseIndexMap(VM& vm)
{
    SparseArrayValueMap* result = SparseArrayValueMap::create(vm);
    arrayStorage()->m_sparseMap.set(vm, this, result);
    return result;
}

void JSObject::deallocateSparseIndexMap()
{
    if (ArrayStorage* arrayStorage = arrayStorageOrNull())
        arrayStorage->m_sparseMap.clear();
}

bool JSObject::attemptToInterceptPutByIndexOnHoleForPrototype(JSGlobalObject* globalObject, JSValue thisValue, unsigned i, JSValue value, bool shouldThrow, bool& putResult)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    for (JSObject* current = this; ;) {
        // This has the same behavior with respect to prototypes as JSObject::put(). It only
        // allows a prototype to intercept a put if (a) the prototype declares the property
        // we're after rather than intercepting it via an override of JSObject::put(), and
        // (b) that property is declared as ReadOnly or Accessor.
        
        ArrayStorage* storage = current->arrayStorageOrNull();
        if (storage && storage->m_sparseMap) {
            SparseArrayValueMap::iterator iter = storage->m_sparseMap->find(i);
            if (iter != storage->m_sparseMap->notFound() && (iter->value.attributes() & (PropertyAttribute::Accessor | PropertyAttribute::ReadOnly))) {
                scope.release();
                putResult = iter->value.put(globalObject, thisValue, storage->m_sparseMap.get(), value, shouldThrow);
                return true;
            }
        }

        if (current->type() == ProxyObjectType) {
            scope.release();
            ProxyObject* proxy = jsCast<ProxyObject*>(current);
            putResult = proxy->putByIndexCommon(globalObject, thisValue, i, value, shouldThrow);
            return true;
        }
        
        JSValue prototypeValue = current->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        if (prototypeValue.isNull())
            return false;
        
        current = asObject(prototypeValue);
    }
}

bool JSObject::attemptToInterceptPutByIndexOnHole(JSGlobalObject* globalObject, unsigned i, JSValue value, bool shouldThrow, bool& putResult)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue prototypeValue = getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (prototypeValue.isNull())
        return false;
    
    RELEASE_AND_RETURN(scope, asObject(prototypeValue)->attemptToInterceptPutByIndexOnHoleForPrototype(globalObject, this, i, value, shouldThrow, putResult));
}

template<IndexingType indexingShape>
bool JSObject::putByIndexBeyondVectorLengthWithoutAttributes(JSGlobalObject* globalObject, unsigned i, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isCopyOnWrite(indexingMode()));
    ASSERT((indexingType() & IndexingShapeMask) == indexingShape);
    ASSERT(!indexingShouldBeSparse(vm));

    Butterfly* butterfly = m_butterfly.get();
    
    // For us to get here, the index is either greater than the public length, or greater than
    // or equal to the vector length.
    ASSERT(i >= butterfly->vectorLength());
    
    if (i > MAX_STORAGE_VECTOR_INDEX
        || (i >= MIN_SPARSE_ARRAY_INDEX && !isDenseEnoughForVector(i, countElements<indexingShape>(butterfly)))
        || indexIsSufficientlyBeyondLengthForSparseMap(i, butterfly->vectorLength())) {
        ASSERT(i <= MAX_ARRAY_INDEX);
        ensureArrayStorageSlow(vm);
        SparseArrayValueMap* map = allocateSparseIndexMap(vm);
        bool result = map->putEntry(globalObject, this, i, value, false);
        RETURN_IF_EXCEPTION(scope, false);
        ASSERT(i >= arrayStorage()->length());
        arrayStorage()->setLength(i + 1);
        return result;
    }

    if (!ensureLength(vm, i + 1)) {
        throwOutOfMemoryError(globalObject, scope);
        return false;
    }
    butterfly = m_butterfly.get();

    RELEASE_ASSERT(i < butterfly->vectorLength());
    switch (indexingShape) {
    case Int32Shape:
        ASSERT(value.isInt32());
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(value);
        return true;
        
    case DoubleShape: {
        ASSERT(value.isNumber());
        double valueAsDouble = value.asNumber();
        ASSERT(valueAsDouble == valueAsDouble);
        butterfly->contiguousDouble().at(this, i) = valueAsDouble;
        return true;
    }
        
    case ContiguousShape:
        butterfly->contiguous().at(this, i).set(vm, this, value);
        return true;
        
    default:
        CRASH();
        return false;
    }
}

// Explicit instantiations needed by JSArray.cpp.
template bool JSObject::putByIndexBeyondVectorLengthWithoutAttributes<Int32Shape>(JSGlobalObject*, unsigned, JSValue);
template bool JSObject::putByIndexBeyondVectorLengthWithoutAttributes<DoubleShape>(JSGlobalObject*, unsigned, JSValue);
template bool JSObject::putByIndexBeyondVectorLengthWithoutAttributes<ContiguousShape>(JSGlobalObject*, unsigned, JSValue);

bool JSObject::putByIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject* globalObject, unsigned i, JSValue value, bool shouldThrow, ArrayStorage* storage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isCopyOnWrite(indexingMode()));
    // i should be a valid array index that is outside of the current vector.
    ASSERT(i <= MAX_ARRAY_INDEX);
    ASSERT(i >= storage->vectorLength());
    
    SparseArrayValueMap* map = storage->m_sparseMap.get();
    
    // First, handle cases where we don't currently have a sparse map.
    if (LIKELY(!map)) {
        // If the array is not extensible, we should have entered dictionary mode, and created the sparse map.
        ASSERT(isStructureExtensible(vm));
    
        // Update m_length if necessary.
        if (i >= storage->length())
            storage->setLength(i + 1);

        // Check that it is sensible to still be using a vector, and then try to grow the vector.
        if (LIKELY(!indexIsSufficientlyBeyondLengthForSparseMap(i, storage->vectorLength()) 
            && isDenseEnoughForVector(i, storage->m_numValuesInVector)
            && increaseVectorLength(vm, i + 1))) {
            // success! - reread m_storage since it has likely been reallocated, and store to the vector.
            storage = arrayStorage();
            storage->m_vector[i].set(vm, this, value);
            ++storage->m_numValuesInVector;
            return true;
        }
        // We don't want to, or can't use a vector to hold this property - allocate a sparse map & add the value.
        map = allocateSparseIndexMap(vm);
        RELEASE_AND_RETURN(scope, map->putEntry(globalObject, this, i, value, shouldThrow));
    }

    // Update m_length if necessary.
    unsigned length = storage->length();
    if (i >= length) {
        // Prohibit growing the array if length is not writable.
        if (map->lengthIsReadOnly() || !isStructureExtensible(vm))
            return typeError(globalObject, scope, shouldThrow, ReadonlyPropertyWriteError);
        length = i + 1;
        storage->setLength(length);
    }

    // We are currently using a map - check whether we still want to be doing so.
    // We will continue  to use a sparse map if SparseMode is set, a vector would be too sparse, or if allocation fails.
    unsigned numValuesInArray = storage->m_numValuesInVector + map->size();
    if (map->sparseMode() || !isDenseEnoughForVector(length, numValuesInArray) || !increaseVectorLength(vm, length))
        RELEASE_AND_RETURN(scope, map->putEntry(globalObject, this, i, value, shouldThrow));

    // Reread m_storage after increaseVectorLength, update m_numValuesInVector.
    storage = arrayStorage();
    storage->m_numValuesInVector = numValuesInArray;

    // Copy all values from the map into the vector, and delete the map.
    WriteBarrier<Unknown>* vector = storage->m_vector;
    SparseArrayValueMap::const_iterator end = map->end();
    for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it)
        vector[it->key].set(vm, this, it->value.getNonSparseMode());
    deallocateSparseIndexMap();

    // Store the new property into the vector.
    WriteBarrier<Unknown>& valueSlot = vector[i];
    if (!valueSlot)
        ++storage->m_numValuesInVector;
    valueSlot.set(vm, this, value);
    return true;
}

bool JSObject::putByIndexBeyondVectorLength(JSGlobalObject* globalObject, unsigned i, JSValue value, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isCopyOnWrite(indexingMode()));

    // i should be a valid array index that is outside of the current vector.
    ASSERT(i <= MAX_ARRAY_INDEX);
    
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES: {
        if (indexingShouldBeSparse(vm)) {
            RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithArrayStorage(
                globalObject, i, value, shouldThrow,
                ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm)));
        }
        if (indexIsSufficientlyBeyondLengthForSparseMap(i, 0) || i >= MIN_SPARSE_ARRAY_INDEX) {
            RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, shouldThrow, createArrayStorage(vm, 0, 0)));
        }
        if (needsSlowPutIndexing(vm)) {
            // Convert the indexing type to the SlowPutArrayStorage and retry.
            createArrayStorage(vm, i + 1, getNewVectorLength(vm, 0, 0, 0, i + 1));
            RELEASE_AND_RETURN(scope, putByIndex(this, globalObject, i, value, shouldThrow));
        }
        
        createInitialForValueAndSet(vm, i, value);
        return true;
    }
        
    case ALL_UNDECIDED_INDEXING_TYPES: {
        CRASH();
        break;
    }
        
    case ALL_INT32_INDEXING_TYPES:
        RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithoutAttributes<Int32Shape>(globalObject, i, value));
        
    case ALL_DOUBLE_INDEXING_TYPES:
        RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithoutAttributes<DoubleShape>(globalObject, i, value));
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithoutAttributes<ContiguousShape>(globalObject, i, value));
        
    case NonArrayWithSlowPutArrayStorage:
    case ArrayWithSlowPutArrayStorage: {
        // No own property present in the vector, but there might be in the sparse map!
        SparseArrayValueMap* map = arrayStorage()->m_sparseMap.get();
        bool putResult = false;
        if (!(map && map->contains(i))) {
            bool result = attemptToInterceptPutByIndexOnHole(globalObject, i, value, shouldThrow, putResult);
            RETURN_IF_EXCEPTION(scope, false);
            if (result)
                return putResult;
        }
        FALLTHROUGH;
    }

    case NonArrayWithArrayStorage:
    case ArrayWithArrayStorage:
        RELEASE_AND_RETURN(scope, putByIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, shouldThrow, arrayStorage()));
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return false;
}

bool JSObject::putDirectIndexBeyondVectorLengthWithArrayStorage(JSGlobalObject* globalObject, unsigned i, JSValue value, unsigned attributes, PutDirectIndexMode mode, ArrayStorage* storage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    // i should be a valid array index that is outside of the current vector.
    ASSERT(hasAnyArrayStorage(indexingType()));
    ASSERT(arrayStorage() == storage);
    ASSERT(i >= storage->vectorLength() || attributes);
    ASSERT(i <= MAX_ARRAY_INDEX);

    SparseArrayValueMap* map = storage->m_sparseMap.get();

    // First, handle cases where we don't currently have a sparse map.
    if (LIKELY(!map)) {
        // If the array is not extensible, we should have entered dictionary mode, and created the spare map.
        ASSERT(isStructureExtensible(vm));
    
        // Update m_length if necessary.
        if (i >= storage->length())
            storage->setLength(i + 1);

        // Check that it is sensible to still be using a vector, and then try to grow the vector.
        if (LIKELY(
                !attributes
                && (isDenseEnoughForVector(i, storage->m_numValuesInVector))
                && !indexIsSufficientlyBeyondLengthForSparseMap(i, storage->vectorLength()))
                && increaseVectorLength(vm, i + 1)) {
            // success! - reread m_storage since it has likely been reallocated, and store to the vector.
            storage = arrayStorage();
            storage->m_vector[i].set(vm, this, value);
            ++storage->m_numValuesInVector;
            return true;
        }
        // We don't want to, or can't use a vector to hold this property - allocate a sparse map & add the value.
        map = allocateSparseIndexMap(vm);
        RELEASE_AND_RETURN(scope, map->putDirect(globalObject, this, i, value, attributes, mode));
    }

    // Update m_length if necessary.
    unsigned length = storage->length();
    if (i >= length) {
        if (mode != PutDirectIndexLikePutDirect) {
            // Prohibit growing the array if length is not writable.
            if (map->lengthIsReadOnly())
                return typeError(globalObject, scope, mode == PutDirectIndexShouldThrow, ReadonlyPropertyWriteError);
            if (!isStructureExtensible(vm))
                return typeError(globalObject, scope, mode == PutDirectIndexShouldThrow, NonExtensibleObjectPropertyDefineError);
        }
        length = i + 1;
        storage->setLength(length);
    }

    // We are currently using a map - check whether we still want to be doing so.
    // We will continue  to use a sparse map if SparseMode is set, a vector would be too sparse, or if allocation fails.
    unsigned numValuesInArray = storage->m_numValuesInVector + map->size();
    if (map->sparseMode() || attributes || !isDenseEnoughForVector(length, numValuesInArray) || !increaseVectorLength(vm, length))
        RELEASE_AND_RETURN(scope, map->putDirect(globalObject, this, i, value, attributes, mode));

    // Reread m_storage after increaseVectorLength, update m_numValuesInVector.
    storage = arrayStorage();
    storage->m_numValuesInVector = numValuesInArray;

    // Copy all values from the map into the vector, and delete the map.
    WriteBarrier<Unknown>* vector = storage->m_vector;
    SparseArrayValueMap::const_iterator end = map->end();
    for (SparseArrayValueMap::const_iterator it = map->begin(); it != end; ++it)
        vector[it->key].set(vm, this, it->value.getNonSparseMode());
    deallocateSparseIndexMap();

    // Store the new property into the vector.
    WriteBarrier<Unknown>& valueSlot = vector[i];
    if (!valueSlot)
        ++storage->m_numValuesInVector;
    valueSlot.set(vm, this, value);
    return true;
}

bool JSObject::putDirectIndexSlowOrBeyondVectorLength(JSGlobalObject* globalObject, unsigned i, JSValue value, unsigned attributes, PutDirectIndexMode mode)
{
    VM& vm = globalObject->vm();
    ASSERT(!value.isCustomGetterSetter());

    if (!canDoFastPutDirectIndex(vm, this)) {
        PropertyDescriptor descriptor;
        descriptor.setDescriptor(value, attributes);
        return methodTable(vm)->defineOwnProperty(this, globalObject, Identifier::from(vm, i), descriptor, mode == PutDirectIndexShouldThrow);
    }

    // i should be a valid array index that is outside of the current vector.
    ASSERT(i <= MAX_ARRAY_INDEX);
    
    if (attributes & (PropertyAttribute::ReadOnly | PropertyAttribute::Accessor))
        notifyPresenceOfIndexedAccessors(vm);
    
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES: {
        if (indexingShouldBeSparse(vm) || attributes) {
            return putDirectIndexBeyondVectorLengthWithArrayStorage(
                globalObject, i, value, attributes, mode,
                ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm));
        }
        if (indexIsSufficientlyBeyondLengthForSparseMap(i, 0) || i >= MIN_SPARSE_ARRAY_INDEX) {
            return putDirectIndexBeyondVectorLengthWithArrayStorage(
                globalObject, i, value, attributes, mode, createArrayStorage(vm, 0, 0));
        }
        if (needsSlowPutIndexing(vm)) {
            ArrayStorage* storage = createArrayStorage(vm, i + 1, getNewVectorLength(vm, 0, 0, 0, i + 1));
            storage->m_vector[i].set(vm, this, value);
            storage->m_numValuesInVector++;
            return true;
        }
        
        createInitialForValueAndSet(vm, i, value);
        return true;
    }
        
    case ALL_UNDECIDED_INDEXING_TYPES: {
        convertUndecidedForValue(vm, value);
        // Reloop.
        return putDirectIndex(globalObject, i, value, attributes, mode);
    }
        
    case ALL_INT32_INDEXING_TYPES: {
        ASSERT(!indexingShouldBeSparse(vm));
        if (attributes)
            return putDirectIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, attributes, mode, ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm));
        if (!value.isInt32()) {
            convertInt32ForValue(vm, value);
            return putDirectIndexSlowOrBeyondVectorLength(globalObject, i, value, attributes, mode);
        }
        putByIndexBeyondVectorLengthWithoutAttributes<Int32Shape>(globalObject, i, value);
        return true;
    }
        
    case ALL_DOUBLE_INDEXING_TYPES: {
        ASSERT(!indexingShouldBeSparse(vm));
        if (attributes)
            return putDirectIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, attributes, mode, ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm));
        if (!value.isNumber()) {
            convertDoubleToContiguous(vm);
            return putDirectIndexSlowOrBeyondVectorLength(globalObject, i, value, attributes, mode);
        }
        double valueAsDouble = value.asNumber();
        if (valueAsDouble != valueAsDouble) {
            convertDoubleToContiguous(vm);
            return putDirectIndexSlowOrBeyondVectorLength(globalObject, i, value, attributes, mode);
        }
        putByIndexBeyondVectorLengthWithoutAttributes<DoubleShape>(globalObject, i, value);
        return true;
    }
        
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        ASSERT(!indexingShouldBeSparse(vm));
        if (attributes)
            return putDirectIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, attributes, mode, ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm));
        putByIndexBeyondVectorLengthWithoutAttributes<ContiguousShape>(globalObject, i, value);
        return true;
    }

    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        if (attributes)
            return putDirectIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, attributes, mode, ensureArrayStorageExistsAndEnterDictionaryIndexingMode(vm));
        return putDirectIndexBeyondVectorLengthWithArrayStorage(globalObject, i, value, attributes, mode, arrayStorage());
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

bool JSObject::putDirectNativeIntrinsicGetter(VM& vm, JSGlobalObject* globalObject, Identifier name, NativeFunction nativeFunction, Intrinsic intrinsic, unsigned attributes)
{
    JSFunction* function = JSFunction::create(vm, globalObject, 0, makeString("get ", name.string()), nativeFunction, intrinsic);
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, function, nullptr);
    return putDirectNonIndexAccessor(vm, name, accessor, attributes);
}

void JSObject::putDirectNativeIntrinsicGetterWithoutTransition(VM& vm, JSGlobalObject* globalObject, Identifier name, NativeFunction nativeFunction, Intrinsic intrinsic, unsigned attributes)
{
    JSFunction* function = JSFunction::create(vm, globalObject, 0, makeString("get ", name.string()), nativeFunction, intrinsic);
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, function, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, name, accessor, attributes);
}

bool JSObject::putDirectNativeFunction(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, unsigned functionLength, NativeFunction nativeFunction, Intrinsic intrinsic, unsigned attributes)
{
    StringImpl* name = propertyName.publicName();
    if (!name)
        name = vm.propertyNames->anonymous.impl();
    ASSERT(name);

    JSFunction* function = JSFunction::create(vm, globalObject, functionLength, name, nativeFunction, intrinsic);
    return putDirect(vm, propertyName, function, attributes);
}

bool JSObject::putDirectNativeFunction(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, unsigned functionLength, NativeFunction nativeFunction, Intrinsic intrinsic, const DOMJIT::Signature* signature, unsigned attributes)
{
    StringImpl* name = propertyName.publicName();
    if (!name)
        name = vm.propertyNames->anonymous.impl();
    ASSERT(name);

    JSFunction* function = JSFunction::create(vm, globalObject, functionLength, name, nativeFunction, intrinsic, callHostFunctionAsConstructor, signature);
    return putDirect(vm, propertyName, function, attributes);
}

void JSObject::putDirectNativeFunctionWithoutTransition(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, unsigned functionLength, NativeFunction nativeFunction, Intrinsic intrinsic, unsigned attributes)
{
    StringImpl* name = propertyName.publicName();
    if (!name)
        name = vm.propertyNames->anonymous.impl();
    ASSERT(name);
    JSFunction* function = JSFunction::create(vm, globalObject, functionLength, name, nativeFunction, intrinsic);
    putDirectWithoutTransition(vm, propertyName, function, attributes);
}

JSFunction* JSObject::putDirectBuiltinFunction(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, FunctionExecutable* functionExecutable, unsigned attributes)
{
    StringImpl* name = propertyName.publicName();
    if (!name)
        name = vm.propertyNames->anonymous.impl();
    ASSERT(name);
    JSFunction* function = JSFunction::create(vm, static_cast<FunctionExecutable*>(functionExecutable), globalObject);
    putDirect(vm, propertyName, function, attributes);
    return function;
}

JSFunction* JSObject::putDirectBuiltinFunctionWithoutTransition(VM& vm, JSGlobalObject* globalObject, const PropertyName& propertyName, FunctionExecutable* functionExecutable, unsigned attributes)
{
    JSFunction* function = JSFunction::create(vm, static_cast<FunctionExecutable*>(functionExecutable), globalObject);
    putDirectWithoutTransition(vm, propertyName, function, attributes);
    return function;
}

// NOTE: This method is for ArrayStorage vectors.
ALWAYS_INLINE unsigned JSObject::getNewVectorLength(VM& vm, unsigned indexBias, unsigned currentVectorLength, unsigned currentLength, unsigned desiredLength)
{
    ASSERT(desiredLength <= MAX_STORAGE_VECTOR_LENGTH);

    unsigned increasedLength;
    unsigned maxInitLength = std::min(currentLength, 100000U);

    if (desiredLength < maxInitLength)
        increasedLength = maxInitLength;
    else if (!currentVectorLength)
        increasedLength = std::max(desiredLength, lastArraySize);
    else {
        increasedLength = timesThreePlusOneDividedByTwo(desiredLength);
    }

    ASSERT(increasedLength >= desiredLength);

    lastArraySize = std::min(increasedLength, FIRST_ARRAY_STORAGE_VECTOR_GROW);

    return ArrayStorage::optimalVectorLength(
        indexBias, structure(vm)->outOfLineCapacity(),
        std::min(increasedLength, MAX_STORAGE_VECTOR_LENGTH));
}

ALWAYS_INLINE unsigned JSObject::getNewVectorLength(VM& vm, unsigned desiredLength)
{
    unsigned indexBias = 0;
    unsigned vectorLength = 0;
    unsigned length = 0;
    
    if (hasIndexedProperties(indexingType())) {
        if (ArrayStorage* storage = arrayStorageOrNull())
            indexBias = storage->m_indexBias;
        vectorLength = m_butterfly->vectorLength();
        length = m_butterfly->publicLength();
    }

    return getNewVectorLength(vm, indexBias, vectorLength, length, desiredLength);
}

template<IndexingType indexingShape>
unsigned JSObject::countElements(Butterfly* butterfly)
{
    unsigned numValues = 0;
    for (unsigned i = butterfly->publicLength(); i--;) {
        switch (indexingShape) {
        case Int32Shape:
        case ContiguousShape:
            if (butterfly->contiguous().at(this, i))
                numValues++;
            break;
            
        case DoubleShape: {
            double value = butterfly->contiguousDouble().at(this, i);
            if (value == value)
                numValues++;
            break;
        }
            
        default:
            CRASH();
        }
    }
    return numValues;
}

unsigned JSObject::countElements()
{
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        return 0;
        
    case ALL_INT32_INDEXING_TYPES:
        return countElements<Int32Shape>(butterfly());
        
    case ALL_DOUBLE_INDEXING_TYPES:
        return countElements<DoubleShape>(butterfly());
        
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        return countElements<ContiguousShape>(butterfly());
        
    default:
        CRASH();
        return 0;
    }
}

bool JSObject::increaseVectorLength(VM& vm, unsigned newLength)
{
    ArrayStorage* storage = arrayStorage();
    
    unsigned vectorLength = storage->vectorLength();
    unsigned availableVectorLength = storage->availableVectorLength(structure(vm), vectorLength); 
    if (availableVectorLength >= newLength) {
        // The cell was already big enough for the desired length!
        for (unsigned i = vectorLength; i < availableVectorLength; ++i)
            storage->m_vector[i].clear();
        storage->setVectorLength(availableVectorLength);
        return true;
    }
    
    // This function leaves the array in an internally inconsistent state, because it does not move any values from sparse value map
    // to the vector. Callers have to account for that, because they can do it more efficiently.
    if (newLength > MAX_STORAGE_VECTOR_LENGTH)
        return false;

    if (newLength >= MIN_SPARSE_ARRAY_INDEX
        && !isDenseEnoughForVector(newLength, storage->m_numValuesInVector))
        return false;

    unsigned indexBias = storage->m_indexBias;
    ASSERT(newLength > vectorLength);
    unsigned newVectorLength = getNewVectorLength(vm, newLength);

    // Fast case - there is no precapacity. In these cases a realloc makes sense.
    Structure* structure = this->structure(vm);
    if (LIKELY(!indexBias)) {
        DeferGC deferGC(vm.heap);
        Butterfly* newButterfly = storage->butterfly()->growArrayRight(
            vm, this, structure, structure->outOfLineCapacity(), true,
            ArrayStorage::sizeFor(vectorLength), ArrayStorage::sizeFor(newVectorLength));
        if (!newButterfly)
            return false;
        for (unsigned i = vectorLength; i < newVectorLength; ++i)
            newButterfly->arrayStorage()->m_vector[i].clear();
        newButterfly->arrayStorage()->setVectorLength(newVectorLength);
        setButterfly(vm, newButterfly);
        return true;
    }
    
    // Remove some, but not all of the precapacity. Atomic decay, & capped to not overflow array length.
    DeferGC deferGC(vm.heap);
    unsigned newIndexBias = std::min(indexBias >> 1, MAX_STORAGE_VECTOR_LENGTH - newVectorLength);
    Butterfly* newButterfly = storage->butterfly()->resizeArray(
        vm, this,
        structure->outOfLineCapacity(), true, ArrayStorage::sizeFor(vectorLength),
        newIndexBias, true, ArrayStorage::sizeFor(newVectorLength));
    if (!newButterfly)
        return false;
    for (unsigned i = vectorLength; i < newVectorLength; ++i)
        newButterfly->arrayStorage()->m_vector[i].clear();
    newButterfly->arrayStorage()->setVectorLength(newVectorLength);
    newButterfly->arrayStorage()->m_indexBias = newIndexBias;
    setButterfly(vm, newButterfly);
    return true;
}

bool JSObject::ensureLengthSlow(VM& vm, unsigned length)
{
    if (isCopyOnWrite(indexingMode())) {
        convertFromCopyOnWrite(vm);
        if (m_butterfly->vectorLength() >= length)
            return true;
    }

    Butterfly* butterfly = this->butterfly();
    
    ASSERT(length <= MAX_STORAGE_VECTOR_LENGTH);
    ASSERT(hasContiguous(indexingType()) || hasInt32(indexingType()) || hasDouble(indexingType()) || hasUndecided(indexingType()));
    ASSERT(length > butterfly->vectorLength());

    unsigned oldVectorLength = butterfly->vectorLength();
    unsigned newVectorLength;
    
    Structure* structure = this->structure(vm);
    unsigned propertyCapacity = structure->outOfLineCapacity();
    
    GCDeferralContext deferralContext(vm.heap);
    DisallowGC disallowGC;
    unsigned availableOldLength =
        Butterfly::availableContiguousVectorLength(propertyCapacity, oldVectorLength);
    Butterfly* newButterfly = nullptr;
    if (availableOldLength >= length) {
        // This is the case where someone else selected a vector length that caused internal
        // fragmentation. If we did our jobs right, this would never happen. But I bet we will mess
        // this up, so this defense should stay.
        newVectorLength = availableOldLength;
    } else {
        newVectorLength = Butterfly::optimalContiguousVectorLength(
            propertyCapacity, std::min(length * 2, MAX_STORAGE_VECTOR_LENGTH));
        butterfly = butterfly->reallocArrayRightIfPossible(
            vm, deferralContext, this, structure, propertyCapacity, true,
            oldVectorLength * sizeof(EncodedJSValue),
            newVectorLength * sizeof(EncodedJSValue));
        if (!butterfly)
            return false;
        newButterfly = butterfly;
    }

    if (hasDouble(indexingType())) {
        for (unsigned i = oldVectorLength; i < newVectorLength; ++i)
            butterfly->indexingPayload<double>()[i] = PNaN;
    } else {
        for (unsigned i = oldVectorLength; i < newVectorLength; ++i)
            butterfly->indexingPayload<WriteBarrier<Unknown>>()[i].clear();
    }

    if (newButterfly) {
        butterfly->setVectorLength(newVectorLength);
        WTF::storeStoreFence();
        m_butterfly.set(vm, this, newButterfly);
    } else {
        WTF::storeStoreFence();
        butterfly->setVectorLength(newVectorLength);
    }

    return true;
}

void JSObject::reallocateAndShrinkButterfly(VM& vm, unsigned length)
{
    ASSERT(length <= MAX_STORAGE_VECTOR_LENGTH);
    ASSERT(hasContiguous(indexingType()) || hasInt32(indexingType()) || hasDouble(indexingType()) || hasUndecided(indexingType()));
    ASSERT(m_butterfly->vectorLength() > length);
    ASSERT(m_butterfly->publicLength() >= length);
    ASSERT(!m_butterfly->indexingHeader()->preCapacity(structure(vm)));

    DeferGC deferGC(vm.heap);
    Butterfly* newButterfly = butterfly()->resizeArray(vm, this, structure(vm), 0, ArrayStorage::sizeFor(length));
    newButterfly->setVectorLength(length);
    newButterfly->setPublicLength(length);
    WTF::storeStoreFence();
    m_butterfly.set(vm, this, newButterfly);

}

Butterfly* JSObject::allocateMoreOutOfLineStorage(VM& vm, size_t oldSize, size_t newSize)
{
    ASSERT(newSize > oldSize);

    // It's important that this function not rely on structure(), for the property
    // capacity, since we might have already mutated the structure in-place.

    return Butterfly::createOrGrowPropertyStorage(butterfly(), vm, this, structure(vm), oldSize, newSize);
}

static JSCustomGetterSetterFunction* getCustomGetterSetterFunctionForGetterSetter(JSGlobalObject* globalObject, PropertyName propertyName, CustomGetterSetter* getterSetter, JSCustomGetterSetterFunction::Type type)
{
    VM& vm = globalObject->vm();
    auto key = std::make_pair(getterSetter, (int)type);
    JSCustomGetterSetterFunction* customGetterSetterFunction = vm.customGetterSetterFunctionMap.get(key);
    if (!customGetterSetterFunction) {
        customGetterSetterFunction = JSCustomGetterSetterFunction::create(vm, globalObject, getterSetter, type, propertyName.publicName());
        vm.customGetterSetterFunctionMap.set(key, customGetterSetterFunction);
    }
    return customGetterSetterFunction;
}

bool JSObject::getOwnPropertyDescriptor(JSGlobalObject* globalObject, PropertyName propertyName, PropertyDescriptor& descriptor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);

    bool result = methodTable(vm)->getOwnPropertySlot(this, globalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    if (!result)
        return false;

    if (slot.isAccessor())
        descriptor.setAccessorDescriptor(slot.getterSetter(), slot.attributes());
    else if (slot.attributes() & PropertyAttribute::CustomAccessor) {
        CustomGetterSetter* getterSetter;
        if (slot.isCustomAccessor())
            getterSetter = slot.customGetterSetter();
        else {
            ASSERT(slot.slotBase());
            JSObject* thisObject = slot.slotBase();

            JSValue maybeGetterSetter = thisObject->getDirect(vm, propertyName);
            if (!maybeGetterSetter) {
                thisObject->reifyAllStaticProperties(globalObject);
                maybeGetterSetter = thisObject->getDirect(vm, propertyName);
            }

            ASSERT(maybeGetterSetter);
            getterSetter = jsDynamicCast<CustomGetterSetter*>(vm, maybeGetterSetter);
        }
        ASSERT(getterSetter);
        if (!getterSetter)
            return false;

        descriptor.setCustomDescriptor(slot.attributes());
        if (getterSetter->getter())
            descriptor.setGetter(getCustomGetterSetterFunctionForGetterSetter(globalObject, propertyName, getterSetter, JSCustomGetterSetterFunction::Type::Getter));
        if (getterSetter->setter())
            descriptor.setSetter(getCustomGetterSetterFunctionForGetterSetter(globalObject, propertyName, getterSetter, JSCustomGetterSetterFunction::Type::Setter));
    } else {
        JSValue value = slot.getValue(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        descriptor.setDescriptor(value, slot.attributes());
    }

    return true;
}

static bool putDescriptor(JSGlobalObject* globalObject, JSObject* target, PropertyName propertyName, const PropertyDescriptor& descriptor, unsigned attributes, const PropertyDescriptor& oldDescriptor)
{
    VM& vm = globalObject->vm();
    if (descriptor.isGenericDescriptor() || descriptor.isDataDescriptor()) {
        if (descriptor.isGenericDescriptor() && oldDescriptor.isAccessorDescriptor()) {
            JSObject* getter = oldDescriptor.getterPresent() ? oldDescriptor.getterObject() : nullptr;
            JSObject* setter = oldDescriptor.setterPresent() ? oldDescriptor.setterObject() : nullptr;
            GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);
            target->putDirectAccessor(globalObject, propertyName, accessor, attributes | PropertyAttribute::Accessor);
            return true;
        }
        JSValue newValue = jsUndefined();
        if (descriptor.value())
            newValue = descriptor.value();
        else if (oldDescriptor.value())
            newValue = oldDescriptor.value();
        target->putDirect(vm, propertyName, newValue, attributes & ~PropertyAttribute::Accessor);
        if (attributes & PropertyAttribute::ReadOnly)
            target->structure(vm)->setContainsReadOnlyProperties();
        return true;
    }
    attributes &= ~PropertyAttribute::ReadOnly;

    JSObject* getter = descriptor.getterPresent()
        ? descriptor.getterObject() : oldDescriptor.getterPresent()
        ? oldDescriptor.getterObject() : nullptr;
    JSObject* setter = descriptor.setterPresent()
        ? descriptor.setterObject() : oldDescriptor.setterPresent()
        ? oldDescriptor.setterObject() : nullptr;
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);

    target->putDirectAccessor(globalObject, propertyName, accessor, attributes | PropertyAttribute::Accessor);
    return true;
}

bool JSObject::putDirectMayBeIndex(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value)
{
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return putDirectIndex(globalObject, index.value(), value);
    return putDirect(globalObject->vm(), propertyName, value);
}

// 9.1.6.3 of the spec
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-validateandapplypropertydescriptor
bool validateAndApplyPropertyDescriptor(JSGlobalObject* globalObject, JSObject* object, PropertyName propertyName, bool isExtensible,
    const PropertyDescriptor& descriptor, bool isCurrentDefined, const PropertyDescriptor& current, bool throwException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // If we have a new property we can just put it on normally
    // Step 2.
    if (!isCurrentDefined) {
        // unless extensions are prevented!
        // Step 2.a
        if (!isExtensible)
            return typeError(globalObject, scope, throwException, NonExtensibleObjectPropertyDefineError);
        if (!object)
            return true;
        // Step 2.c/d
        PropertyDescriptor oldDescriptor;
        oldDescriptor.setValue(jsUndefined());
        // FIXME: spec says to always return true here.
        return putDescriptor(globalObject, object, propertyName, descriptor, descriptor.attributes(), oldDescriptor);
    }
    // Step 3.
    if (descriptor.isEmpty())
        return true;
    // Step 4.
    bool isEqual = current.equalTo(globalObject, descriptor);
    RETURN_IF_EXCEPTION(scope, false);
    if (isEqual)
        return true;

    // Step 5.
    // Filter out invalid changes
    if (!current.configurable()) {
        if (descriptor.configurable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeConfigurabilityError);
        if (descriptor.enumerablePresent() && descriptor.enumerable() != current.enumerable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeEnumerabilityError);
    }
    
    // Step 6.
    // A generic descriptor is simply changing the attributes of an existing property
    if (descriptor.isGenericDescriptor()) {
        if (!current.attributesEqual(descriptor) && object) {
            JSCell::deleteProperty(object, globalObject, propertyName);
            RETURN_IF_EXCEPTION(scope, false);
            return putDescriptor(globalObject, object, propertyName, descriptor, descriptor.attributesOverridingCurrent(current), current);
        }
        return true;
    }
    
    // Step 7.
    // Changing between a normal property or an accessor property
    if (descriptor.isDataDescriptor() != current.isDataDescriptor()) {
        if (!current.configurable())
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);

        if (!object)
            return true;

        JSCell::deleteProperty(object, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        return putDescriptor(globalObject, object, propertyName, descriptor, descriptor.attributesOverridingCurrent(current), current);
    }

    // Step 8.
    // Changing the value and attributes of an existing property
    if (descriptor.isDataDescriptor()) {
        if (!current.configurable()) {
            if (!current.writable() && descriptor.writable())
                return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeWritabilityError);
            if (!current.writable()) {
                if (descriptor.value()) {
                    bool isSame = sameValue(globalObject, current.value(), descriptor.value());
                    RETURN_IF_EXCEPTION(scope, false);
                    if (!isSame)
                        return typeError(globalObject, scope, throwException, ReadonlyPropertyChangeError);
                }
            }
        }
        if (current.attributesEqual(descriptor) && !descriptor.value())
            return true;
        if (!object)
            return true;
        JSCell::deleteProperty(object, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, false);
        return putDescriptor(globalObject, object, propertyName, descriptor, descriptor.attributesOverridingCurrent(current), current);
    }

    // Step 9.
    // Changing the accessor functions of an existing accessor property
    ASSERT(descriptor.isAccessorDescriptor());
    if (!current.configurable()) {
        if (descriptor.setterPresent() && !(current.setterPresent() && JSValue::strictEqual(globalObject, current.setter(), descriptor.setter())))
            return typeError(globalObject, scope, throwException, "Attempting to change the setter of an unconfigurable property."_s);
        if (descriptor.getterPresent() && !(current.getterPresent() && JSValue::strictEqual(globalObject, current.getter(), descriptor.getter())))
            return typeError(globalObject, scope, throwException, "Attempting to change the getter of an unconfigurable property."_s);
        if (current.attributes() & PropertyAttribute::CustomAccessor)
            return typeError(globalObject, scope, throwException, UnconfigurablePropertyChangeAccessMechanismError);
    }

    // Step 10/11.
    if (!object)
        return true;
    JSValue accessor = object->getDirect(vm, propertyName);
    if (!accessor)
        return false;
    JSObject* getter = nullptr;
    JSObject* setter = nullptr;
    bool getterSetterChanged = false;

    if (accessor.isCustomGetterSetter()) {
        auto* customGetterSetter = jsCast<CustomGetterSetter*>(accessor);
        if (customGetterSetter->setter())
            setter = getCustomGetterSetterFunctionForGetterSetter(globalObject, propertyName, customGetterSetter, JSCustomGetterSetterFunction::Type::Setter);
        if (customGetterSetter->getter())
            getter = getCustomGetterSetterFunctionForGetterSetter(globalObject, propertyName, customGetterSetter, JSCustomGetterSetterFunction::Type::Getter);
    } else {
        ASSERT(accessor.isGetterSetter());
        auto* getterSetter = jsCast<GetterSetter*>(accessor);
        getter = getterSetter->getter();
        setter = getterSetter->setter();
    }
    if (descriptor.setterPresent()) {
        setter = descriptor.setterObject();
        getterSetterChanged = true;
    }
    if (descriptor.getterPresent()) {
        getter = descriptor.getterObject();
        getterSetterChanged = true;
    }

    if (current.attributesEqual(descriptor) && !getterSetterChanged)
        return true;

    GetterSetter* getterSetter = GetterSetter::create(vm, globalObject, getter, setter);

    JSCell::deleteProperty(object, globalObject, propertyName);
    RETURN_IF_EXCEPTION(scope, false);
    unsigned attrs = descriptor.attributesOverridingCurrent(current);
    object->putDirectAccessor(globalObject, propertyName, getterSetter, attrs | PropertyAttribute::Accessor);
    return true;
}

bool JSObject::defineOwnNonIndexProperty(JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    VM& vm  = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    // Track on the globaldata that we're in define property.
    // Currently DefineOwnProperty uses delete to remove properties when they are being replaced
    // (particularly when changing attributes), however delete won't allow non-configurable (i.e.
    // DontDelete) properties to be deleted. For now, we can use this flag to make this work.
    VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
    PropertyDescriptor current;
    bool isCurrentDefined = getOwnPropertyDescriptor(globalObject, propertyName, current);
    RETURN_IF_EXCEPTION(throwScope, false);
    bool isExtensible = this->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(throwScope, false);
    RELEASE_AND_RETURN(throwScope, validateAndApplyPropertyDescriptor(globalObject, this, propertyName, isExtensible, descriptor, isCurrentDefined, current, throwException));
}

bool JSObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    // If it's an array index, then use the indexed property storage.
    if (Optional<uint32_t> index = parseIndex(propertyName)) {
        // c. Let succeeded be the result of calling the default [[DefineOwnProperty]] internal method (8.12.9) on A passing P, Desc, and false as arguments.
        // d. Reject if succeeded is false.
        // e. If index >= oldLen
        // e.i. Set oldLenDesc.[[Value]] to index + 1.
        // e.ii. Call the default [[DefineOwnProperty]] internal method (8.12.9) on A passing "length", oldLenDesc, and false as arguments. This call will always return true.
        // f. Return true.
        return object->defineOwnIndexedProperty(globalObject, index.value(), descriptor, throwException);
    }
    
    return object->defineOwnNonIndexProperty(globalObject, propertyName, descriptor, throwException);
}

void JSObject::convertToDictionary(VM& vm)
{
    DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure(vm));
    setStructure(
        vm, Structure::toCacheableDictionaryTransition(vm, structure(vm), &deferredWatchpointFire));
}

void JSObject::shiftButterflyAfterFlattening(const GCSafeConcurrentJSLocker&, VM& vm, Structure* structure, size_t outOfLineCapacityAfter)
{
    // This could interleave visitChildren because some old structure could have been a non
    // dictionary structure. We have to be crazy careful. But, we are guaranteed to be holding
    // the structure's lock right now, and that helps a bit.

    Butterfly* oldButterfly = this->butterfly();
    size_t preCapacity;
    size_t indexingPayloadSizeInBytes;
    bool hasIndexingHeader = this->hasIndexingHeader(vm);
    if (UNLIKELY(hasIndexingHeader)) {
        preCapacity = oldButterfly->indexingHeader()->preCapacity(structure);
        indexingPayloadSizeInBytes = oldButterfly->indexingHeader()->indexingPayloadSizeInBytes(structure);
    } else {
        preCapacity = 0;
        indexingPayloadSizeInBytes = 0;
    }

    Butterfly* newButterfly = Butterfly::createUninitialized(vm, this, preCapacity, outOfLineCapacityAfter, hasIndexingHeader, indexingPayloadSizeInBytes);

    // No need to copy the precapacity.
    void* currentBase = oldButterfly->base(0, outOfLineCapacityAfter);
    void* newBase = newButterfly->base(0, outOfLineCapacityAfter);

    gcSafeMemcpy(static_cast<JSValue*>(newBase), static_cast<JSValue*>(currentBase), Butterfly::totalSize(0, outOfLineCapacityAfter, hasIndexingHeader, indexingPayloadSizeInBytes));
    
    setButterfly(vm, newButterfly);
}

uint32_t JSObject::getEnumerableLength(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    Structure* structure = object->structure(vm);
    if (structure->holesMustForwardToPrototype(vm, object))
        return 0;
    switch (object->indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        return 0;
        
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        Butterfly* butterfly = object->butterfly();
        unsigned usedLength = butterfly->publicLength();
        for (unsigned i = 0; i < usedLength; ++i) {
            if (!butterfly->contiguous().at(object, i))
                return 0;
        }
        return usedLength;
    }
        
    case ALL_DOUBLE_INDEXING_TYPES: {
        Butterfly* butterfly = object->butterfly();
        unsigned usedLength = butterfly->publicLength();
        for (unsigned i = 0; i < usedLength; ++i) {
            double value = butterfly->contiguousDouble().at(object, i);
            if (value != value)
                return 0;
        }
        return usedLength;
    }
        
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = object->m_butterfly->arrayStorage();
        if (storage->m_sparseMap.get())
            return 0;
        
        unsigned usedVectorLength = std::min(storage->length(), storage->vectorLength());
        for (unsigned i = 0; i < usedVectorLength; ++i) {
            if (!storage->m_vector[i])
                return 0;
        }
        return usedVectorLength;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

void JSObject::getStructurePropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    object->structure(vm)->getPropertyNamesFromStructure(vm, propertyNames, mode);
}

void JSObject::getGenericPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, propertyNames, EnumerationMode(mode, JSObjectPropertiesMode::Exclude));
    RETURN_IF_EXCEPTION(scope, void());

    JSValue nextProto = object->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    if (nextProto.isNull())
        return;

    JSObject* prototype = asObject(nextProto);
    while (true) {
        if (prototype->structure(vm)->typeInfo().overridesGetPropertyNames()) {
            scope.release();
            prototype->methodTable(vm)->getPropertyNames(prototype, globalObject, propertyNames, mode);
            return;
        }
        prototype->methodTable(vm)->getOwnPropertyNames(prototype, globalObject, propertyNames, mode);
        RETURN_IF_EXCEPTION(scope, void());
        nextProto = prototype->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (nextProto.isNull())
            break;
        prototype = asObject(nextProto);
    }
}

// Implements GetMethod(O, P) in section 7.3.9 of the spec.
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-getmethod
JSValue JSObject::getMethod(JSGlobalObject* globalObject, CallData& callData, const Identifier& ident, const String& errorMessage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue method = get(globalObject, ident);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!method.isCell()) {
        if (method.isUndefinedOrNull())
            return jsUndefined();

        throwVMTypeError(globalObject, scope, errorMessage);
        return jsUndefined();
    }

    callData = JSC::getCallData(vm, method);
    if (callData.type == CallData::Type::None) {
        throwVMTypeError(globalObject, scope, errorMessage);
        return jsUndefined();
    }

    return method;
}

bool JSObject::anyObjectInChainMayInterceptIndexedAccesses(VM& vm) const
{
    for (const JSObject* current = this; ;) {
        if (current->structure(vm)->mayInterceptIndexedAccesses())
            return true;
        
        JSValue prototype = current->getPrototypeDirect(vm);
        if (prototype.isNull())
            return false;
        
        current = asObject(prototype);
    }
}

bool JSObject::prototypeChainMayInterceptStoreTo(VM& vm, PropertyName propertyName)
{
    if (parseIndex(propertyName))
        return anyObjectInChainMayInterceptIndexedAccesses(vm);
    
    for (JSObject* current = this; ;) {
        JSValue prototype = current->getPrototypeDirect(vm);
        if (prototype.isNull())
            return false;
        
        current = asObject(prototype);
        
        unsigned attributes;
        PropertyOffset offset = current->structure(vm)->get(vm, propertyName, attributes);
        if (!JSC::isValidOffset(offset))
            continue;
        
        if (attributes & (PropertyAttribute::ReadOnly | PropertyAttribute::Accessor))
            return true;
        
        return false;
    }
}

bool JSObject::needsSlowPutIndexing(VM& vm) const
{
    return anyObjectInChainMayInterceptIndexedAccesses(vm) || globalObject(vm)->isHavingABadTime();
}

NonPropertyTransition JSObject::suggestedArrayStorageTransition(VM& vm) const
{
    if (needsSlowPutIndexing(vm))
        return NonPropertyTransition::AllocateSlowPutArrayStorage;
    
    return NonPropertyTransition::AllocateArrayStorage;
}

} // namespace JSC
