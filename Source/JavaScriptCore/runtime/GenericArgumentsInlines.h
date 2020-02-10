/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#include "GenericArguments.h"
#include "JSCInlines.h"

namespace JSC {

template<typename Type>
void GenericArguments<Type>::visitChildren(JSCell* thisCell, SlotVisitor& visitor)
{
    Type* thisObject = static_cast<Type*>(thisCell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisCell, visitor);
    
    if (thisObject->m_modifiedArgumentsDescriptor)
        visitor.markAuxiliary(thisObject->m_modifiedArgumentsDescriptor.getUnsafe());
}

template<typename Type>
bool GenericArguments<Type>::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName ident, PropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(object);
    VM& vm = globalObject->vm();
    
    if (!thisObject->overrodeThings()) {
        if (ident == vm.propertyNames->length) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum), jsNumber(thisObject->internalLength()));
            return true;
        }
        if (ident == vm.propertyNames->callee) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum), thisObject->callee());
            return true;
        }
        if (ident == vm.propertyNames->iteratorSymbol) {
            slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::DontEnum), thisObject->globalObject(vm)->arrayProtoValuesFunction());
            return true;
        }
    }
    
    if (Optional<uint32_t> index = parseIndex(ident))
        return GenericArguments<Type>::getOwnPropertySlotByIndex(thisObject, globalObject, *index, slot);

    return Base::getOwnPropertySlot(thisObject, globalObject, ident, slot);
}

template<typename Type>
bool GenericArguments<Type>::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned index, PropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(object);
    
    if (!thisObject->isModifiedArgumentDescriptor(index) && thisObject->isMappedArgument(index)) {
        slot.setValue(thisObject, static_cast<unsigned>(PropertyAttribute::None), thisObject->getIndexQuickly(index));
        return true;
    }
    
    bool result = Base::getOwnPropertySlotByIndex(object, globalObject, index, slot);
    
    if (thisObject->isMappedArgument(index)) {
        ASSERT(result);
        slot.setValue(thisObject, slot.attributes(), thisObject->getIndexQuickly(index));
        return true;
    }
    
    return result;
}

template<typename Type>
void GenericArguments<Type>::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& array, EnumerationMode mode)
{
    VM& vm = globalObject->vm();
    Type* thisObject = jsCast<Type*>(object);

    if (array.includeStringProperties()) {
        for (unsigned i = 0; i < thisObject->internalLength(); ++i) {
            if (!thisObject->isMappedArgument(i))
                continue;
            array.add(Identifier::from(vm, i));
        }
    }

    if (mode.includeDontEnumProperties() && !thisObject->overrodeThings()) {
        array.add(vm.propertyNames->length);
        array.add(vm.propertyNames->callee);
        if (array.includeSymbolProperties())
            array.add(vm.propertyNames->iteratorSymbol);
    }
    Base::getOwnPropertyNames(thisObject, globalObject, array, mode);
}

template<typename Type>
bool GenericArguments<Type>::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName ident, JSValue value, PutPropertySlot& slot)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!thisObject->overrodeThings()
        && (ident == vm.propertyNames->length
            || ident == vm.propertyNames->callee
            || ident == vm.propertyNames->iteratorSymbol)) {
        thisObject->overrideThings(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
        PutPropertySlot dummy = slot; // This put is not cacheable, so we shadow the slot that was given to us.
        RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, ident, value, dummy));
    }

    // https://tc39.github.io/ecma262/#sec-arguments-exotic-objects-set-p-v-receiver
    // Fall back to the OrdinarySet when the receiver is altered from the thisObject.
    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        RELEASE_AND_RETURN(scope, ordinarySetSlow(globalObject, thisObject, ident, value, slot.thisValue(), slot.isStrictMode()));
    
    Optional<uint32_t> index = parseIndex(ident);
    if (index && thisObject->isMappedArgument(index.value())) {
        thisObject->setIndexQuickly(vm, index.value(), value);
        return true;
    }
    
    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, ident, value, slot));
}

template<typename Type>
bool GenericArguments<Type>::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned index, JSValue value, bool shouldThrow)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = globalObject->vm();

    if (thisObject->isMappedArgument(index)) {
        thisObject->setIndexQuickly(vm, index, value);
        return true;
    }
    
    return Base::putByIndex(cell, globalObject, index, value, shouldThrow);
}

template<typename Type>
bool GenericArguments<Type>::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName ident)
{
    Type* thisObject = jsCast<Type*>(cell);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!thisObject->overrodeThings()
        && (ident == vm.propertyNames->length
            || ident == vm.propertyNames->callee
            || ident == vm.propertyNames->iteratorSymbol)) {
        thisObject->overrideThings(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
    }

    if (Optional<uint32_t> index = parseIndex(ident))
        RELEASE_AND_RETURN(scope, GenericArguments<Type>::deletePropertyByIndex(thisObject, globalObject, *index));

    RELEASE_AND_RETURN(scope, Base::deleteProperty(thisObject, globalObject, ident));
}

template<typename Type>
bool GenericArguments<Type>::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned index)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Type* thisObject = jsCast<Type*>(cell);

    bool propertyMightBeInJSObjectStorage = thisObject->isModifiedArgumentDescriptor(index) || !thisObject->isMappedArgument(index);
    bool deletedProperty = true;
    if (propertyMightBeInJSObjectStorage) {
        deletedProperty = Base::deletePropertyByIndex(cell, globalObject, index);
        RETURN_IF_EXCEPTION(scope, true);
    }

    if (deletedProperty) {
        // Deleting an indexed property unconditionally unmaps it.
        if (thisObject->isMappedArgument(index)) {
            // We need to check that the property was mapped so we don't write to random memory.
            thisObject->unmapArgument(globalObject, index);
            RETURN_IF_EXCEPTION(scope, true);
        }
        thisObject->setModifiedArgumentDescriptor(globalObject, index);
        RETURN_IF_EXCEPTION(scope, true);
    }

    return deletedProperty;
}

template<typename Type>
bool GenericArguments<Type>::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName ident, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    Type* thisObject = jsCast<Type*>(object);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (ident == vm.propertyNames->length
        || ident == vm.propertyNames->callee
        || ident == vm.propertyNames->iteratorSymbol) {
        thisObject->overrideThingsIfNecessary(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
    } else {
        Optional<uint32_t> optionalIndex = parseIndex(ident);
        if (optionalIndex) {
            uint32_t index = optionalIndex.value();
            if (!descriptor.isAccessorDescriptor() && thisObject->isMappedArgument(optionalIndex.value())) {
                // If the property is not deleted and we are using a non-accessor descriptor, then
                // make sure that the aliased argument sees the value.
                if (descriptor.value())
                    thisObject->setIndexQuickly(vm, index, descriptor.value());
            
                // If the property is not deleted and we are using a non-accessor, writable,
                // configurable and enumerable descriptor and isn't modified, then we are done.
                // The argument continues to be aliased.
                if (descriptor.writable() && descriptor.configurable() && descriptor.enumerable() && !thisObject->isModifiedArgumentDescriptor(index))
                    return true;
                
                if (!thisObject->isModifiedArgumentDescriptor(index)) {
                    // If it is a new entry, we need to put direct to initialize argument[i] descriptor properly
                    JSValue value = thisObject->getIndexQuickly(index);
                    ASSERT(value);
                    object->putDirectMayBeIndex(globalObject, ident, value);
                    scope.assertNoException();

                    thisObject->setModifiedArgumentDescriptor(globalObject, index);
                    RETURN_IF_EXCEPTION(scope, false);
                }
            }
            
            if (thisObject->isMappedArgument(index)) {
                // Just unmap arguments if its descriptor contains {writable: false}.
                // Check https://tc39.github.io/ecma262/#sec-createunmappedargumentsobject
                // and https://tc39.github.io/ecma262/#sec-createmappedargumentsobject to verify that all data
                // property from arguments object are {writable: true, configurable: true, enumerable: true} by default
                if ((descriptor.writablePresent() && !descriptor.writable()) || descriptor.isAccessorDescriptor()) {
                    if (!descriptor.isAccessorDescriptor()) {
                        JSValue value = thisObject->getIndexQuickly(index);
                        ASSERT(value);
                        object->putDirectMayBeIndex(globalObject, ident, value);
                        scope.assertNoException();
                    }
                    thisObject->unmapArgument(globalObject, index);
                    RETURN_IF_EXCEPTION(scope, false);
                    thisObject->setModifiedArgumentDescriptor(globalObject, index);
                    RETURN_IF_EXCEPTION(scope, false);
                }
            }
        }
    }

    // Now just let the normal object machinery do its thing.
    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(object, globalObject, ident, descriptor, shouldThrow));
}

template<typename Type>
void GenericArguments<Type>::initModifiedArgumentsDescriptor(JSGlobalObject* globalObject, unsigned argsLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(!m_modifiedArgumentsDescriptor);

    if (argsLength) {
        void* backingStore = vm.gigacageAuxiliarySpace(m_modifiedArgumentsDescriptor.kind).allocateNonVirtual(vm, WTF::roundUpToMultipleOf<8>(argsLength), nullptr, AllocationFailureMode::ReturnNull);
        if (UNLIKELY(!backingStore)) {
            throwOutOfMemoryError(globalObject, scope);
            return;
        }
        bool* modifiedArguments = static_cast<bool*>(backingStore);
        m_modifiedArgumentsDescriptor.set(vm, this, modifiedArguments, argsLength);
        for (unsigned i = argsLength; i--;)
            modifiedArguments[i] = false;
    }
}

template<typename Type>
void GenericArguments<Type>::initModifiedArgumentsDescriptorIfNecessary(JSGlobalObject* globalObject, unsigned argsLength)
{
    if (!m_modifiedArgumentsDescriptor)
        initModifiedArgumentsDescriptor(globalObject, argsLength);
}

template<typename Type>
void GenericArguments<Type>::setModifiedArgumentDescriptor(JSGlobalObject* globalObject, unsigned index, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    initModifiedArgumentsDescriptorIfNecessary(globalObject, length);
    RETURN_IF_EXCEPTION(scope, void());
    if (index < length)
        m_modifiedArgumentsDescriptor.at(index, length) = true;
}

template<typename Type>
bool GenericArguments<Type>::isModifiedArgumentDescriptor(unsigned index, unsigned length)
{
    if (!m_modifiedArgumentsDescriptor)
        return false;
    if (index < length)
        return m_modifiedArgumentsDescriptor.at(index, length);
    return false;
}

template<typename Type>
void GenericArguments<Type>::copyToArguments(JSGlobalObject* globalObject, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Type* thisObject = static_cast<Type*>(this);
    for (unsigned i = 0; i < length; ++i) {
        if (thisObject->isMappedArgument(i + offset))
            firstElementDest[i] = thisObject->getIndexQuickly(i + offset);
        else {
            firstElementDest[i] = get(globalObject, i + offset);
            RETURN_IF_EXCEPTION(scope, void());
        }
    }
}

} // namespace JSC
