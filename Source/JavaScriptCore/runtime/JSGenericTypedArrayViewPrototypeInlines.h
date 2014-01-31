/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JSGenericTypedArrayViewPrototypeInlines_h
#define JSGenericTypedArrayViewPrototypeInlines_h

#include "CommonIdentifiers.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSFunction.h"
#include "JSGenericTypedArrayViewPrototype.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncSet(ExecState* exec)
{
    ViewClass* thisObject = jsDynamicCast<ViewClass*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view"));
    
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));
    
    JSObject* sourceArray = jsDynamicCast<JSObject*>(exec->uncheckedArgument(0));
    if (!sourceArray)
        return throwVMError(exec, createTypeError(exec, "First argument should be an object"));
    
    unsigned offset;
    if (exec->argumentCount() >= 2) {
        offset = exec->uncheckedArgument(1).toUInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    } else
        offset = 0;
    
    unsigned length = sourceArray->get(exec, exec->vm().propertyNames->length).toUInt32(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    thisObject->set(exec, sourceArray, offset, length);
    return JSValue::encode(jsUndefined());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncSubarray(ExecState* exec)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    
    ViewClass* thisObject = jsDynamicCast<ViewClass*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view"));
    
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));
    
    int32_t begin = exec->uncheckedArgument(0).toInt32(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    int32_t end;
    if (exec->argumentCount() >= 2) {
        end = exec->uncheckedArgument(1).toInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    } else
        end = thisObject->length();
    
    // Get the length here; later assert that the length didn't change.
    unsigned thisLength = thisObject->length();
    
    // Handle negative indices: -x => length - x
    if (begin < 0)
        begin = std::max(static_cast<int>(thisLength + begin), 0);
    if (end < 0)
        end = std::max(static_cast<int>(thisLength + end), 0);
    
    // Clamp the indices to the bounds of the array.
    ASSERT(begin >= 0);
    ASSERT(end >= 0);
    begin = std::min(begin, static_cast<int32_t>(thisLength));
    end = std::min(end, static_cast<int32_t>(thisLength));
    
    // Clamp end to begin.
    end = std::max(begin, end);
    
    ASSERT(end >= begin);
    unsigned offset = begin;
    unsigned length = end - begin;
    
    RefPtr<ArrayBuffer> arrayBuffer = thisObject->buffer();
    RELEASE_ASSERT(thisLength == thisObject->length());
    
    Structure* structure =
        callee->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType);
    
    ViewClass* result = ViewClass::create(
        exec, structure, arrayBuffer,
        thisObject->byteOffset() + offset * ViewClass::elementSize,
        length);
    
    return JSValue::encode(result);
}

template<typename ViewClass>
JSGenericTypedArrayViewPrototype<ViewClass>::JSGenericTypedArrayViewPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename ViewClass>
void JSGenericTypedArrayViewPrototype<ViewClass>::finishCreation(
    VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    
    ASSERT(inherits(info()));
    
    JSC_NATIVE_FUNCTION(vm.propertyNames->set, genericTypedArrayViewProtoFuncSet<ViewClass>, DontEnum, 2);
    JSC_NATIVE_FUNCTION(vm.propertyNames->subarray, genericTypedArrayViewProtoFuncSubarray<ViewClass>, DontEnum, 2);
    putDirect(vm, vm.propertyNames->BYTES_PER_ELEMENT, jsNumber(ViewClass::elementSize), DontEnum | ReadOnly | DontDelete);
}

template<typename ViewClass>
JSGenericTypedArrayViewPrototype<ViewClass>*
JSGenericTypedArrayViewPrototype<ViewClass>::create(
    VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSGenericTypedArrayViewPrototype* prototype =
        new (NotNull, allocateCell<JSGenericTypedArrayViewPrototype>(vm.heap))
        JSGenericTypedArrayViewPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

template<typename ViewClass>
Structure* JSGenericTypedArrayViewPrototype<ViewClass>::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC

#endif // JSGenericTypedArrayViewPrototypeInlines_h
