/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "JSArrayBufferView.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "PropertyMapHashTable.h"
#include "Structure.h"
#include "StructureChain.h"
#include "StructureRareDataInlines.h"
#include <wtf/Threading.h>

namespace JSC {

inline Structure* Structure::create(VM& vm, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
{
    ASSERT(vm.structureStructure);
    ASSERT(classInfo);
    if (auto* object = prototype.getObject()) {
        ASSERT(!object->anyObjectInChainMayInterceptIndexedAccesses(vm) || hasSlowPutArrayStorage(indexingType) || !hasIndexedProperties(indexingType));
        object->didBecomePrototype();
    }

    Structure* structure = new (NotNull, allocateCell<Structure>(vm.heap)) Structure(vm, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);
    structure->finishCreation(vm);
    return structure;
}

inline Structure* Structure::createStructure(VM& vm)
{
    ASSERT(!vm.structureStructure);
    Structure* structure = new (NotNull, allocateCell<Structure>(vm.heap)) Structure(vm);
    structure->finishCreation(vm, CreatingEarlyCell);
    return structure;
}

inline Structure* Structure::create(VM& vm, Structure* previous, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(vm.structureStructure);
    Structure* newStructure = new (NotNull, allocateCell<Structure>(vm.heap)) Structure(vm, previous, deferred);
    newStructure->finishCreation(vm, previous);
    return newStructure;
}

inline bool Structure::mayInterceptIndexedAccesses() const
{
    if (indexingModeIncludingHistory() & MayHaveIndexedAccessors)
        return true;

    // Consider a scenario where object O (of global G1)'s prototype is set to A
    // (of global G2), and G2 is already having a bad time. If an object B with
    // indexed accessors is then set as the prototype of A:
    //      O -> A -> B
    // Then, O should be converted to SlowPutArrayStorage (because it now has an
    // object with indexed accessors in its prototype chain). But it won't be
    // converted because this conversion is done by JSGlobalObject::haveAbadTime(),
    // but G2 is already having a bad time. We solve this by conservatively
    // treating A as potentially having indexed accessors if its global is already
    // having a bad time. Hence, when A is set as O's prototype, O will be
    // converted to SlowPutArrayStorage.

    JSGlobalObject* globalObject = this->globalObject();
    if (!globalObject)
        return false;
    return globalObject->isHavingABadTime();
}

inline JSObject* Structure::storedPrototypeObject() const
{
    ASSERT(hasMonoProto());
    JSValue value = m_prototype.get();
    if (value.isNull())
        return nullptr;
    return asObject(value);
}

inline Structure* Structure::storedPrototypeStructure() const
{
    ASSERT(hasMonoProto());
    JSObject* object = storedPrototypeObject();
    if (!object)
        return nullptr;
    return object->structure();
}

ALWAYS_INLINE JSValue Structure::storedPrototype(const JSObject* object) const
{
    ASSERT(isCompilationThread() || Thread::mayBeGCThread() || object->structure() == this);
    if (hasMonoProto())
        return storedPrototype();
    return object->getDirect(knownPolyProtoOffset);
}

ALWAYS_INLINE JSObject* Structure::storedPrototypeObject(const JSObject* object) const
{
    ASSERT(isCompilationThread() || Thread::mayBeGCThread() || object->structure() == this);
    if (hasMonoProto())
        return storedPrototypeObject();
    JSValue proto = object->getDirect(knownPolyProtoOffset);
    if (proto.isNull())
        return nullptr;
    return asObject(proto);
}

ALWAYS_INLINE Structure* Structure::storedPrototypeStructure(const JSObject* object) const
{
    if (JSObject* proto = storedPrototypeObject(object))
        return proto->structure();
    return nullptr;
}

ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName)
{
    unsigned attributes;
    return get(vm, propertyName, attributes);
}
    
ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName, unsigned& attributes)
{
    ASSERT(!isCompilationThread());
    ASSERT(structure(vm)->classInfo() == info());

    if (ruleOutUnseenProperty(propertyName.uid()))
        return invalidOffset;

    PropertyTable* propertyTable = ensurePropertyTableIfNotEmpty(vm);
    if (!propertyTable)
        return invalidOffset;

    PropertyMapEntry* entry = propertyTable->get(propertyName.uid());
    if (!entry)
        return invalidOffset;

    attributes = entry->attributes;
    return entry->offset;
}

inline bool Structure::ruleOutUnseenProperty(UniquedStringImpl* uid) const
{
    ASSERT(uid);
    return seenProperties().ruleOut(bitwise_cast<uintptr_t>(uid));
}

inline TinyBloomFilter Structure::seenProperties() const
{
#if CPU(ADDRESS64)
    return TinyBloomFilter(bitwise_cast<uintptr_t>(m_propertyHashAndSeenProperties.pointer()));
#else
    return TinyBloomFilter(m_seenProperties);
#endif
}

inline void Structure::addPropertyHashAndSeenProperty(unsigned hash, UniquedStringImpl* pointer)
{
#if CPU(ADDRESS64)
    m_propertyHashAndSeenProperties.setType(m_propertyHashAndSeenProperties.type() ^ hash);
    m_propertyHashAndSeenProperties.setPointer(bitwise_cast<UniquedStringImpl*>(bitwise_cast<uintptr_t>(m_propertyHashAndSeenProperties.pointer()) | bitwise_cast<uintptr_t>(pointer)));
#else
    m_propertyHash = m_propertyHash ^ hash;
    m_seenProperties = bitwise_cast<uintptr_t>(pointer) | m_seenProperties;
#endif
}

template<typename Functor>
void Structure::forEachPropertyConcurrently(const Functor& functor)
{
    Vector<Structure*, 8> structures;
    Structure* tableStructure;
    PropertyTable* table;
    VM& vm = this->vm();
    
    findStructuresAndMapForMaterialization(vm, structures, tableStructure, table);

    HashSet<UniquedStringImpl*> seenProperties;

    for (Structure* structure : structures) {
        UniquedStringImpl* transitionPropertyName = structure->transitionPropertyName();
        if (!transitionPropertyName || seenProperties.contains(transitionPropertyName))
            continue;

        seenProperties.add(transitionPropertyName);

        if (structure->isPropertyDeletionTransition())
            continue;

        if (!functor(PropertyMapEntry(transitionPropertyName, structure->transitionOffset(), structure->transitionPropertyAttributes()))) {
            if (table)
                tableStructure->cellLock().unlock();
            return;
        }
    }
    
    if (table) {
        for (auto& entry : *table) {
            if (seenProperties.contains(entry.key))
                continue;

            if (!functor(entry)) {
                tableStructure->cellLock().unlock();
                return;
            }
        }
        tableStructure->cellLock().unlock();
    }
}

template<typename Functor>
void Structure::forEachProperty(VM& vm, const Functor& functor)
{
    if (PropertyTable* table = ensurePropertyTableIfNotEmpty(vm)) {
        for (auto& entry : *table) {
            if (!functor(entry))
                return;
        }
    }
}

inline PropertyOffset Structure::getConcurrently(UniquedStringImpl* uid)
{
    unsigned attributesIgnored;
    return getConcurrently(uid, attributesIgnored);
}

inline bool Structure::hasIndexingHeader(const JSCell* cell) const
{
    if (hasIndexedProperties(indexingType()))
        return true;
    
    if (!isTypedView(typedArrayTypeForType(m_blob.type())))
        return false;
    
    return jsCast<const JSArrayBufferView*>(cell)->mode() == WastefulTypedArray;
}

inline bool Structure::masqueradesAsUndefined(JSGlobalObject* lexicalGlobalObject)
{
    return typeInfo().masqueradesAsUndefined() && globalObject() == lexicalGlobalObject;
}

inline bool Structure::transitivelyTransitionedFrom(Structure* structureToFind)
{
    VM& vm = this->vm();
    for (Structure* current = this; current; current = current->previousID(vm)) {
        if (current == structureToFind)
            return true;
    }
    return false;
}

inline void Structure::setCachedOwnKeys(VM& vm, JSImmutableButterfly* ownKeys)
{
    ensureRareData(vm)->setCachedOwnKeys(vm, ownKeys);
}

inline JSImmutableButterfly* Structure::cachedOwnKeys() const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedOwnKeys();
}

inline JSImmutableButterfly* Structure::cachedOwnKeysIgnoringSentinel() const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedOwnKeysIgnoringSentinel();
}

inline bool Structure::canCacheOwnKeys() const
{
    if (isDictionary())
        return false;
    if (hasIndexedProperties(indexingType()))
        return false;
    if (typeInfo().overridesGetPropertyNames())
        return false;
    return true;
}

ALWAYS_INLINE JSValue prototypeForLookupPrimitiveImpl(JSGlobalObject* globalObject, const Structure* structure)
{
    ASSERT(!structure->isObject());

    if (structure->typeInfo().type() == StringType)
        return globalObject->stringPrototype();
    
    if (structure->typeInfo().type() == BigIntType)
        return globalObject->bigIntPrototype();

    ASSERT(structure->typeInfo().type() == SymbolType);
    return globalObject->symbolPrototype();
}

inline JSValue Structure::prototypeForLookup(JSGlobalObject* globalObject) const
{
    ASSERT(hasMonoProto());
    if (isObject())
        return storedPrototype();
    return prototypeForLookupPrimitiveImpl(globalObject, this);
}

inline JSValue Structure::prototypeForLookup(JSGlobalObject* globalObject, JSCell* base) const
{
    ASSERT(base->structure() == this);
    if (isObject())
        return storedPrototype(asObject(base));
    return prototypeForLookupPrimitiveImpl(globalObject, this);
}

inline StructureChain* Structure::cachedPrototypeChain() const
{
    JSCell* cell = cachedPrototypeChainOrRareData();
    if (isRareData(cell))
        return jsCast<StructureRareData*>(cell)->cachedPrototypeChain();
    return jsCast<StructureChain*>(cell);
}

inline void Structure::setCachedPrototypeChain(VM& vm, StructureChain* chain)
{
    ASSERT(isObject());
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    JSCell* cell = cachedPrototypeChainOrRareData();
    if (isRareData(cell)) {
        jsCast<StructureRareData*>(cell)->setCachedPrototypeChain(vm, chain);
        return;
    }
#if CPU(ADDRESS64)
    m_inlineCapacityAndCachedPrototypeChainOrRareData.setPointer(chain);
    vm.heap.writeBarrier(this, chain);
#else
    m_cachedPrototypeChainOrRareData.set(vm, this, chain);
#endif
}

inline StructureChain* Structure::prototypeChain(VM& vm, JSGlobalObject* globalObject, JSObject* base) const
{
    ASSERT(this->isObject());
    ASSERT(base->structure(vm) == this);
    // We cache our prototype chain so our clients can share it.
    if (!isValid(globalObject, cachedPrototypeChain(), base)) {
        JSValue prototype = prototypeForLookup(globalObject, base);
        const_cast<Structure*>(this)->setCachedPrototypeChain(vm, StructureChain::create(vm, prototype.isNull() ? nullptr : asObject(prototype)));
    }
    return cachedPrototypeChain();
}

inline StructureChain* Structure::prototypeChain(JSGlobalObject* globalObject, JSObject* base) const
{
    return prototypeChain(globalObject->vm(), globalObject, base);
}

inline bool Structure::isValid(JSGlobalObject* globalObject, StructureChain* cachedPrototypeChain, JSObject* base) const
{
    if (!cachedPrototypeChain)
        return false;

    VM& vm = globalObject->vm();
    JSValue prototype = prototypeForLookup(globalObject, base);
    StructureID* cachedStructure = cachedPrototypeChain->head();
    while (*cachedStructure && !prototype.isNull()) {
        if (asObject(prototype)->structureID() != *cachedStructure)
            return false;
        ++cachedStructure;
        prototype = asObject(prototype)->getPrototypeDirect(vm);
    }
    return prototype.isNull() && !*cachedStructure;
}

inline void Structure::didReplaceProperty(PropertyOffset offset)
{
    if (LIKELY(!hasRareData()))
        return;
    StructureRareData::PropertyWatchpointMap* map = rareData()->m_replacementWatchpointSets.get();
    if (LIKELY(!map))
        return;
    WatchpointSet* set = map->get(offset);
    if (LIKELY(!set))
        return;
    set->fireAll(vm(), "Property did get replaced");
}

inline WatchpointSet* Structure::propertyReplacementWatchpointSet(PropertyOffset offset)
{
    ConcurrentJSCellLocker locker(cellLock());
    if (!hasRareData())
        return nullptr;
    WTF::loadLoadFence();
    StructureRareData::PropertyWatchpointMap* map = rareData()->m_replacementWatchpointSets.get();
    if (!map)
        return nullptr;
    return map->get(offset);
}

template<typename DetailsFunc>
ALWAYS_INLINE bool Structure::checkOffsetConsistency(PropertyTable* propertyTable, const DetailsFunc& detailsFunc) const
{
    // We cannot reliably assert things about the property table in the concurrent
    // compilation thread. It is possible for the table to be stolen and then have
    // things added to it, which leads to the offsets being all messed up. We could
    // get around this by grabbing a lock here, but I think that would be overkill.
    if (isCompilationThread())
        return true;
    
    unsigned totalSize = propertyTable->propertyStorageSize();
    unsigned inlineOverflowAccordingToTotalSize = totalSize < inlineCapacity() ? 0 : totalSize - inlineCapacity();

    auto fail = [&] (const char* description) {
        dataLog("Detected offset inconsistency: ", description, "!\n");
        dataLog("this = ", RawPointer(this), "\n");
        dataLog("transitionOffset = ", transitionOffset(), "\n");
        dataLog("maxOffset = ", maxOffset(), "\n");
        dataLog("m_inlineCapacity = ", inlineCapacity(), "\n");
        dataLog("propertyTable = ", RawPointer(propertyTable), "\n");
        dataLog("numberOfSlotsForMaxOffset = ", numberOfSlotsForMaxOffset(maxOffset(), inlineCapacity()), "\n");
        dataLog("totalSize = ", totalSize, "\n");
        dataLog("inlineOverflowAccordingToTotalSize = ", inlineOverflowAccordingToTotalSize, "\n");
        dataLog("numberOfOutOfLineSlotsForMaxOffset = ", numberOfOutOfLineSlotsForMaxOffset(maxOffset()), "\n");
        detailsFunc();
        UNREACHABLE_FOR_PLATFORM();
    };
    
    if (numberOfSlotsForMaxOffset(maxOffset(), inlineCapacity()) != totalSize)
        fail("numberOfSlotsForMaxOffset doesn't match totalSize");
    if (inlineOverflowAccordingToTotalSize != numberOfOutOfLineSlotsForMaxOffset(maxOffset()))
        fail("inlineOverflowAccordingToTotalSize doesn't match numberOfOutOfLineSlotsForMaxOffset");

    return true;
}

ALWAYS_INLINE bool Structure::checkOffsetConsistency() const
{
    PropertyTable* propertyTable = propertyTableUnsafeOrNull();

    if (!propertyTable) {
        ASSERT(!isPinnedPropertyTable());
        return true;
    }

    // We cannot reliably assert things about the property table in the concurrent
    // compilation thread. It is possible for the table to be stolen and then have
    // things added to it, which leads to the offsets being all messed up. We could
    // get around this by grabbing a lock here, but I think that would be overkill.
    if (isCompilationThread())
        return true;

    return checkOffsetConsistency(propertyTable, [] () { });
}

inline void Structure::checkConsistency()
{
    checkOffsetConsistency();
}

inline size_t nextOutOfLineStorageCapacity(size_t currentCapacity)
{
    if (!currentCapacity)
        return initialOutOfLineCapacity;
    return currentCapacity * outOfLineGrowthFactor;
}

inline void Structure::setObjectToStringValue(JSGlobalObject* globalObject, VM& vm, JSString* value, PropertySlot toStringTagSymbolSlot)
{
    if (!hasRareData())
        allocateRareData(vm);
    rareData()->setObjectToStringValue(globalObject, vm, this, value, toStringTagSymbolSlot);
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::add(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    PropertyTable* table = ensurePropertyTable(vm);

    GCSafeConcurrentJSCellLocker locker(cellLock(), vm.heap);

    switch (shouldPin) {
    case ShouldPin::Yes:
        pin(locker, vm, table);
        break;
    case ShouldPin::No:
        setPropertyTable(vm, table);
        break;
    }
    
    ASSERT(!JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();
    if (attributes & PropertyAttribute::DontEnum || propertyName.isSymbol())
        setIsQuickPropertyAccessAllowedForEnumeration(false);
    if (propertyName == vm.propertyNames->underscoreProto)
        setHasUnderscoreProtoPropertyExcludingOriginalProto(true);

    auto rep = propertyName.uid();

    PropertyOffset newOffset = table->nextOffset(inlineCapacity());

    addPropertyHashAndSeenProperty(rep->existingSymbolAwareHash(), rep);

    auto result = table->add(PropertyMapEntry(rep, newOffset, attributes));
    ASSERT_UNUSED(result, result.second);
    ASSERT_UNUSED(result, result.first.first->offset == newOffset);
    auto newMaxOffset = std::max(newOffset, maxOffset());
    
    func(locker, newOffset, newMaxOffset);
    
    ASSERT(maxOffset() == newMaxOffset);

    checkConsistency();
    return newOffset;
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::remove(VM& vm, PropertyName propertyName, const Func& func)
{
    PropertyTable* table = ensurePropertyTable(vm);
    GCSafeConcurrentJSCellLocker locker(cellLock(), vm.heap);

    switch (shouldPin) {
    case ShouldPin::Yes:
        pin(locker, vm, table);
        break;
    case ShouldPin::No:
        setPropertyTable(vm, table);
        break;
    }

    ASSERT(JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();

    auto rep = propertyName.uid();

    PropertyTable::find_iterator position = table->find(rep);
    if (!position.first)
        return invalidOffset;

    setIsQuickPropertyAccessAllowedForEnumeration(false);
    
    PropertyOffset offset = position.first->offset;

    table->remove(position);
    table->addDeletedOffset(offset);

    PropertyOffset newMaxOffset = maxOffset();

    func(locker, offset, newMaxOffset);

    ASSERT(maxOffset() == newMaxOffset);
    ASSERT(!JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();
    return offset;
}

template<typename Func>
inline PropertyOffset Structure::addPropertyWithoutTransition(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    return add<ShouldPin::Yes>(vm, propertyName, attributes, func);
}

template<typename Func>
inline PropertyOffset Structure::removePropertyWithoutTransition(VM& vm, PropertyName propertyName, const Func& func)
{
    ASSERT(isUncacheableDictionary());
    ASSERT(isPinnedPropertyTable());
    ASSERT(propertyTableUnsafeOrNull());
    
    return remove<ShouldPin::Yes>(vm, propertyName, func);
}

ALWAYS_INLINE void Structure::setPrototypeWithoutTransition(VM& vm, JSValue prototype)
{
    ASSERT(isValidPrototype(prototype));
    m_prototype.set(vm, this, prototype);
}

ALWAYS_INLINE void Structure::setGlobalObject(VM& vm, JSGlobalObject* globalObject)
{
    m_globalObject.set(vm, this, globalObject);
}

ALWAYS_INLINE void Structure::setPropertyTable(VM& vm, PropertyTable* table)
{
#if CPU(ADDRESS64)
    m_outOfLineTypeFlagsAndPropertyTableUnsafe.setPointer(table);
    vm.heap.writeBarrier(this, table);
#else
    m_propertyTableUnsafe.setMayBeNull(vm, this, table);
#endif
}

ALWAYS_INLINE void Structure::clearPropertyTable()
{
#if CPU(ADDRESS64)
    m_outOfLineTypeFlagsAndPropertyTableUnsafe.setPointer(nullptr);
#else
    m_propertyTableUnsafe.clear();
#endif
}

ALWAYS_INLINE void Structure::setOutOfLineTypeFlags(TypeInfo::OutOfLineTypeFlags outOfLineTypeFlags)
{
#if CPU(ADDRESS64)
    m_outOfLineTypeFlagsAndPropertyTableUnsafe.setType(outOfLineTypeFlags);
#else
    m_outOfLineTypeFlags = outOfLineTypeFlags;
#endif
}

ALWAYS_INLINE void Structure::setInlineCapacity(uint8_t inlineCapacity)
{
#if CPU(ADDRESS64)
    m_inlineCapacityAndCachedPrototypeChainOrRareData.setType(inlineCapacity);
#else
    m_inlineCapacity = inlineCapacity;
#endif
}

ALWAYS_INLINE void Structure::setClassInfo(const ClassInfo* classInfo)
{
#if CPU(ADDRESS64)
    m_transitionOffsetAndClassInfo.setPointer(classInfo);
#else
    m_classInfo = classInfo;
#endif
}

ALWAYS_INLINE void Structure::setPreviousID(VM& vm, Structure* structure)
{
    ASSERT(structure);
    m_previousID = structure->id();
    vm.heap.writeBarrier(this, structure);
}

inline void Structure::clearPreviousID()
{
    m_previousID = 0;
}

ALWAYS_INLINE bool Structure::shouldConvertToPolyProto(const Structure* a, const Structure* b)
{
    if (!a || !b)
        return false;

    if (a == b)
        return false;

    if (a->propertyHash() != b->propertyHash())
        return false;

    // We only care about objects created via a constructor's to_this. These
    // all have Structures with rare data and a sharedPolyProtoWatchpoint.
    if (!a->hasRareData() || !b->hasRareData())
        return false;

    // We only care about Structure's generated from functions that share
    // the same executable.
    const Box<InlineWatchpointSet>& aInlineWatchpointSet = a->rareData()->sharedPolyProtoWatchpoint();
    const Box<InlineWatchpointSet>& bInlineWatchpointSet = b->rareData()->sharedPolyProtoWatchpoint();
    if (aInlineWatchpointSet.get() != bInlineWatchpointSet.get() || !aInlineWatchpointSet)
        return false;
    ASSERT(aInlineWatchpointSet && bInlineWatchpointSet && aInlineWatchpointSet.get() == bInlineWatchpointSet.get());

    if (a->hasPolyProto() || b->hasPolyProto())
        return false;

    if (a->storedPrototype() == b->storedPrototype())
        return false;

    VM& vm = a->vm();
    JSObject* aObj = a->storedPrototypeObject();
    JSObject* bObj = b->storedPrototypeObject();
    while (aObj && bObj) {
        a = aObj->structure(vm);
        b = bObj->structure(vm);

        if (a->propertyHash() != b->propertyHash())
            return false;

        aObj = a->storedPrototypeObject(aObj);
        bObj = b->storedPrototypeObject(bObj);
    }

    return !aObj && !bObj;
}

inline Structure* Structure::nonPropertyTransition(VM& vm, Structure* structure, NonPropertyTransition transitionKind)
{
    IndexingType indexingModeIncludingHistory = newIndexingType(structure->indexingModeIncludingHistory(), transitionKind);

    if (changesIndexingType(transitionKind)) {
        if (JSGlobalObject* globalObject = structure->m_globalObject.get()) {
            if (globalObject->isOriginalArrayStructure(structure)) {
                Structure* result = globalObject->originalArrayStructureForIndexingType(indexingModeIncludingHistory);
                if (result->indexingModeIncludingHistory() == indexingModeIncludingHistory) {
                    structure->didTransitionFromThisStructure();
                    return result;
                }
            }
        }
    }

    return nonPropertyTransitionSlow(vm, structure, transitionKind);
}

} // namespace JSC
