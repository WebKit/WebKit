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
#include <JavaScriptCore/StructureCache.h>
#include <JavaScriptCore/StructureChain.h>
#include <JavaScriptCore/StructureCreateInlines.h>
#include <JavaScriptCore/StructureInlinesLight.h>
#include <JavaScriptCore/StructureRareDataInlines.h>
#include <JavaScriptCore/SymbolPrototype.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
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
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setHasRawDoubleFields(true);
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

    // Per-offset raw-double mask, paired with the summary bit set above.
    //
    // MUST COME AFTER func(), WHICH IS WHAT PUBLISHES THE NEW maxOffset. setRawDoubleOffset gates on
    // isValidOffset(offset), and isValidOffset compares against maxOffset() -- so calling it before the update means
    // the property being added is ALWAYS out of range and the bit is silently declined. Measured on Box2D:
    // every decline was `offset=N validOffset=false maxOffset=N-1`, i.e. the new property itself, 1152 times.
    //
    // The consequence is the worst possible one, because it breaks the mask/attributes invariant in the UNSAFE
    // direction: the attributes byte carries RepresentationDouble (so putDirectOffsetRawDoubleAware stores raw bits)
    // while the mask says boxed (so every reader, and GC tracing, interprets those bits as a JSValue). For a slot
    // holding 0.0 the raw bits are 0x0 -- the empty JSValue -- which is the Box2D b2Body.m_angularDamping crash:
    // EXC_BAD_ACCESS at JSCell::type via slow_path_inc, from #CSK4YU bc#208.
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setRawDoubleOffset(vm, newOffset);

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

    // Clear the per-offset raw-double bit as the offset is freed. addDeletedOffset() puts it on the free list, so the
    // NEXT property added to this Structure can land here -- and if that property is an Int32 while a stale bit still
    // says "raw", the mask OVER-claims: readers reconstruct a boxed slot and GC tracing reads double bits as a
    // pointer. Measured before this line existed: `o.a=1.5; o.b=2.5; delete o.b; o.d=42` left offset 1 with
    // attrsSayDouble=false but maskBit=true. The summary bit deliberately stays set: it may over-approximate, costing
    // only an extra mask test, but the per-offset bit may not (Structure.h, m_rawDoubleMask).
    clearRawDoubleOffset(offset);

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
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setHasRawDoubleFields(true);
    // `offset` is already known on this path. An attribute update can also REMOVE the Double representation, so
    // clear as well as set: a stale set bit would over-claim, the unsafe direction (Structure.h, m_rawDoubleMask).
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setRawDoubleOffset(vm, offset);
    else
        clearRawDoubleOffset(offset);
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
    if (newAttributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setHasRawDoubleFields(true);
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

    // Per-offset mask, AFTER func() publishes the new maxOffset -- setRawDoubleOffset gates on isValidOffset, which
    // compares against maxOffset(), so setting it earlier always declines the property being added. Same defect and
    // same reasoning as Structure::add above.
    if (newAttributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        setRawDoubleOffset(vm, newOffset);

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

// Per-offset raw-double map. Two levels of laziness -- StructureRareData, then a side table inside it -- because
// neither Structure nor StructureRareData has room to spare; see StructureRareData::m_rawDoubleMask.
//
// READ PATH runs on GC marking threads (JSObject.cpp, the two property-storage visit sites), so it must stay
// lock-free and allocation-free. hasRawDoubleFields() rejects the overwhelming majority before any load, and both
// pointers for a Structure that answered yes were published before the Structure became reachable.
inline const RawDoubleMask* Structure::rawDoubleMaskIfAny() const
{
    if (!hasRawDoubleFields()) [[likely]]
        return nullptr;
    // THE OPTION IS PART OF THE QUESTION, NOT A FILTER SOME CALLERS MAY APPLY. Whether the slot physically holds raw
    // bits is decided by putDirectOffsetRawDoubleAware, which is gated on this option, so an answer that ignores it
    // OVER-CLAIMS -- and the collector then skips a slot holding a live JSCell (appendNonRawDoubleValues in
    // JSObject.cpp freed 20000/20000 live objects with --useRawDoubleFieldStorage=0). Placed AFTER the summary-bit
    // early return, so a structure with no Double-represented field pays nothing.
    if (!Options::useRawDoubleFieldStorage()) [[unlikely]]
        return nullptr;
    // DO NOT add a useBoxedDoubleFieldSlots() check here. isRawDoubleOffset() -- the CLAIM predicate that the
    // compilers, the store-side coercion and $vm.isRawDoubleField all depend on -- is literally this function
    // composed with maskSaysRawDouble(). Filtering the encoding here therefore makes every claim INVISIBLE: still
    // correct, but the whole optimisation silently stops happening while every test keeps passing. Caught by
    // raw-double-field-reader-surface.js's own "sections 1-12 are not vacuous" guard. Encoding filtering belongs in
    // the consumers (see JSONObject.cpp).
    if (!hasRareData())
        return nullptr;
    return rareData()->m_rawDoubleMask.get();
}

ALWAYS_INLINE bool Structure::maskSaysRawDouble(const RawDoubleMask* mask, PropertyOffset offset)
{
    ASSERT(mask);
    unsigned bit = static_cast<unsigned>(offset);
    // NO isValidOffset() HERE, deliberately, and it is also why this half needs nothing but the mask. isValidOffset
    // calls maxOffset(), which is a load plus two branches plus -- when m_maxOffset is useRareDataFlag -- a second
    // dependent load into StructureRareData, all on the hottest read in the feature. It cannot produce a false
    // positive, the only unsafe direction: a bit is set exclusively by setRawDoubleOffset, which itself gates on
    // isValidOffset, so a SET bit already implies a valid offset. An offset the structure does not have has a clear
    // bit and answers false; invalidOffset (-1) casts to 0xFFFFFFFF and is rejected by the bound check below.
    // Measured on json-stringify-inspector: -6.11% -> -3.34%.
    if (bit >= s_rawDoubleMaskBits)
        return false;
    return mask->bits[bit / 64] & (1ULL << (bit % 64));
}

inline InlineWatchpointSet* Structure::claimWatchpointIfAny() const
{
    if (!hasRawDoubleFields()) [[likely]]
        return nullptr;
    if (!hasRareData())
        return nullptr;
    const auto* mask = rareData()->m_rawDoubleMask.get();
    if (!mask || !mask->claimRecord)
        return nullptr;
    return &mask->claimRecord->set;
}

// Creates the set on first use. Only ever called where a mask already exists, so this allocates nothing for a
// Structure with no claims.
inline void Structure::ensureClaimWatchpoint(VM& vm)
{
    if (!Options::useDoubleFieldClaimWatchpoint()) [[likely]]
        return;
    auto& slot = ensureRareData(vm)->m_rawDoubleMask;
    if (!slot)
        slot = makeUnique<RawDoubleMask>();
    if (!slot->claimRecord)
        slot->claimRecord = Box<DoubleFieldClaimRecord>::create();
}


// PHASE B2: a claim that has been GIVEN UP answers false here, for every consumer at once. That is the whole of
// "migration is free" -- clearing the claim reinterprets nothing, because under boxed slots the slot always held a
// valid JSValue, so every object of every Structure in the lineage converges in one store.
//
// The check belongs in the CLAIM predicate, not in rawDoubleMaskIfAny(): that one answers a question about the mask
// and is composed into this, so filtering there turns claims off wholesale (see the comment on it).
inline bool Structure::claimGivenUp() const
{
    if (!Options::useDoubleFieldClaimInvalidation()) [[likely]]
        return false;
    if (!hasRareData())
        return false;
    const auto* mask = rareData()->m_rawDoubleMask.get();
    if (!mask || !mask->claimRecord)
        return false;
    return mask->claimRecord->givenUp;
}

// THE FAST PATH MUST STAY TINY AND INLINE. This is queried per property store, including 6.2M times on
// json-parse-inspector via LiteralParser. When the body grew (mask walk + claimGivenUp's second rare-data walk) the
// compiler stopped inlining it, and the whole store chain de-inlined with it -- see JSObject::putDirectOffset.
// So: one bit test here, and a call only for the tiny minority of Structures that actually carry a claim.
// Kept as a single plain-inline predicate. Two variants were measured and both were worse overall: splitting it
// into an inline summary-bit test plus an out-of-line remainder cost json-parse-inspector 3 points with
// NEVER_INLINE and 0.7 without, because JSON shapes DO carry claims and query the remainder on every one of 6.2M
// stores. See repro/bugs/open/25-ROOT-CAUSE-class-c-is-code-size.md.
inline bool Structure::isRawDoubleOffset(PropertyOffset offset) const
{
    if (!mightHaveClaimAt(offset)) [[likely]]
        return false;
    const RawDoubleMask* mask = rawDoubleMaskIfAny();
    if (!mask || !maskSaysRawDouble(mask, offset))
        return false;
    return !claimGivenUp();
}

inline bool Structure::inlineCachesCanAccessPropertySlotsDirectly() const
{
    // See the long comment on the declaration in Structure.h.
    //
    // NOTE for Phase B: boxed slots make the ENCODING reason for this restriction go away -- the 64 bits are a
    // JSValue whether claimed or not. It is still held in Phase A because the CLAIM remains part of transition
    // identity, so a claim-violating store must reach the widening path. Lifting it belongs with the fork's removal.
    return !(Options::useRawDoubleFieldStorage() && hasRawDoubleFields());
}


// Whether an inline cache may resolve this slot with a bare load/store. It may not when the slot holds a raw double:
// the compile-time-emitted fast paths have no way to apply the 2^49 bias. This used to be gated on a pricing-ablation
// option, which meant a command-line flag could turn a correctness rule off; correctness must not depend on one.
inline bool Structure::inlineCachesCanAccessSlotDirectly(PropertyOffset) const
{
    // Always yes now. The restriction existed because a raw-double slot is not a JSValue and the compile-time-emitted
    // fast paths had no way to apply the 2^49 bias. Guarantee-only storage keeps every slot a JSValue.
    return true;
}

// Returns false if the offset cannot be represented, so a writer about to store raw bits can decline rather than
// create a slot no reader will recognise. Allocating here is the price of a Double-represented field existing at
// all, and is paid once per Structure, not per object.
inline bool Structure::setRawDoubleOffset(VM& vm, PropertyOffset offset)
{
    unsigned bit = static_cast<unsigned>(offset);
    if (!isValidOffset(offset) || bit >= s_rawDoubleMaskBits) {
        // Declining leaves the attributes saying Double while the mask says boxed -- the unsafe direction, since
        // readers and GC tracing trust the MASK. This log found the ordering bug where setRawDoubleOffset ran before
        // maxOffset was published (1152 declines on Box2D, all `offset=N maxOffset=N-1`); it now reports zero.
        // Kept, but rate-limited to the first few so it cannot flood a run.
        return false;
    }
    // DIAGNOSTIC: is this bit being set on a structure that is ALREADY LIVE, i.e. mutated in place rather than
    // created fresh? A dictionary adds and replaces properties without transitioning, and
    // attributeChangeTransition's uncacheable-dictionary early return (Structure.cpp:829-833) mutates in place and
    // returns the SAME structure -- with no new structure and no watchpoint fire. Any compiled code that recorded
    // Boxed for this (structure, offset) is instantly stale and will read a now-raw slot with the bias, producing
    // bits(d)-2^49. That is the direction actually observed. See 07-PLAN 5ah.
    dataLogLnIf(Options::dumpDoubleFieldSplitCensus() && isDictionary(),
        "[rawdouble] IN-PLACE mask set on a live DICTIONARY structure, offset=", offset,
        " kind=", static_cast<unsigned>(dictionaryKind()));
    auto& slot = ensureRareData(vm)->m_rawDoubleMask;
    if (!slot)
        slot = makeUnique<RawDoubleMask>();
    slot->bits[bit / 64] |= (1ULL << (bit % 64));
    // Widen the conservative range alongside the exact bit.
    if (bit < m_rawDoubleFirstOffset)
        m_rawDoubleFirstOffset = static_cast<uint8_t>(bit);
    if (bit > m_rawDoubleLastOffset)
        m_rawDoubleLastOffset = static_cast<uint8_t>(bit);
    // PHASE B1: a claim and its invalidation channel are created together, so no claim can ever exist without one.
    if (Options::useDoubleFieldClaimWatchpoint() && !slot->claimRecord) [[unlikely]] {
        slot->claimRecord = Box<DoubleFieldClaimRecord>::create();
        dataLogLnIf(Options::dumpDoubleFieldSplitCensus(), "[rawdouble] CLAIM-WATCHPOINT created structure=",
            RawPointer(this), " firstOffset=", offset);
    }
    // SUCCESS TRACE. Pairs with the DECLINED log above so "the mask bit is missing" can be told apart from "the mask
    // bit was never asked for": grep this for the structure the reader complained about. An explicit `if` rather than
    // dataLogLnIf, because dataLogLnIf still evaluates its arguments and isRawDoubleOffset() walks the rare data.
    return true;
}

// Never allocates: no side table means no bit to clear.
inline void Structure::renarrowRawDoubleRange()
{
    if (!hasRareData())
        return;
    auto* mask = rareData()->m_rawDoubleMask.get();
    if (!mask)
        return;
    unsigned first = UINT8_MAX, last = 0;
    for (unsigned bit = 0; bit < s_rawDoubleMaskBits; ++bit) {
        if (mask->bits[bit / 64] & (1ULL << (bit % 64))) {
            if (first == UINT8_MAX)
                first = bit;
            last = bit;
        }
    }
    m_rawDoubleFirstOffset = static_cast<uint8_t>(first);
    m_rawDoubleLastOffset = static_cast<uint8_t>(first == UINT8_MAX ? 0 : last);
}

inline void Structure::clearRawDoubleOffset(PropertyOffset offset)
{
    unsigned bit = static_cast<unsigned>(offset);
    if (!isValidOffset(offset) || bit >= s_rawDoubleMaskBits)
        return;
    if (!hasRareData())
        return;
    if (auto* mask = rareData()->m_rawDoubleMask.get()) {
        // CLEAR TRACE. Only reports when a bit that was actually SET goes away, which is the event that turns
        // "attributes say Double" into "mask says boxed" -- the disagreement the aware reader reports as an empty
        // value. Pairs with the SET trace in setRawDoubleOffset.
        dataLogLnIf(Options::dumpDoubleFieldSplitCensus() && (mask->bits[bit / 64] & (1ULL << (bit % 64))),
            "[rawdouble] clearRawDoubleOffset CLEARED a SET bit offset=", offset,
            " structure=", RawPointer(this), " isDictionary=", isDictionary());
        mask->bits[bit / 64] &= ~(1ULL << (bit % 64));
        renarrowRawDoubleRange();
    }
}

// Only materialises anything when the source actually has bits set, so ordinary transition chains off a mask-less
// Structure stay allocation-free.
inline void Structure::copyRawDoubleMaskFrom(VM& vm, const Structure* other)
{
    // PROPAGATION TRACE. This is the only path by which a mask reaches a transition target, and it is called ONLY
    // from finishCreation's `if (previous->hasRareData())` block. Log every early return so a lost mask shows up as
    // the exact reason it was not copied, rather than as a wrong value ten frames later.
    if (!other->hasRareData()) [[likely]] {
        dataLogLnIf(Options::dumpRawDoubleCorruption() && other->hasRawDoubleFields(),
            "[rawdouble] MASK NOT PROPAGATED: source has summaryBit but NO rare data. from=",
            RawPointer(other), " to=", RawPointer(this));
        return;
    }
    const auto* source = other->rareData()->m_rawDoubleMask.get();
    if (!source) [[likely]] {
        dataLogLnIf(Options::dumpRawDoubleCorruption() && other->hasRawDoubleFields(),
            "[rawdouble] MASK NOT PROPAGATED: source has summaryBit but NO mask. from=",
            RawPointer(other), " to=", RawPointer(this));
        return;
    }
    if (!source->bits[0] && !source->bits[1]) [[likely]] {
        dataLogLnIf(Options::dumpRawDoubleCorruption() && other->hasRawDoubleFields(),
            "[rawdouble] MASK NOT PROPAGATED: source mask is EMPTY but summaryBit set. from=",
            RawPointer(other), " to=", RawPointer(this));
        return;
    }
    auto& slot = ensureRareData(vm)->m_rawDoubleMask;
    if (!slot)
        slot = makeUnique<RawDoubleMask>();
    slot->bits = source->bits;
    // BY REFERENCE, not by value -- this is the whole reason invalidation needs no transition-tree walk. One refcount
    // bump per transition off a claiming Structure; see RawDoubleMask::claimRecord.
    slot->claimRecord = source->claimRecord;
    // The range rides the same propagation as the bits it summarises.
    m_rawDoubleFirstOffset = other->m_rawDoubleFirstOffset;
    m_rawDoubleLastOffset = other->m_rawDoubleLastOffset;
    // Deliberately NOT logged: the success path runs on every transition off a mask-carrying structure and drowns
    // the log. Only the three failure returns above report.
}

// Never allocates, and never SETS the summary bit: this only permutes bits that already existed, so a Structure with
// no mask still has none afterwards. If the permutation empties the mask (every raw property was deleted, or was
// pushed past s_rawDoubleMaskBits) leaving hasRawDoubleFields() set is harmless -- readers then take the aware branch
// and it answers "boxed", which is correct, just not free.
inline void Structure::renumberRawDoubleMask(const std::array<uint64_t, s_rawDoubleMaskWords>& bits)
{
    // Offsets have been permuted, so the old range no longer bounds them. Widen to everything the mask can represent
    // -- still correct (it is only ever used to SKIP work) and it merely costs a mask walk on this rare structure.
    m_rawDoubleFirstOffset = 0;
    m_rawDoubleLastOffset = static_cast<uint8_t>(s_rawDoubleMaskBits - 1);
    if (!hasRawDoubleFields()) [[likely]]
        return;
    if (!hasRareData())
        return;
    if (auto* mask = rareData()->m_rawDoubleMask.get())
        mask->bits = bits;
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

inline void StructureTransitionTable::reconcileWeakReferencesAtGCEnd(VM& vm, CollectionScope)
{
    if (auto* transition = trySingleTransition()) {
        if (!vm.heap.isMarked(transition))
            m_data = UsingSingleSlotFlag;
    }
}

inline void Structure::finishCreation(VM& vm, const Structure* previous, DeferredStructureTransitionWatchpointFire* deferred)
{
    this->finishCreation(vm);
    // NOTE: no per-transition logging here. It fires on essentially every structure creation once
    // rawDoubleMarkIntegerCreations is on (measured: millions of lines), which buries the rare events that matter.
    // The FAILURE cases in copyRawDoubleMaskFrom below are the ones worth reporting.
    if (previous->hasRareData()) {
        const StructureRareData* previousRareData = previous->rareData();
        if (previousRareData->hasSharedPolyProtoWatchpoint()) {
            ensureRareData(vm);
            rareData()->setSharedPolyProtoWatchpoint(previousRareData->copySharedPolyProtoWatchpoint());
        }
        // The per-offset raw-double mask must ride the same propagation as its summary bit. It is copied HERE rather
        // than in the constructor because it lives in StructureRareData, and allocating a cell mid-construction trips
        // ASSERT(!vm.isInitializingObject()). copyRawDoubleMaskFrom materialises rare data only when the source
        // actually has bits set, so this stays allocation-free for every Structure without raw-double fields.
        copyRawDoubleMaskFrom(vm, previous);
    }
    previous->fireStructureTransitionWatchpoint(deferred);
}

IGNORE_RETURN_TYPE_WARNINGS_BEGIN
ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, Concurrency concurrency, UniquedStringImpl* uid, unsigned& attributes)
{
    switch (concurrency) {
    case Concurrency::MainThread:
        ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
        return get(vm, uid, attributes);
    case Concurrency::ConcurrentThread:
        return getConcurrently(uid, attributes);
    }
}
IGNORE_RETURN_TYPE_WARNINGS_END

IGNORE_RETURN_TYPE_WARNINGS_BEGIN
ALWAYS_INLINE PropertyOffset Structure::get(VM& vm, Concurrency concurrency, UniquedStringImpl* uid)
{
    switch (concurrency) {
    case Concurrency::MainThread:
        ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
        return get(vm, uid);
    case Concurrency::ConcurrentThread:
        return getConcurrently(uid);
    }
}
IGNORE_RETURN_TYPE_WARNINGS_END

inline PropertyOffset Structure::getConcurrently(UniquedStringImpl* uid)
{
    unsigned attributesIgnored;
    return getConcurrently(uid, attributesIgnored);
}

inline void Structure::startWatchingPropertyForReplacements(VM& vm, PropertyOffset offset)
{
    ensurePropertyReplacementWatchpointSet(vm, offset);
}

inline void Structure::startWatchingInternalPropertiesIfNecessary(VM& vm)
{
    if (didWatchInternalProperties()) [[likely]]
        return;
    startWatchingInternalProperties(vm);
}

inline JSCellButterfly* Structure::cachedPropertyNames(CachedPropertyNamesKind kind) const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedPropertyNames(kind);
}

inline JSCellButterfly* Structure::cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind kind) const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedPropertyNamesIgnoringSentinel(kind);
}

inline JSValue Structure::cachedSpecialProperty(CachedSpecialPropertyKey key)
{
    if (!hasRareData())
        return JSValue();
    return rareData()->cachedSpecialProperty(key);
}

inline JSString* Structure::defaultToPrimitiveFastAndNonObservable(VM& vm)
{
    if (typeInfo().type() != FinalObjectType)
        return nullptr;

    if (!hasRareData())
        return nullptr;

    // Use the object's own realm: the cached special properties below were resolved against this
    // structure's prototype chain, so they must be compared against that realm's primordials.
    JSGlobalObject* globalObject = this->realm();
    if (!globalObject) [[unlikely]]
        return nullptr;

    if (!globalObject->objectPrototypeChainIsSaneWatchpointSet().isStillValid()) [[unlikely]]
        return nullptr;

    StructureRareData* rareData = this->rareData();
    switch (rareData->cachedHasDefaultToPrimitiveFastAndNonObservable()) {
    case TriState::True:
        return asString(rareData->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag));
    case TriState::False:
        return nullptr;
    case TriState::Indeterminate:
        break;
    }

    JSValue toPrimitive = rareData->cachedSpecialProperty(CachedSpecialPropertyKey::ToPrimitive);
    JSValue valueOf = rareData->cachedSpecialProperty(CachedSpecialPropertyKey::ValueOf);
    JSValue toString = rareData->cachedSpecialProperty(CachedSpecialPropertyKey::ToString);
    JSValue toStringTag = rareData->cachedSpecialProperty(CachedSpecialPropertyKey::ToStringTag);

    bool definitelySlow = (toPrimitive && !toPrimitive.isUndefined())
        || (valueOf && valueOf != globalObject->objectProtoValueOfFunction())
        || (toString && toString != globalObject->objectProtoToStringFunction())
        || (toStringTag && !toStringTag.isString());

    if (definitelySlow)
        rareData->setCachedHasDefaultToPrimitiveFastAndNonObservable(TriState::False);
    else {
        if (toPrimitive && valueOf && toString && toStringTag) {
            rareData->setCachedHasDefaultToPrimitiveFastAndNonObservable(TriState::True);
            return asString(toStringTag);
        }

        // Memoize Slow only when the verdict is stable. An empty cache with no own special property is
        // merely not-yet-probed, so leave it Unknown and let a later call reach Fast once it populates.
        bool hasOwnSpecialProperty = isValidOffset(get(vm, vm.propertyNames->valueOf))
            || isValidOffset(get(vm, vm.propertyNames->toString))
            || isValidOffset(get(vm, vm.propertyNames->toPrimitiveSymbol))
            || isValidOffset(get(vm, vm.propertyNames->toStringTagSymbol));
        if (hasOwnSpecialProperty)
            rareData->setCachedHasDefaultToPrimitiveFastAndNonObservable(TriState::False);
    }

    return nullptr;
}

inline void Structure::clearCachedPrototypeChain()
{
    m_cachedPrototypeChain.clear();
    if (!hasRareData())
        return;
    rareData()->clearCachedPropertyNameEnumerator();
}

inline StructureFireDetail::StructureFireDetail(const Structure* structure)
    : m_structure(structure)
{
}

inline StructureTransitionTable::~StructureTransitionTable()
{
    if (!isUsingSingleSlot())
        delete map();
}

inline StructureCache::StructureCache(VM& vm)
    : m_structures(vm)
{
}

inline StructureCache::~StructureCache() = default;

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
