/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSImmutableButterfly.h"
#include "JSPropertyNameEnumerator.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "ObjectPropertyConditionSet.h"
#include "ObjectToStringAdaptiveStructureWatchpoint.h"

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
    visitor.appendUnbarriered(thisObject->objectToStringValue());
    visitor.append(thisObject->m_cachedPropertyNameEnumerator);
    auto* cachedOwnKeys = thisObject->m_cachedOwnKeys.unvalidatedGet();
    if (cachedOwnKeys != cachedOwnKeysSentinel())
        visitor.appendUnbarriered(cachedOwnKeys);
}

// ----------- Object.prototype.toString() helper watchpoint classes -----------

class ObjectToStringAdaptiveInferredPropertyValueWatchpoint final : public AdaptiveInferredPropertyValueWatchpointBase {
public:
    typedef AdaptiveInferredPropertyValueWatchpointBase Base;
    ObjectToStringAdaptiveInferredPropertyValueWatchpoint(const ObjectPropertyCondition&, StructureRareData*);

private:
    bool isValid() const override;
    void handleFire(VM&, const FireDetail&) override;

    StructureRareData* m_structureRareData;
};

void StructureRareData::setObjectToStringValue(JSGlobalObject* globalObject, VM& vm, Structure* ownStructure, JSString* value, PropertySlot toStringTagSymbolSlot)
{
    if (canCacheObjectToStringValue())
        return;

    ObjectPropertyConditionSet conditionSet;
    if (toStringTagSymbolSlot.isValue()) {
        // We don't handle the own property case of Symbol.toStringTag because we would never know if a new
        // object transitioning to the same structure had the same value stored in Symbol.toStringTag.
        // Additionally, this is a super unlikely case anyway.
        if (!toStringTagSymbolSlot.isCacheable() || toStringTagSymbolSlot.slotBase()->structure(vm) == ownStructure)
            return;


        // This will not create a condition for the current structure but that is good because we know the Symbol.toStringTag
        // is not on the ownStructure so we will transisition if one is added and this cache will no longer be used.
        prepareChainForCaching(globalObject, ownStructure, toStringTagSymbolSlot.slotBase());
        conditionSet = generateConditionsForPrototypePropertyHit(vm, this, globalObject, ownStructure, toStringTagSymbolSlot.slotBase(), vm.propertyNames->toStringTagSymbol.impl());
        ASSERT(!conditionSet.isValid() || conditionSet.hasOneSlotBaseCondition());
    } else if (toStringTagSymbolSlot.isUnset()) {
        prepareChainForCaching(globalObject, ownStructure, nullptr);
        conditionSet = generateConditionsForPropertyMiss(vm, this, globalObject, ownStructure, vm.propertyNames->toStringTagSymbol.impl());
    } else
        return;

    if (!conditionSet.isValid()) {
        giveUpOnObjectToStringValueCache();
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
                giveUpOnObjectToStringValueCache();
                return;
            }
        } else if (!condition.isWatchable()) {
            giveUpOnObjectToStringValueCache();
            return;
        }
    }

    ASSERT(conditionSet.structuresEnsureValidity());
    for (ObjectPropertyCondition condition : conditionSet) {
        if (condition.condition().kind() == PropertyCondition::Presence) {
            m_objectToStringAdaptiveInferredValueWatchpoint = makeUnique<ObjectToStringAdaptiveInferredPropertyValueWatchpoint>(equivCondition, this);
            m_objectToStringAdaptiveInferredValueWatchpoint->install(vm);
        } else
            m_objectToStringAdaptiveWatchpointSet.add(condition, this)->install(vm);
    }

    m_objectToStringValue.set(vm, this, value);
}

void StructureRareData::clearObjectToStringValue()
{
    m_objectToStringAdaptiveWatchpointSet.clear();
    m_objectToStringAdaptiveInferredValueWatchpoint.reset();
    if (!canCacheObjectToStringValue())
        m_objectToStringValue.clear();
}

void StructureRareData::finalizeUnconditionally(VM& vm)
{
    if (m_objectToStringAdaptiveInferredValueWatchpoint) {
        if (!m_objectToStringAdaptiveInferredValueWatchpoint->key().isStillLive(vm)) {
            clearObjectToStringValue();
            return;
        }
    }
    for (auto* watchpoint : m_objectToStringAdaptiveWatchpointSet) {
        if (!watchpoint->key().isStillLive(vm)) {
            clearObjectToStringValue();
            return;
        }
    }
}

// ------------- Methods for Object.prototype.toString() helper watchpoint classes --------------

ObjectToStringAdaptiveInferredPropertyValueWatchpoint::ObjectToStringAdaptiveInferredPropertyValueWatchpoint(const ObjectPropertyCondition& key, StructureRareData* structureRareData)
    : Base(key)
    , m_structureRareData(structureRareData)
{
}

bool ObjectToStringAdaptiveInferredPropertyValueWatchpoint::isValid() const
{
    return m_structureRareData->isLive();
}

void ObjectToStringAdaptiveInferredPropertyValueWatchpoint::handleFire(VM&, const FireDetail&)
{
    m_structureRareData->clearObjectToStringValue();
}

} // namespace JSC
