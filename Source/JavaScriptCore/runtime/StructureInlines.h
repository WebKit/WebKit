/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

namespace JSC {

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
    ASSERT(object->structure() == this);
    if (hasMonoProto())
        return storedPrototype();
    return object->getDirect(knownPolyProtoOffset);
}

ALWAYS_INLINE JSObject* Structure::storedPrototypeObject(const JSObject* object) const
{
    ASSERT(object->structure() == this);
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
    bool hasInferredType;
    return get(vm, propertyName, attributes, hasInferredType);
}
    
ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName, unsigned& attributes)
{
    bool hasInferredType;
    return get(vm, propertyName, attributes, hasInferredType);
}

ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, PropertyName propertyName, unsigned& attributes, bool& hasInferredType)
{
    ASSERT(!isCompilationThread());
    ASSERT(structure(vm)->classInfo() == info());

    PropertyTable* propertyTable = ensurePropertyTableIfNotEmpty(vm);
    if (!propertyTable)
        return invalidOffset;

    PropertyMapEntry* entry = propertyTable->get(propertyName.uid());
    if (!entry)
        return invalidOffset;

    attributes = entry->attributes;
    hasInferredType = entry->hasInferredType;
    return entry->offset;
}

template<typename Functor>
void Structure::forEachPropertyConcurrently(const Functor& functor)
{
    Vector<Structure*, 8> structures;
    Structure* structure;
    PropertyTable* table;
    
    findStructuresAndMapForMaterialization(structures, structure, table);
    
    if (table) {
        for (auto& entry : *table) {
            if (!functor(entry)) {
                structure->m_lock.unlock();
                return;
            }
        }
        structure->m_lock.unlock();
    }
    
    for (unsigned i = structures.size(); i--;) {
        structure = structures[i];
        if (!structure->m_nameInPrevious)
            continue;
        
        if (!functor(PropertyMapEntry(structure->m_nameInPrevious.get(), structure->m_offset, structure->attributesInPrevious())))
            return;
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
    for (Structure* current = this; current; current = current->previousID()) {
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

inline StructureChain* Structure::prototypeChain(VM& vm, JSGlobalObject* globalObject, JSObject* base) const
{
    ASSERT(base->structure(vm) == this);
    // We cache our prototype chain so our clients can share it.
    if (!isValid(globalObject, m_cachedPrototypeChain.get(), base)) {
        JSValue prototype = prototypeForLookup(globalObject, base);
        m_cachedPrototypeChain.set(vm, this, StructureChain::create(vm, prototype.isNull() ? nullptr : asObject(prototype)));
    }
    return m_cachedPrototypeChain.get();
}

inline StructureChain* Structure::prototypeChain(ExecState* exec, JSObject* base) const
{
    return prototypeChain(exec->vm(), exec->lexicalGlobalObject(), base);
}

inline bool Structure::isValid(JSGlobalObject* globalObject, StructureChain* cachedPrototypeChain, JSObject* base) const
{
    if (!cachedPrototypeChain)
        return false;

    VM& vm = globalObject->vm();
    JSValue prototype = prototypeForLookup(globalObject, base);
    WriteBarrier<Structure>* cachedStructure = cachedPrototypeChain->head();
    while (*cachedStructure && !prototype.isNull()) {
        if (asObject(prototype)->structure(vm) != cachedStructure->get())
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
    set->fireAll(*vm(), "Property did get replaced");
}

inline WatchpointSet* Structure::propertyReplacementWatchpointSet(PropertyOffset offset)
{
    ConcurrentJSLocker locker(m_lock);
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
    unsigned inlineOverflowAccordingToTotalSize = totalSize < m_inlineCapacity ? 0 : totalSize - m_inlineCapacity;

    auto fail = [&] (const char* description) {
        dataLog("Detected offset inconsistency: ", description, "!\n");
        dataLog("this = ", RawPointer(this), "\n");
        dataLog("m_offset = ", m_offset, "\n");
        dataLog("m_inlineCapacity = ", m_inlineCapacity, "\n");
        dataLog("propertyTable = ", RawPointer(propertyTable), "\n");
        dataLog("numberOfSlotsForLastOffset = ", numberOfSlotsForLastOffset(m_offset, m_inlineCapacity), "\n");
        dataLog("totalSize = ", totalSize, "\n");
        dataLog("inlineOverflowAccordingToTotalSize = ", inlineOverflowAccordingToTotalSize, "\n");
        dataLog("numberOfOutOfLineSlotsForLastOffset = ", numberOfOutOfLineSlotsForLastOffset(m_offset), "\n");
        detailsFunc();
        UNREACHABLE_FOR_PLATFORM();
    };
    
    if (numberOfSlotsForLastOffset(m_offset, m_inlineCapacity) != totalSize)
        fail("numberOfSlotsForLastOffset doesn't match totalSize");
    if (inlineOverflowAccordingToTotalSize != numberOfOutOfLineSlotsForLastOffset(m_offset))
        fail("inlineOverflowAccordingToTotalSize doesn't match numberOfOutOfLineSlotsForLastOffset");

    return true;
}

ALWAYS_INLINE bool Structure::checkOffsetConsistency() const
{
    PropertyTable* propertyTable = propertyTableOrNull();

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

inline void Structure::setObjectToStringValue(ExecState* exec, VM& vm, JSString* value, PropertySlot toStringTagSymbolSlot)
{
    if (!hasRareData())
        allocateRareData(vm);
    rareData()->setObjectToStringValue(exec, vm, this, value, toStringTagSymbolSlot);
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::add(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    PropertyTable* table = ensurePropertyTable(vm);

    GCSafeConcurrentJSLocker locker(m_lock, vm.heap);

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

    PropertyOffset newOffset = table->nextOffset(m_inlineCapacity);

    m_propertyHash = m_propertyHash ^ rep->existingSymbolAwareHash();
    
    PropertyOffset newLastOffset = m_offset;
    table->add(PropertyMapEntry(rep, newOffset, attributes), newLastOffset, PropertyTable::PropertyOffsetMayChange);
    
    func(locker, newOffset, newLastOffset);
    
    ASSERT(m_offset == newLastOffset);

    checkConsistency();
    return newOffset;
}

template<typename Func>
inline PropertyOffset Structure::remove(PropertyName propertyName, const Func& func)
{
    ConcurrentJSLocker locker(m_lock);
    
    checkConsistency();

    auto rep = propertyName.uid();
    
    // We ONLY remove from uncacheable dictionaries, which will have a pinned property table.
    // The only way for them not to have a table is if they are empty.
    PropertyTable* table = propertyTableOrNull();

    if (!table)
        return invalidOffset;

    PropertyTable::find_iterator position = table->find(rep);
    if (!position.first)
        return invalidOffset;
    
    PropertyOffset offset = position.first->offset;

    table->remove(position);
    table->addDeletedOffset(offset);

    checkConsistency();

    func(locker, offset);
    return offset;
}

template<typename Func>
inline PropertyOffset Structure::addPropertyWithoutTransition(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    return add<ShouldPin::Yes>(vm, propertyName, attributes, func);
}

template<typename Func>
inline PropertyOffset Structure::removePropertyWithoutTransition(VM&, PropertyName propertyName, const Func& func)
{
    ASSERT(isUncacheableDictionary());
    ASSERT(isPinnedPropertyTable());
    ASSERT(propertyTableOrNull());
    
    return remove(propertyName, func);
}

ALWAYS_INLINE void Structure::setPrototypeWithoutTransition(VM& vm, JSValue prototype)
{
    m_prototype.set(vm, this, prototype);
}

ALWAYS_INLINE void Structure::setGlobalObject(VM& vm, JSGlobalObject* globalObject)
{
    m_globalObject.set(vm, this, globalObject);
}

ALWAYS_INLINE void Structure::setPropertyTable(VM& vm, PropertyTable* table)
{
    m_propertyTableUnsafe.setMayBeNull(vm, this, table);
}

ALWAYS_INLINE void Structure::setPreviousID(VM& vm, Structure* structure)
{
    if (hasRareData())
        rareData()->setPreviousID(vm, structure);
    else
        m_previousOrRareData.set(vm, this, structure);
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

    VM& vm = *a->vm();
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
