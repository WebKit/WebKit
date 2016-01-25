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

#include "config.h"
#include "JSTypedArrayViewConstructor.h"

#include "CallFrame.h"
#include "Error.h"
#include "GetterSetter.h"
#include "JSCBuiltins.h"
#include "JSCellInlines.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSObject.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"

namespace JSC {

JSTypedArrayViewConstructor::JSTypedArrayViewConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

const ClassInfo JSTypedArrayViewConstructor::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(JSTypedArrayViewConstructor) };

void JSTypedArrayViewConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, JSTypedArrayViewPrototype* prototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, "TypedArray");
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(3), DontEnum | DontDelete | ReadOnly);
    putDirectNonIndexAccessor(vm, vm.propertyNames->speciesSymbol, speciesSymbol, Accessor | ReadOnly | DontEnum | DontDelete);

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->of, typedArrayConstructorOfCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->from, typedArrayConstructorFromCodeGenerator, DontEnum);
}

Structure* JSTypedArrayViewConstructor::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}


// The only way we can call this function is through Reflect.construct or if they mucked with the TypedArray prototype chain.
// In either case we are ok with this being slow.
static EncodedJSValue JSC_HOST_CALL constructTypedArrayView(ExecState* exec)
{
    JSValue value = exec->newTarget();

    JSObject* object = jsDynamicCast<JSObject*>(value);
    if (!object)
        return JSValue::encode(throwTypeError(exec, "new.target passed to TypedArray is not an object."));

    ConstructData data;
    if (object->methodTable()->getConstructData(object, data) == ConstructTypeNone)
        return JSValue::encode(throwTypeError(exec, "new.target passed to TypedArray is not a valid constructor."));

    for (; !value.isNull(); value = jsCast<JSObject*>(value)->prototype()) {
        if (jsDynamicCast<JSTypedArrayViewConstructor*>(value))
            return JSValue::encode(throwTypeError(exec, "Unable to find TypedArray constructor that inherits from TypedArray."));
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSInt8Array>*>(value))
            return constructGenericTypedArrayView<JSInt8Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSInt16Array>*>(value))
            return constructGenericTypedArrayView<JSInt16Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSInt32Array>*>(value))
            return constructGenericTypedArrayView<JSInt32Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSUint8Array>*>(value))
            return constructGenericTypedArrayView<JSUint8Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSUint16Array>*>(value))
            return constructGenericTypedArrayView<JSUint16Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSUint32Array>*>(value))
            return constructGenericTypedArrayView<JSUint32Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSUint8ClampedArray>*>(value))
            return constructGenericTypedArrayView<JSUint8ClampedArray>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSFloat32Array>*>(value))
            return constructGenericTypedArrayView<JSFloat32Array>(exec);
        if (jsDynamicCast<JSGenericTypedArrayViewConstructor<JSFloat64Array>*>(value))
            return constructGenericTypedArrayView<JSFloat64Array>(exec);
    }
    
    return JSValue::encode(throwTypeError(exec, "Unable to find TypedArray constructor in prototype-chain, hit null."));
}

ConstructType JSTypedArrayViewConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructTypedArrayView;
    return ConstructTypeHost;
}

CallType JSTypedArrayViewConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = nullptr;
    return CallTypeNone;
}

} // namespace JSC
