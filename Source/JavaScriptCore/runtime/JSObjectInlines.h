/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2006, 2008, 2009, 2012-2016 Apple Inc. All rights reserved.
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

#ifndef JSObjectInlines_h
#define JSObjectInlines_h

#include "AuxiliaryBarrierInlines.h"
#include "Error.h"
#include "JSObject.h"
#include "Lookup.h"

namespace JSC {

// Section 7.3.17 of the spec.
template <typename AddFunction> // Add function should have a type like: (JSValue, RuntimeType) -> bool
void createListFromArrayLike(ExecState* exec, JSValue arrayLikeValue, RuntimeTypeMask legalTypesFilter, const String& errorMessage, AddFunction addFunction)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    Vector<JSValue> result;
    JSValue lengthProperty = arrayLikeValue.get(exec, vm.propertyNames->length);
    if (vm.exception())
        return;
    double lengthAsDouble = lengthProperty.toLength(exec);
    if (vm.exception())
        return;
    RELEASE_ASSERT(lengthAsDouble >= 0.0 && lengthAsDouble == std::trunc(lengthAsDouble));
    uint64_t length = static_cast<uint64_t>(lengthAsDouble);
    for (uint64_t index = 0; index < length; index++) {
        JSValue next = arrayLikeValue.get(exec, index);
        if (vm.exception())
            return;
        
        RuntimeType type = runtimeTypeForValue(next);
        if (!(type & legalTypesFilter)) {
            throwTypeError(exec, scope, errorMessage);
            return;
        }
        
        bool exitEarly = addFunction(next, type);
        if (exitEarly)
            return;
    }
}

ALWAYS_INLINE bool JSObject::canPerformFastPutInline(ExecState* exec, VM& vm, PropertyName propertyName)
{
    if (UNLIKELY(propertyName == exec->propertyNames().underscoreProto))
        return false;

    // Check if there are any setters or getters in the prototype chain
    JSValue prototype;
    JSObject* obj = this;
    while (true) {
        if (obj->structure(vm)->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || obj->type() == ProxyObjectType)
            return false;

        prototype = obj->getPrototypeDirect();
        if (prototype.isNull())
            return true;

        obj = asObject(prototype);
    }

    ASSERT_NOT_REACHED();
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type JSObject::getPropertySlot(ExecState* exec, PropertyName propertyName, CallbackWhenNoException callback) const
{
    PropertySlot slot(this, PropertySlot::InternalMethodType::Get);
    return getPropertySlot(exec, propertyName, slot, callback);
}

template<typename CallbackWhenNoException>
ALWAYS_INLINE typename std::result_of<CallbackWhenNoException(bool, PropertySlot&)>::type JSObject::getPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot, CallbackWhenNoException callback) const
{
    bool found = const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot);
    if (UNLIKELY(exec->hadException()))
        return { };
    return callback(found, slot);
}

ALWAYS_INLINE bool JSObject::getPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    MethodTable::GetPrototypeFunctionPtr defaultGetPrototype = JSObject::getPrototype;
    while (true) {
        Structure& structure = *structureIDTable.get(object->structureID());
        if (structure.classInfo()->methodTable.getOwnPropertySlotByIndex(object, exec, propertyName, slot))
            return true;
        if (UNLIKELY(vm.exception()))
            return false;
        JSValue prototype;
        if (LIKELY(structure.classInfo()->methodTable.getPrototype == defaultGetPrototype || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry))
            prototype = structure.storedPrototype();
        else {
            prototype = object->getPrototype(vm, exec);
            if (vm.exception())
                return false;
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

ALWAYS_INLINE bool JSObject::getNonIndexPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    // This method only supports non-index PropertyNames.
    ASSERT(!parseIndex(propertyName));

    VM& vm = exec->vm();
    auto& structureIDTable = vm.heap.structureIDTable();
    JSObject* object = this;
    MethodTable::GetPrototypeFunctionPtr defaultGetPrototype = JSObject::getPrototype;
    while (true) {
        Structure& structure = *structureIDTable.get(object->structureID());
        if (LIKELY(!TypeInfo::overridesGetOwnPropertySlot(object->inlineTypeFlags()))) {
            if (object->getOwnNonIndexPropertySlot(vm, structure, propertyName, slot))
                return true;
        } else {
            if (structure.classInfo()->methodTable.getOwnPropertySlot(object, exec, propertyName, slot))
                return true;
            if (UNLIKELY(vm.exception()))
                return false;
        }
        JSValue prototype;
        if (LIKELY(structure.classInfo()->methodTable.getPrototype == defaultGetPrototype || slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry))
            prototype = structure.storedPrototype();
        else {
            prototype = object->getPrototype(vm, exec);
            if (vm.exception())
                return false;
        }
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }
}

// ECMA 8.6.2.2
ALWAYS_INLINE bool JSObject::putInline(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = jsCast<JSObject*>(cell);
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());

    // Try indexed put first. This is required for correctness, since loads on property names that appear like
    // valid indices will never look in the named property storage.
    if (Optional<uint32_t> index = parseIndex(propertyName))
        return putByIndex(thisObject, exec, index.value(), value, slot.isStrictMode());

    if (thisObject->canPerformFastPutInline(exec, vm, propertyName)) {
        ASSERT(!thisObject->structure(vm)->prototypeChainMayInterceptStoreTo(vm, propertyName));
        if (!thisObject->putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot)) {
            if (slot.isStrictMode())
                throwTypeError(exec, scope, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
            return false;
        }
        return true;
    }

    return thisObject->putInlineSlow(exec, propertyName, value, slot);
}

} // namespace JSC

#endif // JSObjectInlines_h

