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
#include "JSTypedArrayViewPrototype.h"

#include "CallFrame.h"
#include "GetterSetter.h"
#include "JSCellInlines.h"
#include "JSFunction.h"
#include "JSGenericTypedArrayViewPrototypeFunctions.h"
#include "TypedArrayAdaptors.h"

namespace JSC {

#define CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(functionName) do {                           \
    switch (thisValue.getObject()->classInfo()->typedArrayStorageType) {                        \
    case TypeUint8Clamped:                                                                      \
        return functionName<JSUint8ClampedArray>(exec);                                         \
    case TypeInt32:                                                                             \
        return functionName<JSInt32Array>(exec);                                                \
    case TypeUint32:                                                                            \
        return functionName<JSUint32Array>(exec);                                               \
    case TypeFloat64:                                                                           \
        return functionName<JSFloat64Array>(exec);                                              \
    case TypeFloat32:                                                                           \
        return functionName<JSFloat32Array>(exec);                                              \
    case TypeInt8:                                                                              \
        return functionName<JSInt8Array>(exec);                                                 \
    case TypeUint8:                                                                             \
        return functionName<JSUint8Array>(exec);                                                \
    case TypeInt16:                                                                             \
        return functionName<JSInt16Array>(exec);                                                \
    case TypeUint16:                                                                            \
        return functionName<JSUint16Array>(exec);                                               \
    case NotTypedArray:                                                                         \
    case TypeDataView:                                                                          \
        return throwVMError(exec, createTypeError(exec,                                         \
            "Receiver should be a typed array view"));                                          \
    }                                                                                           \
    RELEASE_ASSERT_NOT_REACHED();                                                               \
} while (false)

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncLength(ExecState* exec)
{
    JSArrayBufferView* thisObject = jsDynamicCast<JSArrayBufferView*>(exec->argument(0));
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view"));
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, "Underlying ArrayBuffer has been detached from the view");

    return JSValue::encode(jsNumber(thisObject->length()));
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncGetOriginalConstructor(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    TypedArrayType type = exec->uncheckedArgument(0).getObject()->classInfo()->typedArrayStorageType;
    ASSERT(isTypedView(type));
    return JSValue::encode(globalObject->typedArrayConstructor(type));
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncSort(ExecState* exec)
{
    JSValue thisValue = exec->argument(0);
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewPrivateFuncSort);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncSet(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSet);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncEntries(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncEntries);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncCopyWithin(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncCopyWithin);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncFill(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncFill);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncLastIndexOf(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncLastIndexOf);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncIndexOf(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncIndexOf);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncJoin(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncJoin);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncKeys(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncKeys);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncLength(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncLength);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncByteLength(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteLength);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncByteOffset(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteOffset);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncReverse(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncReverse);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncSubarray(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSubarray);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncSlice(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSlice);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncValues(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(typedArrayViewProtoFuncValues);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncToStringTag(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view but was not an object"));

    VM& vm = exec->vm();
    switch (thisValue.getObject()->classInfo()->typedArrayStorageType) {
    case TypeUint8Clamped:
        return JSValue::encode(jsString(&vm, "Uint8ClampedArray"));
    case TypeInt32:
        return JSValue::encode(jsString(&vm, "Int32Array"));
    case TypeUint32:
        return JSValue::encode(jsString(&vm, "Uint32Array"));
    case TypeFloat64:
        return JSValue::encode(jsString(&vm, "Float64Array"));
    case TypeFloat32:
        return JSValue::encode(jsString(&vm, "Float32Array"));
    case TypeInt8:
        return JSValue::encode(jsString(&vm, "Int8Array"));
    case TypeUint8:
        return JSValue::encode(jsString(&vm, "Uint8Array"));
    case TypeInt16:
        return JSValue::encode(jsString(&vm, "Int16Array"));
    case TypeUint16:
        return JSValue::encode(jsString(&vm, "Uint16Array"));
    case NotTypedArray:
    case TypeDataView:
        return throwVMError(exec, createTypeError(exec, "Receiver should be a typed array view"));
    }
    RELEASE_ASSERT_NOT_REACHED();
}


#undef CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION

JSTypedArrayViewPrototype::JSTypedArrayViewPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSTypedArrayViewPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);

    ASSERT(inherits(info()));

    JSC_NATIVE_INTRINSIC_GETTER(vm.propertyNames->byteLength, typedArrayViewProtoGetterFuncByteLength, DontEnum | ReadOnly | DontDelete, TypedArrayByteLengthIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER(vm.propertyNames->byteOffset, typedArrayViewProtoGetterFuncByteOffset, DontEnum | ReadOnly | DontDelete, TypedArrayByteOffsetIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("copyWithin", typedArrayViewProtoFuncCopyWithin, DontEnum, 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("every", typedArrayPrototypeEveryCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("filter", typedArrayPrototypeFilterCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("sort", typedArrayPrototypeSortCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->entries, typedArrayViewProtoFuncEntries, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("fill", typedArrayViewProtoFuncFill, DontEnum, 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("find", typedArrayPrototypeFindCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("findIndex", typedArrayPrototypeFindIndexCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->forEach, typedArrayPrototypeForEachCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf", typedArrayViewProtoFuncIndexOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, typedArrayViewProtoFuncJoin, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->keys, typedArrayViewProtoFuncKeys, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf", typedArrayViewProtoFuncLastIndexOf, DontEnum, 1);
    JSC_NATIVE_INTRINSIC_GETTER(vm.propertyNames->length, typedArrayViewProtoGetterFuncLength, DontEnum | ReadOnly | DontDelete, TypedArrayLengthIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("map", typedArrayPrototypeMapCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduce", typedArrayPrototypeReduceCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduceRight", typedArrayPrototypeReduceRightCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse", typedArrayViewProtoFuncReverse, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, typedArrayViewProtoFuncSet, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, typedArrayViewProtoFuncSlice, DontEnum, 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("some", typedArrayPrototypeSomeCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->subarray, typedArrayViewProtoFuncSubarray, DontEnum, 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, typedArrayPrototypeToLocaleStringCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, arrayProtoFuncToString, DontEnum, 0);
    JSC_NATIVE_GETTER(vm.propertyNames->toStringTagSymbol, typedArrayViewProtoGetterFuncToStringTag, DontEnum | ReadOnly);

    JSFunction* valuesFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->values.string(), typedArrayViewProtoFuncValues);

    putDirectWithoutTransition(vm, vm.propertyNames->values, valuesFunction, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, valuesFunction, DontEnum);

}

JSTypedArrayViewPrototype* JSTypedArrayViewPrototype::create(
    VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSTypedArrayViewPrototype* prototype =
        new (NotNull, allocateCell<JSTypedArrayViewPrototype>(vm.heap))
        JSTypedArrayViewPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* JSTypedArrayViewPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC
