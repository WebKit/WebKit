/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <JavaScriptCore/AuxiliaryBarrierInlines.h>
#include <JavaScriptCore/BrandedStructure.h>
#include <JavaScriptCore/ButterflyInlines.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSArrayInlines.h>
#include <JavaScriptCore/JSFunctionInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSGlobalProxy.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSTypedArrays.h>
#include <JavaScriptCore/Lookup.h>
#include <JavaScriptCore/MegamorphicCache.h>
#include <JavaScriptCore/ObjectInitializationScope.h>
#include <JavaScriptCore/SparseArrayValueMap.h>
#include <JavaScriptCore/StructureInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <JavaScriptCore/VM.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

inline JSCell* getJSFunction(JSValue value)
{
    if (value.isCell() && (value.asCell()->type() == JSFunctionType))
        return value.asCell();
    return nullptr;
}

inline JSValue JSObject::getPrototypeDirect() const
{
    return structure()->storedPrototype(this);
}

inline JSValue JSObject::getPrototype(JSGlobalObject* globalObject)
{
    if (!structure()->typeInfo().overridesGetPrototype()) [[likely]]
        return getPrototypeDirect();
    return methodTable()->getPrototype(this, globalObject);
}

inline bool JSValue::put(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    if (!isCell()) [[unlikely]]
        return putToPrimitive(globalObject, propertyName, value, slot);

    return asCell()->methodTable()->put(asCell(), globalObject, propertyName, value, slot);
}

inline bool JSValue::putByIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    if (!isCell()) [[unlikely]]
        return putToPrimitiveByIndex(globalObject, propertyName, value, shouldThrow);

    return asCell()->methodTable()->putByIndex(asCell(), globalObject, propertyName, value, shouldThrow);
}

ALWAYS_INLINE JSValue JSValue::getPrototype(JSGlobalObject* globalObject) const
{
    if (isObject())
        return asObject(asCell())->getPrototype(globalObject);
    return synthesizePrototype(globalObject);
}

inline Structure* JSObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

inline Structure* JSNonFinalObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

inline Structure* JSFinalObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, unsigned inlineCapacity)
{
    return Structure::create(vm, globalObject, prototype, typeInfo(), info(), defaultIndexingType, inlineCapacity);
}

inline void JSObject::setButterfly(VM& vm, Butterfly* butterfly)
{
    if (isX86() || vm.heap.mutatorShouldBeFenced()) {
        WTF::storeStoreFence();
        butterflyRef().set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    butterflyRef().set(vm, this, butterfly);
}

inline void JSObject::nukeStructureAndSetButterfly(VM& vm, StructureID oldStructureID, Butterfly* butterfly)
{
    if (isX86() || vm.heap.mutatorShouldBeFenced()) {
        setStructureIDDirectly(oldStructureID.nuke());
        WTF::storeStoreFence();
        butterflyRef().set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    butterflyRef().set(vm, this, butterfly);
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    bool hasProperty = const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);

    EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException() || !hasProperty);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    if (hasProperty)
        RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));

    return jsUndefined();
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, unsigned propertyName) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    bool hasProperty = const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);

    EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException() || !hasProperty);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    if (hasProperty)
        RELEASE_AND_RETURN(scope, slot.getValue(globalObject, propertyName));

    return jsUndefined();
}

template<typename T, typename PropertyNameType>
inline T JSObject::getAs(JSGlobalObject* globalObject, PropertyNameType propertyName) const
{
    JSValue value = get(globalObject, propertyName);
#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    VM& vm = getVM(globalObject);
    if (vm.exceptionForInspection())
        return nullptr;
#endif
    return uncheckedDowncast<std::remove_pointer_t<T>>(value);
}

template<typename CellType, SubspaceAccess>
CompleteSubspace* JSFinalObject::subspaceFor(VM& vm)
{
    static_assert(CellType::needsDestruction == DoesNotNeedDestruction);
    return &vm.cellSpace();
}

// https://tc39.es/ecma262/#sec-createlistfromarraylike
template <typename Functor> // A functor should have a type like: (JSValue) -> bool
void forEachInArrayLike(JSGlobalObject* globalObject, JSObject* arrayLikeObject, Functor functor)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint64_t length = toLength(globalObject, arrayLikeObject);
    RETURN_IF_EXCEPTION(scope, void());
    for (uint64_t index = 0; index < length; index++) {
        JSValue value = arrayLikeObject->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, void());
        if (!functor(value))
            return;
    }
}

ALWAYS_INLINE bool JSObject::canPerformFastPutInlineExcludingProto()
{
    // Check if there are any setters or getters in the prototype chain
    JSValue prototype;
    JSObject* obj = this;
    while (true) {
        Structure* structure = obj->structure();
        if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype())
            return false;
        if (obj != this && structure->typeInfo().overridesPut())
            return false;

        prototype = obj->getPrototypeDirect();
        if (prototype.isNull())
            return true;

        obj = asObject(prototype);
    }

    ASSERT_NOT_REACHED();
}

ALWAYS_INLINE bool JSObject::canPerformFastPutInline(VM& vm, PropertyName propertyName)
{
    if (propertyName == vm.propertyNames->underscoreProto) [[unlikely]]
        return false;
    return canPerformFastPutInlineExcludingProto();
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, CallbackWhenNoException callback) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    return getPropertySlot(globalObject, propertyName, slot, callback);
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::invoke_result<CallbackWhenNoException, bool, PropertySlot&>::type JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot, CallbackWhenNoException callback) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    bool found = const_cast<JSObject*>(this)->getPropertySlot(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, callback(found, slot));
}

ALWAYS_INLINE bool JSObject::getPropertySlot(JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = this;
    while (true) {
        Structure* structure = object->structureID().decode();
        bool hasSlot = structure->classInfoForCells()->methodTable.getOwnPropertySlotByIndex(object, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, false);
        if (hasSlot)
            return true;
        if (slot.isVMInquiry() && slot.isTaintedByOpaqueObject()) [[unlikely]]
            return false;
        if (object->type() == ProxyObjectType && slot.internalMethodType() == PropertySlot::InternalMethodType::HasProperty)
            return false;
        if (isTypedArrayType(object->type()) && propertyName >= uncheckedDowncast<JSArrayBufferView>(object)->length())
            return false;
        JSValue prototype;
        if (!structure->typeInfo().overridesGetPrototype() || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry) [[likely]]
            prototype = object->getPrototypeDirect();
        else {
            prototype = object->getPrototype(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

ALWAYS_INLINE bool JSObject::getPropertySlot(JSGlobalObject* globalObject, uint64_t propertyName, PropertySlot& slot)
{
    if (propertyName <= MAX_ARRAY_INDEX) [[likely]]
        return getPropertySlot(globalObject, static_cast<uint32_t>(propertyName), slot);
    return getPropertySlot(globalObject, Identifier::from(globalObject->vm(), propertyName), slot);
}

ALWAYS_INLINE bool JSObject::getNonIndexPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    // This method only supports non-index PropertyNames.
    ASSERT(!parseIndex(propertyName));

    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = this;
    while (true) {
        Structure* structure = object->structureID().decode();
        if (!TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags())) [[likely]] {
            if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
                return true;
        } else {
            bool hasSlot = structure->classInfoForCells()->methodTable.getOwnPropertySlot(object, globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, false);
            if (hasSlot)
                return true;
            if (slot.isVMInquiry() && slot.isTaintedByOpaqueObject()) [[unlikely]]
                return false;
            if (object->type() == ProxyObjectType && slot.internalMethodType() == PropertySlot::InternalMethodType::HasProperty)
                return false;
            if (isTypedArrayType(object->type()) && isCanonicalNumericIndexString(propertyName.uid()))
                return false;
        }
        JSValue prototype;
        if (!structure->typeInfo().overridesGetPrototype() || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry) [[likely]]
            prototype = object->getPrototypeDirect();
        else {
            prototype = object->getPrototype(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

inline bool JSObject::getOwnPropertySlotInline(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    if (TypeInfo::overridesGetOwnPropertySlot(inlineTypeFlags())) [[unlikely]]
        return methodTable()->getOwnPropertySlot(this, globalObject, propertyName, slot);
    return JSObject::getOwnPropertySlot(this, globalObject, propertyName, slot);
}

template<typename PropertyNameType> inline JSValue JSObject::getIfPropertyExists(JSGlobalObject* globalObject, const PropertyNameType& propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertySlot slot(this, PropertySlot::InternalMethodType::HasProperty);
    bool hasProperty = getPropertySlot(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, { });
    if (!hasProperty)
        return { };

    scope.release();
    if (slot.isTaintedByOpaqueObject()) [[unlikely]]
        return get(globalObject, propertyName);

    return slot.getValue(globalObject, propertyName);
}

// FIXME: Given the single special purpose this is used for, it's unclear if this needs to be a JSObject member function.
inline bool JSObject::noSideEffectMayHaveNonIndexProperty(VM& vm, PropertyName propertyName)
{
    // This function only supports non-index PropertyNames.
    ASSERT(!parseIndex(propertyName));
    ASSERT(propertyName != vm.propertyNames->length);
    for (auto* object = this; object; object = object->getPrototypeDirect().getObject()) {
        auto inlineTypeFlags = object->inlineTypeFlags();
        if (TypeInfo::overridesGetOwnPropertySlot(inlineTypeFlags) && object->classInfo() != ArrayPrototype::info()) [[unlikely]]
            return true;
        auto& structure = *object->structureID().decode();
        unsigned attributes;
        if (isValidOffset(structure.get(vm, propertyName, attributes))) [[unlikely]]
            return true;
        if (object->hasNonReifiedStaticProperties()) {
            for (auto* ancestorClass = object->classInfo(); ancestorClass; ancestorClass = ancestorClass->parentClass) {
                if (auto* table = ancestorClass->staticPropHashTable; table && table->entry(propertyName)) [[unlikely]]
                    return true;
            }
        }
        if (structure.typeInfo().overridesGetPrototype()) [[unlikely]]
            return true;
    }
    return false;
}

inline bool JSObject::mayInterceptIndexedAccesses()
{
    return structure()->mayInterceptIndexedAccesses();
}

inline void JSObject::putDirectWithoutTransition(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!value.isGetterSetter() && !(attributes & PropertyAttribute::Accessor));
    ASSERT(!value.isCustomGetterSetter());
    StructureID structureID = this->structureID();
    Structure* structure = structureID.decode();
    PropertyOffset offset = prepareToPutDirectWithoutTransition(vm, propertyName, attributes, structureID, structure);
    putDirectOffset(vm, offset, value);
    if (attributes & PropertyAttribute::ReadOnly)
        structure->setContainsReadOnlyProperties();
}

ALWAYS_INLINE PropertyOffset JSObject::prepareToPutDirectWithoutTransition(VM& vm, PropertyName propertyName, unsigned attributes, StructureID structureID, Structure* structure)
{
    unsigned oldOutOfLineCapacity = structure->outOfLineCapacity();
    PropertyOffset result;
    structure->addPropertyWithoutTransition(
        vm, propertyName, attributes,
        [&] (const GCSafeConcurrentJSLocker&, PropertyOffset offset, PropertyOffset newMaxOffset) {
            unsigned newOutOfLineCapacity = Structure::outOfLineCapacity(newMaxOffset);
            if (newOutOfLineCapacity != oldOutOfLineCapacity) {
                Butterfly* butterfly = allocateMoreOutOfLineStorage(vm, oldOutOfLineCapacity, newOutOfLineCapacity);
                nukeStructureAndSetButterfly(vm, structureID, butterfly);
                structure->setMaxOffset(vm, newMaxOffset);
                WTF::storeStoreFence();
                setStructureIDDirectly(structureID);
            } else
                structure->setMaxOffset(vm, newMaxOffset);

            // This assertion verifies that the concurrent GC won't read garbage if the concurrentGC
            // is running at the same time we put without transitioning.
            ASSERT(!getDirect(offset) || !JSValue::encode(getDirect(offset)));
            result = offset;
        });
    if (mayBePrototype()) [[unlikely]]
        vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Add);
    return result;
}

// https://tc39.es/ecma262/#sec-ordinaryset
ALWAYS_INLINE bool JSObject::putInlineForJSObject(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);

    JSObject* thisObject = uncheckedDowncast<JSObject>(cell);
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    // Try indexed put first. This is required for correctness, since loads on property names that appear like
    // valid indices will never look in the named property storage.
    if (std::optional<uint32_t> index = parseIndex(propertyName)) {
        if (isThisValueAltered(slot, thisObject)) [[unlikely]]
            return ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());
        return thisObject->methodTable()->putByIndex(thisObject, globalObject, index.value(), value, slot.isStrictMode());
    }

    if (!thisObject->canPerformFastPutInline(vm, propertyName))
        return thisObject->putInlineSlow(globalObject, propertyName, value, slot);
    if (isThisValueAltered(slot, thisObject)) [[unlikely]]
        return definePropertyOnReceiver(globalObject, propertyName, value, slot);
    if (thisObject->hasNonReifiedStaticProperties()) [[unlikely]]
        return thisObject->putInlineFastReplacingStaticPropertyIfNeeded(globalObject, propertyName, value, slot);
    return thisObject->putInlineFast(globalObject, propertyName, value, slot);
}

ALWAYS_INLINE bool JSCell::putInline(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    Structure* structure = this->structure();
    if (!structure->typeInfo().overridesPut()) [[likely]]
        return JSObject::putInlineForJSObject(asObject(this), globalObject, propertyName, value, slot);
    return structure->methodTable()->put(this, globalObject, propertyName, value, slot);
}

ALWAYS_INLINE bool JSValue::putInline(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    if (!isCell()) [[unlikely]]
        return putToPrimitive(globalObject, propertyName, value, slot);
    return asCell()->putInline(globalObject, propertyName, value, slot);
}

ALWAYS_INLINE bool JSObject::putInlineFast(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto error = putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot);
    if (!error.isNull())
        return typeError(globalObject, scope, slot.isStrictMode(), error);
    return true;
}

// https://tc39.es/ecma262/#sec-createdataproperty
ALWAYS_INLINE bool JSObject::createDataProperty(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, bool shouldThrow)
{
    PropertyDescriptor descriptor(value, static_cast<unsigned>(PropertyAttribute::None));
    return methodTable()->defineOwnProperty(this, globalObject, propertyName, descriptor, shouldThrow);
}

// HasOwnProperty(O, P) from section 7.3.11 in the spec.
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-hasownproperty
ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot) const
{
    ASSERT(slot.internalMethodType() == PropertySlot::InternalMethodType::GetOwnProperty);
    if (const_cast<JSObject*>(this)->methodTable()->getOwnPropertySlot == JSObject::getOwnPropertySlot) [[likely]]
        return JSObject::getOwnPropertySlot(const_cast<JSObject*>(this), globalObject, propertyName, slot);
    return const_cast<JSObject*>(this)->methodTable()->getOwnPropertySlot(const_cast<JSObject*>(this), globalObject, propertyName, slot);
}

ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    return hasOwnProperty(globalObject, propertyName, slot);
}

ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, unsigned propertyName) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    return const_cast<JSObject*>(this)->methodTable()->getOwnPropertySlotByIndex(const_cast<JSObject*>(this), globalObject, propertyName, slot);
}

// Field-type record maintenance for the C++ property-store paths. If a field's record says it always holds
// structure T, a store of a differently-shaped value must generalise the record. The JIT tiers check inline
// or decline the site and generalise the field; these hooks cover the paths that store with no emitted check
// at all, so JIT writers are not excluded by construction. Deliberately not gated on a per-structure summary
// bit: such a bit answers whether a structure's transition subtree has some record, but a store needs
// whether THIS offset has one, and the two almost always diverge.
// True when a property creation provably has nothing to maintain, decided entirely from the shape that
// owns the field: two loads off `owner` and a compare, with no lock, hash or allocation. Callers hoist
// this into their own fast path so the common case does not even make a call.
static ALWAYS_INLINE bool fieldTypeCreationNeedsNoWork(VM& vm, Structure* owner, JSValue value)
{
    // One load off the owner's own cache line answers the whole question; see
    // Structure::m_fieldTypeClaimIndex for the encoding.
    uint16_t index = owner->fieldTypeClaimIndex();
    if (index == FieldTypeClaimIndex::entryWithoutClaim)
        return true;
    if (index < FieldTypeClaimIndex::firstClaim)
        return false;   // nothing recorded yet, or an ancestor may own this offset
    // A live claim. Only now is the stored value's header touched -- a different cache line -- which is why
    // the no-claim cases are ordered first. The bounds check is an ASSERT rather than a branch because
    // indices are produced only by allocateClaimIndex, slots are append-only and never recycled, and
    // exhaustion returns noEntryYet, which exits above; the base is non-null whenever index >= firstClaim,
    // since that index can only exist if a slot was appended.
    uint32_t observedBits = value.isCell() ? value.asCell()->structureID().bits() : 0;
    ASSERT(vm.fieldTypeClaimBits());
    ASSERT(vm.fieldTypeWatchpoints()
        && index - FieldTypeClaimIndex::firstClaim < vm.fieldTypeWatchpoints()->claimIndexCount());
    return vm.fieldTypeClaimBits()[index - FieldTypeClaimIndex::firstClaim] == observedBits;
}

// A structure whose properties the VM writes with putDirectOffset and no field-type maintenance must have
// those fields permanently generalised when the structure is built -- not because the VM's own writes need
// recording, but because user code can reach the same structure. Adding `index`, `input`, `groups` to a
// `new Array` builds transition-for-transition what createRegExpMatchesArrayStructure builds, so both land on
// one cached structure: the user's creation claims `groups`, the VM's unhooked write falsifies it, and a
// narrowed load reads a double as a pointer (JSTests/stress/inferred-types-regex-matches-array.js).
static ALWAYS_INLINE void poisonFieldTypesForVMWrittenProperty(VM& vm, Structure* owner, PropertyOffset offset)
{
    if (!Options::useFieldTypeAssumptions()) [[unlikely]]
        return;
    if (!owner || offset == invalidOffset)
        return;
    // recordForStoreSite inserts the permanently-generalised (null) entry when the field is untouched and
    // otherwise returns the existing record, which must then be withdrawn in case user code got here first
    // and already established a claim.
    if (RefPtr record = vm.ensureFieldTypeWatchpoints().recordForStoreSite(owner->id(), offset))
        record->generalize(vm);
    owner->setFieldTypeClaimIndex(FieldTypeClaimIndex::entryWithoutClaim);
}

// Records the field type at property creation. `owner` is the structure the add transition created, which
// is the offset owner by definition, so the record is keyed correctly with no walk and predates every
// object that will ever have this shape. Only cell-valued fields get a record, matching V8, where a Class
// field type exists only for kHeapObject-representation fields (map-updater.cc:1310).
//
// Must be called from EVERY creation path: both the add transition that creates a brand new structure and
// the reuse of an existing transition, which is the path the second and every later object of a shape
// takes. Hooking only the former is silently unsound -- the record then keeps the first object's field
// structure forever and the "a later object disagrees" test can never fire.
static ALWAYS_INLINE void recordFieldTypeAtCreationImpl(VM&, Structure*, PropertyOffset, JSValue);

// Direct timing of the recorder, because neither sampling nor ablation could locate its cost on chai-wtb:
// every individual operation inside it (its reads, the claim-word write, a table lookup, the locked hash
// insert, the O(n^2) ancestor loop, arming the value's transition watchpoint) measures ~0 in a layout-matched
// pair, while the recorder as a whole measures 2.16 points -- and the samply profiles failed their own
// reproduction check (todo/24). Two MonotonicTime reads per call is ~40ns of overhead against a suspected
// ~266ns per call, so the ratio is readable; the overhead is measured separately by fieldTypeTimeRecorder=2,
// which times an empty section and reports the floor.
static ALWAYS_INLINE void recordFieldTypeAtCreation(VM& vm, Structure* owner, PropertyOffset offset, JSValue value)
{
    if (Options::fieldTypeTimeRecorder()) [[unlikely]] {
        MonotonicTime start = MonotonicTime::now();
        // Mode 3 must run the real body too -- it times the LOCK ACQUISITION inside it. Only mode 2 is the
        // empty-section floor. Getting this wrong made mode 3 silently measure the floor (10 ns, no LOCK-WAIT line).
        if (Options::fieldTypeTimeRecorder() == 1 || Options::fieldTypeTimeRecorder() == 3)
            recordFieldTypeAtCreationImpl(vm, owner, offset, value);
        uint64_t elapsed = static_cast<uint64_t>((MonotonicTime::now() - start).nanoseconds());
        vm.fieldTypeRecorderNanos().fetch_add(elapsed, std::memory_order_relaxed);
        vm.fieldTypeRecorderCalls().fetch_add(1, std::memory_order_relaxed);
        return;
    }
    recordFieldTypeAtCreationImpl(vm, owner, offset, value);
}

static ALWAYS_INLINE void recordFieldTypeAtCreationImpl(VM& vm, Structure* owner, PropertyOffset offset, JSValue value)
{
    if (!Options::useFieldTypeCreationRecording()) [[unlikely]] {
        // MEASUREMENT ONLY. Creation recording carries json-parse-inspector's whole +1.7%, and that win
        // survives NeverClaim and survives DFG+FTL off, so it is not narrowing, not speculation and not a
        // claim. What is left is the recorder's own effect on the C++ property-creation path -- and "any
        // added work here would do this" is a live alternative that has to be excluded rather than argued
        // away. These levels substitute a strict subset of the recorder's work so the win can be attributed
        // to reads, to the Structure write, or to the table:
        //   1 = the reads only          2 = reads + the claim-word write     3 = reads + a table lookup
        if (unsigned level = Options::fieldTypeCreationDummyWork()) [[unlikely]] {
            uint64_t acc = owner->maxOffset() + static_cast<uint64_t>(owner->transitionOffset());
            if (Structure* previous = owner->previousID())
                acc += previous->maxOffset();
            if (value.isCell())
                acc += value.asCell()->structureID().bits();
            if (level >= 2)
                owner->setFieldTypeClaimIndex(FieldTypeClaimIndex::entryWithoutClaim);
            if (level >= 3) {
                if (auto* table = vm.fieldTypeWatchpoints())
                    acc += table->expectedFor(owner->id(), offset).bits();
            }
            if (level >= 4)
                vm.ensureFieldTypeWatchpoints().insertGeneralizedForMeasurement(owner->id(), offset);
            asm volatile("" :: "r"(acc) : "memory");
        }
        return;
    }
    if (!Options::useFieldTypeAssumptions()) [[unlikely]]
        return;

    // Never record a dictionary owner. addNewPropertyTransition can return a cacheable dictionary that added
    // the property in place, and the claim word is per-structure, so one word cannot describe the many
    // properties a dictionary adds to one structure -- the fast path would answer for a different offset's
    // claim. Nothing could consume such a record anyway: findOffsetOwner returns null for a dictionary, so
    // the compiler and the store paths decline it and the heap verifier skips it.
    if (owner->isDictionary())
        return;
    if (Options::fieldTypeTimeRecorderStopAfter() == 1) [[unlikely]]
        return;

    // With dictionaries excluded, the invariant the one-word-per-Structure design rests on holds: `owner`
    // is the structure that added this offset, so its single claim word describes THIS field. Callers pass
    // either the transition target or findOffsetOwner(offset), both of which are the adding structure.
    ASSERT(owner->transitionOffset() == offset);

    // Withdraw any claim in the ANCESTRY that this creation contradicts, not just the one keyed on `owner`.
    // Two objects of the same final shape can reach it by different transition paths, so the structure that
    // adds the property differs and a withdrawal aimed only at `owner` misses the claim the other path
    // established. Withdrawing all claims naming the field is sound and precise: only claims this exact
    // value contradicts are touched.
    //
    // The loop does a locked hash lookup per ancestor, hence O(n^2) for an n-property object (+60% on a
    // JSON.parse microbenchmark), so it is skipped when no ancestor can own a record at this offset.
    // Structure::add sets maxOffset = max(newOffset, oldMaxOffset), so owner->maxOffset() == offset for both
    // a fresh offset and a reused offset that was already the shape's maximum; only the latter needs the
    // loop, and the two are indistinguishable from `owner` alone, hence the previousID() chase. maxOffset()
    // is invalidOffset for the root structure, so the comparison is false there.
    //
    // This cannot be hoisted behind fieldTypeCreationNeedsNoWork below: if the owner's own claim matches the
    // value, the fast path returns and an ancestor's contradicted claim at the same offset would never be
    // withdrawn. Caching the negative answer would need a fifth claim-word state and there is none spare.
    bool ancestorCouldOwnThisOffset = false;
    if (Options::useFieldTypeCreationAncestorCheck()) [[likely]] {
        if (owner->maxOffset() != offset)
            ancestorCouldOwnThisOffset = true;
        else if (Structure* previousOwner = owner->previousID()) {
            // Counted, not assumed: `skippable` is the share of these chases whose answer the claim word
            // already determines, because any state other than noEntryYet can only have been reached by a
            // completed chase. That share is what a cache would remove; see VM::m_fieldTypeCreationChaseCount.
            if (Options::useDollarVM()) [[unlikely]] {
                vm.fieldTypeCreationChaseCount().fetch_add(1, std::memory_order_relaxed);
                if (owner->fieldTypeClaimIndex() != FieldTypeClaimIndex::noEntryYet)
                    vm.fieldTypeCreationChaseSkippableCount().fetch_add(1, std::memory_order_relaxed);
            }
            ancestorCouldOwnThisOffset = offset <= previousOwner->maxOffset();
        }
    }
    // Recorded in the claim word so every later creation on this shape takes the slow path without
    // re-deriving it, which also keeps the fast path below from skipping a shape with a reused offset.
    if (ancestorCouldOwnThisOffset) [[unlikely]]
        owner->setFieldTypeClaimIndex(FieldTypeClaimIndex::offsetWasReused);

    if (Options::fieldTypeTimeRecorderStopAfter() == 2) [[unlikely]]
        return;
    // Fast path: the shape itself answers both questions the table would be asked, so the steady-state cost
    // of recording is a load and a compare. The claim word is a CACHE of the table and the safety direction
    // is asymmetric -- a stale-SET claim is conservative (we go to the table and find nothing to do), while
    // a stale-CLEAR claim would skip maintenance of a live claim and be UNSOUND. FieldTypeRecord::generalize
    // therefore clears the word as it clears the record.
    // Before the fast path, deliberately: the creations this skips are exactly the ones the census exists to see.
    if (Options::logFieldTypes()) [[unlikely]] {
        // claimedIsLeaf is V8's admission gate: Object::OptimalType creates a Class field type ONLY when the
        // value's map is_stable(), i.e. nothing has ever been derived from it. JSC claims regardless and then
        // invalidates reactively when the shape moves.
        bool claimedIsLeaf = value.isCell() && !value.asCell()->structure()->hasBeenTransitionedFrom();
        vm.ensureFieldTypeWatchpoints().noteCreationForCensus(owner->id(), offset, value, owner->mayBePrototype(), claimedIsLeaf);
    }
    if (fieldTypeCreationNeedsNoWork(vm, owner, value))
        return;
    if (Options::fieldTypeTimeRecorderStopAfter() == 3) [[unlikely]]
        return;

    if (ancestorCouldOwnThisOffset) [[unlikely]] {
        if (auto* table = vm.fieldTypeWatchpoints(); table && table->sizeRelaxed()) [[unlikely]] {
            StructureID observed = value.isCell() ? value.asCell()->structureID() : StructureID();
            for (Structure* candidate = owner->previousID(); candidate; candidate = candidate->previousID()) {
                StructureID expected = table->expectedFor(candidate->id(), offset);
                if (!expected || expected == observed)
                    continue;
                if (Options::logFieldTypes()) [[unlikely]]
                    dataLogLn("[fieldtype] WITHDRAW-ANCESTOR-AT-CREATION owner=", candidate->id().bits(), " offset=", offset, " had=", expected.bits(), " got=", observed.bits());
                if (Options::useDollarVM()) [[unlikely]]
                    vm.fieldTypeAncestorWithdrawalCount().fetch_add(1, std::memory_order_relaxed);
                table->generalize(vm, candidate->id(), offset);
            }
        }
    }

    if (value.isCell()) {
        // A record claims "this field holds an object of structure S", but S describes a DIFFERENT object,
        // which can transition away at any time with nothing stored to this field: `A.prototype` is
        // recorded when constructPrototypeObject sets `constructor`, and a later `A.prototype.x = 1` moves
        // it. JSC's transition watchpoint starts UNARMED and WatchpointSet::fireAll returns without
        // invalidating a ClearWatchpoint set, so such a structure still looks pristine and Graph::tryWatch
        // would adopt an already-false claim. Arming here makes every claimed structure behave like a V8
        // map, which is stable until its first transition and never again (property-access-builder.cc:381).
    if (Options::fieldTypeTimeRecorderStopAfter() == 4) [[unlikely]]
        return;
        Structure* observedStructure = value.asCell()->structure();
        if (Options::useFieldTypeCreationArmsTransitionWatchpoint()) [[likely]] {
            if (observedStructure->transitionWatchpointSetIsStillValid())
                observedStructure->transitionWatchpointSet().startWatching();
        }

    if (Options::fieldTypeTimeRecorderStopAfter() == 5) [[unlikely]]
        return;
        StructureID bornClaim { };
        const char* bornSite = nullptr;
        uint32_t claimBits = 0;
        uint16_t claimIndex = FieldTypeClaimIndex::noEntryYet;
        vm.ensureFieldTypeWatchpoints().recordAtCreation(vm, owner->id(), offset, value, &claimBits, &claimIndex, &bornClaim, &bornSite);
        // bornClaim/bornSite were plumbed all the way out of recordAtCreation and then DROPPED -- the claim's
        // provenance was tracked and never reported, which is why the provenance axis could not be tested. There
        // are exactly two birth sites, so this one line splits every live claim into creation-provenance
        // ("creation-on-brand-new-entry") and store-site-provenance ("creation-establishing-on-store-site-record").
        if (Options::logFieldTypes() && bornClaim) [[unlikely]] {
            // The CLAIMED VALUE's type and class, not the owner's. Needed to test whether any kind or shape
            // restriction could separate raytrace's three harmful claims from delta-blue's valuable ones --
            // fieldTypeClaimableKinds filters exactly this JSType, and FinalObjectsOnly is its strictest
            // setting before NeverClaim, so if both sides are FinalObject the axis is arithmetically dead.
            Structure* claimedStructure = bornClaim.decode();
            dataLogLn("[fieldtype] CLAIM-BORN site=", bornSite ? bornSite : "<null>",
                " field=", fieldTypeFieldName(owner, offset).data(),
                " owner=", owner->id().bits(), " ownerClass=", owner->classInfoForCells()->className,
                " offset=", offset, " expected=", bornClaim.bits(),
                " valueType=", claimedStructure ? static_cast<unsigned>(claimedStructure->typeInfo().type()) : 0u,
                " valueClass=", claimedStructure ? claimedStructure->classInfoForCells()->className : "<dead>",
                " valueProps=", claimedStructure ? static_cast<unsigned>(claimedStructure->maxOffset() + 1) : 0u);
        }
        // Publish the outcome on the shape, so later creations of it hit the fast path above without
        // touching the table. A real claim is a live StructureID, hence even (bit 0 is
        // StructureID::nukedStructureIDBit), so it can never be mistaken for one of the odd sentinels.
        ASSERT(!claimBits || !(claimBits & 1));
        if (owner->fieldTypeClaimIndex() != FieldTypeClaimIndex::offsetWasReused) {
            // The slot was allocated inside recordAtCreation, under the lock it already held, and the record
            // remembers which one it got so pruneAfterMarking can hand it back.
            owner->setFieldTypeClaimIndex(claimBits ? claimIndex : FieldTypeClaimIndex::entryWithoutClaim);
        }
        // Deliberately does NOT start watching the property for replacements: the consumer depends only on
        // the record's own set, because every store path checks inline. Watching here forced a
        // StructureRareData plus a WatchpointSet for every cell-valued field in the program.
        return;
    }

    // A non-cell creation must INSERT the permanently-generalised entry, not merely generalise one that
    // happens to already exist. FieldTypeWatchpointTable::generalize does nothing when there is no entry, so
    // an object that created this field with a number would leave NO trace, and a later object of the same
    // shape creating it with a cell would then take recordAtCreation's brand-new-entry branch and establish a
    // claim the first object already contradicted. No store is involved, which is why the store-side
    // diagnostic reports nothing here.
    //
    // recordAtCreation handles all four cases for a non-cell: it inserts Entry { owner, nullptr } on a
    // brand-new entry (the permanently-generalised state), returns for an already-null entry, and generalises
    // an unclaimed or claimed record. The cost is bounded by the number of distinct (owner, offset) shapes.
    uint32_t claimBits = 0;
    uint16_t claimIndex = FieldTypeClaimIndex::noEntryYet;
    vm.ensureFieldTypeWatchpoints().recordAtCreation(vm, owner->id(), offset, value, &claimBits, &claimIndex, nullptr, nullptr);
    if (owner->fieldTypeClaimIndex() != FieldTypeClaimIndex::offsetWasReused) {
        // Via the out-param, so the record remembers its slot and pruneAfterMarking can reclaim it.
        owner->setFieldTypeClaimIndex(claimBits ? claimIndex : FieldTypeClaimIndex::entryWithoutClaim);
    }
}

static ALWAYS_INLINE void maintainFieldTypeRecord(VM& vm, Structure* structure, PropertyOffset offset, JSValue value)
{
    auto* table = vm.fieldTypeWatchpoints();
    if (!table || !table->sizeRelaxed()) [[likely]]
        return;
    if (!Options::useFieldTypeReplaceMaintenance()) [[unlikely]]
        return;

    // The population counter for verified/03's mechanism. CXX-REPLACE cannot serve: it is logged AFTER the
    // claim-word short-circuit below, so it counts CLAIMED replace stores rather than calls, and reading it as a
    // call count is the F6 error. This one counts every call that reaches the findOffsetOwner chain walk.
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] CXX-REPLACE-CALL offset=", offset);

    StructureID observed = value.isCell() ? value.asCell()->structureID() : StructureID();

    // Precise first: when the owner resolves, generalise exactly that one record. Walking the ancestry
    // unconditionally instead withdraws claims on every ancestor with a record at this offset, which gave
    // away a third of the delta-blue win for nothing.
    //
    // Memoised: the walk is this function's dominant cost (verified/03) and its answer is immutable for a live
    // (structure, offset). useFieldTypeOwnerMemo exists to price the memo itself, not to change behaviour --
    // both legs compute the same owner.
    Structure* owner = Options::useFieldTypeOwnerMemo()
        ? table->findOffsetOwnerMemoised(structure, offset)
        : structure->findOffsetOwner(offset);
    if (owner) {
        // Lock-free short-circuit. entryWithoutClaim is written by clearOwnerShapeClaimCache, which
        // generalize() calls only AFTER nulling the record's m_expected, so this state implies there is no
        // claim left to withdraw. Every other state (noEntryYet, offsetWasReused, a live index) falls through
        // to the table, which is the conservative direction. Without this, every C++ replace store paid a
        // locked hash lookup: 30219 of them on babel-wtb, which regressed 5.32%.
        if (owner->fieldTypeClaimIndex() == FieldTypeClaimIndex::entryWithoutClaim) [[likely]]
            return;
        StructureID expected = table->expectedFor(owner->id(), offset);
        // The pattern hunt's store-traffic feature, counted against the SAME key the census records at creation.
        // Placed after the owner resolves so the key matches; the entryWithoutClaim short-circuit above means a
        // field whose claim is already withdrawn stops being counted, which is the intended semantics -- what
        // matters is store traffic on a field while a claim on it is live.
        if (Options::logFieldTypes()) [[unlikely]]
            table->noteStoreForCensus(owner->id(), offset);
        if (Options::logFieldTypes()) [[unlikely]]
            dataLogLn("[fieldtype] CXX-REPLACE owner=", owner->id().bits(), " offset=", offset, " expected=", expected.bits(), " observed=", observed.bits());
        if (expected && expected != observed) {
            // THE QUESTION THIS ANSWERS: is the contradicting value the SAME JS class as the claim? If it is, JSC is
            // splitting into two Structures what V8 represents with one Map, and the "contradiction" is an artefact
            // of structure identity rather than real polymorphism. Printed only for claims that have dependents,
            // i.e. the ones whose withdrawal actually costs something.
            if (Options::logFieldTypes()) [[unlikely]] {
                Structure* had = expected.decode();
                Structure* got = observed.decode();
                if (had && got) {
                    dataLogLn("[fieldtype] STORE-CONTRADICTION offset=", offset,
                        " hadClass=", had->classInfoForCells()->className,
                        " gotClass=", got->classInfoForCells()->className,
                        " sameClass=", had->classInfoForCells() == got->classInfoForCells(),
                        " sameProto=", had->storedPrototype() == got->storedPrototype(),
                        " hadProps=", had->outOfLineSize(), " gotProps=", got->outOfLineSize(),
                        " hadTransitionOffset=", had->transitionOffset(), " gotTransitionOffset=", got->transitionOffset());
                }
            }
            // The STORE-side contradiction. raytrace's three harmful withdrawals (color 131, direction 117,
            // position 70 dependents) come through here, not through creation -- the creation-side CONTRADICTION
            // log names only harness fields. Naming both structures here is what distinguishes "the program really
            // is polymorphic" from "JSC split into two Structures what V8 represents with one Map".
            if (Options::logFieldTypes()) [[unlikely]]
                FieldTypeWatchpointTable::reportContradiction(owner->id(), offset, expected, observed);
            table->generalize(vm, owner->id(), offset);
        }
        return;
    }

    // The owner could not be resolved -- a dictionary, or offsets reused after a deletion -- so fall back to
    // the ancestry. Treating that as "nothing to do" leaves a claim keyed on an ancestor silently violated,
    // which is how Object.defineProperty violated 115572 claims across JetStream3. Generalising a record
    // that turns out to belong to an unrelated property sharing this offset only costs a claim; the other
    // direction corrupts the heap.
    for (Structure* candidate = structure; candidate; candidate = candidate->previousID()) {
        StructureID expected = table->expectedFor(candidate->id(), offset);
        if (!expected || expected == observed)
            continue;
        if (Options::logFieldTypes()) [[unlikely]]
            FieldTypeWatchpointTable::reportContradiction(candidate->id(), offset, expected, observed);
        table->generalize(vm, candidate->id(), offset);
    }
}

// An object built by copying another object's property storage wholesale -- Object.assign's and object
// spread's fast paths, which memcpy the butterfly and inline storage -- has had no per-property hook run on
// it. Claims that already exist stay true, since the copy takes the source's structure and values, but a
// copy holding a non-cell would leave no record and let a LATER object of the same shape establish a claim
// the copy already contradicts, with no store afterwards for any check to catch. No object may predate a
// record, so the copy must record too.
static ALWAYS_INLINE void recordFieldTypesForCopiedObject(VM& vm, JSObject* object, Structure* structure)
{
    if (!Options::useFieldTypeAssumptions()) [[unlikely]]
        return;
    if (!structure || structure->isDictionary())
        return;

    for (const PropertyTableEntry& entry : structure->getPropertiesConcurrently()) {
        PropertyOffset offset = entry.offset();
        JSValue value = object->getDirect(offset);
        if (!value)
            continue;
        if (Structure* owner = structure->findOffsetOwner(offset))
            recordFieldTypeAtCreation(vm, owner, offset, value);
        else
            maintainFieldTypeRecord(vm, structure, offset, value);
    }
}

template<JSObject::PutMode mode>
ALWAYS_INLINE ASCIILiteral JSObject::putDirectInternal(VM& vm, PropertyName propertyName, JSValue value, unsigned newAttributes, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(value.isGetterSetter() == !!(newAttributes & PropertyAttribute::Accessor));
    // A hook that generalises a record calls WatchpointSet::fireAll, which constructs DeferGCForAWhile, so
    // its destructor can collect in the MIDDLE of this function. Deferring across the whole function moves
    // any such collection to the exit, where JSC already tolerates one. Gated on the feature because
    // ungated this cost two Heap::m_deferralDepth RMWs on every property creation even with the option off,
    // i.e. the kill switch did not restore baseline.
    std::optional<DeferGCForAWhile> fieldTypeDeferGC;
    // useFieldTypeCreationDeferGC is a MEASUREMENT SWITCH: off leaves the nuked-transition window that caused
    // the babylon-wtb heap corruption open, and exists only to price these two m_deferralDepth RMWs.
    if (Options::useFieldTypeAssumptions() && Options::useFieldTypeCreationDeferGC()) [[likely]]
        fieldTypeDeferGC.emplace(vm);
    ASSERT(value.isCustomGetterSetter() == !!(newAttributes & PropertyAttribute::CustomAccessorOrValue));
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    ASSERT(!parseIndex(propertyName));

    StructureID structureID = this->structureID();
    Structure* structure = structureID.decode();
    if (structure->isDictionary()) {
        ASSERT(!isCopyOnWrite(indexingMode()));
        if constexpr (mode == PutModePut) {
            if (!isStructureExtensible()) [[unlikely]]
                return putDirectToDictionaryWithoutExtensibility(vm, propertyName, value, slot);
        }

        auto [offset, attributes, isAdded] = structure->addOrReplacePropertyWithoutTransition(vm, propertyName, newAttributes, [&](const GCSafeConcurrentJSLocker&, PropertyOffset offset, PropertyOffset newMaxOffset) {
#if JSC_FIELD_TYPE_DIAGNOSTIC_STORE_PROBE
            if (Options::logFieldTypes()) [[unlikely]]
                dataLogLn("[fieldtype] PATH=dictionary-add structure=", structure->id().bits(), " offset=", offset, " valueIsCell=", value.isCell());
#endif
            unsigned oldOutOfLineCapacity = structure->outOfLineCapacity();
            unsigned newOutOfLineCapacity = Structure::outOfLineCapacity(newMaxOffset);
            if (newOutOfLineCapacity != oldOutOfLineCapacity) {
                Butterfly* butterfly = allocateMoreOutOfLineStorage(vm, oldOutOfLineCapacity, newOutOfLineCapacity);
                nukeStructureAndSetButterfly(vm, structureID, butterfly);
                structure->setMaxOffset(vm, newMaxOffset);
                WTF::storeStoreFence();
                setStructureIDDirectly(structureID);
            } else
                structure->setMaxOffset(vm, newMaxOffset);

            // This assertion verifies that the concurrent GC won't read garbage if the concurrentGC
            // is running at the same time we put without transitioning.
            ASSERT_UNUSED(offset, !getDirect(offset) || !JSValue::encode(getDirect(offset)));
        });

        if (!isAdded) {
            if constexpr (mode == PutModePut) {
                if (attributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessor) [[unlikely]]
                    return ReadonlyPropertyChangeError;
            }

            putDirectOffset(vm, offset, value);
            structure->didReplaceProperty(offset);

            // FIXME: Check attributes against PropertyAttribute::CustomAccessorOrValue. Changing GetterSetter should work w/o transition.
            // https://bugs.webkit.org/show_bug.cgi?id=214342
            if ((mode == PutModeDefineOwnProperty) && (newAttributes != attributes || (newAttributes & PropertyAttribute::AccessorOrCustomAccessorOrValue))) {
                DeferredStructureTransitionWatchpointFire deferred(vm, structure);
                setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, newAttributes, &deferred));
                if (mayBePrototype()) [[unlikely]]
                    vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Change);
            } else {
                ASSERT(!(attributes & PropertyAttribute::AccessorOrCustomAccessorOrValue));
                slot.setExistingProperty(this, offset);
            }
            return { };
        }

        validateOffset(offset);
        putDirectOffset(vm, offset, value);
        slot.setNewProperty(this, offset);
        if (attributes & PropertyAttribute::ReadOnly)
            this->structure()->setContainsReadOnlyProperties();
        if (mayBePrototype()) [[unlikely]]
            vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Add);
        return { };
    }

    {
        PropertyOffset offset;
        Structure* newStructure = Structure::addPropertyTransitionToExistingStructure(structure, propertyName, newAttributes, offset);
        if (newStructure) {
        #if JSC_FIELD_TYPE_DIAGNOSTIC_STORE_PROBE
            if (Options::logFieldTypes()) [[unlikely]] {
                dataLogLn("[fieldtype] PATH=existing-transition owner=", newStructure->id().bits(),
                    " offset=", offset, " capChange=", structure->outOfLineCapacity() != newStructure->outOfLineCapacity(),
                    " valueIsCell=", value.isCell());
            }
#endif

            Butterfly* newButterfly = butterfly();
            if (structure->outOfLineCapacity() != newStructure->outOfLineCapacity()) {
                ASSERT(newStructure != this->structure());
                newButterfly = allocateMoreOutOfLineStorage(vm, structure->outOfLineCapacity(), newStructure->outOfLineCapacity());
                nukeStructureAndSetButterfly(vm, structureID, newButterfly);
            }

            validateOffset(offset);
            ASSERT(newStructure->isValidOffset(offset));

            // This assertion verifies that the concurrent GC won't read garbage if the concurrentGC
            // is running at the same time we put without transitioning.
            ASSERT(!getDirect(offset) || !JSValue::encode(getDirect(offset)));
            putDirectOffset(vm, offset, value);
            setStructure(vm, newStructure);
            // After setStructure for the same reason as the new-transition path below, and because
            // newStructure comes from a WeakGCMap transition table: until setStructure runs, nothing but
            // conservative stack scanning keeps it alive across a collection.
            recordFieldTypeAtCreation(vm, newStructure, offset, value);
            slot.setNewProperty(this, offset);
            if (mayBePrototype()) [[unlikely]]
                vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Add);
            return { };
        }
    }

    unsigned currentAttributes;
    PropertyOffset offset = structure->get(vm, propertyName, currentAttributes);
    if (offset != invalidOffset) {
        if (mode == PutModePut && (currentAttributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessor))
            return ReadonlyPropertyChangeError;

        structure->didReplaceProperty(offset);
        maintainFieldTypeRecord(vm, structure, offset, value);
        putDirectOffset(vm, offset, value);

        // FIXME: Check attributes against PropertyAttribute::CustomAccessorOrValue. Changing GetterSetter should work w/o transition.
        // https://bugs.webkit.org/show_bug.cgi?id=214342
        if ((mode == PutModeDefineOwnProperty) && (newAttributes != currentAttributes || (newAttributes & PropertyAttribute::AccessorOrCustomAccessorOrValue))) {
            // We want the structure transition watchpoint to fire after this object has switched structure.
            // This allows adaptive watchpoints to observe if the new structure is the one we want.
            DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);
            setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, newAttributes, &deferredWatchpointFire));
            if (mayBePrototype()) [[unlikely]]
                vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Change);
        } else {
            ASSERT(!(currentAttributes & PropertyAttribute::AccessorOrCustomAccessorOrValue));
            slot.setExistingProperty(this, offset);
        }

        return { };
    }

    if constexpr (mode == PutModePut) {
        if (!isStructureExtensible()) [[unlikely]]
            return NonExtensibleObjectPropertyDefineError;
    }
    
    // We want the structure transition watchpoint to fire after this object has switched structure.
    // This allows adaptive watchpoints to observe if the new structure is the one we want.
    DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);
    Structure* newStructure = Structure::addNewPropertyTransition(vm, structure, propertyName, newAttributes, offset, slot.context(), &deferredWatchpointFire);
    
    validateOffset(offset);
    ASSERT(newStructure->isValidOffset(offset));
    size_t oldCapacity = structure->outOfLineCapacity();
    size_t newCapacity = newStructure->outOfLineCapacity();
    ASSERT(oldCapacity <= newCapacity);
    if (oldCapacity != newCapacity) {
        Butterfly* newButterfly = allocateMoreOutOfLineStorage(vm, oldCapacity, newCapacity);
        nukeStructureAndSetButterfly(vm, structureID, newButterfly);
    }

    // This assertion verifies that the concurrent GC won't read garbage if the concurrentGC
    // is running at the same time we put without transitioning.
    ASSERT(!getDirect(offset) || !JSValue::encode(getDirect(offset)));
#if JSC_FIELD_TYPE_DIAGNOSTIC_STORE_PROBE
    if (Options::logFieldTypes()) [[unlikely]] {
        dataLogLn("[fieldtype] PATH=new-transition owner=", newStructure->id().bits(),
            " offset=", offset, " valueIsCell=", value.isCell());
    }
#endif
    putDirectOffset(vm, offset, value);
    setStructure(vm, newStructure);
    // AFTER setStructure, never before: the hook can allocate, and between nukeStructureAndSetButterfly and
    // setStructure this object's structureID is nuked, meaning its (structure, butterfly, maxOffset) triple
    // is deliberately inconsistent and nothing may exit or call out. setStructure closes that window.
    recordFieldTypeAtCreation(vm, newStructure, offset, value);
    slot.setNewProperty(this, offset);
    if (newAttributes & PropertyAttribute::ReadOnly)
        newStructure->setContainsReadOnlyProperties();
    if (mayBePrototype()) [[unlikely]]
        vm.invalidateStructureChainIntegrity(VM::StructureChainIntegrityEvent::Add);
    return { };
}

inline bool JSObject::mayBePrototype() const
{
    return structure()->mayBePrototype();
}

inline bool JSObject::canGetIndexQuicklyForTypedArray(unsigned i) const
{
    switch (type()) {
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType :\
        return uncheckedDowncast<JS ## name ## Array>(this)->canGetIndexQuickly(i);
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE
    default:
        return false;
    }
}

inline JSValue JSObject::getIndexQuicklyForTypedArray(unsigned i, ArrayProfile* arrayProfile) const
{
#if USE(LARGE_TYPED_ARRAYS)
    if (i > ArrayProfile::s_smallTypedArrayMaxLength && arrayProfile)
        arrayProfile->setMayBeLargeTypedArray();
#else
    UNUSED_PARAM(arrayProfile);
#endif

    switch (type()) {
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType : {\
        auto* typedArray = uncheckedDowncast<JS ## name ## Array>(this);\
        RELEASE_ASSERT(typedArray->canGetIndexQuickly(i));\
        return typedArray->getIndexQuickly(i);\
    }
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return JSValue();
    }
}

inline void JSObject::setIndexQuicklyForTypedArray(unsigned i, JSValue value)
{
    switch (type()) {
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType : {\
        auto* typedArray = uncheckedDowncast<JS ## name ## Array>(this);\
        RELEASE_ASSERT(typedArray->canSetIndexQuickly(i, value));\
        typedArray->setIndexQuickly(i, value);\
        break;\
    }
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

ALWAYS_INLINE void JSObject::setIndexQuicklyForArrayStorageIndexingType(VM& vm, unsigned i, JSValue v)
{
    ArrayStorage* storage = this->butterfly()->arrayStorage();
    WriteBarrier<Unknown>& x = storage->m_vector[i];
    JSValue old = x.get();
    x.set(vm, this, v);
    if (!old) {
        ++storage->m_numValuesInVector;
        if (i >= storage->length())
            storage->setLength(i + 1);
    }
}

inline bool JSObject::trySetIndexQuicklyForTypedArray(unsigned i, JSValue v, ArrayProfile* arrayProfile)
{
    switch (type()) {
#if USE(LARGE_TYPED_ARRAYS)
#define UPDATE_ARRAY_PROFILE(i, arrayProfile) do { \
        if ((i > ArrayProfile::s_smallTypedArrayMaxLength) && arrayProfile)\
            arrayProfile->setMayBeLargeTypedArray();\
    } while (false)
#else
#define UPDATE_ARRAY_PROFILE(i, arrayProfile) do { \
    UNUSED_PARAM(arrayProfile);\
    } while (false)
#endif
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType : { \
        auto* typedArray = uncheckedDowncast<JS ## name ## Array>(this);\
        if (!typedArray->canSetIndexQuickly(i, v))\
            return false;\
        typedArray->setIndexQuickly(i, v);\
        UPDATE_ARRAY_PROFILE(i, arrayProfile);\
        return true;\
    }
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE
#undef UPDATE_ARRAY_PROFILE
    default:
        return false;
    }
}

inline void JSObject::validatePutOwnDataProperty(VM& vm, PropertyName propertyName, JSValue value)
{
#if ASSERT_ENABLED
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    unsigned attributes;
    PropertyOffset offset = structure()->get(vm, propertyName, attributes);
    if (isValidOffset(offset))
        ASSERT(!(attributes & (PropertyAttribute::Accessor | PropertyAttribute::CustomAccessor | PropertyAttribute::ReadOnly)));
    else if (TypeInfo::hasStaticPropertyTable(inlineTypeFlags())) {
        if (auto entry = findPropertyHashEntry(propertyName))
            ASSERT(!(entry->value->attributes() & (PropertyAttribute::Accessor | PropertyAttribute::CustomAccessor | PropertyAttribute::ReadOnly)));
    }
#else // not ASSERT_ENABLED
    UNUSED_PARAM(vm);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(value);
#endif // not ASSERT_ENABLED
}

inline bool JSObject::putOwnDataProperty(VM& vm, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    validatePutOwnDataProperty(vm, propertyName, value);
    return putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot).isNull();
}

inline bool JSObject::putOwnDataPropertyMayBeIndex(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    validatePutOwnDataProperty(vm, propertyName, value);
    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return putDirectIndex(globalObject, index.value(), value, 0, PutDirectIndexLikePutDirect);

    return putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot).isNull();
}

ALWAYS_INLINE CallData getCallData(JSCell* cell)
{
    if (cell->type() == JSFunctionType)
        return JSFunction::getCallData(cell);
    CallData result = cell->methodTable()->getCallData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

inline CallData getCallData(JSValue value)
{
    if (!value.isCell())
        return { };
    return getCallData(value.asCell());
}

ALWAYS_INLINE CallData getCallDataInline(JSCell* cell)
{
    if (cell->type() == JSFunctionType)
        return JSFunction::getCallDataInline(cell);
    CallData result = cell->methodTable()->getCallData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

ALWAYS_INLINE CallData getCallDataInline(JSValue value)
{
    if (!value.isCell())
        return { };
    return getCallDataInline(value.asCell());
}

inline CallData getConstructData(JSValue value)
{
    if (!value.isCell())
        return { };
    JSCell* cell = value.asCell();
    if (cell->type() == JSFunctionType)
        return JSFunction::getConstructData(cell);
    CallData result = cell->methodTable()->getConstructData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

ALWAYS_INLINE CallData getConstructDataInline(JSCell* cell)
{
    if (cell->type() == JSFunctionType)
        return JSFunction::getConstructDataInline(cell);
    CallData result = cell->methodTable()->getConstructData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

ALWAYS_INLINE CallData getConstructDataInline(JSValue value)
{
    if (!value.isCell())
        return { };
    return getConstructDataInline(value.asCell());
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, PropertyName propertyName)
{
    DeletePropertySlot slot;
    return this->methodTable()->deleteProperty(this, globalObject, propertyName, slot);
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, uint32_t propertyName)
{
    return this->methodTable()->deletePropertyByIndex(this, globalObject, propertyName);
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, uint64_t propertyName)
{
    if (propertyName <= MAX_ARRAY_INDEX) [[likely]]
        return deleteProperty(globalObject, static_cast<uint32_t>(propertyName));
    ASSERT(propertyName <= maxSafeInteger());
    return deleteProperty(globalObject, Identifier::from(globalObject->vm(), propertyName));
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, uint64_t propertyName) const
{
    if (propertyName <= MAX_ARRAY_INDEX) [[likely]]
        return get(globalObject, static_cast<uint32_t>(propertyName));
    ASSERT(propertyName <= maxSafeInteger());
    return get(globalObject, Identifier::from(globalObject->vm(), propertyName));
}

JSObject* createInvalidPrivateNameError(JSGlobalObject*);
JSObject* createRedefinedPrivateNameError(JSGlobalObject*);
JSObject* createReinstallPrivateMethodError(JSGlobalObject*);
JSObject* createPrivateMethodAccessError(JSGlobalObject*);

ALWAYS_INLINE bool JSObject::getPrivateFieldSlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    ASSERT(propertyName.isPrivateName());
    VM& vm = getVM(globalObject);
    Structure* structure = object->structure();

    unsigned attributes;
    PropertyOffset offset = structure->get(vm, propertyName, attributes);
    if (offset == invalidOffset)
        return false;

    JSValue value = object->getDirect(offset);
#if ASSERT_ENABLED
    ASSERT(value);
    if (value.isCell()) {
        JSCell* cell = value.asCell();
        JSType type = cell->type();
        UNUSED_PARAM(cell);
        ASSERT_UNUSED(type, type != GetterSetterType && type != CustomGetterSetterType);
        // FIXME: For now, private fields do not support getter/setter fields. Later on, we will need to fill in accessor metadata here,
        // as in JSObject::getOwnNonIndexPropertySlot()
        // https://bugs.webkit.org/show_bug.cgi?id=194435
    }
#endif

    slot.setValue(object, attributes, value, offset);
    return true;
}

inline bool JSObject::hasPrivateField(JSGlobalObject* globalObject, PropertyName propertyName)
{
    ASSERT(propertyName.isPrivateName());
    VM& vm = getVM(globalObject);
    unsigned attributes;
    return structure()->get(vm, propertyName, attributes) != invalidOffset;
}

inline bool JSObject::getPrivateField(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(!slot.isVMInquiry());
    if (!JSObject::getPrivateFieldSlot(this, globalObject, propertyName, slot)) {
        throwException(globalObject, scope, createInvalidPrivateNameError(globalObject));
        return false;
    }
    EXCEPTION_ASSERT(!scope.exception());
    RELEASE_AND_RETURN(scope, true);
}

inline void JSObject::setPrivateField(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& putSlot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::HasProperty);
    if (!JSObject::getPrivateFieldSlot(this, globalObject, propertyName, slot)) {
        throwException(globalObject, scope, createInvalidPrivateNameError(globalObject));
        return;
    }
    EXCEPTION_ASSERT(!scope.exception());

    scope.release();
    putDirect(vm, propertyName, value, putSlot);
}

inline void JSObject::definePrivateField(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& putSlot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (type() == WebAssemblyGCObjectType) {
        throwTypeError(globalObject, scope, "Cannot define private field on a WebAssembly GC object"_s);
        return;
    }

    PropertySlot slot(this, PropertySlot::InternalMethodType::HasProperty);
    if (JSObject::getPrivateFieldSlot(this, globalObject, propertyName, slot)) {
        throwException(globalObject, scope, createRedefinedPrivateNameError(globalObject));
        return;
    }
    EXCEPTION_ASSERT(!scope.exception());

    scope.release();
    putDirect(vm, propertyName, value, putSlot);
}

ALWAYS_INLINE void JSObject::getNonReifiedStaticPropertyNames(VM& vm, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
{
    if (staticPropertiesReified())
        return;

    Structure* structure = this->structure();
    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = classInfo(); info; info = info->parentClass) {
        const HashTable* table = info->staticPropHashTable;
        if (!table)
            continue;

        for (auto iter = table->begin(); iter != table->end(); ++iter) {
            if (mode == DontEnumPropertiesMode::Include || !(iter->attributes() & PropertyAttribute::DontEnum)) {
                auto identifier = Identifier::fromString(vm, iter.key());
                // If the structure is shadowing the static property use it's attributes to determine if
                // the property name is enumerable but add it here to preserve the right property order.
                unsigned structureAttributes;
                if (isValidOffset(structure->get(vm, identifier, structureAttributes)) && (mode == DontEnumPropertiesMode::Exclude && (structureAttributes & PropertyAttribute::DontEnum)))
                    continue;
                propertyNames.add(identifier);
            }
        }
    }
}

inline bool JSObject::hasPrivateBrand(JSGlobalObject*, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    Structure* structure = this->structure();
    return structure->isBrandedStructure() && uncheckedDowncast<BrandedStructure>(structure)->checkBrand(asSymbol(brand));
}

inline void JSObject::checkPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure();
    if (!structure->isBrandedStructure() || !uncheckedDowncast<BrandedStructure>(structure)->checkBrand(asSymbol(brand)))
        throwException(globalObject, scope, createPrivateMethodAccessError(globalObject));
}

inline void JSObject::setPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure();
    if (structure->isBrandedStructure() && uncheckedDowncast<BrandedStructure>(structure)->checkBrand(asSymbol(brand))) {
        throwException(globalObject, scope, createReinstallPrivateMethodError(globalObject));
        return;
    }
    EXCEPTION_ASSERT(!scope.exception());

    if (type() == WebAssemblyGCObjectType) {
        throwTypeError(globalObject, scope, "Cannot add private method to a WebAssembly GC object"_s);
        return;
    }

    scope.release();

    DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);

    Structure* newStructure = Structure::setBrandTransition(vm, structure, asSymbol(brand), &deferredWatchpointFire);
    ASSERT(newStructure->isBrandedStructure());
    ASSERT(newStructure->outOfLineCapacity() || !this->structure()->outOfLineCapacity());
    this->setStructure(vm, newStructure);
}

// Function forEachOwnIndexedProperty should only used in the fast path
// for copying own non-GetterSetter indexed properties.
template<JSObject::SortMode mode, typename Functor>
void JSObject::forEachOwnIndexedProperty(JSGlobalObject* globalObject, const Functor& functor)
{
    ASSERT(structure()->canPerformFastPropertyEnumerationCommon());
    ASSERT(canHaveExistingOwnIndexedProperties() && !canHaveExistingOwnIndexedGetterSetterProperties());
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        break;

    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES: {
        unsigned usedLength = butterfly()->publicLength();
        for (unsigned i = 0; i < usedLength; ++i) {
            JSValue value = getDirectIndex(globalObject, i);
            RETURN_IF_EXCEPTION(scope, void());
            if (value && functor(i, value) == IterationStatus::Done)
                return;
        }
        break;
    }

    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly()->arrayStorage();
        unsigned usedVectorLength = std::min(storage->length(), storage->vectorLength());
        for (unsigned i = 0; i < usedVectorLength; ++i) {
            auto value = storage->m_vector[i];
            if (!value)
                continue;
            if (functor(i, value.get()) == IterationStatus::Done)
                return;
        }

        if (SparseArrayValueMap* map = storage->m_sparseMap.get()) {
            MarkedArgumentBuffer values;
            if constexpr (mode == JSObject::SortMode::Default) {
                Vector<unsigned, 8> properties;
                for (auto& entry : *map) {
                    if (!(entry.attributes() & PropertyAttribute::DontEnum)) {
                        properties.append(entry.index());
                        values.appendWithCrashOnOverflow(entry.get());
                    }
                }

                for (size_t i = 0; i < properties.size(); ++i) {
                    if (functor(properties[i], values.at(i)) == IterationStatus::Done)
                        return;
                }
            } else {
                Vector<std::tuple<unsigned, unsigned>, 8> propertyAndValueIndexTuples;
                unsigned valueIndex = 0;
                for (auto& entry : *map) {
                    if (!(entry.attributes() & PropertyAttribute::DontEnum)) {
                        propertyAndValueIndexTuples.append({ entry.index(), valueIndex++ });
                        values.appendWithCrashOnOverflow(entry.get());
                    }
                }

                std::ranges::sort(propertyAndValueIndexTuples, [](auto a, auto b) {
                    return std::get<0>(a) < std::get<0>(b);
                });
                for (size_t i = 0; i < propertyAndValueIndexTuples.size(); ++i) {
                    auto [property, valueIndex] = propertyAndValueIndexTuples.at(i);
                    if (functor(property, values.at(valueIndex)) == IterationStatus::Done)
                        return;
                }
            }
        }
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline void JSObject::initializeIndex(ObjectInitializationScope& scope, unsigned i, JSValue v)
{
    initializeIndex(scope, i, v, indexingType());
}

ALWAYS_INLINE void JSObject::initializeIndex(ObjectInitializationScope& scope, unsigned i, JSValue v, IndexingType indexingType)
{
    VM& vm = scope.vm();
    auto* butterfly = this->butterfly();
    switch (indexingType) {
    case ALL_UNDECIDED_INDEXING_TYPES: {
        setIndexQuicklyToUndecided(vm, i, v);
        break;
    }
    case ALL_INT32_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        if (!v.isInt32()) {
            convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(vm, i, v);
            break;
        }
        [[fallthrough]];
    }
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        butterfly->contiguous().at(this, i).set(vm, this, v);
        break;
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        if (!v.isNumber()) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return;
        }
        double value = v.asNumber();
        if (value != value) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return;
        }
        butterfly->contiguousDouble().at(this, i) = value;
        break;
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly->arrayStorage();
        ASSERT(i < storage->length());
        ASSERT(i < storage->m_numValuesInVector);
        storage->m_vector[i].set(vm, this, v);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline void JSObject::initializeIndexWithoutBarrier(ObjectInitializationScope& scope, unsigned i, JSValue v)
{
    initializeIndexWithoutBarrier(scope, i, v, indexingType());
}

ALWAYS_INLINE void JSObject::initializeIndexWithoutBarrier(ObjectInitializationScope&, unsigned i, JSValue v, IndexingType indexingType)
{
    auto* butterfly = this->butterfly();
    switch (indexingType) {
    case ALL_UNDECIDED_INDEXING_TYPES: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case ALL_INT32_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        RELEASE_ASSERT(v.isInt32());
        [[fallthrough]];
    }
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
        break;
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        ASSERT(i < butterfly->publicLength());
        ASSERT(i < butterfly->vectorLength());
        RELEASE_ASSERT(v.isNumber());
        double value = v.asNumber();
        RELEASE_ASSERT(value == value);
        butterfly->contiguousDouble().at(this, i) = value;
        break;
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = butterfly->arrayStorage();
        ASSERT(i < storage->length());
        ASSERT(i < storage->m_numValuesInVector);
        storage->m_vector[i].setWithoutWriteBarrier(v);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline bool JSObject::canHaveExistingOwnIndexedGetterSetterProperties()
{
    if (!hasIndexedProperties(indexingType()))
        return false;

    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
        return false;
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        SparseArrayValueMap* map = butterfly()->arrayStorage()->m_sparseMap.get();
        if (!map)
            return false;
        return map->hasAnyKindOfGetterSetterProperties();
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline unsigned JSObject::canHaveExistingOwnIndexedProperties() const
{
    if (!hasIndexedProperties(indexingType()))
        return false;

    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        return false;
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
        return butterfly()->publicLength();
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        auto* storage = butterfly()->arrayStorage();
        unsigned usedVectorLength = std::min(storage->length(), storage->vectorLength());
        if (usedVectorLength)
            return true;
        SparseArrayValueMap* map = storage->m_sparseMap.get();
        if (!map)
            return false;
        return map->size();
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

ALWAYS_INLINE JSFinalObject* JSFinalObject::createDefaultEmptyObject(JSGlobalObject* globalObject)
{
    VM& vm = getVM(globalObject);
    JSFinalObject* finalObject = new (NotNull, allocateCell<JSFinalObject>(vm, allocationSize(defaultInlineCapacity))) JSFinalObject(CreatingWellDefinedBuiltinCell, globalObject->objectStructureIDForObjectConstructor());
    finalObject->finishCreation(vm);
    ASSERT(globalObject->objectStructureForObjectConstructor()->id() == globalObject->objectStructureIDForObjectConstructor());
    ASSERT(globalObject->objectStructureForObjectConstructor()->inlineCapacity() == defaultInlineCapacity);
    return finalObject;
}

inline ContiguousJSValues JSObject::tryMakeWritableInt32(VM& vm)
{
    if (hasInt32(indexingType()) && !isCopyOnWrite(indexingMode())) [[likely]]
        return butterfly()->contiguousInt32();

    return tryMakeWritableInt32Slow(vm);
}

inline ContiguousDoubles JSObject::tryMakeWritableDouble(VM& vm)
{
    if (hasDouble(indexingType()) && !isCopyOnWrite(indexingMode())) [[likely]]
        return butterfly()->contiguousDouble();

    return tryMakeWritableDoubleSlow(vm);
}

inline ContiguousJSValues JSObject::tryMakeWritableContiguous(VM& vm)
{
    if (hasContiguous(indexingType()) && !isCopyOnWrite(indexingMode())) [[likely]]
        return butterfly()->contiguous();

    return tryMakeWritableContiguousSlow(vm);
}

inline bool JSObject::ensureLength(VM& vm, unsigned length)
{
    RELEASE_ASSERT(length <= MAX_STORAGE_VECTOR_LENGTH);
    ASSERT(hasContiguous(indexingType()) || hasInt32(indexingType()) || hasDouble(indexingType()) || hasUndecided(indexingType()));

    if (butterfly()->vectorLength() < length || isCopyOnWrite(indexingMode())) {
        if (!ensureLengthSlow(vm, length))
            return false;
    }

    if (butterfly()->publicLength() < length)
        butterfly()->setPublicLength(length);
    return true;
}

inline bool JSObject::canGetIndexQuickly(unsigned i) const
{
    const Butterfly* butterfly = this->butterfly();
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        return canGetIndexQuicklyForTypedArray(i);
    case ALL_UNDECIDED_INDEXING_TYPES:
        return false;
    case ALL_INT32_INDEXING_TYPES:
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        return i < butterfly->vectorLength() && butterfly->contiguous().at(this, i);
    case ALL_DOUBLE_INDEXING_TYPES: {
        if (i >= butterfly->vectorLength())
            return false;
        double value = butterfly->contiguousDouble().at(this, i);
        if (value != value)
            return false;
        return true;
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return i < butterfly->arrayStorage()->vectorLength() && butterfly->arrayStorage()->m_vector[i];
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

inline bool JSObject::canGetIndexQuickly(uint64_t i) const
{
    ASSERT(i <= maxSafeInteger());
    if (i <= MAX_ARRAY_INDEX) [[likely]]
        return canGetIndexQuickly(static_cast<uint32_t>(i));
    return false;
}

inline JSValue JSObject::getIndexQuickly(unsigned i) const
{
    const Butterfly* butterfly = this->butterfly();
    switch (indexingType()) {
    case ALL_INT32_INDEXING_TYPES:
        return jsNumber(butterfly->contiguous().at(this, i).get().asInt32());
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        return butterfly->contiguous().at(this, i).get();
    case ALL_DOUBLE_INDEXING_TYPES:
        return JSValue(JSValue::EncodeAsDouble, butterfly->contiguousDouble().at(this, i));
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        return butterfly->arrayStorage()->m_vector[i].get();
    case ALL_BLANK_INDEXING_TYPES:
        return getIndexQuicklyForTypedArray(i);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return JSValue();
    }
}

inline JSValue JSObject::tryGetIndexQuickly(unsigned i, ArrayProfile* arrayProfile) const
{
    const Butterfly* butterfly = this->butterfly();
    switch (indexingType()) {
    case ALL_BLANK_INDEXING_TYPES:
        if (canGetIndexQuicklyForTypedArray(i))
            return getIndexQuicklyForTypedArray(i, arrayProfile);
        break;
    case ALL_UNDECIDED_INDEXING_TYPES:
        break;
    case ALL_INT32_INDEXING_TYPES:
        if (i < butterfly->publicLength()) {
            JSValue result = butterfly->contiguous().at(this, i).get();
            ASSERT(result.isInt32() || !result);
            return result;
        }
        break;
    case ALL_CONTIGUOUS_INDEXING_TYPES:
        if (i < butterfly->publicLength())
            return butterfly->contiguous().at(this, i).get();
        break;
    case ALL_DOUBLE_INDEXING_TYPES: {
        if (i >= butterfly->publicLength())
            break;
        double result = butterfly->contiguousDouble().at(this, i);
        if (result != result)
            break;
        return JSValue(JSValue::EncodeAsDouble, result);
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        if (i < butterfly->arrayStorage()->vectorLength())
            return butterfly->arrayStorage()->m_vector[i].get();
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return JSValue();
}

inline JSValue JSObject::tryGetIndexQuickly(uint64_t i) const
{
    ASSERT(i <= maxSafeInteger());
    if (i <= MAX_ARRAY_INDEX) [[likely]]
        return tryGetIndexQuickly(static_cast<uint32_t>(i));
    return JSValue();
}

inline JSValue JSObject::getDirectIndex(JSGlobalObject* globalObject, unsigned i)
{
    if (JSValue result = tryGetIndexQuickly(i))
        return result;
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    if (methodTable()->getOwnPropertySlotByIndex(this, globalObject, i, slot))
        return slot.getValue(globalObject, i);
    return JSValue();
}

inline JSValue JSObject::getIndex(JSGlobalObject* globalObject, uint64_t i) const
{
    if (JSValue result = tryGetIndexQuickly(i))
        return result;
    return get(globalObject, i);
}

inline bool JSObject::trySetIndexQuickly(VM& vm, unsigned i, JSValue v, ArrayProfile* arrayProfile)
{
    Butterfly* butterfly = this->butterfly();
    switch (indexingMode()) {
    case ALL_BLANK_INDEXING_TYPES:
        return trySetIndexQuicklyForTypedArray(i, v, arrayProfile);
    case ALL_UNDECIDED_INDEXING_TYPES:
        return false;
    case ALL_WRITABLE_INT32_INDEXING_TYPES: {
        if (i >= butterfly->vectorLength())
            return false;
        if (!v.isInt32()) {
            convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(vm, i, v);
            return true;
        }
        [[fallthrough]];
    }
    case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES: {
        if (i >= butterfly->vectorLength())
            return false;
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
        if (i >= butterfly->publicLength()) {
            butterfly->setPublicLength(i + 1);
            if (arrayProfile)
                arrayProfile->setMayStoreHole();
        }
        vm.writeBarrier(this, v);
        return true;
    }
    case ALL_WRITABLE_DOUBLE_INDEXING_TYPES: {
        if (i >= butterfly->vectorLength())
            return false;
        if (!v.isNumber()) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return true;
        }
        double value = v.asNumber();
        if (value != value) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return true;
        }
        butterfly->contiguousDouble().at(this, i) = value;
        if (i >= butterfly->publicLength()) {
            butterfly->setPublicLength(i + 1);
            if (arrayProfile)
                arrayProfile->setMayStoreHole();
        }
        return true;
    }
    case NonArrayWithArrayStorage:
    case ArrayWithArrayStorage: {
        ArrayStorage* storage = butterfly->arrayStorage();
        if (i >= storage->vectorLength())
            return false;
        if (arrayProfile && !storage->m_vector[i])
            arrayProfile->setMayStoreHole();
        setIndexQuicklyForArrayStorageIndexingType(vm, i, v);
        return true;
    }
    case NonArrayWithSlowPutArrayStorage:
    case ArrayWithSlowPutArrayStorage:
        if (i >= butterfly->arrayStorage()->vectorLength() || !butterfly->arrayStorage()->m_vector[i])
            return false;
        setIndexQuicklyForArrayStorageIndexingType(vm, i, v);
        return true;
    default:
        RELEASE_ASSERT(isCopyOnWrite(indexingMode()));
        return false;
    }
}

inline void JSObject::setIndexQuickly(VM& vm, unsigned i, JSValue v)
{
    Butterfly* butterfly = this->butterfly();
    ASSERT(!isCopyOnWrite(indexingMode()));
    switch (indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        ASSERT(i < butterfly->vectorLength());
        if (!v.isInt32()) {
            convertInt32ToDoubleOrContiguousWhilePerformingSetIndex(vm, i, v);
            return;
        }
        [[fallthrough]];
    }
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        ASSERT(i < butterfly->vectorLength());
        butterfly->contiguous().at(this, i).setWithoutWriteBarrier(v);
        if (i >= butterfly->publicLength())
            butterfly->setPublicLength(i + 1);
        vm.writeBarrier(this, v);
        break;
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        ASSERT(i < butterfly->vectorLength());
        if (!v.isNumber()) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return;
        }
        double value = v.asNumber();
        if (value != value) {
            convertDoubleToContiguousWhilePerformingSetIndex(vm, i, v);
            return;
        }
        butterfly->contiguousDouble().at(this, i) = value;
        if (i >= butterfly->publicLength())
            butterfly->setPublicLength(i + 1);
        break;
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES:
        setIndexQuicklyForArrayStorageIndexingType(vm, i, v);
        break;
    case ALL_BLANK_INDEXING_TYPES:
        setIndexQuicklyForTypedArray(i, v);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

ALWAYS_INLINE bool JSObject::putByIndexInline(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = getVM(globalObject);
    if (trySetIndexQuickly(vm, propertyName, value))
        return true;
    return methodTable()->putByIndex(this, globalObject, propertyName, value, shouldThrow);
}

ALWAYS_INLINE bool JSObject::putByIndexInline(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, bool shouldThrow)
{
    VM& vm = getVM(globalObject);
    if (propertyName <= MAX_ARRAY_INDEX) [[likely]]
        return putByIndexInline(globalObject, static_cast<uint32_t>(propertyName), value, shouldThrow);

    ASSERT(propertyName <= maxSafeInteger());
    PutPropertySlot slot(this, shouldThrow);
    return methodTable()->put(this, globalObject, Identifier::from(vm, propertyName), value, slot);
}

inline bool JSObject::putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode)
{
    ASSERT(!value.isCustomGetterSetterSlow());
    auto canSetIndexQuicklyForPutDirect = [&] () -> bool {
        switch (indexingMode()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return false;
        case ALL_WRITABLE_INT32_INDEXING_TYPES:
        case ALL_WRITABLE_DOUBLE_INDEXING_TYPES:
        case ALL_WRITABLE_CONTIGUOUS_INDEXING_TYPES:
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return propertyName < butterfly()->vectorLength();
        default:
            if (isCopyOnWrite(indexingMode()))
                return false;
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    };

    if (!attributes && canSetIndexQuicklyForPutDirect()) {
        setIndexQuickly(getVM(globalObject), propertyName, value);
        return true;
    }
    return putDirectIndexSlowOrBeyondVectorLength(globalObject, propertyName, value, attributes, mode);
}

inline bool JSObject::putDirectIndex(JSGlobalObject* globalObject, unsigned propertyName, JSValue value)
{
    return putDirectIndex(globalObject, propertyName, value, 0, PutDirectIndexLikePutDirect);
}

ALWAYS_INLINE bool JSObject::putDirectIndex(JSGlobalObject* globalObject, uint64_t propertyName, JSValue value, unsigned attributes, PutDirectIndexMode mode)
{
    if (propertyName <= MAX_ARRAY_INDEX) [[likely]]
        return putDirectIndex(globalObject, static_cast<uint32_t>(propertyName), value, attributes, mode);
    return putDirect(getVM(globalObject), Identifier::from(getVM(globalObject), propertyName), value, attributes);
}

inline void JSObject::ensureWritable(VM& vm)
{
    if (isCopyOnWrite(indexingMode()))
        convertFromCopyOnWrite(vm);
}

} // namespace JSC


WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
