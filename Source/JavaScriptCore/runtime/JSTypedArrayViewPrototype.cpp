/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "GetterSetter.h"
#include "JSArrayIterator.h"
#include "JSCInlines.h"
#include "JSGenericTypedArrayViewPrototypeFunctions.h"
#include "JSTypedArrayConstructors.h"
#include "Operations.h"
#include "VMTrapsInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncValues);
static JSC_DECLARE_HOST_FUNCTION(typedArrayProtoViewFuncEntries);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncKeys);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSet);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncCopyWithin);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncIncludes);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncLastIndexOf);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncIndexOf);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncJoin);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFill);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncBuffer);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncLength);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteLength);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteOffset);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncReverse);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSubarray);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSlice);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncToStringTag);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncToReversed);
static JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncWith);

#define CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION_ON_TYPE(type, functionName) do {             \
    switch ((type)) {                                                                             \
    case Uint8ClampedArrayType:                                                                 \
        return functionName<JSUint8ClampedArray>(vm, globalObject, callFrame);                  \
    case Int32ArrayType:                                                                        \
        return functionName<JSInt32Array>(vm, globalObject, callFrame);                         \
    case Uint32ArrayType:                                                                       \
        return functionName<JSUint32Array>(vm, globalObject, callFrame);                        \
    case Float64ArrayType:                                                                      \
        return functionName<JSFloat64Array>(vm, globalObject, callFrame);                       \
    case Float32ArrayType:                                                                      \
        return functionName<JSFloat32Array>(vm, globalObject, callFrame);                       \
    case Int8ArrayType:                                                                         \
        return functionName<JSInt8Array>(vm, globalObject, callFrame);                          \
    case Uint8ArrayType:                                                                        \
        return functionName<JSUint8Array>(vm, globalObject, callFrame);                         \
    case Int16ArrayType:                                                                        \
        return functionName<JSInt16Array>(vm, globalObject, callFrame);                         \
    case Uint16ArrayType:                                                                       \
        return functionName<JSUint16Array>(vm, globalObject, callFrame);                        \
    case BigInt64ArrayType:                                                                     \
        return functionName<JSBigInt64Array>(vm, globalObject, callFrame);                      \
    case BigUint64ArrayType:                                                                    \
        return functionName<JSBigUint64Array>(vm, globalObject, callFrame);                     \
    default:                                                                                    \
        return throwVMTypeError(globalObject, scope,                                            \
            "Receiver should be a typed array view"_s);                                         \
    }                                                                                           \
    RELEASE_ASSERT_NOT_REACHED();                                                               \
} while (false)


#define CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(functionName) do { \
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION_ON_TYPE(thisValue.getObject()->type(), functionName); \
} while (false)

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsTypedArrayView, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue value = callFrame->uncheckedArgument(0);
    return JSValue::encode(jsBoolean(value.isCell() && isTypedView(value.asCell()->type())));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsSharedTypedArrayView, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue value = callFrame->uncheckedArgument(0);
    if (!value.isCell())
        return JSValue::encode(jsBoolean(false));
    if (!isTypedView(value.asCell()->type()))
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(jsCast<JSArrayBufferView*>(value)->isShared()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsResizableOrGrowableSharedTypedArrayView, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue value = callFrame->uncheckedArgument(0);
    if (!value.isCell())
        return JSValue::encode(jsBoolean(false));
    if (!isTypedView(value.asCell()->type()))
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(jsCast<JSArrayBufferView*>(value)->isResizableOrGrowableShared()));
}

static inline std::optional<JSType> isTypedArrayViewConstructor(JSValue value)
{
    if (UNLIKELY(!value.isCell()))
        return std::nullopt;
    const auto* classInfo = value.asCell()->classInfo();

#define CASE_TYPED_ARRAY_TYPE(name) \
    if (JS##name##ArrayConstructor::info() == classInfo) \
        return name##ArrayType;
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(CASE_TYPED_ARRAY_TYPE)
#undef CASE_TYPED_ARRAY_TYPE

    return std::nullopt;
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncTypedArrayFromFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue constructor = callFrame->uncheckedArgument(0);
    auto type = isTypedArrayViewConstructor(constructor);
    if (!type)
        return JSValue::encode(jsUndefined());

    if (jsCast<JSObject*>(constructor)->globalObject() != globalObject)
        return JSValue::encode(jsUndefined());

    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION_ON_TYPE(type.value(), genericTypedArrayViewPrivateFuncFromFast);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsDetached, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue argument = callFrame->uncheckedArgument(0);
    ASSERT(argument.isCell() && isTypedView(argument.asCell()->type()));
    return JSValue::encode(jsBoolean(jsCast<JSArrayBufferView*>(argument)->isDetached()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncDefaultComparator, (JSGlobalObject*, CallFrame* callFrame))
{
    // https://tc39.es/ecma262/#sec-%typedarray%.prototype.sort

    JSValue x = callFrame->uncheckedArgument(0);
    JSValue y = callFrame->uncheckedArgument(1);

    if (x.isNumber()) {
        ASSERT(y.isNumber());
        if (x.isInt32() && y.isInt32()) {
            int32_t xInt32 = x.asInt32();
            int32_t yInt32 = y.asInt32();
            if (xInt32 < yInt32)
                return JSValue::encode(jsNumber(-1));
            if (xInt32 > yInt32)
                return JSValue::encode(jsNumber(1));
            return JSValue::encode(jsNumber(0));
        }

        double xDouble = x.asNumber();
        double yDouble = y.asNumber();
        if (std::isnan(xDouble) && std::isnan(yDouble))
            return JSValue::encode(jsNumber(0));
        if (std::isnan(xDouble))
            return JSValue::encode(jsNumber(1));
        if (std::isnan(yDouble))
            return JSValue::encode(jsNumber(-1));
        if (xDouble < yDouble)
            return JSValue::encode(jsNumber(-1));
        if (xDouble > yDouble)
            return JSValue::encode(jsNumber(1));
        if (!xDouble && !yDouble) {
            if (std::signbit(xDouble) && !std::signbit(yDouble))
                return JSValue::encode(jsNumber(-1));
            if (!std::signbit(xDouble) && std::signbit(yDouble))
                return JSValue::encode(jsNumber(1));
        }
        return JSValue::encode(jsNumber(0));
    }
    ASSERT(x.isBigInt() && y.isBigInt());
    switch (compareBigInt(x, y)) {
    case JSBigInt::ComparisonResult::Equal:
    case JSBigInt::ComparisonResult::Undefined:
        return JSValue::encode(jsNumber(0));
    case JSBigInt::ComparisonResult::GreaterThan:
        return JSValue::encode(jsNumber(1));
    case JSBigInt::ComparisonResult::LessThan:
        return JSValue::encode(jsNumber(-1));
    }
    return JSValue::encode(jsNumber(0));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    if (!argument.isCell() || !isTypedView(argument.asCell()->type()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);

    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(argument);
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsNumber(thisObject->length()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncContentType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    if (!argument.isCell() || !isTypedView(argument.asCell()->type()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);
    return JSValue::encode(jsNumber(static_cast<int32_t>(contentType(argument.asCell()->type()))));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncGetOriginalConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    TypedArrayType type = typedArrayType(callFrame->uncheckedArgument(0).getObject()->type());
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
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), thisObject, jsNumber(static_cast<unsigned>(kind))));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncValues, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Values);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayProtoViewFuncEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Entries);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createTypedArrayIteratorObject(globalObject, callFrame, IterationKind::Keys);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncClone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (UNLIKELY(!thisValue.isObject()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewPrivateFuncClone);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncSort, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->argument(0);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewPrivateFuncSort);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (UNLIKELY(!thisValue.isObject()))
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSet);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncCopyWithin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncCopyWithin);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncIncludes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMError(globalObject, scope, createTypeError(globalObject, "Receiver should be a typed array view but was not an object"_s));
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncIncludes);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncLastIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncLastIndexOf);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncIndexOf);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncJoin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncJoin);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncFill, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncFill);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoGetterFuncBuffer, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncBuffer);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoGetterFuncLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncLength);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteLength);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteOffset, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoGetterFuncByteOffset);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncReverse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncReverse);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncSubarray, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSubarray);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSlice);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoGetterFuncToStringTag, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return JSValue::encode(jsUndefined());

    VM& vm = globalObject->vm();
    switch (thisValue.getObject()->type()) {
    case Uint8ClampedArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Uint8ClampedArray"_s));
    case Int32ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Int32Array"_s));
    case Uint32ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Uint32Array"_s));
    case Float64ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Float64Array"_s));
    case Float32ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Float32Array"_s));
    case Int8ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Int8Array"_s));
    case Uint8ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Uint8Array"_s));
    case Int16ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Int16Array"_s));
    case Uint16ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Uint16Array"_s));
    case BigInt64ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "BigInt64Array"_s));
    case BigUint64ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "BigUint64Array"_s));
    default:
        return JSValue::encode(jsUndefined());
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncToReversed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncToReversed);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncWith);
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

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().sortPublicName(), globalObject->typedArrayProtoSort(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("buffer"_s, typedArrayViewProtoGetterFuncBuffer, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, typedArrayViewProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteLengthIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteOffset, typedArrayViewProtoGetterFuncByteOffset, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteOffsetIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("copyWithin"_s, typedArrayViewProtoFuncCopyWithin, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("every"_s, typedArrayPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("filter"_s, typedArrayPrototypeFilterCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().entriesPublicName(), typedArrayProtoViewFuncEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, TypedArrayEntriesIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("includes"_s, typedArrayViewProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().fillPublicName(), typedArrayViewProtoFuncFill, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("find"_s, typedArrayPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    if (Options::useArrayFindLastMethod())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastPublicName(), typedArrayPrototypeFindLastCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("findIndex"_s, typedArrayPrototypeFindIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    if (Options::useArrayFindLastMethod())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastIndexPublicName(), typedArrayPrototypeFindLastIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->forEach, typedArrayPrototypeForEachCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf"_s, typedArrayViewProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, typedArrayViewProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), typedArrayViewProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, TypedArrayKeysIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf"_s, typedArrayViewProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    JSFunction* lengthFunction = JSFunction::create(vm, globalObject, 0, "get length"_s, typedArrayViewProtoGetterFuncLength, ImplementationVisibility::Public, TypedArrayLengthIntrinsic);
    GetterSetter* lengthGetterSetter = GetterSetter::create(vm, globalObject, lengthFunction, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->length, lengthGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::Accessor);

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("map"_s, typedArrayPrototypeMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduce"_s, typedArrayPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduceRight"_s, typedArrayPrototypeReduceRightCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse"_s, typedArrayViewProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, typedArrayViewProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, typedArrayViewProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("some"_s, typedArrayPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->subarray, typedArrayViewProtoFuncSubarray, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, typedArrayPrototypeToLocaleStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useChangeArrayByCopyMethods()) {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toReversed"_s, typedArrayViewProtoFuncToReversed, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("toSorted"_s, typedArrayPrototypeToSortedCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->withKeyword, typedArrayViewProtoFuncWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    }
    if (Options::useAtMethod())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().atPublicName(), typedArrayPrototypeAtCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* toStringTagFunction = JSFunction::create(vm, globalObject, 0, "get [Symbol.toStringTag]"_s, typedArrayViewProtoGetterFuncToStringTag, ImplementationVisibility::Public);
    GetterSetter* toStringTagAccessor = GetterSetter::create(vm, globalObject, toStringTagFunction, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, toStringTagAccessor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::Accessor);

    JSFunction* valuesFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().valuesPublicName().string(), typedArrayViewProtoFuncValues, ImplementationVisibility::Public, TypedArrayValuesIntrinsic);

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), valuesFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, valuesFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    globalObject->installTypedArrayPrototypeIteratorProtocolWatchpoint(this);
}

JSTypedArrayViewPrototype* JSTypedArrayViewPrototype::create(
    VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSTypedArrayViewPrototype* prototype =
        new (NotNull, allocateCell<JSTypedArrayViewPrototype>(vm))
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
