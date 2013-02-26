/*
 * Copyright (C) 2008, 2009, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Structure.h"

#include "CodeBlock.h"
#include "JSObject.h"
#include "JSPropertyNameIterator.h"
#include "Lookup.h"
#include "PropertyNameArray.h"
#include "StructureChain.h"
#include "StructureRareDataInlines.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

#define DUMP_STRUCTURE_ID_STATISTICS 0

#ifndef NDEBUG
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#else
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#endif

using namespace std;
using namespace WTF;

#if DUMP_PROPERTYMAP_STATS

int numProbes;
int numCollisions;
int numRehashes;
int numRemoves;

#endif

namespace JSC {

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<Structure*>& liveStructureSet = *(new HashSet<Structure*>);
#endif

bool StructureTransitionTable::contains(StringImpl* rep, unsigned attributes) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = singleTransition();
        return transition && transition->m_nameInPrevious == rep && transition->m_attributesInPrevious == attributes;
    }
    return map()->get(make_pair(rep, attributes));
}

inline Structure* StructureTransitionTable::get(StringImpl* rep, unsigned attributes) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = singleTransition();
        return (transition && transition->m_nameInPrevious == rep && transition->m_attributesInPrevious == attributes) ? transition : 0;
    }
    return map()->get(make_pair(rep, attributes));
}

inline void StructureTransitionTable::add(JSGlobalData& globalData, Structure* structure)
{
    if (isUsingSingleSlot()) {
        Structure* existingTransition = singleTransition();

        // This handles the first transition being added.
        if (!existingTransition) {
            setSingleTransition(globalData, structure);
            return;
        }

        // This handles the second transition being added
        // (or the first transition being despecified!)
        setMap(new TransitionMap());
        add(globalData, existingTransition);
    }

    // Add the structure to the map.

    // Newer versions of the STL have an std::make_pair function that takes rvalue references.
    // When either of the parameters are bitfields, the C++ compiler will try to bind them as lvalues, which is invalid. To work around this, use unary "+" to make the parameter an rvalue.
    // See https://bugs.webkit.org/show_bug.cgi?id=59261 for more details
    map()->set(make_pair(structure->m_nameInPrevious, +structure->m_attributesInPrevious), structure);
}

void Structure::dumpStatistics()
{
#if DUMP_STRUCTURE_ID_STATISTICS
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned numberWithPropertyMaps = 0;
    unsigned totalPropertyMapsSize = 0;

    HashSet<Structure*>::const_iterator end = liveStructureSet.end();
    for (HashSet<Structure*>::const_iterator it = liveStructureSet.begin(); it != end; ++it) {
        Structure* structure = *it;

        switch (structure->m_transitionTable.size()) {
            case 0:
                ++numberLeaf;
                if (!structure->previousID())
                    ++numberSingletons;
                break;

            case 1:
                ++numberUsingSingleSlot;
                break;
        }

        if (structure->m_propertyTable) {
            ++numberWithPropertyMaps;
            totalPropertyMapsSize += structure->m_propertyTable->sizeInMemory();
        }
    }

    dataLogF("Number of live Structures: %d\n", liveStructureSet.size());
    dataLogF("Number of Structures using the single item optimization for transition map: %d\n", numberUsingSingleSlot);
    dataLogF("Number of Structures that are leaf nodes: %d\n", numberLeaf);
    dataLogF("Number of Structures that singletons: %d\n", numberSingletons);
    dataLogF("Number of Structures with PropertyMaps: %d\n", numberWithPropertyMaps);

    dataLogF("Size of a single Structures: %d\n", static_cast<unsigned>(sizeof(Structure)));
    dataLogF("Size of sum of all property maps: %d\n", totalPropertyMapsSize);
    dataLogF("Size of average of all property maps: %f\n", static_cast<double>(totalPropertyMapsSize) / static_cast<double>(liveStructureSet.size()));
#else
    dataLogF("Dumping Structure statistics is not enabled.\n");
#endif
}

Structure::Structure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
    : JSCell(globalData, globalData.structureStructure.get())
    , m_typeInfo(typeInfo)
    , m_indexingType(indexingType)
    , m_globalObject(globalData, this, globalObject, WriteBarrier<JSGlobalObject>::MayBeNull)
    , m_prototype(globalData, this, prototype)
    , m_classInfo(classInfo)
    , m_transitionWatchpointSet(InitializedWatching)
    , m_inlineCapacity(inlineCapacity)
    , m_offset(invalidOffset)
    , m_dictionaryKind(NoneDictionaryKind)
    , m_isPinnedPropertyTable(false)
    , m_hasGetterSetterProperties(false)
    , m_hasReadOnlyOrGetterSetterPropertiesExcludingProto(false)
    , m_hasNonEnumerableProperties(false)
    , m_attributesInPrevious(0)
    , m_specificFunctionThrashCount(0)
    , m_preventExtensions(false)
    , m_didTransition(false)
    , m_staticFunctionReified(false)
{
    ASSERT(inlineCapacity <= JSFinalObject::maxInlineCapacity());
    ASSERT(static_cast<PropertyOffset>(inlineCapacity) < firstOutOfLineOffset);
    ASSERT(!typeInfo.structureHasRareData());
}

const ClassInfo Structure::s_info = { "Structure", 0, 0, 0, CREATE_METHOD_TABLE(Structure) };

Structure::Structure(JSGlobalData& globalData)
    : JSCell(CreatingEarlyCell)
    , m_typeInfo(CompoundType, OverridesVisitChildren)
    , m_indexingType(0)
    , m_prototype(globalData, this, jsNull())
    , m_classInfo(&s_info)
    , m_transitionWatchpointSet(InitializedWatching)
    , m_inlineCapacity(0)
    , m_offset(invalidOffset)
    , m_dictionaryKind(NoneDictionaryKind)
    , m_isPinnedPropertyTable(false)
    , m_hasGetterSetterProperties(false)
    , m_hasReadOnlyOrGetterSetterPropertiesExcludingProto(false)
    , m_hasNonEnumerableProperties(false)
    , m_attributesInPrevious(0)
    , m_specificFunctionThrashCount(0)
    , m_preventExtensions(false)
    , m_didTransition(false)
    , m_staticFunctionReified(false)
{
}

Structure::Structure(JSGlobalData& globalData, const Structure* previous)
    : JSCell(globalData, globalData.structureStructure.get())
    , m_typeInfo(previous->typeInfo())
    , m_indexingType(previous->indexingTypeIncludingHistory())
    , m_prototype(globalData, this, previous->storedPrototype())
    , m_classInfo(previous->m_classInfo)
    , m_transitionWatchpointSet(InitializedWatching)
    , m_inlineCapacity(previous->m_inlineCapacity)
    , m_offset(invalidOffset)
    , m_dictionaryKind(previous->m_dictionaryKind)
    , m_isPinnedPropertyTable(false)
    , m_hasGetterSetterProperties(previous->m_hasGetterSetterProperties)
    , m_hasReadOnlyOrGetterSetterPropertiesExcludingProto(previous->m_hasReadOnlyOrGetterSetterPropertiesExcludingProto)
    , m_hasNonEnumerableProperties(previous->m_hasNonEnumerableProperties)
    , m_attributesInPrevious(0)
    , m_specificFunctionThrashCount(previous->m_specificFunctionThrashCount)
    , m_preventExtensions(previous->m_preventExtensions)
    , m_didTransition(true)
    , m_staticFunctionReified(previous->m_staticFunctionReified)
{
    previous->notifyTransitionFromThisStructure();
    if (previous->m_globalObject)
        m_globalObject.set(globalData, this, previous->m_globalObject.get());
}

void Structure::destroy(JSCell* cell)
{
    static_cast<Structure*>(cell)->Structure::~Structure();
}

void Structure::materializePropertyMap(JSGlobalData& globalData)
{
    ASSERT(structure()->classInfo() == &s_info);
    ASSERT(!m_propertyTable);

    Vector<Structure*, 8> structures;
    structures.append(this);

    Structure* structure = this;

    // Search for the last Structure with a property table.
    while ((structure = structure->previousID())) {
        if (structure->m_isPinnedPropertyTable) {
            ASSERT(structure->m_propertyTable);
            ASSERT(!structure->previousID());

            m_propertyTable.set(globalData, this, structure->m_propertyTable->copy(globalData, 0, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity)));
            break;
        }

        structures.append(structure);
    }

    if (!m_propertyTable)
        createPropertyMap(globalData, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity));

    for (ptrdiff_t i = structures.size() - 1; i >= 0; --i) {
        structure = structures[i];
        if (!structure->m_nameInPrevious)
            continue;
        PropertyMapEntry entry(globalData, this, structure->m_nameInPrevious.get(), structure->m_offset, structure->m_attributesInPrevious, structure->m_specificValueInPrevious.get());
        m_propertyTable->add(entry, m_offset, PropertyTable::PropertyOffsetMustNotChange);
    }
    
    checkOffsetConsistency();
}

inline size_t nextOutOfLineStorageCapacity(size_t currentCapacity)
{
    if (!currentCapacity)
        return initialOutOfLineCapacity;
    return currentCapacity * outOfLineGrowthFactor;
}

size_t Structure::suggestedNewOutOfLineStorageCapacity()
{
    return nextOutOfLineStorageCapacity(outOfLineCapacity());
}
 
void Structure::despecifyDictionaryFunction(JSGlobalData& globalData, PropertyName propertyName)
{
    StringImpl* rep = propertyName.uid();

    materializePropertyMapIfNecessary(globalData);

    ASSERT(isDictionary());
    ASSERT(m_propertyTable);

    PropertyMapEntry* entry = m_propertyTable->find(rep).first;
    ASSERT(entry);
    entry->specificValue.clear();
}

Structure* Structure::addPropertyTransitionToExistingStructure(Structure* structure, PropertyName propertyName, unsigned attributes, JSCell* specificValue, PropertyOffset& offset)
{
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());

    if (Structure* existingTransition = structure->m_transitionTable.get(propertyName.uid(), attributes)) {
        JSCell* specificValueInPrevious = existingTransition->m_specificValueInPrevious.get();
        if (specificValueInPrevious && specificValueInPrevious != specificValue)
            return 0;
        validateOffset(existingTransition->m_offset, existingTransition->inlineCapacity());
        offset = existingTransition->m_offset;
        return existingTransition;
    }

    return 0;
}

bool Structure::anyObjectInChainMayInterceptIndexedAccesses() const
{
    for (const Structure* current = this; ;) {
        if (current->mayInterceptIndexedAccesses())
            return true;
        
        JSValue prototype = current->storedPrototype();
        if (prototype.isNull())
            return false;
        
        current = asObject(prototype)->structure();
    }
}

bool Structure::needsSlowPutIndexing() const
{
    return anyObjectInChainMayInterceptIndexedAccesses()
        || globalObject()->isHavingABadTime();
}

NonPropertyTransition Structure::suggestedArrayStorageTransition() const
{
    if (needsSlowPutIndexing())
        return AllocateSlowPutArrayStorage;
    
    return AllocateArrayStorage;
}

Structure* Structure::addPropertyTransition(JSGlobalData& globalData, Structure* structure, PropertyName propertyName, unsigned attributes, JSCell* specificValue, PropertyOffset& offset)
{
    // If we have a specific function, we may have got to this point if there is
    // already a transition with the correct property name and attributes, but
    // specialized to a different function.  In this case we just want to give up
    // and despecialize the transition.
    // In this case we clear the value of specificFunction which will result
    // in us adding a non-specific transition, and any subsequent lookup in
    // Structure::addPropertyTransitionToExistingStructure will just use that.
    if (specificValue && structure->m_transitionTable.contains(propertyName.uid(), attributes))
        specificValue = 0;

    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());
    ASSERT(!Structure::addPropertyTransitionToExistingStructure(structure, propertyName, attributes, specificValue, offset));
    
    if (structure->m_specificFunctionThrashCount == maxSpecificFunctionThrashCount)
        specificValue = 0;

    if (structure->transitionCount() > s_maxTransitionLength) {
        Structure* transition = toCacheableDictionaryTransition(globalData, structure);
        ASSERT(structure != transition);
        offset = transition->putSpecificValue(globalData, propertyName, attributes, specificValue);
        return transition;
    }
    
    Structure* transition = create(globalData, structure);

    transition->m_cachedPrototypeChain.setMayBeNull(globalData, transition, structure->m_cachedPrototypeChain.get());
    transition->setPreviousID(globalData, transition, structure);
    transition->m_nameInPrevious = propertyName.uid();
    transition->m_attributesInPrevious = attributes;
    transition->m_specificValueInPrevious.setMayBeNull(globalData, transition, specificValue);

    if (structure->m_propertyTable) {
        structure->checkOffsetConsistency();
        if (structure->m_isPinnedPropertyTable)
            transition->m_propertyTable.set(globalData, transition, structure->m_propertyTable->copy(globalData, transition, structure->m_propertyTable->size() + 1));
        else {
            transition->m_propertyTable.set(globalData, transition, structure->m_propertyTable.get());
            structure->m_propertyTable.clear();
        }
    } else {
        if (structure->previousID())
            transition->materializePropertyMap(globalData);
        else
            transition->createPropertyMap(globalData);
    }
    transition->m_offset = structure->m_offset;

    offset = transition->putSpecificValue(globalData, propertyName, attributes, specificValue);

    checkOffset(transition->m_offset, transition->inlineCapacity());
    structure->m_transitionTable.add(globalData, transition);
    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::removePropertyTransition(JSGlobalData& globalData, Structure* structure, PropertyName propertyName, PropertyOffset& offset)
{
    ASSERT(!structure->isUncacheableDictionary());

    Structure* transition = toUncacheableDictionaryTransition(globalData, structure);

    offset = transition->remove(propertyName);

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::changePrototypeTransition(JSGlobalData& globalData, Structure* structure, JSValue prototype)
{
    Structure* transition = create(globalData, structure);

    transition->m_prototype.set(globalData, transition, prototype);

    structure->materializePropertyMapIfNecessary(globalData);
    transition->m_propertyTable.set(globalData, transition, structure->copyPropertyTableForPinning(globalData, transition));
    transition->m_offset = structure->m_offset;
    transition->pin();

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::despecifyFunctionTransition(JSGlobalData& globalData, Structure* structure, PropertyName replaceFunction)
{
    ASSERT(structure->m_specificFunctionThrashCount < maxSpecificFunctionThrashCount);
    Structure* transition = create(globalData, structure);

    ++transition->m_specificFunctionThrashCount;

    structure->materializePropertyMapIfNecessary(globalData);
    transition->m_propertyTable.set(globalData, transition, structure->copyPropertyTableForPinning(globalData, transition));
    transition->m_offset = structure->m_offset;
    transition->pin();

    if (transition->m_specificFunctionThrashCount == maxSpecificFunctionThrashCount)
        transition->despecifyAllFunctions(globalData);
    else {
        bool removed = transition->despecifyFunction(globalData, replaceFunction);
        ASSERT_UNUSED(removed, removed);
    }

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::attributeChangeTransition(JSGlobalData& globalData, Structure* structure, PropertyName propertyName, unsigned attributes)
{
    if (!structure->isUncacheableDictionary()) {
        Structure* transition = create(globalData, structure);

        structure->materializePropertyMapIfNecessary(globalData);
        transition->m_propertyTable.set(globalData, transition, structure->copyPropertyTableForPinning(globalData, transition));
        transition->m_offset = structure->m_offset;
        transition->pin();
        
        structure = transition;
    }

    ASSERT(structure->m_propertyTable);
    PropertyMapEntry* entry = structure->m_propertyTable->find(propertyName.uid()).first;
    ASSERT(entry);
    entry->attributes = attributes;

    structure->checkOffsetConsistency();
    return structure;
}

Structure* Structure::toDictionaryTransition(JSGlobalData& globalData, Structure* structure, DictionaryKind kind)
{
    ASSERT(!structure->isUncacheableDictionary());
    
    Structure* transition = create(globalData, structure);

    structure->materializePropertyMapIfNecessary(globalData);
    transition->m_propertyTable.set(globalData, transition, structure->copyPropertyTableForPinning(globalData, transition));
    transition->m_offset = structure->m_offset;
    transition->m_dictionaryKind = kind;
    transition->pin();

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::toCacheableDictionaryTransition(JSGlobalData& globalData, Structure* structure)
{
    return toDictionaryTransition(globalData, structure, CachedDictionaryKind);
}

Structure* Structure::toUncacheableDictionaryTransition(JSGlobalData& globalData, Structure* structure)
{
    return toDictionaryTransition(globalData, structure, UncachedDictionaryKind);
}

// In future we may want to cache this transition.
Structure* Structure::sealTransition(JSGlobalData& globalData, Structure* structure)
{
    Structure* transition = preventExtensionsTransition(globalData, structure);

    if (transition->m_propertyTable) {
        PropertyTable::iterator end = transition->m_propertyTable->end();
        for (PropertyTable::iterator iter = transition->m_propertyTable->begin(); iter != end; ++iter)
            iter->attributes |= DontDelete;
    }

    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this transition.
Structure* Structure::freezeTransition(JSGlobalData& globalData, Structure* structure)
{
    Structure* transition = preventExtensionsTransition(globalData, structure);

    if (transition->m_propertyTable) {
        PropertyTable::iterator iter = transition->m_propertyTable->begin();
        PropertyTable::iterator end = transition->m_propertyTable->end();
        if (iter != end)
            transition->m_hasReadOnlyOrGetterSetterPropertiesExcludingProto = true;
        for (; iter != end; ++iter)
            iter->attributes |= iter->attributes & Accessor ? DontDelete : (DontDelete | ReadOnly);
    }

    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this transition.
Structure* Structure::preventExtensionsTransition(JSGlobalData& globalData, Structure* structure)
{
    Structure* transition = create(globalData, structure);

    // Don't set m_offset, as one can not transition to this.

    structure->materializePropertyMapIfNecessary(globalData);
    transition->m_propertyTable.set(globalData, transition, structure->copyPropertyTableForPinning(globalData, transition));
    transition->m_offset = structure->m_offset;
    transition->m_preventExtensions = true;
    transition->pin();

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::nonPropertyTransition(JSGlobalData& globalData, Structure* structure, NonPropertyTransition transitionKind)
{
    unsigned attributes = toAttributes(transitionKind);
    IndexingType indexingType = newIndexingType(structure->indexingTypeIncludingHistory(), transitionKind);
    
    if (JSGlobalObject* globalObject = structure->m_globalObject.get()) {
        if (globalObject->isOriginalArrayStructure(structure)) {
            Structure* result = globalObject->originalArrayStructureForIndexingType(indexingType);
            if (result->indexingTypeIncludingHistory() == indexingType) {
                structure->notifyTransitionFromThisStructure();
                return result;
            }
        }
    }
    
    if (Structure* existingTransition = structure->m_transitionTable.get(0, attributes)) {
        ASSERT(existingTransition->m_attributesInPrevious == attributes);
        ASSERT(existingTransition->indexingTypeIncludingHistory() == indexingType);
        return existingTransition;
    }
    
    Structure* transition = create(globalData, structure);
    transition->setPreviousID(globalData, transition, structure);
    transition->m_attributesInPrevious = attributes;
    transition->m_indexingType = indexingType;
    transition->m_offset = structure->m_offset;
    checkOffset(transition->m_offset, transition->inlineCapacity());
    
    if (structure->m_propertyTable) {
        structure->checkOffsetConsistency();
        if (structure->m_isPinnedPropertyTable)
            transition->m_propertyTable.set(globalData, transition, structure->m_propertyTable->copy(globalData, transition, structure->m_propertyTable->size() + 1));
        else {
            transition->m_propertyTable.set(globalData, transition, structure->m_propertyTable.get());
            structure->m_propertyTable.clear();
        }
    } else {
        if (structure->previousID())
            transition->materializePropertyMap(globalData);
        else
            transition->createPropertyMap(globalData);
    }
    
    structure->m_transitionTable.add(globalData, transition);
    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this property.
bool Structure::isSealed(JSGlobalData& globalData)
{
    if (isExtensible())
        return false;

    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return true;

    PropertyTable::iterator end = m_propertyTable->end();
    for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter) {
        if ((iter->attributes & DontDelete) != DontDelete)
            return false;
    }
    return true;
}

// In future we may want to cache this property.
bool Structure::isFrozen(JSGlobalData& globalData)
{
    if (isExtensible())
        return false;

    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return true;

    PropertyTable::iterator end = m_propertyTable->end();
    for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter) {
        if (!(iter->attributes & DontDelete))
            return false;
        if (!(iter->attributes & (ReadOnly | Accessor)))
            return false;
    }
    return true;
}

Structure* Structure::flattenDictionaryStructure(JSGlobalData& globalData, JSObject* object)
{
    checkOffsetConsistency();
    ASSERT(isDictionary());
    if (isUncacheableDictionary()) {
        ASSERT(m_propertyTable);

        size_t propertyCount = m_propertyTable->size();

        // Holds our values compacted by insertion order.
        Vector<JSValue> values(propertyCount);

        // Copies out our values from their hashed locations, compacting property table offsets as we go.
        unsigned i = 0;
        PropertyTable::iterator end = m_propertyTable->end();
        m_offset = invalidOffset;
        for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter, ++i) {
            values[i] = object->getDirect(iter->offset);
            m_offset = iter->offset = offsetForPropertyNumber(i, m_inlineCapacity);
        }
        
        // Copies in our values to their compacted locations.
        for (unsigned i = 0; i < propertyCount; i++)
            object->putDirect(globalData, offsetForPropertyNumber(i, m_inlineCapacity), values[i]);

        m_propertyTable->clearDeletedOffsets();
        checkOffsetConsistency();
    }

    m_dictionaryKind = NoneDictionaryKind;
    return this;
}

PropertyOffset Structure::addPropertyWithoutTransition(JSGlobalData& globalData, PropertyName propertyName, unsigned attributes, JSCell* specificValue)
{
    ASSERT(!enumerationCache());

    if (m_specificFunctionThrashCount == maxSpecificFunctionThrashCount)
        specificValue = 0;

    materializePropertyMapIfNecessaryForPinning(globalData);
    
    pin();

    return putSpecificValue(globalData, propertyName, attributes, specificValue);
}

PropertyOffset Structure::removePropertyWithoutTransition(JSGlobalData& globalData, PropertyName propertyName)
{
    ASSERT(isUncacheableDictionary());
    ASSERT(!enumerationCache());

    materializePropertyMapIfNecessaryForPinning(globalData);

    pin();
    return remove(propertyName);
}

void Structure::pin()
{
    ASSERT(m_propertyTable);
    m_isPinnedPropertyTable = true;
    clearPreviousID();
    m_nameInPrevious.clear();
}

void Structure::allocateRareData(JSGlobalData& globalData)
{
    ASSERT(!typeInfo().structureHasRareData());
    StructureRareData* rareData = StructureRareData::create(globalData, previous());
    m_typeInfo = TypeInfo(typeInfo().type(), typeInfo().flags() | StructureHasRareData);
    m_previousOrRareData.set(globalData, this, rareData);
}

void Structure::cloneRareDataFrom(JSGlobalData& globalData, const Structure* other)
{
    ASSERT(other->typeInfo().structureHasRareData());
    StructureRareData* newRareData = StructureRareData::clone(globalData, other->rareData());
    m_previousOrRareData.set(globalData, this, newRareData);
}

#if DUMP_PROPERTYMAP_STATS

struct PropertyMapStatisticsExitLogger {
    ~PropertyMapStatisticsExitLogger();
};

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    dataLogF("\nJSC::PropertyMap statistics\n\n");
    dataLogF("%d probes\n", numProbes);
    dataLogF("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
    dataLogF("%d rehashes\n", numRehashes);
    dataLogF("%d removes\n", numRemoves);
}

#endif

#if !DO_PROPERTYMAP_CONSTENCY_CHECK

inline void Structure::checkConsistency()
{
    checkOffsetConsistency();
}

#endif

PropertyTable* Structure::copyPropertyTable(JSGlobalData& globalData, Structure* owner)
{
    if (!m_propertyTable)
        return 0;
    return PropertyTable::clone(globalData, owner, *m_propertyTable.get());
}

PropertyTable* Structure::copyPropertyTableForPinning(JSGlobalData& globalData, Structure* owner)
{
    if (m_propertyTable)
        return PropertyTable::clone(globalData, owner, *m_propertyTable.get());
    return PropertyTable::create(globalData, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity));
}

PropertyOffset Structure::get(JSGlobalData& globalData, PropertyName propertyName, unsigned& attributes, JSCell*& specificValue)
{
    ASSERT(structure()->classInfo() == &s_info);

    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return invalidOffset;

    PropertyMapEntry* entry = m_propertyTable->find(propertyName.uid()).first;
    if (!entry)
        return invalidOffset;

    attributes = entry->attributes;
    specificValue = entry->specificValue.get();
    return entry->offset;
}

bool Structure::despecifyFunction(JSGlobalData& globalData, PropertyName propertyName)
{
    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return false;

    PropertyMapEntry* entry = m_propertyTable->find(propertyName.uid()).first;
    if (!entry)
        return false;

    ASSERT(entry->specificValue);
    entry->specificValue.clear();
    return true;
}

void Structure::despecifyAllFunctions(JSGlobalData& globalData)
{
    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return;

    PropertyTable::iterator end = m_propertyTable->end();
    for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter)
        iter->specificValue.clear();
}

PropertyOffset Structure::putSpecificValue(JSGlobalData& globalData, PropertyName propertyName, unsigned attributes, JSCell* specificValue)
{
    ASSERT(!JSC::isValidOffset(get(globalData, propertyName)));

    checkConsistency();
    if (attributes & DontEnum)
        m_hasNonEnumerableProperties = true;

    StringImpl* rep = propertyName.uid();

    if (!m_propertyTable)
        createPropertyMap(globalData);

    PropertyOffset newOffset = m_propertyTable->nextOffset(m_inlineCapacity);

    m_propertyTable->add(PropertyMapEntry(globalData, this, rep, newOffset, attributes, specificValue), m_offset, PropertyTable::PropertyOffsetMayChange);
    
    checkConsistency();
    return newOffset;
}

PropertyOffset Structure::remove(PropertyName propertyName)
{
    checkConsistency();

    StringImpl* rep = propertyName.uid();

    if (!m_propertyTable)
        return invalidOffset;

    PropertyTable::find_iterator position = m_propertyTable->find(rep);
    if (!position.first)
        return invalidOffset;

    PropertyOffset offset = position.first->offset;

    m_propertyTable->remove(position);
    m_propertyTable->addDeletedOffset(offset);

    checkConsistency();
    return offset;
}

void Structure::createPropertyMap(JSGlobalData& globalData, unsigned capacity)
{
    ASSERT(!m_propertyTable);

    checkConsistency();
    m_propertyTable.set(globalData, this, PropertyTable::create(globalData, capacity));
}

void Structure::getPropertyNamesFromStructure(JSGlobalData& globalData, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    materializePropertyMapIfNecessary(globalData);
    if (!m_propertyTable)
        return;

    bool knownUnique = !propertyNames.size();

    PropertyTable::iterator end = m_propertyTable->end();
    for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter) {
        ASSERT(m_hasNonEnumerableProperties || !(iter->attributes & DontEnum));
        if (iter->key->isIdentifier() && (!(iter->attributes & DontEnum) || mode == IncludeDontEnumProperties)) {
            if (knownUnique)
                propertyNames.addKnownUnique(iter->key);
            else
                propertyNames.add(iter->key);
        }
    }
}

JSValue Structure::prototypeForLookup(CodeBlock* codeBlock) const
{
    return prototypeForLookup(codeBlock->globalObject());
}

void Structure::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Structure* thisObject = jsCast<Structure*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    JSCell::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_globalObject);
    if (!thisObject->isObject())
        thisObject->m_cachedPrototypeChain.clear();
    else {
        visitor.append(&thisObject->m_prototype);
        visitor.append(&thisObject->m_cachedPrototypeChain);
    }
    visitor.append(&thisObject->m_previousOrRareData);
    visitor.append(&thisObject->m_specificValueInPrevious);

    if (thisObject->m_isPinnedPropertyTable) {
        ASSERT(thisObject->m_propertyTable);
        visitor.append(&thisObject->m_propertyTable);
    } else if (thisObject->m_propertyTable)
        thisObject->m_propertyTable.clear();
}

bool Structure::prototypeChainMayInterceptStoreTo(JSGlobalData& globalData, PropertyName propertyName)
{
    unsigned i = propertyName.asIndex();
    if (i != PropertyName::NotAnIndex)
        return anyObjectInChainMayInterceptIndexedAccesses();
    
    for (Structure* current = this; ;) {
        JSValue prototype = current->storedPrototype();
        if (prototype.isNull())
            return false;
        
        current = prototype.asCell()->structure();
        
        unsigned attributes;
        JSCell* specificValue;
        PropertyOffset offset = current->get(globalData, propertyName, attributes, specificValue);
        if (!JSC::isValidOffset(offset))
            continue;
        
        if (attributes & (ReadOnly | Accessor))
            return true;
        
        return false;
    }
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void PropertyTable::checkConsistency()
{
    checkOffsetConsistency();
    ASSERT(m_indexSize >= PropertyTable::MinimumTableSize);
    ASSERT(m_indexMask);
    ASSERT(m_indexSize == m_indexMask + 1);
    ASSERT(!(m_indexSize & m_indexMask));

    ASSERT(m_keyCount <= m_indexSize / 2);
    ASSERT(m_keyCount + m_deletedCount <= m_indexSize / 2);
    ASSERT(m_deletedCount <= m_indexSize / 4);

    unsigned indexCount = 0;
    unsigned deletedIndexCount = 0;
    for (unsigned a = 0; a != m_indexSize; ++a) {
        unsigned entryIndex = m_index[a];
        if (entryIndex == PropertyTable::EmptyEntryIndex)
            continue;
        if (entryIndex == deletedEntryIndex()) {
            ++deletedIndexCount;
            continue;
        }
        ASSERT(entryIndex < deletedEntryIndex());
        ASSERT(entryIndex - 1 <= usedCount());
        ++indexCount;

        for (unsigned b = a + 1; b != m_indexSize; ++b)
            ASSERT(m_index[b] != entryIndex);
    }
    ASSERT(indexCount == m_keyCount);
    ASSERT(deletedIndexCount == m_deletedCount);

    ASSERT(!table()[deletedEntryIndex() - 1].key);

    unsigned nonEmptyEntryCount = 0;
    for (unsigned c = 0; c < usedCount(); ++c) {
        StringImpl* rep = table()[c].key;
        if (rep == PROPERTY_MAP_DELETED_ENTRY_KEY)
            continue;
        ++nonEmptyEntryCount;
        unsigned i = rep->existingHash();
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_index[i & m_indexMask];
            ASSERT(entryIndex != PropertyTable::EmptyEntryIndex);
            if (rep == table()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(rep->existingHash());
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_keyCount);
}

void Structure::checkConsistency()
{
    if (!m_propertyTable)
        return;

    if (!m_hasNonEnumerableProperties) {
        PropertyTable::iterator end = m_propertyTable->end();
        for (PropertyTable::iterator iter = m_propertyTable->begin(); iter != end; ++iter) {
            ASSERT(!(iter->attributes & DontEnum));
        }
    }

    m_propertyTable->checkConsistency();
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

} // namespace JSC
