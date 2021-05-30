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

    Vector<ObjectPropertyCondition> result;
    
    if (!isEmpty())
        result.append(m_data->m_vector.begin(), m_data->m_vector.size());
    
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

    return create(WTFMove(result));
}

bool ObjectPropertyConditionSet::structuresEnsureValidity() const
{
    if (!isValid())
        return false;
    
    for (const ObjectPropertyCondition& condition : *this) {
        if (!condition.structureEnsuresValidity())
            return false;
    }
    return true;
}

bool ObjectPropertyConditionSet::structuresEnsureValidityAssumingImpurePropertyWatchpoint() const
{
    if (!isValid())
        return false;
    
    for (const ObjectPropertyCondition& condition : *this) {
        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint())
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
        out.print(listDumpInContext(m_data->m_vector, context));
    out.print("]");
}

void ObjectPropertyConditionSet::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

bool ObjectPropertyConditionSet::isValidAndWatchable() const
{
    if (!isValid())
        return false;

    for (auto& condition : m_data->m_vector) {
        if (!condition.isWatchable())
            return false;
    }
    return true;
}

namespace {

namespace ObjectPropertyConditionSetInternal {
static constexpr bool verbose = false;
}

ObjectPropertyCondition generateCondition(
    VM& vm, JSCell* owner, JSObject* object, UniquedStringImpl* uid, PropertyCondition::Kind conditionKind)
{
    Structure* structure = object->structure(vm);
    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("Creating condition ", conditionKind, " for ", pointerDump(structure), "\n");

    ObjectPropertyCondition result;
    switch (conditionKind) {
    case PropertyCondition::Presence: {
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (offset == invalidOffset)
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::presence(vm, owner, object, uid, offset, attributes);
        break;
    }
    case PropertyCondition::Absence: {
        if (structure->hasPolyProto())
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::absence(
            vm, owner, object, uid, object->structure(vm)->storedPrototypeObject());
        break;
    }
    case PropertyCondition::AbsenceOfSetEffect: {
        if (structure->hasPolyProto())
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::absenceOfSetEffect(
            vm, owner, object, uid, object->structure(vm)->storedPrototypeObject());
        break;
    }
    case PropertyCondition::Equivalence: {
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (offset == invalidOffset)
            return ObjectPropertyCondition();
        JSValue value = object->getDirectConcurrently(structure, offset);
        if (!value)
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::equivalence(vm, owner, object, uid, value);
        break;
    }
    case PropertyCondition::HasStaticProperty: {
        auto entry = object->findPropertyHashEntry(vm, uid);
        if (!entry)
            return ObjectPropertyCondition();
        result = ObjectPropertyCondition::hasStaticProperty(vm, owner, object, uid);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return ObjectPropertyCondition();
    }

    if (!result.isStillValidAssumingImpurePropertyWatchpoint()) {
        if (ObjectPropertyConditionSetInternal::verbose)
            dataLog("Failed to create condition: ", result, "\n");
        return ObjectPropertyCondition();
    }

    if (ObjectPropertyConditionSetInternal::verbose)
        dataLog("New condition: ", result, "\n");
    return result;
}

template<typename Functor>
ObjectPropertyConditionSet generateConditions(
    VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* prototype, const Functor& functor)
{
    Vector<ObjectPropertyCondition> conditions;
    
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
        structure = object->structure(vm);
        
        if (structure->isDictionary()) {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Cannot cache dictionary.\n");
            return ObjectPropertyConditionSet::invalid();
        }

        if (!functor(conditions, object)) {
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
        vm, globalObject, headStructure, nullptr,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, uid, PropertyCondition::Absence);
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
        vm, globalObject, headStructure, nullptr,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, uid, PropertyCondition::AbsenceOfSetEffect);
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
        vm, globalObject, headStructure, prototype,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            PropertyCondition::Kind kind =
                object == prototype ? PropertyCondition::Presence : PropertyCondition::Absence;
            ObjectPropertyCondition result =
                generateCondition(vm, owner, object, uid, kind);
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
        vm, globalObject, headStructure, prototype,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            auto kind = PropertyCondition::Absence;
            if (object == prototype) {
                Structure* structure = object->structure(vm);
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
            ObjectPropertyCondition result = generateCondition(vm, owner, object, uid, kind);
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
        vm, globalObject, headStructure, shouldHit ? prototype : nullptr,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            if (ObjectPropertyConditionSetInternal::verbose)
                dataLog("Encountered object: ", RawPointer(object), "\n");
            if (object == prototype) {
                RELEASE_ASSERT(shouldHit);
                didHit = true;
                return true;
            }

            Structure* structure = object->structure(vm);
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
    return generateConditions(vm, globalObject, headStructure, prototype,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            PropertyCondition::Kind kind =
                object == prototype ? PropertyCondition::Equivalence : PropertyCondition::Absence;
            ObjectPropertyCondition result = generateCondition(vm, nullptr, object, uid, kind);
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
        vm, globalObject, headStructure, nullptr,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            ObjectPropertyCondition result = generateCondition(vm, nullptr, object, uid, PropertyCondition::Absence);
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
        vm, globalObject, headStructure, nullptr,
        [&] (Vector<ObjectPropertyCondition>& conditions, JSObject* object) -> bool {
            ObjectPropertyCondition result =
                generateCondition(vm, nullptr, object, uid, PropertyCondition::AbsenceOfSetEffect);
            if (!result)
                return false;
            conditions.append(result);
            return true;
        });
}

ObjectPropertyCondition generateConditionForSelfEquivalence(
    VM& vm, JSCell* owner, JSObject* object, UniquedStringImpl* uid)
{
    return generateCondition(vm, owner, object, uid, PropertyCondition::Equivalence);
}

// Current might be null. Structure can't be null.
static std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* current, Structure* structure, JSObject* target)
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
        structure = current->structure(vm);
    }

    if (!found && !!target)
        return std::nullopt;

    PrototypeChainCachingStatus result;
    result.usesPolyProto = usesPolyProto;
    result.flattenedDictionary = flattenedDictionary;

    return result;
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* base, JSObject* target)
{
    return prepareChainForCaching(globalObject, base, base->structure(globalObject->vm()), target);
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, JSCell* base, const PropertySlot& slot)
{
    JSObject* target = slot.isUnset() ? nullptr : slot.slotBase();
    return prepareChainForCaching(globalObject, base, target);
}

std::optional<PrototypeChainCachingStatus> prepareChainForCaching(JSGlobalObject* globalObject, Structure* baseStructure, JSObject* target)
{
    return prepareChainForCaching(globalObject, nullptr, baseStructure, target);
}

} // namespace JSC

