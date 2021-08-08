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

#include "AuxiliaryBarrierInlines.h"
#include "BrandedStructure.h"
#include "ButterflyInlines.h"
#include "Error.h"
#include "JSArrayInlines.h"
#include "JSFunction.h"
#include "JSObject.h"
#include "JSTypedArrays.h"
#include "Lookup.h"
#include "StructureInlines.h"
#include "TypedArrayType.h"

namespace JSC {

template<typename CellType, SubspaceAccess>
CompleteSubspace* JSFinalObject::subspaceFor(VM& vm)
{
    static_assert(!CellType::needsDestruction);
    return &vm.cellSpace;
}

// https://tc39.es/ecma262/#sec-createlistfromarraylike
template <typename Functor> // A functor should have a type like: (JSValue) -> bool
void forEachInArrayLike(JSGlobalObject* globalObject, JSObject* arrayLikeObject, Functor functor)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint64_t length = static_cast<uint64_t>(toLength(globalObject, arrayLikeObject));
    RETURN_IF_EXCEPTION(scope, void());
    for (uint64_t index = 0; index < length; index++) {
        JSValue value = arrayLikeObject->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, void());
        if (!functor(value))
            return;
    }
}

ALWAYS_INLINE bool JSObject::canPerformFastPutInlineExcludingProto(VM& vm)
{
    // Check if there are any setters or getters in the prototype chain
    JSValue prototype;
    JSObject* obj = this;
    while (true) {
        Structure* structure = obj->structure(vm);
        if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype())
            return false;
        if (obj != this && structure->typeInfo().overridesPut())
            return false;

        prototype = obj->getPrototypeDirect(vm);
        if (prototype.isNull())
            return true;

        obj = asObject(prototype);
    }

    ASSERT_NOT_REACHED();
}

ALWAYS_INLINE bool JSObject::canPerformFastPutInline(VM& vm, PropertyName propertyName)
{
    if (UNLIKELY(propertyName == vm.propertyNames->underscoreProto))
        return false;
    return canPerformFastPutInlineExcludingProto(vm);
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, CallbackWhenNoException callback) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    return getPropertySlot(globalObject, propertyName, slot, callback);
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type JSObject::getPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot, CallbackWhenNoException callback) const
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
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    while (true) {
        Structure* structure = structureIDTable.get(object->structureID());
        bool hasSlot = structure->classInfo()->methodTable.getOwnPropertySlotByIndex(object, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, false);
        if (hasSlot)
            return true;
        if (UNLIKELY(slot.isVMInquiry() && slot.isTaintedByOpaqueObject()))
            return false;
        if (object->type() == ProxyObjectType && slot.internalMethodType() == PropertySlot::InternalMethodType::HasProperty)
            return false;
        if (isTypedArrayType(object->type()) && propertyName >= jsCast<JSArrayBufferView*>(object)->length())
            return false;
        JSValue prototype;
        if (LIKELY(!structure->typeInfo().overridesGetPrototype() || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry))
            prototype = object->getPrototypeDirect(vm);
        else {
            prototype = object->getPrototype(vm, globalObject);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

ALWAYS_INLINE bool JSObject::getPropertySlot(JSGlobalObject* globalObject, uint64_t propertyName, PropertySlot& slot)
{
    if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
        return getPropertySlot(globalObject, static_cast<uint32_t>(propertyName), slot);
    return getPropertySlot(globalObject, Identifier::from(globalObject->vm(), propertyName), slot);
}

ALWAYS_INLINE bool JSObject::getNonIndexPropertySlot(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    // This method only supports non-index PropertyNames.
    ASSERT(!parseIndex(propertyName));

    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    while (true) {
        Structure* structure = structureIDTable.get(object->structureID());
        if (LIKELY(!TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()))) {
            if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
                return true;
        } else {
            bool hasSlot = structure->classInfo()->methodTable.getOwnPropertySlot(object, globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, false);
            if (hasSlot)
                return true;
            if (UNLIKELY(slot.isVMInquiry() && slot.isTaintedByOpaqueObject()))
                return false;
            if (object->type() == ProxyObjectType && slot.internalMethodType() == PropertySlot::InternalMethodType::HasProperty)
                return false;
            if (isTypedArrayType(object->type()) && isCanonicalNumericIndexString(propertyName))
                return false;
        }
        JSValue prototype;
        if (LIKELY(!structure->typeInfo().overridesGetPrototype() || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry))
            prototype = object->getPrototypeDirect(vm);
        else {
            prototype = object->getPrototype(vm, globalObject);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

inline bool JSObject::getOwnPropertySlotInline(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    if (UNLIKELY(TypeInfo::overridesGetOwnPropertySlot(inlineTypeFlags())))
        return methodTable(vm)->getOwnPropertySlot(this, globalObject, propertyName, slot);
    return JSObject::getOwnPropertySlot(this, globalObject, propertyName, slot);
}

inline JSValue JSObject::getIfPropertyExists(JSGlobalObject* globalObject, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertySlot slot(this, PropertySlot::InternalMethodType::HasProperty);
    bool hasProperty = getPropertySlot(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, { });
    if (!hasProperty)
        return { };

    scope.release();
    if (UNLIKELY(slot.isTaintedByOpaqueObject()))
        return get(globalObject, propertyName);

    return slot.getValue(globalObject, propertyName);
}

inline bool JSObject::mayInterceptIndexedAccesses(VM& vm)
{
    return structure(vm)->mayInterceptIndexedAccesses();
}

inline void JSObject::putDirectWithoutTransition(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!value.isGetterSetter() && !(attributes & PropertyAttribute::Accessor));
    ASSERT(!value.isCustomGetterSetter());
    StructureID structureID = this->structureID();
    Structure* structure = vm.heap.structureIDTable().get(structureID);
    PropertyOffset offset = prepareToPutDirectWithoutTransition(vm, propertyName, attributes, structureID, structure);
    putDirect(vm, offset, value);
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
        if (UNLIKELY(isThisValueAltered(slot, thisObject)))
            return ordinarySetSlow(globalObject, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());
        return thisObject->methodTable(vm)->putByIndex(thisObject, globalObject, index.value(), value, slot.isStrictMode());
    }

    if (!thisObject->canPerformFastPutInline(vm, propertyName))
        return thisObject->putInlineSlow(globalObject, propertyName, value, slot);
    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return definePropertyOnReceiver(globalObject, propertyName, value, slot);
    if (UNLIKELY(thisObject->hasNonReifiedStaticProperties(vm)))
        return thisObject->putInlineFastReplacingStaticPropertyIfNeeded(globalObject, propertyName, value, slot);
    return thisObject->putInlineFast(globalObject, propertyName, value, slot);
}

ALWAYS_INLINE bool JSObject::putInlineFast(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: For a failure due to non-extensible structure, the error message is misleading
    if (!putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot))
        return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
    return true;
}

// https://tc39.es/ecma262/#sec-createdataproperty
ALWAYS_INLINE bool JSObject::createDataProperty(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, bool shouldThrow)
{
    PropertyDescriptor descriptor(value, static_cast<unsigned>(PropertyAttribute::None));
    return methodTable(getVM(globalObject))->defineOwnProperty(this, globalObject, propertyName, descriptor, shouldThrow);
}

// HasOwnProperty(O, P) from section 7.3.11 in the spec.
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-hasownproperty
ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot) const
{
    VM& vm = getVM(globalObject);
    ASSERT(slot.internalMethodType() == PropertySlot::InternalMethodType::GetOwnProperty);
    if (LIKELY(const_cast<JSObject*>(this)->methodTable(vm)->getOwnPropertySlot == JSObject::getOwnPropertySlot))
        return JSObject::getOwnPropertySlot(const_cast<JSObject*>(this), globalObject, propertyName, slot);
    return const_cast<JSObject*>(this)->methodTable(vm)->getOwnPropertySlot(const_cast<JSObject*>(this), globalObject, propertyName, slot);
}

ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, PropertyName propertyName) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    return hasOwnProperty(globalObject, propertyName, slot);
}

ALWAYS_INLINE bool JSObject::hasOwnProperty(JSGlobalObject* globalObject, unsigned propertyName) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    return const_cast<JSObject*>(this)->methodTable(getVM(globalObject))->getOwnPropertySlotByIndex(const_cast<JSObject*>(this), globalObject, propertyName, slot);
}

template<JSObject::PutMode mode>
ALWAYS_INLINE bool JSObject::putDirectInternal(VM& vm, PropertyName propertyName, JSValue value, unsigned attributes, PutPropertySlot& slot)
{
    ASSERT(value);
    ASSERT(value.isGetterSetter() == !!(attributes & PropertyAttribute::Accessor));
    ASSERT(value.isCustomGetterSetter() == !!(attributes & PropertyAttribute::CustomAccessorOrValue));
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    ASSERT(!parseIndex(propertyName));

    StructureID structureID = this->structureID();
    Structure* structure = vm.heap.structureIDTable().get(structureID);
    if (structure->isDictionary()) {
        ASSERT(!isCopyOnWrite(indexingMode()));
        
        unsigned currentAttributes;
        PropertyOffset offset = structure->get(vm, propertyName, currentAttributes);
        if (offset != invalidOffset) {
            if ((mode == PutModePut) && currentAttributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessor)
                return false;

            putDirect(vm, offset, value);
            structure->didReplaceProperty(offset);

            // FIXME: Check attributes against PropertyAttribute::CustomAccessorOrValue. Changing GetterSetter should work w/o transition.
            // https://bugs.webkit.org/show_bug.cgi?id=214342
            if (mode == PutModeDefineOwnProperty && (attributes != currentAttributes || (attributes & PropertyAttribute::AccessorOrCustomAccessorOrValue)))
                setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, attributes));
            else {
                ASSERT(!(currentAttributes & PropertyAttribute::AccessorOrCustomAccessorOrValue));
                slot.setExistingProperty(this, offset);
            }

            return true;
        }

        if ((mode == PutModePut) && !isStructureExtensible(vm))
            return false;

        offset = prepareToPutDirectWithoutTransition(vm, propertyName, attributes, structureID, structure);
        validateOffset(offset);
        putDirect(vm, offset, value);
        slot.setNewProperty(this, offset);
        if (attributes & PropertyAttribute::ReadOnly)
            this->structure(vm)->setContainsReadOnlyProperties();
        return true;
    }

    PropertyOffset offset;
    size_t currentCapacity = this->structure(vm)->outOfLineCapacity();
    Structure* newStructure = Structure::addPropertyTransitionToExistingStructure(
        structure, propertyName, attributes, offset);
    if (newStructure) {
        Butterfly* newButterfly = butterfly();
        if (currentCapacity != newStructure->outOfLineCapacity()) {
            ASSERT(newStructure != this->structure(vm));
            newButterfly = allocateMoreOutOfLineStorage(vm, currentCapacity, newStructure->outOfLineCapacity());
            nukeStructureAndSetButterfly(vm, structureID, newButterfly);
        }

        validateOffset(offset);
        ASSERT(newStructure->isValidOffset(offset));

        // This assertion verifies that the concurrent GC won't read garbage if the concurrentGC
        // is running at the same time we put without transitioning.
        ASSERT(!getDirect(offset) || !JSValue::encode(getDirect(offset)));
        putDirect(vm, offset, value);
        setStructure(vm, newStructure);
        slot.setNewProperty(this, offset);
        return true;
    }

    unsigned currentAttributes;
    offset = structure->get(vm, propertyName, currentAttributes);
    if (offset != invalidOffset) {
        if ((mode == PutModePut) && currentAttributes & PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessor)
            return false;

        structure->didReplaceProperty(offset);
        putDirect(vm, offset, value);

        // FIXME: Check attributes against PropertyAttribute::CustomAccessorOrValue. Changing GetterSetter should work w/o transition.
        // https://bugs.webkit.org/show_bug.cgi?id=214342
        if (mode == PutModeDefineOwnProperty && (attributes != currentAttributes || (attributes & PropertyAttribute::AccessorOrCustomAccessorOrValue))) {
            // We want the structure transition watchpoint to fire after this object has switched structure.
            // This allows adaptive watchpoints to observe if the new structure is the one we want.
            DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);
            setStructure(vm, Structure::attributeChangeTransition(vm, structure, propertyName, attributes, &deferredWatchpointFire));
        } else {
            ASSERT(!(currentAttributes & PropertyAttribute::AccessorOrCustomAccessorOrValue));
            slot.setExistingProperty(this, offset);
        }

        return true;
    }

    if ((mode == PutModePut) && !isStructureExtensible(vm))
        return false;
    
    // We want the structure transition watchpoint to fire after this object has switched structure.
    // This allows adaptive watchpoints to observe if the new structure is the one we want.
    DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);
    newStructure = Structure::addNewPropertyTransition(vm, structure, propertyName, attributes, offset, slot.context(), &deferredWatchpointFire);
    
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
    putDirect(vm, offset, value);
    setStructure(vm, newStructure);
    slot.setNewProperty(this, offset);
    if (attributes & PropertyAttribute::ReadOnly)
        newStructure->setContainsReadOnlyProperties();
    return true;
}

inline bool JSObject::mayBePrototype() const
{
    return perCellBit();
}

inline void JSObject::didBecomePrototype()
{
    setPerCellBit(true);
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

inline bool JSObject::canSetIndexQuicklyForTypedArray(unsigned i, JSValue value) const
{
    switch (type()) {
#define CASE_TYPED_ARRAY_TYPE(name) \
    case name ## ArrayType :\
        return jsCast<const JS ## name ## Array *>(this)->canSetIndexQuickly(i, value);
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE
    default:
        return false;
    }
}

inline JSValue JSObject::getIndexQuicklyForTypedArray(unsigned i) const
{
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
    
inline void JSObject::validatePutOwnDataProperty(VM& vm, PropertyName propertyName, JSValue value)
{
#if ASSERT_ENABLED
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));
    unsigned attributes;
    PropertyOffset offset = structure(vm)->get(vm, propertyName, attributes);
    if (isValidOffset(offset))
        ASSERT(!(attributes & (PropertyAttribute::Accessor | PropertyAttribute::CustomAccessor | PropertyAttribute::ReadOnly)));
    else if (TypeInfo::hasStaticPropertyTable(inlineTypeFlags())) {
        if (auto entry = findPropertyHashEntry(vm, propertyName))
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
    return putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot);
}

inline bool JSObject::putOwnDataPropertyMayBeIndex(JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = getVM(globalObject);
    validatePutOwnDataProperty(vm, propertyName, value);
    if (std::optional<uint32_t> index = parseIndex(propertyName))
        return putDirectIndex(globalObject, index.value(), value, 0, PutDirectIndexLikePutDirect);

    return putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot);
}

ALWAYS_INLINE CallData getCallData(VM& vm, JSCell* cell)
{
    if (cell->type() == JSFunctionType)
        return JSFunction::getCallData(cell);
    CallData result = cell->methodTable(vm)->getCallData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

inline CallData getCallData(VM& vm, JSValue value)
{
    if (!value.isCell()) 
        return { };
    return getCallData(vm, value.asCell());
}

inline CallData getConstructData(VM& vm, JSValue value)
{
    if (!value.isCell())
        return { };
    JSCell* cell = value.asCell();
    if (cell->type() == JSFunctionType)
        return JSFunction::getConstructData(cell);
    CallData result = cell->methodTable(vm)->getConstructData(cell);
    ASSERT(result.type == CallData::Type::None || cell->isValidCallee());
    return result;
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, PropertyName propertyName)
{
    DeletePropertySlot slot;
    return this->methodTable(globalObject->vm())->deleteProperty(this, globalObject, propertyName, slot);
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, uint32_t propertyName)
{
    return this->methodTable(globalObject->vm())->deletePropertyByIndex(this, globalObject, propertyName);
}

inline bool JSObject::deleteProperty(JSGlobalObject* globalObject, uint64_t propertyName)
{
    if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
        return deleteProperty(globalObject, static_cast<uint32_t>(propertyName));
    ASSERT(propertyName <= maxSafeInteger());
    return deleteProperty(globalObject, Identifier::from(globalObject->vm(), propertyName));
}

inline JSValue JSObject::get(JSGlobalObject* globalObject, uint64_t propertyName) const
{
    if (LIKELY(propertyName <= MAX_ARRAY_INDEX))
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
    Structure* structure = object->structure(vm);

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
    return structure(vm)->get(vm, propertyName, attributes) != invalidOffset;
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

ALWAYS_INLINE void JSObject::getNonReifiedStaticPropertyNames(VM& vm, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    if (staticPropertiesReified(vm))
        return;

    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = classInfo(vm); info; info = info->parentClass) {
        const HashTable* table = info->staticPropHashTable;
        if (!table)
            continue;

        for (auto iter = table->begin(); iter != table->end(); ++iter) {
            if (mode == DontEnumPropertiesMode::Include || !(iter->attributes() & PropertyAttribute::DontEnum))
                propertyNames.add(Identifier::fromString(vm, iter.key()));
        }
    }
}

inline bool JSObject::hasPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    Structure* structure = this->structure(vm);
    return structure->isBrandedStructure() && jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand));
}

inline void JSObject::checkPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure(vm);
    if (!structure->isBrandedStructure() || !jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand)))
        throwException(globalObject, scope, createPrivateMethodAccessError(globalObject));
}

inline void JSObject::setPrivateBrand(JSGlobalObject* globalObject, JSValue brand)
{
    ASSERT(brand.isSymbol() && asSymbol(brand)->uid().isPrivate());
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = this->structure(vm);
    if (structure->isBrandedStructure() && jsCast<BrandedStructure*>(structure)->checkBrand(asSymbol(brand))) {
        throwException(globalObject, scope, createReinstallPrivateMethodError(globalObject));
        RELEASE_AND_RETURN(scope, void());
    }
    EXCEPTION_ASSERT(!scope.exception());

    scope.release();

    DeferredStructureTransitionWatchpointFire deferredWatchpointFire(vm, structure);

    Structure* newStructure = Structure::setBrandTransition(vm, structure, asSymbol(brand), &deferredWatchpointFire);
    ASSERT(newStructure->isBrandedStructure());
    ASSERT(newStructure->outOfLineCapacity() || !this->structure(vm)->outOfLineCapacity());
    this->setStructure(vm, newStructure);
}

} // namespace JSC
