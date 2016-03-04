/*
 * Copyright (C) 2008, 2009, 2013-2015 Apple Inc. All rights reserved.
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
#include "Structure.h"

#include "CodeBlock.h"
#include "DumpContext.h"
#include "JSCInlines.h"
#include "JSObject.h"
#include "JSPropertyNameEnumerator.h"
#include "Lookup.h"
#include "PropertyMapHashTable.h"
#include "PropertyNameArray.h"
#include "StructureChain.h"
#include "StructureRareDataInlines.h"
#include "WeakGCMapInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>
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

namespace JSC {

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<Structure*>& liveStructureSet = *(new HashSet<Structure*>);
#endif

class SingleSlotTransitionWeakOwner final : public WeakHandleOwner {
    void finalize(Handle<Unknown>, void* context) override
    {
        StructureTransitionTable* table = reinterpret_cast<StructureTransitionTable*>(context);
        ASSERT(table->isUsingSingleSlot());
        WeakSet::deallocate(table->weakImpl());
        table->m_data = StructureTransitionTable::UsingSingleSlotFlag;
    }
};

static SingleSlotTransitionWeakOwner& singleSlotTransitionWeakOwner()
{
    static NeverDestroyed<SingleSlotTransitionWeakOwner> owner;
    return owner;
}

inline Structure* StructureTransitionTable::singleTransition() const
{
    ASSERT(isUsingSingleSlot());
    if (WeakImpl* impl = this->weakImpl()) {
        if (impl->state() == WeakImpl::Live)
            return jsCast<Structure*>(impl->jsValue().asCell());
    }
    return nullptr;
}

inline void StructureTransitionTable::setSingleTransition(Structure* structure)
{
    ASSERT(isUsingSingleSlot());
    if (WeakImpl* impl = this->weakImpl())
        WeakSet::deallocate(impl);
    WeakImpl* impl = WeakSet::allocate(structure, &singleSlotTransitionWeakOwner(), this);
    m_data = reinterpret_cast<intptr_t>(impl) | UsingSingleSlotFlag;
}

bool StructureTransitionTable::contains(UniquedStringImpl* rep, unsigned attributes) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = singleTransition();
        return transition && transition->m_nameInPrevious == rep && transition->attributesInPrevious() == attributes;
    }
    return map()->get(std::make_pair(rep, attributes));
}

Structure* StructureTransitionTable::get(UniquedStringImpl* rep, unsigned attributes) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = singleTransition();
        return (transition && transition->m_nameInPrevious == rep && transition->attributesInPrevious() == attributes) ? transition : 0;
    }
    return map()->get(std::make_pair(rep, attributes));
}

void StructureTransitionTable::add(VM& vm, Structure* structure)
{
    if (isUsingSingleSlot()) {
        Structure* existingTransition = singleTransition();

        // This handles the first transition being added.
        if (!existingTransition) {
            setSingleTransition(structure);
            return;
        }

        // This handles the second transition being added
        // (or the first transition being despecified!)
        setMap(new TransitionMap(vm));
        add(vm, existingTransition);
    }

    // Add the structure to the map.

    // Newer versions of the STL have an std::make_pair function that takes rvalue references.
    // When either of the parameters are bitfields, the C++ compiler will try to bind them as lvalues, which is invalid. To work around this, use unary "+" to make the parameter an rvalue.
    // See https://bugs.webkit.org/show_bug.cgi?id=59261 for more details
    map()->set(std::make_pair(structure->m_nameInPrevious.get(), +structure->attributesInPrevious()), structure);
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

        if (structure->propertyTable()) {
            ++numberWithPropertyMaps;
            totalPropertyMapsSize += structure->propertyTable()->sizeInMemory();
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

Structure::Structure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
    : JSCell(vm, vm.structureStructure.get())
    , m_blob(vm.heap.structureIDTable().allocateID(this), indexingType, typeInfo)
    , m_outOfLineTypeFlags(typeInfo.outOfLineTypeFlags())
    , m_globalObject(vm, this, globalObject, WriteBarrier<JSGlobalObject>::MayBeNull)
    , m_prototype(vm, this, prototype)
    , m_classInfo(classInfo)
    , m_transitionWatchpointSet(IsWatched)
    , m_offset(invalidOffset)
    , m_inlineCapacity(inlineCapacity)
    , m_bitField(0)
{
    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasGetterSetterProperties(classInfo->hasStaticSetterOrReadonlyProperties());
    setHasCustomGetterSetterProperties(false);
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(classInfo->hasStaticSetterOrReadonlyProperties());
    setIsQuickPropertyAccessAllowedForEnumeration(true);
    setAttributesInPrevious(0);
    setDidPreventExtensions(false);
    setDidTransition(false);
    setStaticFunctionsReified(false);
    setHasRareData(false);
    setTransitionWatchpointIsLikelyToBeFired(false);
    setHasBeenDictionary(false);
 
    ASSERT(inlineCapacity <= JSFinalObject::maxInlineCapacity());
    ASSERT(static_cast<PropertyOffset>(inlineCapacity) < firstOutOfLineOffset);
    ASSERT(!hasRareData());
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
    ASSERT(hasGetterSetterProperties() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
}

const ClassInfo Structure::s_info = { "Structure", 0, 0, CREATE_METHOD_TABLE(Structure) };

Structure::Structure(VM& vm)
    : JSCell(CreatingEarlyCell)
    , m_prototype(vm, this, jsNull())
    , m_classInfo(info())
    , m_transitionWatchpointSet(IsWatched)
    , m_offset(invalidOffset)
    , m_inlineCapacity(0)
    , m_bitField(0)
{
    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasGetterSetterProperties(m_classInfo->hasStaticSetterOrReadonlyProperties());
    setHasCustomGetterSetterProperties(false);
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(m_classInfo->hasStaticSetterOrReadonlyProperties());
    setIsQuickPropertyAccessAllowedForEnumeration(true);
    setAttributesInPrevious(0);
    setDidPreventExtensions(false);
    setDidTransition(false);
    setStaticFunctionsReified(false);
    setHasRareData(false);
    setTransitionWatchpointIsLikelyToBeFired(false);
    setHasBeenDictionary(false);
 
    TypeInfo typeInfo = TypeInfo(CellType, StructureFlags);
    m_blob = StructureIDBlob(vm.heap.structureIDTable().allocateID(this), 0, typeInfo);
    m_outOfLineTypeFlags = typeInfo.outOfLineTypeFlags();

    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
    ASSERT(hasGetterSetterProperties() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
}

Structure::Structure(VM& vm, Structure* previous, DeferredStructureTransitionWatchpointFire* deferred)
    : JSCell(vm, vm.structureStructure.get())
    , m_prototype(vm, this, previous->storedPrototype())
    , m_classInfo(previous->m_classInfo)
    , m_transitionWatchpointSet(IsWatched)
    , m_offset(invalidOffset)
    , m_inlineCapacity(previous->m_inlineCapacity)
    , m_bitField(0)
{
    setDictionaryKind(previous->dictionaryKind());
    setIsPinnedPropertyTable(previous->hasBeenFlattenedBefore());
    setHasGetterSetterProperties(previous->hasGetterSetterProperties());
    setHasCustomGetterSetterProperties(previous->hasCustomGetterSetterProperties());
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(previous->hasReadOnlyOrGetterSetterPropertiesExcludingProto());
    setIsQuickPropertyAccessAllowedForEnumeration(previous->isQuickPropertyAccessAllowedForEnumeration());
    setAttributesInPrevious(0);
    setDidPreventExtensions(previous->didPreventExtensions());
    setDidTransition(true);
    setStaticFunctionsReified(previous->staticFunctionsReified());
    setHasRareData(false);
    setHasBeenDictionary(previous->hasBeenDictionary());
 
    TypeInfo typeInfo = previous->typeInfo();
    m_blob = StructureIDBlob(vm.heap.structureIDTable().allocateID(this), previous->indexingTypeIncludingHistory(), typeInfo);
    m_outOfLineTypeFlags = typeInfo.outOfLineTypeFlags();

    ASSERT(!previous->typeInfo().structureIsImmortal());
    setPreviousID(vm, previous);

    previous->didTransitionFromThisStructure(deferred);
    
    // Copy this bit now, in case previous was being watched.
    setTransitionWatchpointIsLikelyToBeFired(previous->transitionWatchpointIsLikelyToBeFired());

    if (previous->m_globalObject)
        m_globalObject.set(vm, this, previous->m_globalObject.get());
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
    ASSERT(hasGetterSetterProperties() || !m_classInfo->hasStaticSetterOrReadonlyProperties());
}

Structure::~Structure()
{
    if (typeInfo().structureIsImmortal())
        return;
    Heap::heap(this)->structureIDTable().deallocateID(this, m_blob.structureID());
}

void Structure::destroy(JSCell* cell)
{
    static_cast<Structure*>(cell)->Structure::~Structure();
}

void Structure::findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*& structure, PropertyTable*& table)
{
    ASSERT(structures.isEmpty());
    table = 0;

    for (structure = this; structure; structure = structure->previousID()) {
        structure->m_lock.lock();
        
        table = structure->propertyTable().get();
        if (table) {
            // Leave the structure locked, so that the caller can do things to it atomically
            // before it loses its property table.
            return;
        }
        
        structures.append(structure);
        structure->m_lock.unlock();
    }
    
    ASSERT(!structure);
    ASSERT(!table);
}

void Structure::materializePropertyMap(VM& vm)
{
    ASSERT(structure()->classInfo() == info());
    ASSERT(!propertyTable());

    Vector<Structure*, 8> structures;
    Structure* structure;
    PropertyTable* table;
    
    findStructuresAndMapForMaterialization(structures, structure, table);
    
    if (table) {
        table = table->copy(vm, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity));
        structure->m_lock.unlock();
    }
    
    // Must hold the lock on this structure, since we will be modifying this structure's
    // property map. We don't want getConcurrently() to see the property map in a half-baked
    // state.
    GCSafeConcurrentJITLocker locker(m_lock, vm.heap);
    if (!table)
        createPropertyMap(locker, vm, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity));
    else
        propertyTable().set(vm, this, table);

    InferredTypeTable* typeTable = m_inferredTypeTable.get();

    for (size_t i = structures.size(); i--;) {
        structure = structures[i];
        if (!structure->m_nameInPrevious)
            continue;
        PropertyMapEntry entry(structure->m_nameInPrevious.get(), structure->m_offset, structure->attributesInPrevious());
        if (typeTable && typeTable->get(structure->m_nameInPrevious.get()))
            entry.hasInferredType = true;
        propertyTable()->add(entry, m_offset, PropertyTable::PropertyOffsetMustNotChange);
    }
    
    checkOffsetConsistency();
}

Structure* Structure::addPropertyTransitionToExistingStructureImpl(Structure* structure, UniquedStringImpl* uid, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());

    if (Structure* existingTransition = structure->m_transitionTable.get(uid, attributes)) {
        validateOffset(existingTransition->m_offset, existingTransition->inlineCapacity());
        offset = existingTransition->m_offset;
        return existingTransition;
    }

    return 0;
}

Structure* Structure::addPropertyTransitionToExistingStructure(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!isCompilationThread());
    return addPropertyTransitionToExistingStructureImpl(structure, propertyName.uid(), attributes, offset);
}

Structure* Structure::addPropertyTransitionToExistingStructureConcurrently(Structure* structure, UniquedStringImpl* uid, unsigned attributes, PropertyOffset& offset)
{
    ConcurrentJITLocker locker(structure->m_lock);
    return addPropertyTransitionToExistingStructureImpl(structure, uid, attributes, offset);
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

bool Structure::holesMustForwardToPrototype(VM& vm) const
{
    if (this->mayInterceptIndexedAccesses())
        return true;

    JSValue prototype = this->storedPrototype();
    if (!prototype.isObject())
        return false;
    JSObject* object = asObject(prototype);

    while (true) {
        Structure& structure = *object->structure(vm);
        if (hasIndexedProperties(object->indexingType()) || structure.mayInterceptIndexedAccesses())
            return true;
        prototype = structure.storedPrototype();
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
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

Structure* Structure::addPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset, PutPropertySlot::Context context, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());
    ASSERT(!Structure::addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset));
    
    int maxTransitionLength;
    if (context == PutPropertySlot::PutById)
        maxTransitionLength = s_maxTransitionLengthForNonEvalPutById;
    else
        maxTransitionLength = s_maxTransitionLength;
    if (structure->transitionCount() > maxTransitionLength) {
        Structure* transition = toCacheableDictionaryTransition(vm, structure, deferred);
        ASSERT(structure != transition);
        offset = transition->add(vm, propertyName, attributes);
        return transition;
    }
    
    Structure* transition = create(vm, structure, deferred);

    transition->m_cachedPrototypeChain.setMayBeNull(vm, transition, structure->m_cachedPrototypeChain.get());
    transition->m_nameInPrevious = propertyName.uid();
    transition->setAttributesInPrevious(attributes);
    transition->propertyTable().set(vm, transition, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->m_offset = structure->m_offset;
    transition->m_inferredTypeTable.setMayBeNull(vm, transition, structure->m_inferredTypeTable.get());

    offset = transition->add(vm, propertyName, attributes);

    checkOffset(transition->m_offset, transition->inlineCapacity());
    {
        ConcurrentJITLocker locker(structure->m_lock);
        structure->m_transitionTable.add(vm, transition);
    }
    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::removePropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, PropertyOffset& offset)
{
    // NOTE: There are some good reasons why this goes directly to uncacheable dictionary rather than
    // caching the removal. We can fix all of these things, but we must remember to do so, if we ever try
    // to optimize this case.
    //
    // - Cached transitions usually steal the property table, and assume that this is possible because they
    //   can just rebuild the table by looking at past transitions. That code assumes that the table only
    //   grew and never shrank. To support removals, we'd have to change the property table materialization
    //   code to handle deletions. Also, we have logic to get the list of properties on a structure that
    //   lacks a property table by just looking back through the set of transitions since the last
    //   structure that had a pinned table. That logic would also have to be changed to handle cached
    //   removals.
    //
    // - InferredTypeTable assumes that removal has never happened. This is important since if we could
    //   remove a property and then re-add it later, then the "absence means top" optimization wouldn't
    //   work anymore, unless removal also either poisoned type inference (by doing something equivalent to
    //   hasBeenDictionary) or by strongly marking the entry as Top by ensuring that it is not absent, but
    //   instead, has a null entry.
    
    ASSERT(!structure->isUncacheableDictionary());

    Structure* transition = toUncacheableDictionaryTransition(vm, structure);

    offset = transition->remove(propertyName);

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::changePrototypeTransition(VM& vm, Structure* structure, JSValue prototype)
{
    Structure* transition = create(vm, structure);

    transition->m_prototype.set(vm, transition, prototype);

    DeferGC deferGC(vm.heap);
    structure->materializePropertyMapIfNecessary(vm, deferGC);
    transition->propertyTable().set(vm, transition, structure->copyPropertyTableForPinning(vm));
    transition->m_offset = structure->m_offset;
    transition->pin();

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::attributeChangeTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes)
{
    DeferGC deferGC(vm.heap);
    if (!structure->isUncacheableDictionary()) {
        Structure* transition = create(vm, structure);

        structure->materializePropertyMapIfNecessary(vm, deferGC);
        transition->propertyTable().set(vm, transition, structure->copyPropertyTableForPinning(vm));
        transition->m_offset = structure->m_offset;
        transition->pin();
        
        structure = transition;
    }

    ASSERT(structure->propertyTable());
    PropertyMapEntry* entry = structure->propertyTable()->get(propertyName.uid());
    ASSERT(entry);
    entry->attributes = attributes;

    structure->checkOffsetConsistency();
    return structure;
}

Structure* Structure::toDictionaryTransition(VM& vm, Structure* structure, DictionaryKind kind, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(!structure->isUncacheableDictionary());
    
    Structure* transition = create(vm, structure, deferred);

    DeferGC deferGC(vm.heap);
    structure->materializePropertyMapIfNecessary(vm, deferGC);
    transition->propertyTable().set(vm, transition, structure->copyPropertyTableForPinning(vm));
    transition->m_offset = structure->m_offset;
    transition->setDictionaryKind(kind);
    transition->pin();
    transition->setHasBeenDictionary(true);

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::toCacheableDictionaryTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return toDictionaryTransition(vm, structure, CachedDictionaryKind, deferred);
}

Structure* Structure::toUncacheableDictionaryTransition(VM& vm, Structure* structure)
{
    return toDictionaryTransition(vm, structure, UncachedDictionaryKind);
}

// In future we may want to cache this transition.
Structure* Structure::sealTransition(VM& vm, Structure* structure)
{
    Structure* transition = preventExtensionsTransition(vm, structure);

    if (transition->propertyTable()) {
        PropertyTable::iterator end = transition->propertyTable()->end();
        for (PropertyTable::iterator iter = transition->propertyTable()->begin(); iter != end; ++iter)
            iter->attributes |= DontDelete;
    }

    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this transition.
Structure* Structure::freezeTransition(VM& vm, Structure* structure)
{
    Structure* transition = preventExtensionsTransition(vm, structure);

    if (transition->propertyTable()) {
        PropertyTable::iterator iter = transition->propertyTable()->begin();
        PropertyTable::iterator end = transition->propertyTable()->end();
        if (iter != end)
            transition->setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
        for (; iter != end; ++iter)
            iter->attributes |= iter->attributes & Accessor ? DontDelete : (DontDelete | ReadOnly);
    }

    ASSERT(transition->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !transition->classInfo()->hasStaticSetterOrReadonlyProperties());
    ASSERT(transition->hasGetterSetterProperties() || !transition->classInfo()->hasStaticSetterOrReadonlyProperties());
    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this transition.
Structure* Structure::preventExtensionsTransition(VM& vm, Structure* structure)
{
    Structure* transition = create(vm, structure);

    // Don't set m_offset, as one can not transition to this.

    DeferGC deferGC(vm.heap);
    structure->materializePropertyMapIfNecessary(vm, deferGC);
    transition->propertyTable().set(vm, transition, structure->copyPropertyTableForPinning(vm));
    transition->m_offset = structure->m_offset;
    transition->setDidPreventExtensions(true);
    transition->pin();

    transition->checkOffsetConsistency();
    return transition;
}

PropertyTable* Structure::takePropertyTableOrCloneIfPinned(VM& vm)
{
    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessaryForPinning(vm, deferGC);
    
    if (isPinnedPropertyTable())
        return propertyTable()->copy(vm, propertyTable()->size() + 1);
    
    // Hold the lock while stealing the table - so that getConcurrently() on another thread
    // will either have to bypass this structure, or will get to use the property table
    // before it is stolen.
    ConcurrentJITLocker locker(m_lock);
    PropertyTable* takenPropertyTable = propertyTable().get();
    propertyTable().clear();
    return takenPropertyTable;
}

Structure* Structure::nonPropertyTransition(VM& vm, Structure* structure, NonPropertyTransition transitionKind)
{
    unsigned attributes = toAttributes(transitionKind);
    IndexingType indexingType = newIndexingType(structure->indexingTypeIncludingHistory(), transitionKind);
    
    if (JSGlobalObject* globalObject = structure->m_globalObject.get()) {
        if (globalObject->isOriginalArrayStructure(structure)) {
            Structure* result = globalObject->originalArrayStructureForIndexingType(indexingType);
            if (result->indexingTypeIncludingHistory() == indexingType) {
                structure->didTransitionFromThisStructure();
                return result;
            }
        }
    }
    
    Structure* existingTransition;
    if (!structure->isDictionary() && (existingTransition = structure->m_transitionTable.get(0, attributes))) {
        ASSERT(existingTransition->attributesInPrevious() == attributes);
        ASSERT(existingTransition->indexingTypeIncludingHistory() == indexingType);
        return existingTransition;
    }
    
    Structure* transition = create(vm, structure);
    transition->setAttributesInPrevious(attributes);
    transition->m_blob.setIndexingType(indexingType);
    transition->propertyTable().set(vm, transition, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->m_offset = structure->m_offset;
    checkOffset(transition->m_offset, transition->inlineCapacity());
    
    if (structure->isDictionary())
        transition->pin();
    else {
        ConcurrentJITLocker locker(structure->m_lock);
        structure->m_transitionTable.add(vm, transition);
    }
    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this property.
bool Structure::isSealed(VM& vm)
{
    if (isStructureExtensible())
        return false;

    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessary(vm, deferGC);
    if (!propertyTable())
        return true;

    PropertyTable::iterator end = propertyTable()->end();
    for (PropertyTable::iterator iter = propertyTable()->begin(); iter != end; ++iter) {
        if ((iter->attributes & DontDelete) != DontDelete)
            return false;
    }
    return true;
}

// In future we may want to cache this property.
bool Structure::isFrozen(VM& vm)
{
    if (isStructureExtensible())
        return false;

    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessary(vm, deferGC);
    if (!propertyTable())
        return true;

    PropertyTable::iterator end = propertyTable()->end();
    for (PropertyTable::iterator iter = propertyTable()->begin(); iter != end; ++iter) {
        if (!(iter->attributes & DontDelete))
            return false;
        if (!(iter->attributes & (ReadOnly | Accessor)))
            return false;
    }
    return true;
}

Structure* Structure::flattenDictionaryStructure(VM& vm, JSObject* object)
{
    checkOffsetConsistency();
    ASSERT(isDictionary());

    size_t beforeOutOfLineCapacity = this->outOfLineCapacity();
    if (isUncacheableDictionary()) {
        ASSERT(propertyTable());

        size_t propertyCount = propertyTable()->size();

        // Holds our values compacted by insertion order.
        Vector<JSValue> values(propertyCount);

        // Copies out our values from their hashed locations, compacting property table offsets as we go.
        unsigned i = 0;
        PropertyTable::iterator end = propertyTable()->end();
        m_offset = invalidOffset;
        for (PropertyTable::iterator iter = propertyTable()->begin(); iter != end; ++iter, ++i) {
            values[i] = object->getDirect(iter->offset);
            m_offset = iter->offset = offsetForPropertyNumber(i, m_inlineCapacity);
        }
        
        // Copies in our values to their compacted locations.
        for (unsigned i = 0; i < propertyCount; i++)
            object->putDirect(vm, offsetForPropertyNumber(i, m_inlineCapacity), values[i]);

        propertyTable()->clearDeletedOffsets();
        checkOffsetConsistency();
    }

    setDictionaryKind(NoneDictionaryKind);
    setHasBeenFlattenedBefore(true);

    size_t afterOutOfLineCapacity = this->outOfLineCapacity();

    if (beforeOutOfLineCapacity != afterOutOfLineCapacity) {
        ASSERT(beforeOutOfLineCapacity > afterOutOfLineCapacity);
        // If the object had a Butterfly but after flattening/compacting we no longer have need of it,
        // we need to zero it out because the collector depends on the Structure to know the size for copying.
        if (object->butterfly() && !afterOutOfLineCapacity && !this->hasIndexingHeader(object))
            object->setStructureAndButterfly(vm, this, 0);
        // If the object was down-sized to the point where the base of the Butterfly is no longer within the 
        // first CopiedBlock::blockSize bytes, we'll get the wrong answer if we try to mask the base back to 
        // the CopiedBlock header. To prevent this case we need to memmove the Butterfly down.
        else if (object->butterfly())
            object->shiftButterflyAfterFlattening(vm, beforeOutOfLineCapacity, afterOutOfLineCapacity);
    }

    return this;
}

PropertyOffset Structure::addPropertyWithoutTransition(VM& vm, PropertyName propertyName, unsigned attributes)
{
    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessaryForPinning(vm, deferGC);
    
    pin();

    return add(vm, propertyName, attributes);
}

PropertyOffset Structure::removePropertyWithoutTransition(VM& vm, PropertyName propertyName)
{
    ASSERT(isUncacheableDictionary());

    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessaryForPinning(vm, deferGC);

    pin();
    return remove(propertyName);
}

void Structure::pin()
{
    ASSERT(propertyTable());
    setIsPinnedPropertyTable(true);
    clearPreviousID();
    m_nameInPrevious = nullptr;
}

void Structure::allocateRareData(VM& vm)
{
    ASSERT(!hasRareData());
    StructureRareData* rareData = StructureRareData::create(vm, previous());
    WTF::storeStoreFence();
    m_previousOrRareData.set(vm, this, rareData);
    WTF::storeStoreFence();
    setHasRareData(true);
    ASSERT(hasRareData());
}

WatchpointSet* Structure::ensurePropertyReplacementWatchpointSet(VM& vm, PropertyOffset offset)
{
    ASSERT(!isUncacheableDictionary());

    // In some places it's convenient to call this with an invalid offset. So, we do the check here.
    if (!isValidOffset(offset))
        return nullptr;
    
    if (!hasRareData())
        allocateRareData(vm);
    ConcurrentJITLocker locker(m_lock);
    StructureRareData* rareData = this->rareData();
    if (!rareData->m_replacementWatchpointSets) {
        rareData->m_replacementWatchpointSets =
            std::make_unique<StructureRareData::PropertyWatchpointMap>();
        WTF::storeStoreFence();
    }
    auto result = rareData->m_replacementWatchpointSets->add(offset, nullptr);
    if (result.isNewEntry)
        result.iterator->value = adoptRef(new WatchpointSet(IsWatched));
    return result.iterator->value.get();
}

void Structure::startWatchingPropertyForReplacements(VM& vm, PropertyName propertyName)
{
    ASSERT(!isUncacheableDictionary());
    
    startWatchingPropertyForReplacements(vm, get(vm, propertyName));
}

void Structure::didCachePropertyReplacement(VM& vm, PropertyOffset offset)
{
    ensurePropertyReplacementWatchpointSet(vm, offset)->fireAll("Did cache property replacement");
}

void Structure::startWatchingInternalProperties(VM& vm)
{
    if (!isUncacheableDictionary()) {
        startWatchingPropertyForReplacements(vm, vm.propertyNames->toString);
        startWatchingPropertyForReplacements(vm, vm.propertyNames->valueOf);
    }
    setDidWatchInternalProperties(true);
}

void Structure::willStoreValueSlow(
    VM& vm, PropertyName propertyName, JSValue value, bool shouldOptimize,
    InferredTypeTable::StoredPropertyAge age)
{
    ASSERT(!isCompilationThread());
    ASSERT(structure()->classInfo() == info());
    ASSERT(!hasBeenDictionary());

    // Create the inferred type table before doing anything else, so that we don't GC after we have already
    // grabbed a pointer into the property map.
    InferredTypeTable* table = m_inferredTypeTable.get();
    if (!table) {
        table = InferredTypeTable::create(vm);
        WTF::storeStoreFence();
        m_inferredTypeTable.set(vm, this, table);
    }

    // This only works if we've got a property table.
    PropertyTable* propertyTable;
    materializePropertyMapIfNecessary(vm, propertyTable);

    // We must be calling this after having created the given property or confirmed that it was present
    // already, so we must have a property table now.
    ASSERT(propertyTable);

    // ... and the property must be present.
    PropertyMapEntry* entry = propertyTable->get(propertyName.uid());
    ASSERT(entry);

    if (shouldOptimize)
        entry->hasInferredType = table->willStoreValue(vm, propertyName, value, age);
    else {
        table->makeTop(vm, propertyName, age);
        entry->hasInferredType = false;
    }
}

#if DUMP_PROPERTYMAP_STATS

PropertyMapHashTableStats* propertyMapHashTableStats = 0;

struct PropertyMapStatisticsExitLogger {
    PropertyMapStatisticsExitLogger();
    ~PropertyMapStatisticsExitLogger();
};

DEFINE_GLOBAL_FOR_LOGGING(PropertyMapStatisticsExitLogger, logger, );

PropertyMapStatisticsExitLogger::PropertyMapStatisticsExitLogger()
{
    propertyMapHashTableStats = adoptPtr(new PropertyMapHashTableStats()).leakPtr();
}

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    unsigned finds = propertyMapHashTableStats->numFinds;
    unsigned collisions = propertyMapHashTableStats->numCollisions;
    dataLogF("\nJSC::PropertyMap statistics for process %d\n\n", getCurrentProcessID());
    dataLogF("%d finds\n", finds);
    dataLogF("%d collisions (%.1f%%)\n", collisions, 100.0 * collisions / finds);
    dataLogF("%d lookups\n", propertyMapHashTableStats->numLookups.load());
    dataLogF("%d lookup probings\n", propertyMapHashTableStats->numLookupProbing.load());
    dataLogF("%d adds\n", propertyMapHashTableStats->numAdds.load());
    dataLogF("%d removes\n", propertyMapHashTableStats->numRemoves.load());
    dataLogF("%d rehashes\n", propertyMapHashTableStats->numRehashes.load());
    dataLogF("%d reinserts\n", propertyMapHashTableStats->numReinserts.load());
}

#endif

PropertyTable* Structure::copyPropertyTable(VM& vm)
{
    if (!propertyTable())
        return 0;
    return PropertyTable::clone(vm, *propertyTable().get());
}

PropertyTable* Structure::copyPropertyTableForPinning(VM& vm)
{
    if (propertyTable())
        return PropertyTable::clone(vm, *propertyTable().get());
    return PropertyTable::create(vm, numberOfSlotsForLastOffset(m_offset, m_inlineCapacity));
}

PropertyOffset Structure::getConcurrently(UniquedStringImpl* uid, unsigned& attributes)
{
    PropertyOffset result = invalidOffset;
    
    forEachPropertyConcurrently(
        [&] (const PropertyMapEntry& candidate) -> bool {
            if (candidate.key != uid)
                return true;
            
            result = candidate.offset;
            attributes = candidate.attributes;
            return false;
        });
    
    return result;
}

Vector<PropertyMapEntry> Structure::getPropertiesConcurrently()
{
    Vector<PropertyMapEntry> result;

    forEachPropertyConcurrently(
        [&] (const PropertyMapEntry& entry) -> bool {
            result.append(entry);
            return true;
        });
    
    return result;
}

PropertyOffset Structure::add(VM& vm, PropertyName propertyName, unsigned attributes)
{
    GCSafeConcurrentJITLocker locker(m_lock, vm.heap);
    
    ASSERT(!JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();
    if (attributes & DontEnum)
        setIsQuickPropertyAccessAllowedForEnumeration(false);

    auto rep = propertyName.uid();

    if (!propertyTable())
        createPropertyMap(locker, vm);

    PropertyOffset newOffset = propertyTable()->nextOffset(m_inlineCapacity);

    propertyTable()->add(PropertyMapEntry(rep, newOffset, attributes), m_offset, PropertyTable::PropertyOffsetMayChange);
    
    checkConsistency();
    return newOffset;
}

PropertyOffset Structure::remove(PropertyName propertyName)
{
    ConcurrentJITLocker locker(m_lock);
    
    checkConsistency();

    auto rep = propertyName.uid();

    if (!propertyTable())
        return invalidOffset;

    PropertyTable::find_iterator position = propertyTable()->find(rep);
    if (!position.first)
        return invalidOffset;

    PropertyOffset offset = position.first->offset;

    propertyTable()->remove(position);
    propertyTable()->addDeletedOffset(offset);

    checkConsistency();
    return offset;
}

void Structure::createPropertyMap(const GCSafeConcurrentJITLocker&, VM& vm, unsigned capacity)
{
    ASSERT(!propertyTable());

    checkConsistency();
    propertyTable().set(vm, this, PropertyTable::create(vm, capacity));
}

void Structure::getPropertyNamesFromStructure(VM& vm, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    DeferGC deferGC(vm.heap);
    materializePropertyMapIfNecessary(vm, deferGC);
    if (!propertyTable())
        return;

    bool knownUnique = propertyNames.canAddKnownUniqueForStructure();

    PropertyTable::iterator end = propertyTable()->end();
    for (PropertyTable::iterator iter = propertyTable()->begin(); iter != end; ++iter) {
        ASSERT(!isQuickPropertyAccessAllowedForEnumeration() || !(iter->attributes & DontEnum));
        if (!(iter->attributes & DontEnum) || mode.includeDontEnumProperties()) {
            if (iter->key->isSymbol() && !propertyNames.includeSymbolProperties())
                continue;
            if (knownUnique)
                propertyNames.addKnownUnique(iter->key);
            else
                propertyNames.add(iter->key);
        }
    }
}

void StructureFireDetail::dump(PrintStream& out) const
{
    out.print("Structure transition from ", *m_structure);
}

DeferredStructureTransitionWatchpointFire::DeferredStructureTransitionWatchpointFire()
    : m_structure(nullptr)
{
}

DeferredStructureTransitionWatchpointFire::~DeferredStructureTransitionWatchpointFire()
{
    if (m_structure)
        m_structure->transitionWatchpointSet().fireAll(StructureFireDetail(m_structure));
}

void DeferredStructureTransitionWatchpointFire::add(const Structure* structure)
{
    RELEASE_ASSERT(!m_structure);
    RELEASE_ASSERT(structure);
    m_structure = structure;
}

void Structure::didTransitionFromThisStructure(DeferredStructureTransitionWatchpointFire* deferred) const
{
    // If the structure is being watched, and this is the kind of structure that the DFG would
    // like to watch, then make sure to note for all future versions of this structure that it's
    // unwise to watch it.
    if (m_transitionWatchpointSet.isBeingWatched())
        const_cast<Structure*>(this)->setTransitionWatchpointIsLikelyToBeFired(true);
    
    if (deferred)
        deferred->add(this);
    else
        m_transitionWatchpointSet.fireAll(StructureFireDetail(this));
}

JSValue Structure::prototypeForLookup(CodeBlock* codeBlock) const
{
    return prototypeForLookup(codeBlock->globalObject());
}

void Structure::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Structure* thisObject = jsCast<Structure*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    JSCell::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_globalObject);
    if (!thisObject->isObject())
        thisObject->m_cachedPrototypeChain.clear();
    else {
        visitor.append(&thisObject->m_prototype);
        visitor.append(&thisObject->m_cachedPrototypeChain);
    }
    visitor.append(&thisObject->m_previousOrRareData);

    if (thisObject->isPinnedPropertyTable()) {
        ASSERT(thisObject->m_propertyTableUnsafe);
        visitor.append(&thisObject->m_propertyTableUnsafe);
    } else if (thisObject->m_propertyTableUnsafe)
        thisObject->m_propertyTableUnsafe.clear();

    visitor.append(&thisObject->m_inferredTypeTable);
}

bool Structure::prototypeChainMayInterceptStoreTo(VM& vm, PropertyName propertyName)
{
    if (parseIndex(propertyName))
        return anyObjectInChainMayInterceptIndexedAccesses();
    
    for (Structure* current = this; ;) {
        JSValue prototype = current->storedPrototype();
        if (prototype.isNull())
            return false;
        
        current = prototype.asCell()->structure(vm);
        
        unsigned attributes;
        PropertyOffset offset = current->get(vm, propertyName, attributes);
        if (!JSC::isValidOffset(offset))
            continue;
        
        if (attributes & (ReadOnly | Accessor))
            return true;
        
        return false;
    }
}

PassRefPtr<StructureShape> Structure::toStructureShape(JSValue value)
{
    RefPtr<StructureShape> baseShape = StructureShape::create();
    RefPtr<StructureShape> curShape = baseShape;
    Structure* curStructure = this;
    JSValue curValue = value;
    while (curStructure) {
        curStructure->forEachPropertyConcurrently(
            [&] (const PropertyMapEntry& entry) -> bool {
                curShape->addProperty(*entry.key);
                return true;
            });

        if (JSObject* curObject = curValue.getObject())
            curShape->setConstructorName(JSObject::calculatedClassName(curObject));
        else
            curShape->setConstructorName(curStructure->classInfo()->className);

        if (curStructure->isDictionary())
            curShape->enterDictionaryMode();

        curShape->markAsFinal();

        if (curStructure->storedPrototypeStructure()) {
            RefPtr<StructureShape> newShape = StructureShape::create();
            curShape->setProto(newShape);
            curShape = newShape.release();
            curValue = curStructure->storedPrototype();
        }

        curStructure = curStructure->storedPrototypeStructure();
    }
    
    return baseShape.release();
}

bool Structure::canUseForAllocationsOf(Structure* other)
{
    return inlineCapacity() == other->inlineCapacity()
        && storedPrototype() == other->storedPrototype()
        && objectInitializationBlob() == other->objectInitializationBlob();
}

void Structure::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":[", classInfo()->className, ", {");
    
    CommaPrinter comma;
    
    const_cast<Structure*>(this)->forEachPropertyConcurrently(
        [&] (const PropertyMapEntry& entry) -> bool {
            out.print(comma, entry.key, ":", static_cast<int>(entry.offset));
            return true;
        });
    
    out.print("}, ", IndexingTypeDump(indexingType()));
    
    if (m_prototype.get().isCell())
        out.print(", Proto:", RawPointer(m_prototype.get().asCell()));

    switch (dictionaryKind()) {
    case NoneDictionaryKind:
        if (hasBeenDictionary())
            out.print(", Has been dictionary");
        break;
    case CachedDictionaryKind:
        out.print(", Dictionary");
        break;
    case UncachedDictionaryKind:
        out.print(", UncacheableDictionary");
        break;
    }

    if (transitionWatchpointSetIsStillValid())
        out.print(", Leaf");
    else if (transitionWatchpointIsLikelyToBeFired())
        out.print(", Shady leaf");
    
    out.print("]");
}

void Structure::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (context)
        context->structures.dumpBrief(this, out);
    else
        dump(out);
}

void Structure::dumpBrief(PrintStream& out, const CString& string) const
{
    out.print("%", string, ":", classInfo()->className);
}

void Structure::dumpContextHeader(PrintStream& out)
{
    out.print("Structures:");
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void PropertyTable::checkConsistency()
{
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
        unsigned i = IdentifierRepHash::hash(rep);
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_index[i & m_indexMask];
            ASSERT(entryIndex != PropertyTable::EmptyEntryIndex);
            if (rep == table()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(IdentifierRepHash::hash(rep));
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_keyCount);
}

void Structure::checkConsistency()
{
    checkOffsetConsistency();

    if (!propertyTable())
        return;

    if (isQuickPropertyAccessAllowedForEnumeration()) {
        PropertyTable::iterator end = propertyTable()->end();
        for (PropertyTable::iterator iter = propertyTable()->begin(); iter != end; ++iter) {
            ASSERT(!(iter->attributes & DontEnum));
        }
    }

    propertyTable()->checkConsistency();
}

#else

inline void Structure::checkConsistency()
{
    checkOffsetConsistency();
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

bool ClassInfo::hasStaticSetterOrReadonlyProperties() const
{
    for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
        if (const HashTable* table = ci->staticPropHashTable) {
            if (table->hasSetterOrReadonlyProperties)
                return true;
        }
    }
    return false;
}

void Structure::setCachedPropertyNameEnumerator(VM& vm, JSPropertyNameEnumerator* enumerator)
{
    ASSERT(!isDictionary());
    if (!hasRareData())
        allocateRareData(vm);
    rareData()->setCachedPropertyNameEnumerator(vm, enumerator);
}

JSPropertyNameEnumerator* Structure::cachedPropertyNameEnumerator() const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedPropertyNameEnumerator();
}

bool Structure::canCachePropertyNameEnumerator() const
{
    if (isDictionary())
        return false;

    if (hasIndexedProperties(indexingType()))
        return false;

    if (typeInfo().overridesGetPropertyNames())
        return false;

    StructureChain* structureChain = m_cachedPrototypeChain.get();
    ASSERT(structureChain);
    WriteBarrier<Structure>* structure = structureChain->head();
    while (true) {
        if (!structure->get())
            break;
        if (structure->get()->typeInfo().overridesGetPropertyNames())
            return false;
        structure++;
    }
    
    return true;
}
    
bool Structure::canAccessPropertiesQuicklyForEnumeration() const
{
    if (!isQuickPropertyAccessAllowedForEnumeration())
        return false;
    if (hasGetterSetterProperties())
        return false;
    if (isUncacheableDictionary())
        return false;
    return true;
}

} // namespace JSC
