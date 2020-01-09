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
#include "JSTypedArrayViewPrototype.h"

#include "BuiltinNames.h"
#include "CallFrame.h"
#include "GetterSetter.h"
#include "JSArrayIterator.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGenericTypedArrayViewPrototypeFunctions.h"
#include "JSObjectInlines.h"
#include "TypedArrayAdaptors.h"

namespace JSC {

#define CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(functionName) do {                           \
    switch (thisValue.getObject()->classInfo(vm)->typedArrayStorageType) {                      \
    case TypeUint8Clamped:                                                                      \
        return functionName<JSUint8ClampedArray>(vm, globalObject, callFrame);                  \
    case TypeInt32:                                                                             \
        return functionName<JSInt32Array>(vm, globalObject, callFrame);                         \
    case TypeUint32:                                                                            \
        return functionName<JSUint32Array>(vm, globalObject, callFrame);                        \
    case TypeFloat64:                                                                           \
        return functionName<JSFloat64Array>(vm, globalObject, callFrame);                       \
    case TypeFloat32:                                                                           \
        return functionName<JSFloat32Array>(vm, globalObject, callFrame);                       \
    case TypeInt8:                                                                              \
        return functionName<JSInt8Array>(vm, globalObject, callFrame);                          \
    case TypeUint8:                                                                             \
        return functionName<JSUint8Array>(vm, globalObject, callFrame);                         \
    case TypeInt16:                                                                             \
        return functionName<JSInt16Array>(vm, globalObject, callFrame);                         \
    case TypeUint16:                                                                            \
        return functionName<JSUint16Array>(vm, globalObject, callFrame);                        \
    case NotTypedArray:                                                                         \
    case TypeDataView:                                                                          \
        return throwVMTypeError(globalObject, scope,                                            \
            "Receiver should be a typed array view"_s);                                         \
    }                                                                                           \
    RELEASE_ASSERT_NOT_REACHED();                                                               \
} while (false)

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncIsTypedArrayView(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue value = callFrame->uncheckedArgument(0);
    return JSValue::encode(jsBoolean(value.isCell() && isTypedView(value.asCell()->classInfo(globalObject->vm())->typedArrayStorageType)));
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    if (!argument.isCell() || !isTypedView(argument.asCell()->classInfo(vm)->typedArrayStorageType))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);

    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(argument);

    if (thisObject->isNeutered())
        return throwVMTypeError(globalObject, scope, "Underlying ArrayBuffer has been detached from the view"_s);

    return JSValue::encode(jsNumber(thisObject->length()));
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncGetOriginalConstructor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    TypedArrayType type = callFrame->uncheckedArgument(0).getObject()->classInfo(vm)->typedArrayStorageType;
    ASSERT(isTypedView(type));
    return JSValue::encode(globalObject->typedArrayConstructor(type));
}

inline EncodedJSValue createTypedArrayIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->thisValue().isCell() || !isTypedArrayType(callFrame->thisValue().asCell()->type()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);

    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(callFrame->thisValue());

    if (thisObject->isNeutered())
        return throwVMTypeError(globalObject, scope, "Underlying ArrayBuffer has been detached from the view"_s);

    return JSValue::encode(JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), thisObject, jsNumber(static_cast<unsigned>(kind))));
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncValues(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Values);
}

static EncodedJSValue JSC_HOST_CALL typedArrayProtoViewFuncEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Entries);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncKeys(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Keys);
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncSort(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->argument(0);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewPrivateFuncSort);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncSet(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (UNLIKELY(!thisValue.isObject()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSet);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncCopyWithin(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncCopyWithin);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncIncludes(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMError(globalObject, scope, createTypeError(globalObject, "Receiver should be a typed array view but was not an object"));
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncIncludes);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncLastIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncLastIndexOf);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncIndexOf);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncJoin(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncJoin);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncBuffer(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncBuffer);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncLength);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncByteLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteLength);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncByteOffset(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteOffset);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncReverse(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncReverse);
}

EncodedJSValue JSC_HOST_CALL typedArrayViewPrivateFuncSubarrayCreate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewPrivateFuncSubarrayCreate);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncSlice(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSlice);
}

static EncodedJSValue JSC_HOST_CALL typedArrayViewProtoGetterFuncToStringTag(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return JSValue::encode(jsUndefined());

    VM& vm = globalObject->vm();
    switch (thisValue.getObject()->classInfo(vm)->typedArrayStorageType) {
    case TypeUint8Clamped:
        return JSValue::encode(jsNontrivialString(vm, "Uint8ClampedArray"_s));
    case TypeInt32:
        return JSValue::encode(jsNontrivialString(vm, "Int32Array"_s));
    case TypeUint32:
        return JSValue::encode(jsNontrivialString(vm, "Uint32Array"_s));
    case TypeFloat64:
        return JSValue::encode(jsNontrivialString(vm, "Float64Array"_s));
    case TypeFloat32:
        return JSValue::encode(jsNontrivialString(vm, "Float32Array"_s));
    case TypeInt8:
        return JSValue::encode(jsNontrivialString(vm, "Int8Array"_s));
    case TypeUint8:
        return JSValue::encode(jsNontrivialString(vm, "Uint8Array"_s));
    case TypeInt16:
        return JSValue::encode(jsNontrivialString(vm, "Int16Array"_s));
    case TypeUint16:
        return JSValue::encode(jsNontrivialString(vm, "Uint16Array"_s));
    case NotTypedArray:
    case TypeDataView:
        return JSValue::encode(jsUndefined());
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

    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("buffer", typedArrayViewProtoGetterFuncBuffer, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, typedArrayViewProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteLengthIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteOffset, typedArrayViewProtoGetterFuncByteOffset, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteOffsetIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("copyWithin", typedArrayViewProtoFuncCopyWithin, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("every", typedArrayPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("filter", typedArrayPrototypeFilterCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("sort", typedArrayPrototypeSortCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->entries, typedArrayProtoViewFuncEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, TypedArrayEntriesIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("includes", typedArrayViewProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("fill", typedArrayPrototypeFillCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("find", typedArrayPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("findIndex", typedArrayPrototypeFindIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->forEach, typedArrayPrototypeForEachCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf", typedArrayViewProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, typedArrayViewProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->keys, typedArrayViewProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, TypedArrayKeysIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf", typedArrayViewProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->length, typedArrayViewProtoGetterFuncLength, static_cast<unsigned>(PropertyAttribute::DontEnum) | PropertyAttribute::ReadOnly, TypedArrayLengthIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("map", typedArrayPrototypeMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduce", typedArrayPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduceRight", typedArrayPrototypeReduceRightCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse", typedArrayViewProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, typedArrayViewProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, typedArrayViewProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("some", typedArrayPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->subarray, typedArrayPrototypeSubarrayCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, typedArrayPrototypeToLocaleStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* toStringTagFunction = JSFunction::create(vm, globalObject, 0, "get [Symbol.toStringTag]"_s, typedArrayViewProtoGetterFuncToStringTag, NoIntrinsic);
    GetterSetter* toStringTagAccessor = GetterSetter::create(vm, globalObject, toStringTagFunction, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, toStringTagAccessor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::Accessor);

    JSFunction* valuesFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->values.string(), typedArrayViewProtoFuncValues, TypedArrayValuesIntrinsic);

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), valuesFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, valuesFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

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
