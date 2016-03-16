/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef GenericArgumentsInlines_h
#define GenericArgumentsInlines_h

#include "GenericArguments.h"
#include "JSCInlines.h"

namespace JSC {

template<typename Type>
bool GenericArguments<Type>::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName ident, PropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(object);
    VM& vm = exec->vm();
    
    if (!thisObject->overrodeThings()) {
        if (ident == vm.propertyNames->length) {
            slot.setValue(thisObject, DontEnum, jsNumber(thisObject->internalLength()));
            return true;
        }
        if (ident == vm.propertyNames->callee) {
            slot.setValue(thisObject, DontEnum, thisObject->callee().get());
            return true;
        }
        if (ident == vm.propertyNames->iteratorSymbol) {
            slot.setValue(thisObject, DontEnum, thisObject->globalObject()->arrayProtoValuesFunction());
            return true;
        }
    }
    
    Optional<uint32_t> index = parseIndex(ident);
    if (index && thisObject->canAccessIndexQuickly(index.value())) {
        slot.setValue(thisObject, None, thisObject->getIndexQuickly(index.value()));
        return true;
    }
    
    return Base::getOwnPropertySlot(thisObject, exec, ident, slot);
}

template<typename Type>
bool GenericArguments<Type>::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned index, PropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(object);
    
    if (thisObject->canAccessIndexQuickly(index)) {
        slot.setValue(thisObject, None, thisObject->getIndexQuickly(index));
        return true;
    }
    
    return Base::getOwnPropertySlotByIndex(object, exec, index, slot);
}

template<typename Type>
void GenericArguments<Type>::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& array, EnumerationMode mode)
{
    Type* thisObject = jsCast<Type*>(object);

    if (array.includeStringProperties()) {
        for (unsigned i = 0; i < thisObject->internalLength(); ++i) {
            if (!thisObject->canAccessIndexQuickly(i))
                continue;
            array.add(Identifier::from(exec, i));
        }
    }

    if (mode.includeDontEnumProperties() && !thisObject->overrodeThings()) {
        array.add(exec->propertyNames().length);
        array.add(exec->propertyNames().callee);
        if (array.includeSymbolProperties())
            array.add(exec->propertyNames().iteratorSymbol);
    }
    Base::getOwnPropertyNames(thisObject, exec, array, mode);
}

template<typename Type>
bool GenericArguments<Type>::put(JSCell* cell, ExecState* exec, PropertyName ident, JSValue value, PutPropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = exec->vm();
    
    if (!thisObject->overrodeThings()
        && (ident == vm.propertyNames->length
            || ident == vm.propertyNames->callee
            || ident == vm.propertyNames->iteratorSymbol)) {
        thisObject->overrideThings(vm);
        PutPropertySlot dummy = slot; // This put is not cacheable, so we shadow the slot that was given to us.
        return Base::put(thisObject, exec, ident, value, dummy);
    }

    // https://tc39.github.io/ecma262/#sec-arguments-exotic-objects-set-p-v-receiver
    // Fall back to the OrdinarySet when the receiver is altered from the thisObject.
    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, ident, value, slot.thisValue(), slot.isStrictMode());
    
    Optional<uint32_t> index = parseIndex(ident);
    if (index && thisObject->canAccessIndexQuickly(index.value())) {
        thisObject->setIndexQuickly(vm, index.value(), value);
        return true;
    }
    
    return Base::put(thisObject, exec, ident, value, slot);
}

template<typename Type>
bool GenericArguments<Type>::putByIndex(JSCell* cell, ExecState* exec, unsigned index, JSValue value, bool shouldThrow)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = exec->vm();

    if (thisObject->canAccessIndexQuickly(index)) {
        thisObject->setIndexQuickly(vm, index, value);
        return true;
    }
    
    return Base::putByIndex(cell, exec, index, value, shouldThrow);
}

template<typename Type>
bool GenericArguments<Type>::deleteProperty(JSCell* cell, ExecState* exec, PropertyName ident)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = exec->vm();
    
    if (!thisObject->overrodeThings()
        && (ident == vm.propertyNames->length
            || ident == vm.propertyNames->callee
            || ident == vm.propertyNames->iteratorSymbol))
        thisObject->overrideThings(vm);
    
    Optional<uint32_t> index = parseIndex(ident);
    if (index && thisObject->canAccessIndexQuickly(index.value())) {
        thisObject->overrideArgument(vm, index.value());
        return true;
    }
    
    return Base::deleteProperty(thisObject, exec, ident);
}

template<typename Type>
bool GenericArguments<Type>::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned index)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = exec->vm();
    
    if (thisObject->canAccessIndexQuickly(index)) {
        thisObject->overrideArgument(vm, index);
        return true;
    }
    
    return Base::deletePropertyByIndex(cell, exec, index);
}

template<typename Type>
bool GenericArguments<Type>::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName ident, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    Type* thisObject = jsCast<Type*>(object);
    VM& vm = exec->vm();
    
    if (ident == vm.propertyNames->length
        || ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol)
        thisObject->overrideThingsIfNecessary(vm);
    else {
        Optional<uint32_t> optionalIndex = parseIndex(ident);
        if (optionalIndex && thisObject->canAccessIndexQuickly(optionalIndex.value())) {
            uint32_t index = optionalIndex.value();
            if (!descriptor.isAccessorDescriptor()) {
                // If the property is not deleted and we are using a non-accessor descriptor, then
                // make sure that the aliased argument sees the value.
                if (descriptor.value())
                    thisObject->setIndexQuickly(vm, index, descriptor.value());
            
                // If the property is not deleted and we are using a non-accessor, writable
                // descriptor, then we are done. The argument continues to be aliased. Note that we
                // ignore the request to change enumerability. We appear to have always done so, in
                // cases where the argument was still aliased.
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=141952
                if (descriptor.writable())
                    return true;
            }
        
            // If the property is a non-deleted argument, then move it into the base object and
            // then delete it.
            JSValue value = thisObject->getIndexQuickly(index);
            ASSERT(value);
            object->putDirectMayBeIndex(exec, ident, value);
            thisObject->overrideArgument(vm, index);
        }
    }
    
    // Now just let the normal object machinery do its thing.
    return Base::defineOwnProperty(object, exec, ident, descriptor, shouldThrow);
}

template<typename Type>
void GenericArguments<Type>::copyToArguments(ExecState* exec, VirtualRegister firstElementDest, unsigned offset, unsigned length)
{
    Type* thisObject = static_cast<Type*>(this);
    for (unsigned i = 0; i < length; ++i) {
        if (thisObject->canAccessIndexQuickly(i + offset))
            exec->r(firstElementDest + i) = thisObject->getIndexQuickly(i + offset);
        else {
            exec->r(firstElementDest + i) = get(exec, i + offset);
            if (UNLIKELY(exec->vm().exception()))
                return;
        }
    }
}

} // namespace JSC

#endif // GenericArgumentsInlines_h

