/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "ObjectPropertyConditionSet.h"

#include "JSCInlines.h"
#include <wtf/ListDump.h>

namespace JSC {

ObjectPropertyCondition ObjectPropertyConditionSet::forObject(JSObject* object) const
{
    for (const ObjectPropertyCondition& condition : *this) {
        if (condition.object() == object)
            return condition;
    }
    return ObjectPropertyCondition();
}

ObjectPropertyCondition ObjectPropertyConditionSet::forConditionKind(
    PropertyCondition::Kind kind) const
{
    for (const ObjectPropertyCondition& condition : *this) {
        if (condition.kind() == kind)
            return condition;
    }
    return ObjectPropertyCondition();
}

unsigned ObjectPropertyConditionSet::numberOfConditionsWithKind(PropertyCondition::Kind kind) const
{
    unsigned result = 0;
    for (const ObjectPropertyCondition& condition : *this) {
        if (condition.kind() == kind)
            result++;
    }
    return result;
}

bool ObjectPropertyConditionSet::hasOneSlotBaseCondition() const
{
    bool sawBase = false;
    for (const ObjectPropertyCondition& condition : *this) {
        switch (condition.kind()) {
        case PropertyCondition::Presence:
        case PropertyCondition::Replacement:
        case PropertyCondition::Equivalence:
        case PropertyCondition::HasStaticProperty:
            if (sawBase)
                return false;
            sawBase = true;
            break;
        default:
            break;
        }
    }

    return sawBase;
}

ObjectPropertyCondition ObjectPropertyConditionSet::slotBaseCondition() const
{
    ObjectPropertyCondition result;
    unsigned numFound = 0;
    for (const ObjectPropertyCondition& condition : *this) {
        if (condition.kind() == PropertyCondition::Presence
            || condition.kind() == PropertyCondition::Replacement
            || condition.kind() == PropertyCondition::Equivalence
            || condition.kind() == PropertyCondition::HasStaticProperty) {
            result = condition;
            numFound++;
        }
    }
    RELEASE_ASSERT(numFound == 1);
    return result;
}

ObjectPropertyConditionSet ObjectPropertyConditionSet::mergedWith(
    const ObjectPropertyConditionSet& other) const
{
    if (!isValid() || !other.isValid())
        return invalid();

    Vector<ObjectPropertyCondition, 16> result;
    
    if (!isEmpty())
        result.append(m_data->begin(), m_data->size());
    
    for (const ObjectPropertyCondition& newCondition : other) {
        bool foundMatch = false;
        for (const ObjectPropertyCondition& existingCondition : *this) {
            if (newCondition == existingCondition) {
                foundMatch = true;
                continue;
            }
            if (!newCondition.isCompatibleWith(existingCondition))
                return invalid();
        }
        if (!foundMatch)
            result.append(newCondition);
    }

    return ObjectPropertyConditionSet::create(WTFMove(result));
}

bool ObjectPropertyConditionSet::structuresEnsureValidity() const
{
    if (!isValid())
        return false;
    
    for (const ObjectPropertyCondition& condition : *this) {
        if (!condition.structureEnsuresValidity(Concurrency::ConcurrentThread))
            return false;
    }
    return true;
}

bool ObjectPropertyConditionSet::needImpurePropertyWatchpoint() const
{
    for (const ObjectPropertyCondition& condition : *this) {
        if (condition.validityRequiresImpurePropertyWatchpoint())
            return true;
    }
    return false;
}

bool ObjectPropertyConditionSet::areStillLive(VM& vm) const
{
    bool stillLive = true;
    forEachDependentCell([&](JSCell* cell) {
        stillLive &= vm.heap.isMarked(cell);
    });
    return stillLive;
}

void ObjectPropertyConditionSet::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (!isValid()) {
        out.print("<invalid>");
        return;
    }
    
    out.print("[");
    if (m_data)
        out.print(listDumpInContext(*m_data, context));
    out.print("]");
}

void ObjectPropertyConditionSet::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

namespace {

namespace ObjectPropertyConditionSetInternal {
static constexpr bool verbose = false;
}

ObjectPropertyCondition generateCondition(
    VM& vm, JSCell* owner, JSObject* object, Structure* structure, UniquedStringImpl* uid, PropertyCondition::Kind conditionKind, Concurrency concurrency)
{
    // The structure of object might change by the time we get here, so we need to make sure that our conditions are built against the structure that is passed in.
    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("Creating condition ", conditionKind, " for ", pointerDump(structure), "\n");

    ObjectPropertyCondition result;
    switch (conditionKind) {
    case PropertyCondition::Presence: {
        unsigned attributes;
        PropertyOffset offset = structure->get(vm, concurrency, uid, attributes);
        if (offset == invalidOffset)
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::presence(vm, owner, object, uid, offset, attributes);
        break;
    }
    case PropertyCondition::Replacement: {
        unsigned attributes;
        PropertyOffset offset = structure->get(vm, concurrency, uid, attributes);
        if (offset == invalidOffset || !!(attributes & PropertyAttribute::ReadOnly))
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::replacement(vm, owner, object, uid, offset, attributes);
        break;
    }
    case PropertyCondition::Absence: {
        if (structure->hasPolyProto())
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::absence(
            vm, owner, object, uid, structure->storedPrototypeObject());
        break;
    }
    case PropertyCondition::AbsenceOfSetEffect: {
        if (structure->hasPolyProto())
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::absenceOfSetEffect(
            vm, owner, object, uid, structure->storedPrototypeObject());
        break;
    }
    case PropertyCondition::AbsenceOfIndexedProperties: {
        if (structure->hasPolyProto())
            return ObjectPropertyCondition();
        if (hasIndexedProperties(structure->indexingType()))
            return ObjectPropertyCondition();
        if (structure->mayInterceptIndexedAccesses() || structure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::absenceOfIndexedProperties(vm, owner, object, structure->storedPrototypeObject());
        break;
    }
    case PropertyCondition::Equivalence: {
        JSValue value;
        {
            Locker<JSCellLock> cellLocker { NoLockingNecessary };
            if (concurrency != Concurrency::MainThread) {
                cellLocker = Locker { object->cellLock() };
                if (object->structure() != structure)
                    return ObjectPropertyCondition();
                // The structure might change from now on, but we are guaranteed to have a sane view of the butterfly.
            }
            unsigned attributes;
            PropertyOffset offset = structure->get(vm, concurrency, uid, attributes);
            if (offset == invalidOffset)
                return ObjectPropertyCondition();
            value = object->getDirect(cellLocker, concurrency, structure, offset);
            if (!value)
                return ObjectPropertyCondition();
        }
        result = ObjectPropertyCondition::equivalence(vm, owner, object, uid, value);
        break;
    }
    case PropertyCondition::HasStaticProperty: {
        auto entry = object->findPropertyHashEntry(uid);
        if (!entry)
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::hasStaticProperty(vm, owner, object, uid);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return ObjectPropertyCondition();
    }

    if (!result.isStillValidAssumingImpurePropertyWatchpoint(concurrency)) {
        if (ObjectPropertyConditionSetInternal::verbose)
            dataLog("Failed to create condition: ", result, "\n");
        return ObjectPropertyCondition();
    }

    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("New condition: ", result, "\n");
    return result;
}

template<typename Functor>
ObjectPropertyConditionSet generateConditions(JSGlobalObject* globalObject, Structure* structure, JSObject* prototype, UniquedStringImpl* uid, const Functor& functor)
{
    Vector<ObjectPropertyCondition, 8> conditions;
    
    for (;;) {
        if (ObjectPropertyConditionSetInternal::verbose)
            dataLog("Considering structure: ", pointerDump(structure), "\n");
        
        if (structure->isProxy()) {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("It's a proxy, so invalid.\n");
            return ObjectPropertyConditionSet::invalid();
        }

        if (structure->hasPolyProto()) {
            // FIXME: Integrate this with PolyProtoAccessChain:
            // https://bugs.webkit.org/show_bug.cgi?id=177339
            // Or at least allow OPC set generation when the
            // base is not poly proto:
            // https://bugs.webkit.org/show_bug.cgi?id=177721
            return ObjectPropertyConditionSet::invalid();
        }

        // TypedArray has an ability to stop [[Prototype]] traversing for numeric index string (e.g. "0.1").
        // If we found it, then traverse should stop for Unset case.
        // https://262.ecma-international.org/9.0/#_ref_2826
        if (!prototype && isTypedArrayType(structure->typeInfo().type()) && uid && isCanonicalNumericIndexString(uid))
            break;
        
        JSValue value = structure->prototypeForLookup(globalObject);
        
        if (value.isNull()) {
            if (!prototype) {
                if (ObjectPropertyConditionSetInternal::verbose)
                    dataLog("Reached end of prototype chain as expected, done.\n");
                break;
            }
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Unexpectedly reached end of prototype chain, so invalid.\n");
            return ObjectPropertyConditionSet::invalid();
        }
        
        JSObject* object = jsCast<JSObject*>(value);
        structure = object->structure();
        
        if (structure->isDictionary()) {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Cannot cache dictionary.\n");
            return ObjectPropertyConditionSet::invalid();
        }

        if (!functor(conditions, object, structure)) {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Functor failed, invalid.\n");
            return ObjectPropertyConditionSet::invalid();
        }
        
        if (object == prototype) {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Reached desired prototype, done.\n");
            break;
        }
    }

    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("Returning conditions: ", listDump(conditions), "\n");
    return ObjectPropertyConditionSet::create(WTFMove(conditions));
}

} // anonymous namespace

ObjectPropertyConditionSet generateConditionsForPropertyMiss(
    VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure, UniquedStringImpl* uid)
{
    return generateConditions(
        globalObject, headStructure, nullptr, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, structure, uid, PropertyCondition::Absence, Concurrency::MainThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForPropertySetterMiss(
    VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure, UniquedStringImpl* uid)
{
    return generateConditions(
        globalObject, headStructure, nullptr, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, structure, uid, PropertyCondition::AbsenceOfSetEffect, Concurrency::MainThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForIndexedMiss(VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure)
{
    return generateConditions(
        globalObject, headStructure, nullptr, nullptr,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, structure, nullptr, PropertyCondition::AbsenceOfIndexedProperties, Concurrency::MainThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForPrototypePropertyHit(
    VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure, JSObject* prototype,
    UniquedStringImpl* uid)
{
    return generateConditions(
        globalObject, headStructure, prototype, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            PropertyCondition::Kind kind =
                object == prototype ? PropertyCondition::Presence : PropertyCondition::Absence;
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, structure, uid, kind, Concurrency::MainThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForPrototypePropertyHitCustom(
    VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure, JSObject* prototype,
    UniquedStringImpl* uid, unsigned attributes)
{
    return generateConditions(
        globalObject, headStructure, prototype, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            auto kind = PropertyCondition::Absence;
            if (object == prototype) {
                PropertyOffset offset = structure->get(vm, uid);
                if (isValidOffset(offset)) {
                    // When we reify custom accessors, we wrap them in a JSFunction that we shove
                    // inside a GetterSetter. So, once we've reified a custom accessor, we will
                    // no longer see it as a "custom" accessor/value. Hence, if our property access actually
                    // notices a custom, it must be a CustomGetterSetterType cell or something
                    // in the static property table. Custom values get reified into CustomGetterSetters.
                    JSValue value = object->getDirect(offset);

                    if (!value.isCell() || value.asCell()->type() != CustomGetterSetterType) {
                        // The value could have just got changed to some other type, so check if it's still
                        // a custom getter setter.
                        return false;
                    }

                    kind = PropertyCondition::Equivalence;
                } else if (structure->findPropertyHashEntry(uid))
                    kind = PropertyCondition::HasStaticProperty;
                else if (attributes & PropertyAttribute::DontDelete) {
                    // This can't change, so we can blindly cache it.
                    return true;
                } else {
                    // This means we materialized a custom out of thin air and it's not DontDelete (i.e, it can be
                    // redefined). This is curious. We don't actually need to crash here. We could blindly cache
                    // the function. Or we could blindly not cache it. However, we don't actually do this in WebKit
                    // right now, so it's reasonable to decide what to do later (or to warn people of forgetting DoneDelete.)
                    ASSERT_NOT_REACHED();
                    return false;
                }
            }
            ObjectPropertyCondition result = generateCondition(vm, owner, object, structure, uid, kind, Concurrency::MainThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForInstanceOf(
    VM& vm, JSCell* owner, JSGlobalObject* globalObject, Structure* headStructure, JSObject* prototype,
    bool shouldHit)
{
    bool didHit = false;
    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("Searching for prototype ", JSValue(prototype), " starting with structure ", RawPointer(headStructure), " with shouldHit = ", shouldHit, "\n");
    ObjectPropertyConditionSet result = generateConditions(
        globalObject, headStructure, shouldHit ? prototype : nullptr, nullptr,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Encountered object: ", RawPointer(object), "\n");
            if (object == prototype) {
                RELEASE_ASSERT(shouldHit);
                didHit = true;
                return true;
            }

            if (structure->hasPolyProto())
                return false;
            conditions.append(
                ObjectPropertyCondition::hasPrototype(
                    vm, owner, object, structure->storedPrototypeObject()));
            return true;
        });
    if (result.isValid()) {
        if (ObjectPropertyConditionSetInternal::verbose)
            dataLog("didHit = ", didHit, ", shouldHit = ", shouldHit, "\n");
        RELEASE_ASSERT(didHit == shouldHit);
    }
    return result;
}

ObjectPropertyConditionSet generateConditionsForPrototypeEquivalenceConcurrently(
    VM& vm, JSGlobalObject* globalObject, Structure* headStructure, JSObject* prototype, UniquedStringImpl* uid)
{
    return generateConditions(globalObject, headStructure, prototype, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            PropertyCondition::Kind kind =
                object == prototype ? PropertyCondition::Equivalence : PropertyCondition::Absence;
            ObjectPropertyCondition result = generateCondition(vm, nullptr, object, structure, uid, kind, Concurrency::ConcurrentThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForPropertyMissConcurrently(
    VM& vm, JSGlobalObject* globalObject, Structure* headStructure, UniquedStringImpl* uid)
{
    return generateConditions(
        globalObject, headStructure, nullptr, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            ObjectPropertyCondition result = generateCondition(vm, nullptr, object, structure, uid, PropertyCondition::Absence, Concurrency::ConcurrentThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyConditionSet generateConditionsForPropertySetterMissConcurrently(
    VM& vm, JSGlobalObject* globalObject, Structure* headStructure, UniquedStringImpl* uid)
{
    return generateConditions(
        globalObject, headStructure, nullptr, uid,
        [&](auto& conditions, JSObject* object, Structure* structure) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, nullptr, object, structure, uid, PropertyCondition::AbsenceOfSetEffect, Concurrency::ConcurrentThread);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyCondition generateConditionForSelfEquivalence(
    VM& vm, JSCell* owner, JSObject* object, UniquedStringImpl* uid)
{
    return generateCondition(vm, owner, object, object->structure(), uid, PropertyCondition::Equivalence, Concurrency::MainThread);
}

// Current might be null. Structure can't be null.
static std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* current, Structure* structure, UniquedStringImpl* propertyName, JSObject* target)
{
    ASSERT(structure);
    VM& vm = globalObject->vm();

    bool found = false;
    bool usesPolyProto = false;
    bool flattenedDictionary = false;

    while (true) {
        if (structure->isDictionary()) {
            if (!current)
                return std::nullopt;

            ASSERT(structure->isObject());
            if (structure->hasBeenFlattenedBefore())
                return std::nullopt;

            structure->flattenDictionaryStructure(vm, asObject(current));
            flattenedDictionary = true;
        }

        if (!structure->propertyAccessesAreCacheable())
            return std::nullopt;

        if (structure->isProxy())
            return std::nullopt;

        if (current && current == target) {
            found = true;
            break;
        }

        // TypedArray has an ability to stop [[Prototype]] traversing for numeric index string (e.g. "0.1").
        // If we found it, then traverse should stop for Unset case.
        // https://262.ecma-international.org/9.0/#_ref_2826
        if (!target && propertyName && isTypedArrayType(structure->typeInfo().type()) && isCanonicalNumericIndexString(propertyName)) {
            found = true;
            break;
        }

        // We only have poly proto if we need to access our prototype via
        // the poly proto protocol. If the slot base is the only poly proto
        // thing in the chain, and we have a cache hit on it, then we're not
        // poly proto.
        JSValue prototype;
        if (structure->hasPolyProto()) {
            if (!current)
                return std::nullopt;
            usesPolyProto = true;
            prototype = structure->prototypeForLookup(globalObject, current);
        } else
            prototype = structure->prototypeForLookup(globalObject);

        if (prototype.isNull())
            break;
        current = asObject(prototype);
        structure = current->structure();
    }

    if (!found && !!target)
        return std::nullopt;

    PrototypeChainCachingStatus result;
    result.usesPolyProto = usesPolyProto;
    result.flattenedDictionary = flattenedDictionary;

    return result;
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* base, UniquedStringImpl* propertyName, JSObject* target)
{
    return prepareChainForCaching(globalObject, base, base->structure(), propertyName, target);
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* base, UniquedStringImpl* propertyName, const PropertySlot& slot)
{
    JSObject* target = slot.isUnset() ? nullptr : slot.slotBase();
    return prepareChainForCaching(globalObject, base, propertyName, target);
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, Structure* baseStructure, UniquedStringImpl* propertyName, JSObject* target)
{
    return prepareChainForCaching(globalObject, nullptr, baseStructure, propertyName, target);
}

} // namespace JSC

