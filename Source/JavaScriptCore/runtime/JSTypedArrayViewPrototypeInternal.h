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

#pragma once

// Shared by JSTypedArrayViewPrototype.cpp and the
// JSTypedArrayViewPrototypeFunctions{1..4}.cpp shards. The host functions
// dispatching through CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION instantiate
// genericTypedArrayViewProtoFunc*<View> for each of the 12 typed-array view
// types; sharding their definitions across separate @no-unify TUs lets those
// instantiations compile in parallel instead of stacking onto the JavaScriptCore
// critical path.

#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"

namespace JSC {

#define CALL_GENERIC_TYPEDARRAY_PROTOTYPE_FUNCTION_ON_TYPE(type, functionName) do { \
    switch ((type)) { \
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
    case Float16ArrayType:                                                                      \
        return functionName<JSFloat16Array>(vm, globalObject, callFrame);                       \
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

JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSet);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncCopyWithin);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncForEach);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncMap);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFilter);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFind);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFindIndex);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFindLast);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFindLastIndex);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncEvery);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSome);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSort);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncIncludes);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncLastIndexOf);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncIndexOf);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncJoin);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncFill);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncBuffer);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncLength);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteLength);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncByteOffset);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncReverse);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSubarray);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncSlice);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncToReversed);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncToSorted);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncWith);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncReduce);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncReduceRight);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncValues);
JSC_DECLARE_HOST_FUNCTION(typedArrayProtoViewFuncEntries);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoFuncKeys);
JSC_DECLARE_HOST_FUNCTION(typedArrayViewProtoGetterFuncToStringTag);

} // namespace JSC
