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
#include <JavaScriptCore/JSFunction.h>
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
        m_butterfly.set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    m_butterfly.set(vm, this, butterfly);
}

inline void JSObject::nukeStructureAndSetButterfly(VM& vm, StructureID oldStructureID, Butterfly* butterfly)
{
    if (isX86() || vm.heap.mutatorShouldBeFenced()) {
        setStructureIDDirectly(oldStructureID.nuke());
        WTF::storeStoreFence();
        m_butterfly.set(vm, this, butterfly);
        WTF::storeStoreFence();
        return;
    }

    m_butterfly.set(vm, this, butterfly);
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
    return jsCast<T>(value);
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
        if (isTypedArrayType(object->type()) && propertyName >= jsCast<JSArrayBufferView*>(object)->length())
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
        if (hasNonReifiedStaticProperties()) {
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

    JSObject* thisObject = jsCast<JSObject*>(cell);
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

template<JSObject::PutMode mode>
ALWAYS_INLINE ASCIILiteral JSObject::putDirectInternal(VM& vm, PropertyName propertyName, JSValue value, unsigned newAttributes, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(value.isGetterSetter() == !!(newAttributes & PropertyAttribute::Accessor));
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
    putDirectOffset(vm, offset, value);
    setStructure(vm, newStructure);
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

inline void JSObject::didBecomePrototype(VM& vm)
{
    Structure* oldStructure = structure();
    if (!oldStructure->mayBePrototype()) [[unlikely]] {
        DeferredStructureTransitionWatchpointFire deferred(vm, oldStructure);
        setStructure(vm, Structure::becomePrototypeTransition(vm, oldStructure, &deferred));
    }

    if (type() == GlobalProxyType) [[unlikely]]
        jsCast<JSGlobalProxy*>(this)->target()->didBecomePrototype(vm);
}

inline bool JSObject::canGetIndexQuicklyForTypedArray(unsigned i) const
{
    switch (type()) {
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType :\
        return jsCast<const JS ## name ## Array *>(this)->canGetIndexQuickly(i);
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
        auto* typedArray = jsCast<const JS ## name ## Array *>(this);\
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
        auto* typedArray = jsCast<JS ## name ## Array *>(this);\
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
        auto* typedArray = jsCast<JS ## name ## Array *>(this);\
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
        RELEASE_AND_RETURN(scope, false);
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
        RELEASE_AND_RETURN(scope, void());
    }
    EXCEPTION_ASSERT(!scope.exception());

    scope.release();
    putDirect(vm, propertyName, value, putSlot);
}

inline void JSObject::definePrivateField(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& putSlot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(this, PropertySlot::InternalMethodType::HasProperty);
    if (JSObject::getPrivateFieldSlot(this, globalObject, propertyName, slot)) {
        throwException(globalObject, scope, createRedefinedPrivateNameError(globalObject));
        RELEASE_AND_RETURN(scope, void());
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
    return structure->isBrandedStructure() && jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand));
}

inline void JSObject::checkPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure();
    if (!structure->isBrandedStructure() || !jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand)))
        throwException(globalObject, scope, createPrivateMethodAccessError(globalObject));
}

inline void JSObject::setPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure();
    if (structure->isBrandedStructure() && jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand))) {
        throwException(globalObject, scope, createReinstallPrivateMethodError(globalObject));
        RELEASE_AND_RETURN(scope, void());
    }
    EXCEPTION_ASSERT(!scope.exception());

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
        unsigned usedLength = m_butterfly->publicLength();
        for (unsigned i = 0; i < usedLength; ++i) {
            JSValue value = getDirectIndex(globalObject, i);
            RETURN_IF_EXCEPTION(scope, void());
            if (value && functor(i, value) == IterationStatus::Done)
                return;
        }
        break;
    }

    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
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
                for (auto& [key, value] : *map) {
                    if (!(value.attributes() & PropertyAttribute::DontEnum)) {
                        properties.append(key);
                        values.appendWithCrashOnOverflow(value.get());
                    }
                }

                for (size_t i = 0; i < properties.size(); ++i) {
                    if (functor(properties[i], values.at(i)) == IterationStatus::Done)
                        return;
                }
            } else {
                Vector<std::tuple<unsigned, unsigned>, 8> propertyAndValueIndexTuples;
                unsigned valueIndex = 0;
                for (auto& [key, value] : *map) {
                    if (!(value.attributes() & PropertyAttribute::DontEnum)) {
                        propertyAndValueIndexTuples.append({ key, valueIndex++ });
                        values.appendWithCrashOnOverflow(value.get());
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
    Butterfly* butterfly = m_butterfly.get();
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
    Butterfly* butterfly = m_butterfly.get();
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
        SparseArrayValueMap* map = m_butterfly->arrayStorage()->m_sparseMap.get();
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
        return m_butterfly->publicLength();
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        ArrayStorage* storage = m_butterfly->arrayStorage();
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

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
