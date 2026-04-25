/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/BigIntPrototype.h>
#include <JavaScriptCore/BrandedStructure.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/PropertyTable.h>
#include <JavaScriptCore/StringPrototype.h>
#include <JavaScriptCore/StructureArrayStorageInlines.h>
#include <JavaScriptCore/StructureChain.h>
#include <JavaScriptCore/StructureCreateInlines.h>
#include <JavaScriptCore/StructureRareDataInlines.h>
#include <JavaScriptCore/SymbolPrototype.h>
#include <JavaScriptCore/WebAssemblyGCStructure.h>
#include <JavaScriptCore/WriteBarrierInlines.h>
#include <wtf/Threading.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

inline Structure* Structure::create(VM& vm, Structure* previous, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(vm.structureStructure);
    switch (previous->variant()) {
    case StructureVariant::Normal: {
        auto* result = new (NotNull, allocateCell<Structure>(vm)) Structure(vm, previous->variant(), previous);
        result->finishCreation(vm, previous, deferred);
        return result;
    }
    case StructureVariant::Branded: {
        auto* result = new (NotNull, allocateCell<BrandedStructure>(vm)) BrandedStructure(vm, uncheckedDowncast<BrandedStructure>(previous));
        result->finishCreation(vm, previous, deferred);
        return result;
    }
    case StructureVariant::WebAssemblyGC: {
        RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("WebAssemblyGCStructure should not do transition");
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
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
    ASSERT(structure()->classInfoForCells() == info());

    if (m_seenProperties.ruleOut(CompactPtr<UniquedStringImpl>::encode(propertyName.uid())))
        return invalidOffset;

    PropertyTable* propertyTable = ensurePropertyTableIfNotEmpty(vm);
    if (!propertyTable)
        return invalidOffset;

    auto [offset, entryAttributes] = propertyTable->get(propertyName.uid());
    if (offset != invalidOffset)
        attributes = entryAttributes;
    return offset;
}

template<typename Functor>
void Structure::forEachPropertyConcurrently(const Functor& functor)
{
    Vector<Structure*, 8> structures;
    Structure* tableStructure;
    PropertyTable* table;
    
    bool didFindStructure = findStructuresAndMapForMaterialization(structures, tableStructure, table);

    UncheckedKeyHashSet<UniquedStringImpl*> seenProperties;

    for (auto* structure : structures) {
        if (!structure->m_transitionPropertyName || seenProperties.contains(structure->m_transitionPropertyName.get()))
            continue;

        seenProperties.add(structure->m_transitionPropertyName.get());

        switch (structure->transitionKind()) {
        case TransitionKind::PropertyAddition:
        case TransitionKind::PropertyAttributeChange:
            break;
        case TransitionKind::PropertyDeletion:
        case TransitionKind::SetBrand:
            continue;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        if (!functor(PropertyTableEntry(structure->m_transitionPropertyName.get(), structure->transitionOffset(), structure->transitionPropertyAttributes()))) {
            if (didFindStructure) {
                assertIsHeld(tableStructure->m_lock); // Sadly Clang needs some help here.
                tableStructure->m_lock.unlock();
            }
            return;
        }
    }
    
    if (didFindStructure) {
        assertIsHeld(tableStructure->m_lock); // Sadly Clang needs some help here.
        table->forEachProperty([&](const auto& entry) {
            if (seenProperties.contains(entry.key()))
                return IterationStatus::Continue;

            if (!functor(entry))
                return IterationStatus::Done;

            return IterationStatus::Continue;
        });
        tableStructure->m_lock.unlock();
    }
}

template<typename Functor>
void Structure::forEachProperty(VM& vm, const Functor& functor)
{
    if (PropertyTable* table = ensurePropertyTableIfNotEmpty(vm)) {
        table->forEachProperty([&](const auto& entry) {
            if (!functor(entry))
                return IterationStatus::Done;
            return IterationStatus::Continue;
        });
        ensureStillAliveHere(table);
    }
}

inline bool Structure::hasIndexingHeader(const JSCell* cell) const
{
    if (hasIndexedProperties(indexingType()))
        return true;
    
    if (!isTypedView(m_blob.type()))
        return false;

    TypedArrayMode mode = uncheckedDowncast<JSArrayBufferView>(cell)->mode();
    return isWastefulTypedArray(mode);
}

inline void Structure::setCachedPropertyNames(VM& vm, CachedPropertyNamesKind kind, JSCellButterfly* cached)
{
    ensureRareData(vm)->setCachedPropertyNames(vm, kind, cached);
}

ALWAYS_INLINE JSValue prototypeForLookupPrimitiveImpl(JSGlobalObject* globalObject, const Structure* structure)
{
    ASSERT(!structure->isObject());

    if (structure->typeInfo().type() == StringType)
        return globalObject->stringPrototype();
    
    if (structure->typeInfo().type() == HeapBigIntType)
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
    ASSERT(base->structure() == this);
    // We cache our prototype chain so our clients can share it.
    if (!isValid(globalObject, m_cachedPrototypeChain.get(), base)) {
        JSValue prototype = prototypeForLookup(globalObject, base);
        const_cast<Structure*>(this)->clearCachedPrototypeChain();
        m_cachedPrototypeChain.set(vm, this, StructureChain::create(vm, prototype.isNull() ? nullptr : asObject(prototype)));
    }
    return m_cachedPrototypeChain.get();
}

inline bool Structure::isValid(JSGlobalObject* globalObject, StructureChain* cachedPrototypeChain, JSObject* base) const
{
    if (!cachedPrototypeChain)
        return false;

    JSValue prototype = prototypeForLookup(globalObject, base);
    StructureID* cachedStructure = cachedPrototypeChain->head();
    while (*cachedStructure && !prototype.isNull()) {
        if (asObject(prototype)->structureID() != *cachedStructure)
            return false;
        ++cachedStructure;
        prototype = asObject(prototype)->getPrototypeDirect();
    }
    return prototype.isNull() && !*cachedStructure;
}

inline void Structure::didCachePropertyReplacement(VM& vm, PropertyOffset offset)
{
    ASSERT(isValidOffset(offset));
    firePropertyReplacementWatchpointSet(vm, offset, "Did cache property replacement");
}

inline WatchpointSet* Structure::propertyReplacementWatchpointSet(PropertyOffset offset)
{
    ConcurrentJSLocker locker(m_lock);
    StructureRareData* rareData = tryRareData();
    if (!rareData)
        return nullptr;
    if (!rareData->m_replacementWatchpointSets.isNullStorage())
        return rareData->m_replacementWatchpointSets.get(offset);
    return nullptr;
}

inline size_t nextOutOfLineStorageCapacity(size_t currentCapacity)
{
    if (!currentCapacity)
        return initialOutOfLineCapacity;
    return currentCapacity * outOfLineGrowthFactor;
}

inline void Structure::cacheSpecialProperty(JSGlobalObject* globalObject, VM& vm, JSValue value, CachedSpecialPropertyKey key, const PropertySlot& slot)
{
    if (!hasRareData())
        allocateRareData(vm);
    rareData()->cacheSpecialProperty(globalObject, vm, this, value, key, slot);
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::add(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    ASSERT(!isCompilationThread());
    PropertyTable* table = ensurePropertyTable(vm);

    GCSafeConcurrentJSLocker locker(m_lock, vm);

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
    if (attributes & PropertyAttribute::ReadOnly)
        setContainsReadOnlyProperties();
    if (attributes & PropertyAttribute::DontEnum)
        setHasNonEnumerableProperties(true);
    if (attributes & PropertyAttribute::DontDelete) {
        setHasNonConfigurableProperties(true);
        if (attributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)
            setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    }
    if (propertyName == vm.propertyNames->underscoreProto)
        setHasUnderscoreProtoPropertyExcludingOriginalProto(true);
    else if (propertyName == vm.propertyNames->then)
        setHasSpecialProperties(true);

    auto rep = propertyName.uid();

    PropertyOffset newOffset = table->nextOffset(m_inlineCapacity);

    m_propertyHash = m_propertyHash ^ rep->existingSymbolAwareHash();
    m_seenProperties.add(CompactPtr<UniquedStringImpl>::encode(rep));

    auto [offset, attribute, result] = table->add(vm, PropertyTableEntry(rep, newOffset, attributes));
    ASSERT_UNUSED(result, result);
    ASSERT_UNUSED(offset, offset == newOffset);
    UNUSED_VARIABLE(attribute);
    auto newMaxOffset = std::max(newOffset, maxOffset());
    
    func(locker, newOffset, newMaxOffset);
    
    ASSERT(maxOffset() == newMaxOffset);

    checkConsistency();
    return newOffset;
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::remove(VM& vm, PropertyName propertyName, const Func& func)
{
    ASSERT(!isCompilationThread());
    PropertyTable* table = ensurePropertyTable(vm);
    GCSafeConcurrentJSLocker locker(m_lock, vm);

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

    auto [offset, attributes] = table->take(vm, rep);
    UNUSED_VARIABLE(attributes);
    if (offset == invalidOffset)
        return invalidOffset;

    setIsQuickPropertyAccessAllowedForEnumeration(false);

    table->addDeletedOffset(offset);

    PropertyOffset newMaxOffset = maxOffset();

    func(locker, offset, newMaxOffset);

    ASSERT(maxOffset() == newMaxOffset);
    ASSERT(!JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();
    return offset;
}

template<Structure::ShouldPin shouldPin, typename Func>
inline PropertyOffset Structure::attributeChange(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    ASSERT(!isCompilationThread());
    PropertyTable* table = ensurePropertyTable(vm);

    GCSafeConcurrentJSLocker locker(m_lock, vm);

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
    PropertyOffset offset = table->updateAttributeIfExists(propertyName.uid(), attributes);
    if (offset == invalidOffset)
        return offset;

    if (attributes & PropertyAttribute::DontEnum) {
        setHasNonEnumerableProperties(true);
        setIsQuickPropertyAccessAllowedForEnumeration(false);
    }
    if (attributes & PropertyAttribute::DontDelete) {
        setHasNonConfigurableProperties(true);
        if (attributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)
            setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    }
    if (attributes & PropertyAttribute::ReadOnly)
        setContainsReadOnlyProperties();

    PropertyOffset newMaxOffset = maxOffset();

    func(locker, offset, newMaxOffset);

    ASSERT(maxOffset() == newMaxOffset);
    ASSERT(JSC::isValidOffset(get(vm, propertyName)));

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
    ASSERT(propertyTableOrNull());
    
    return remove<ShouldPin::Yes>(vm, propertyName, func);
}

template<typename Func>
ALWAYS_INLINE auto Structure::addOrReplacePropertyWithoutTransition(VM& vm, PropertyName propertyName, unsigned newAttributes, const Func& func) -> decltype(auto)
{
    ASSERT(!isCompilationThread());
    PropertyTable* table = ensurePropertyTable(vm);

    auto rep = propertyName.uid();
    auto findResult = table->find(rep);
    if (findResult.offset != invalidOffset)
        return std::tuple { findResult.offset, findResult.attributes, false };

    GCSafeConcurrentJSLocker locker(m_lock, vm);

    pin(locker, vm, table);

    ASSERT(!JSC::isValidOffset(get(vm, propertyName)));

    checkConsistency();
    if (newAttributes & PropertyAttribute::DontEnum || propertyName.isSymbol())
        setIsQuickPropertyAccessAllowedForEnumeration(false);
    if (newAttributes & PropertyAttribute::ReadOnly)
        setContainsReadOnlyProperties();
    if (newAttributes & PropertyAttribute::DontEnum)
        setHasNonEnumerableProperties(true);
    if (newAttributes & PropertyAttribute::DontDelete) {
        setHasNonConfigurableProperties(true);
        if (newAttributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)
            setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    }
    if (propertyName == vm.propertyNames->underscoreProto)
        setHasUnderscoreProtoPropertyExcludingOriginalProto(true);
    else if (propertyName == vm.propertyNames->then)
        setHasSpecialProperties(true);

    PropertyOffset newOffset = table->nextOffset(m_inlineCapacity);

    m_propertyHash = m_propertyHash ^ rep->existingSymbolAwareHash();
    m_seenProperties.add(CompactPtr<UniquedStringImpl>::encode(rep));

    auto [offset, attributes, result] = table->addAfterFind(vm, PropertyTableEntry(rep, newOffset, newAttributes), WTF::move(findResult));
    ASSERT_UNUSED(result, result);
    ASSERT_UNUSED(offset, offset == newOffset);
    UNUSED_VARIABLE(attributes);
    auto newMaxOffset = std::max(newOffset, maxOffset());

    func(locker, newOffset, newMaxOffset);

    ASSERT(maxOffset() == newMaxOffset);

    checkConsistency();
    return std::tuple { newOffset, newAttributes, true };
}

template<typename Func>
inline PropertyOffset Structure::attributeChangeWithoutTransition(VM& vm, PropertyName propertyName, unsigned attributes, const Func& func)
{
    return attributeChange<ShouldPin::Yes>(vm, propertyName, attributes, func);
}

ALWAYS_INLINE void Structure::setPrototypeWithoutTransition(VM& vm, JSValue prototype)
{
    ASSERT(isValidPrototype(prototype));
    m_prototype.set(vm, this, prototype);
}

ALWAYS_INLINE void Structure::setRealm(VM& vm, JSGlobalObject* globalObject)
{
    m_realm.set(vm, this, globalObject);
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

inline void Structure::pin(const AbstractLocker&, VM& vm, PropertyTable* table)
{
    setIsPinnedPropertyTable(true);
    setPropertyTable(vm, table);
    clearPreviousID();
    m_transitionPropertyName = nullptr;
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
    if (!aInlineWatchpointSet || !bInlineWatchpointSet || aInlineWatchpointSet.get() != bInlineWatchpointSet.get())
        return false;
    ASSERT(aInlineWatchpointSet && bInlineWatchpointSet && aInlineWatchpointSet.get() == bInlineWatchpointSet.get());

    if (a->hasPolyProto() || b->hasPolyProto())
        return false;

    if (a->storedPrototype() == b->storedPrototype())
        return false;

    JSObject* aObj = a->storedPrototypeObject();
    JSObject* bObj = b->storedPrototypeObject();
    while (aObj && bObj) {
        a = aObj->structure();
        b = bObj->structure();

        if (a->propertyHash() != b->propertyHash())
            return false;

        aObj = a->storedPrototypeObject(aObj);
        bObj = b->storedPrototypeObject(bObj);
    }

    return !aObj && !bObj;
}

inline Structure* Structure::nonPropertyTransition(VM& vm, Structure* structure, TransitionKind transitionKind, DeferredStructureTransitionWatchpointFire* deferred)
{
    if (changesIndexingType(transitionKind)) {
        if (JSGlobalObject* globalObject = structure->m_realm.get()) {
            if (globalObject->isOriginalArrayStructure(structure)) {
                IndexingType indexingModeIncludingHistory = newIndexingType(structure->indexingModeIncludingHistory(), transitionKind);
                Structure* result = globalObject->originalArrayStructureForIndexingType(indexingModeIncludingHistory);
                if (result->indexingModeIncludingHistory() == indexingModeIncludingHistory) {
                    structure->didTransitionFromThisStructure(deferred);
                    return result;
                }
            }
        }
    }

    return nonPropertyTransitionSlow(vm, structure, transitionKind, deferred);
}

ALWAYS_INLINE Structure* Structure::addPropertyTransitionToExistingStructureImpl(Structure* structure, UniquedStringImpl* uid, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());

    offset = invalidOffset;

    if (structure->hasBeenDictionary())
        return nullptr;

    if (Structure* existingTransition = structure->m_transitionTable.get(uid, attributes, TransitionKind::PropertyAddition)) {
        validateOffset(existingTransition->transitionOffset(), existingTransition->inlineCapacity());
        offset = existingTransition->transitionOffset();
        return existingTransition;
    }

    return nullptr;
}

ALWAYS_INLINE Structure* Structure::addPropertyTransitionToExistingStructure(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!isCompilationThread());
    return addPropertyTransitionToExistingStructureImpl(structure, propertyName.uid(), attributes, offset);
}

ALWAYS_INLINE Structure* Structure::addPropertyTransitionToExistingStructureConcurrently(Structure* structure, UniquedStringImpl* uid, unsigned attributes, PropertyOffset& offset)
{
    ConcurrentJSLocker locker(structure->m_lock);
    return addPropertyTransitionToExistingStructureImpl(structure, uid, attributes, offset);
}

ALWAYS_INLINE StructureTransitionTable::Hash::Key StructureTransitionTable::Hash::createKeyFromStructure(Structure* structure)
{
    switch (structure->transitionKind()) {
    case TransitionKind::ChangePrototype:
        return StructureTransitionTable::Hash::createKey(structure->storedPrototype().isNull() ? nullptr : asObject(structure->storedPrototype()), structure->transitionPropertyAttributes(), structure->transitionKind());
    default:
        return StructureTransitionTable::Hash::createKey(structure->m_transitionPropertyName.get(), structure->transitionPropertyAttributes(), structure->transitionKind());
    }
}

inline Structure* StructureTransitionTable::trySingleTransition() const
{
    uintptr_t pointer = m_data;
    if (pointer & UsingSingleSlotFlag)
        return std::bit_cast<Structure*>(pointer & ~UsingSingleSlotFlag);
    return nullptr;
}

inline Structure* StructureTransitionTable::get(PointerKey rep, unsigned attributes, TransitionKind transitionKind) const
{
    if (isUsingSingleSlot()) {
        auto* transition = trySingleTransition();
        if (!transition)
            return nullptr;
        if (Hash::createKeyFromStructure(transition) != Hash::createKey(rep, attributes, transitionKind))
            return nullptr;
        return transition;
    }
    return map()->get(StructureTransitionTable::Hash::createKey(rep, attributes, transitionKind));
}

inline void StructureTransitionTable::finalizeUnconditionally(VM& vm, CollectionScope)
{
    if (auto* transition = trySingleTransition()) {
        if (!vm.heap.isMarked(transition))
            m_data = UsingSingleSlotFlag;
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
