/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "StructureRareData.h"

#include "AdaptiveInferredPropertyValueWatchpointBase.h"
#include "CachedSpecialPropertyAdaptiveStructureWatchpoint.h"
#include "JSImmutableButterfly.h"
#include "JSObjectInlines.h"
#include "JSPropertyNameEnumerator.h"
#include "JSString.h"
#include "ObjectPropertyConditionSet.h"
#include "StructureChain.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"

namespace JSC {

const ClassInfo StructureRareData::s_info = { "StructureRareData", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(StructureRareData) };

Structure* StructureRareData::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

StructureRareData* StructureRareData::create(VM& vm, Structure* previous)
{
    StructureRareData* rareData = new (NotNull, allocateCell<StructureRareData>(vm.heap)) StructureRareData(vm, previous);
    rareData->finishCreation(vm);
    return rareData;
}

void StructureRareData::destroy(JSCell* cell)
{
    static_cast<StructureRareData*>(cell)->StructureRareData::~StructureRareData();
}

StructureRareData::StructureRareData(VM& vm, Structure* previous)
    : JSCell(vm, vm.structureRareDataStructure.get())
    , m_maxOffset(invalidOffset)
    , m_transitionOffset(invalidOffset)
{
    if (previous)
        m_previous.set(vm, this, previous);
}

void StructureRareData::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    StructureRareData* thisObject = jsCast<StructureRareData*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_previous);
    if (thisObject->m_specialPropertyCache) {
        for (unsigned index = 0; index < numberOfCachedSpecialPropertyKeys; ++index)
            visitor.appendUnbarriered(thisObject->cachedSpecialProperty(static_cast<CachedSpecialPropertyKey>(index)));
    }
    visitor.append(thisObject->m_cachedPropertyNameEnumerator);
    for (unsigned index = 0; index < numberOfCachedPropertyNames; ++index) {
        auto* cached = thisObject->m_cachedPropertyNames[index].unvalidatedGet();
        if (cached != cachedPropertyNamesSentinel())
            visitor.appendUnbarriered(cached);
    }
}

// ----------- Cached special properties helper watchpoint classes -----------

class CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint final : public AdaptiveInferredPropertyValueWatchpointBase {
public:
    typedef AdaptiveInferredPropertyValueWatchpointBase Base;
    CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint(const ObjectPropertyCondition&, StructureRareData*);

private:
    bool isValid() const final;
    void handleFire(VM&, const FireDetail&) final;

    StructureRareData* m_structureRareData;
};

SpecialPropertyCacheEntry::~SpecialPropertyCacheEntry() = default;

SpecialPropertyCache& StructureRareData::ensureSpecialPropertyCacheSlow()
{
    ASSERT(!isCompilationThread() && !Thread::mayBeGCThread());
    ASSERT(!m_specialPropertyCache);
    auto cache = makeUnique<SpecialPropertyCache>();
    WTF::storeStoreFence(); // Expose valid struct for concurrent threads including concurrent compilers.
    m_specialPropertyCache = WTFMove(cache);
    return *m_specialPropertyCache.get();
}

inline void StructureRareData::giveUpOnSpecialPropertyCache(CachedSpecialPropertyKey key)
{
    ensureSpecialPropertyCache().m_cache[static_cast<unsigned>(key)].m_value.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());
}

void StructureRareData::cacheSpecialPropertySlow(JSGlobalObject* globalObject, VM& vm, Structure* ownStructure, JSValue value, CachedSpecialPropertyKey key, const PropertySlot& slot)
{
    UniquedStringImpl* uid = nullptr;
    switch (key) {
    case CachedSpecialPropertyKey::ToStringTag:
        uid = vm.propertyNames->toStringTagSymbol.impl();
        break;
    case CachedSpecialPropertyKey::ToString:
        uid = vm.propertyNames->toString.impl();
        break;
    case CachedSpecialPropertyKey::ValueOf:
        uid = vm.propertyNames->valueOf.impl();
        break;
    case CachedSpecialPropertyKey::ToPrimitive:
        uid = vm.propertyNames->toPrimitiveSymbol.impl();
        break;
    }
    ObjectPropertyConditionSet conditionSet;
    if (slot.isValue()) {
        // We don't handle the own property case of special properties (toString, valueOf, @@toPrimitive, @@toStringTag) because we would never know if a new
        // object transitioning to the same structure had the same value stored in that property.
        // Additionally, this is a super unlikely case anyway.
        if (!slot.isCacheable() || slot.slotBase()->structure(vm) == ownStructure)
            return;

        // This will not create a condition for the current structure but that is good because we know that property
        // is not on the ownStructure so we will transisition if one is added and this cache will no longer be used.
        prepareChainForCaching(globalObject, ownStructure, slot.slotBase());
        conditionSet = generateConditionsForPrototypePropertyHit(vm, this, globalObject, ownStructure, slot.slotBase(), uid);
        ASSERT(!conditionSet.isValid() || conditionSet.hasOneSlotBaseCondition());
    } else if (slot.isUnset()) {
        prepareChainForCaching(globalObject, ownStructure, nullptr);
        conditionSet = generateConditionsForPropertyMiss(vm, this, globalObject, ownStructure, uid);
    } else
        return;

    if (!conditionSet.isValid()) {
        giveUpOnSpecialPropertyCache(key);
        return;
    }

    ObjectPropertyCondition equivCondition;
    for (const ObjectPropertyCondition& condition : conditionSet) {
        if (condition.condition().kind() == PropertyCondition::Presence) {
            ASSERT(isValidOffset(condition.offset()));
            condition.object()->structure(vm)->startWatchingPropertyForReplacements(vm, condition.offset());
            equivCondition = condition.attemptToMakeEquivalenceWithoutBarrier(vm);

            // The equivalence condition won't be watchable if we have already seen a replacement.
            if (!equivCondition.isWatchable()) {
                giveUpOnSpecialPropertyCache(key);
                return;
            }
        } else if (!condition.isWatchable()) {
            giveUpOnSpecialPropertyCache(key);
            return;
        }
    }

    ASSERT(conditionSet.structuresEnsureValidity());
    auto& cache = ensureSpecialPropertyCache().m_cache[static_cast<unsigned>(key)];
    for (ObjectPropertyCondition condition : conditionSet) {
        if (condition.condition().kind() == PropertyCondition::Presence) {
            cache.m_equivalenceWatchpoint = makeUnique<CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint>(equivCondition, this);
            cache.m_equivalenceWatchpoint->install(vm);
        } else
            cache.m_missWatchpoints.add(condition, this)->install(vm);
    }
    cache.m_value.set(vm, this, value);
}

void StructureRareData::clearCachedSpecialProperty(CachedSpecialPropertyKey key)
{
    auto* objectToStringCache = m_specialPropertyCache.get();
    if (!objectToStringCache)
        return;
    auto& cache = objectToStringCache->m_cache[static_cast<unsigned>(key)];
    cache.m_missWatchpoints.clear();
    cache.m_equivalenceWatchpoint.reset();
    if (cache.m_value.get() != JSCell::seenMultipleCalleeObjects())
        cache.m_value.clear();
}

void StructureRareData::finalizeUnconditionally(VM& vm)
{
    if (m_specialPropertyCache) {
        auto clearCacheIfInvalidated = [&](CachedSpecialPropertyKey key) {
            auto& cache = m_specialPropertyCache->m_cache[static_cast<unsigned>(key)];
            if (cache.m_equivalenceWatchpoint) {
                if (!cache.m_equivalenceWatchpoint->key().isStillLive(vm)) {
                    clearCachedSpecialProperty(key);
                    return;
                }
            }
            for (auto* watchpoint : cache.m_missWatchpoints) {
                if (!watchpoint->key().isStillLive(vm)) {
                    clearCachedSpecialProperty(key);
                    return;
                }
            }
        };

        for (unsigned index = 0; index < numberOfCachedSpecialPropertyKeys; ++index)
            clearCacheIfInvalidated(static_cast<CachedSpecialPropertyKey>(index));
    }
}

// ------------- Methods for Object.prototype.toString() helper watchpoint classes --------------

CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint::CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint(const ObjectPropertyCondition& key, StructureRareData* structureRareData)
    : Base(key)
    , m_structureRareData(structureRareData)
{
}

bool CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint::isValid() const
{
    return m_structureRareData->isLive();
}

void CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint::handleFire(VM& vm, const FireDetail&)
{
    CachedSpecialPropertyKey key = CachedSpecialPropertyKey::ToStringTag;
    if (this->key().uid() == vm.propertyNames->toStringTagSymbol.impl())
        key = CachedSpecialPropertyKey::ToStringTag;
    else if (this->key().uid() == vm.propertyNames->toString.impl())
        key = CachedSpecialPropertyKey::ToString;
    else if (this->key().uid() == vm.propertyNames->valueOf.impl())
        key = CachedSpecialPropertyKey::ValueOf;
    else {
        ASSERT(this->key().uid() == vm.propertyNames->toPrimitiveSymbol.impl());
        key = CachedSpecialPropertyKey::ToPrimitive;
    }
    m_structureRareData->clearCachedSpecialProperty(key);
}

} // namespace JSC
