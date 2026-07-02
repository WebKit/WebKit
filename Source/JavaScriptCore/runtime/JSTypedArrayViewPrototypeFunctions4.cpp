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
#include "JSTypedArrayViewPrototypeInternal.h"

#include "JSCInlines.h"
#include "JSGenericTypedArrayViewPrototypeFunctions.h"
#include "JSTypedArrayConstructors.h"

namespace JSC {

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncSlice);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncToReversed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncToReversed);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncWith);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncReduce, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncReduce);
}

JSC_DEFINE_HOST_FUNCTION(typedArrayViewProtoFuncReduceRight, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Receiver should be a typed array view but was not an object"_s);
    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION(genericTypedArrayViewProtoFuncReduceRight);
}

static inline std::optional<JSType> NODELETE isTypedArrayViewConstructor(JSValue value)
{
    if (!value.isCell()) [[unlikely]]
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

    if (uncheckedDowncast<JSObject>(constructor)->realm() != globalObject)
        return JSValue::encode(jsUndefined());

    scope.release();
    CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION_ON_TYPE(type.value(), genericTypedArrayViewPrivateFuncFromFast);
}

} // namespace JSC
