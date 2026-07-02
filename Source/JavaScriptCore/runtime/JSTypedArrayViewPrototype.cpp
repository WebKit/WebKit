/*
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
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
#include "JSTypedArrayViewPrototypeInternal.h"
#include "VMTrapsInlines.h"

namespace JSC {

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
    return JSValue::encode(jsBoolean(uncheckedDowncast<JSArrayBufferView>(value)->isShared()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsResizableOrGrowableSharedTypedArrayView, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue value = callFrame->uncheckedArgument(0);
    if (!value.isCell())
        return JSValue::encode(jsBoolean(false));
    if (!isTypedView(value.asCell()->type()))
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsBoolean(uncheckedDowncast<JSArrayBufferView>(value)->isResizableOrGrowableShared()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncIsDetached, (JSGlobalObject*, CallFrame* callFrame))
{
    JSValue argument = callFrame->uncheckedArgument(0);
    ASSERT(argument.isCell() && isTypedView(argument.asCell()->type()));
    return JSValue::encode(jsBoolean(uncheckedDowncast<JSArrayBufferView>(argument)->isDetached()));
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewPrivateFuncLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    if (!argument.isCell() || !isTypedView(argument.asCell()->type())) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);

    JSArrayBufferView* thisObject = uncheckedDowncast<JSArrayBufferView>(argument);
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsNumber(thisObject->length()));
}

inline EncodedJSValue createTypedArrayIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->thisValue().isCell() || !isTypedArrayType(callFrame->thisValue().asCell()->type())) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view"_s);

    JSArrayBufferView* thisObject = uncheckedDowncast<JSArrayBufferView>(callFrame->thisValue());
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
    case Float16ArrayType:
        return JSValue::encode(jsNontrivialString(vm, "Float16Array"_s));
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

JSTypedArrayViewPrototype::JSTypedArrayViewPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSTypedArrayViewPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);

    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("buffer"_s, typedArrayViewProtoGetterFuncBuffer, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, typedArrayViewProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteLengthIntrinsic);
    JSC_NATIVE_INTRINSIC_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteOffset, typedArrayViewProtoGetterFuncByteOffset, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, TypedArrayByteOffsetIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("copyWithin"_s, typedArrayViewProtoFuncCopyWithin, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->sort, typedArrayViewProtoFuncSort, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("every"_s, typedArrayViewProtoFuncEvery, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().filterPublicName(), typedArrayViewProtoFuncFilter, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().entriesPublicName(), typedArrayProtoViewFuncEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, TypedArrayEntriesIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("includes"_s, typedArrayViewProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->fill, typedArrayViewProtoFuncFill, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findPublicName(), typedArrayViewProtoFuncFind, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastPublicName(), typedArrayViewProtoFuncFindLast, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findIndexPublicName(), typedArrayViewProtoFuncFindIndex, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastIndexPublicName(), typedArrayViewProtoFuncFindLastIndex, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->forEach, typedArrayViewProtoFuncForEach, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf"_s, typedArrayViewProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, typedArrayViewProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), typedArrayViewProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, TypedArrayKeysIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf"_s, typedArrayViewProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    JSFunction* lengthFunction = JSFunction::create(vm, globalObject, 0, "get length"_s, typedArrayViewProtoGetterFuncLength, ImplementationVisibility::Public, TypedArrayLengthIntrinsic);
    GetterSetter* lengthGetterSetter = GetterSetter::create(vm, globalObject, lengthFunction, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->length, lengthGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly | PropertyAttribute::Accessor);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().mapPublicName(), typedArrayViewProtoFuncMap, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reducePublicName(), typedArrayViewProtoFuncReduce, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reduceRightPublicName(), typedArrayViewProtoFuncReduceRight, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse"_s, typedArrayViewProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, typedArrayViewProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, typedArrayViewProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("some"_s, typedArrayViewProtoFuncSome, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->subarray, typedArrayViewProtoFuncSubarray, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, typedArrayPrototypeToLocaleStringCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toReversed"_s, typedArrayViewProtoFuncToReversed, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toSorted"_s, typedArrayViewProtoFuncToSorted, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->withKeyword, typedArrayViewProtoFuncWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
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
