/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JSDataViewPrototype.h"

#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSDataView.h"
#include "ToNativeFromValue.h"
#include "TypedArrayAdaptors.h"
#include <wtf/FlipBytes.h>

namespace JSC {

/* Source for JSDataViewPrototype.lut.h
@begin dataViewTable
  getInt8               dataViewProtoFuncGetInt8             DontEnum|Function       1  DataViewGetInt8
  getUint8              dataViewProtoFuncGetUint8            DontEnum|Function       1  DataViewGetUint8
  getInt16              dataViewProtoFuncGetInt16            DontEnum|Function       1  DataViewGetInt16
  getUint16             dataViewProtoFuncGetUint16           DontEnum|Function       1  DataViewGetUint16
  getInt32              dataViewProtoFuncGetInt32            DontEnum|Function       1  DataViewGetInt32
  getUint32             dataViewProtoFuncGetUint32           DontEnum|Function       1  DataViewGetUint32
  getFloat32            dataViewProtoFuncGetFloat32          DontEnum|Function       1  DataViewGetFloat32
  getFloat64            dataViewProtoFuncGetFloat64          DontEnum|Function       1  DataViewGetFloat64
  getBigInt64           dataViewProtoFuncGetBigInt64         DontEnum|Function       1
  getBigUint64          dataViewProtoFuncGetBigUint64        DontEnum|Function       1
  setInt8               dataViewProtoFuncSetInt8             DontEnum|Function       2  DataViewSetInt8
  setUint8              dataViewProtoFuncSetUint8            DontEnum|Function       2  DataViewSetUint8
  setInt16              dataViewProtoFuncSetInt16            DontEnum|Function       2  DataViewSetInt16
  setUint16             dataViewProtoFuncSetUint16           DontEnum|Function       2  DataViewSetUint16
  setInt32              dataViewProtoFuncSetInt32            DontEnum|Function       2  DataViewSetInt32
  setUint32             dataViewProtoFuncSetUint32           DontEnum|Function       2  DataViewSetUint32
  setFloat32            dataViewProtoFuncSetFloat32          DontEnum|Function       2  DataViewSetFloat32
  setFloat64            dataViewProtoFuncSetFloat64          DontEnum|Function       2  DataViewSetFloat64
  setBigInt64           dataViewProtoFuncSetBigInt64         DontEnum|Function       2
  setBigUint64          dataViewProtoFuncSetBigUint64        DontEnum|Function       2
  buffer                dataViewProtoGetterBuffer            DontEnum|ReadOnly|CustomAccessor
  byteLength            dataViewProtoGetterByteLength        DontEnum|ReadOnly|CustomAccessor
  byteOffset            dataViewProtoGetterByteOffset        DontEnum|ReadOnly|CustomAccessor
@end
*/

static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetInt8);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetInt16);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetInt32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetUint8);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetUint16);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetUint32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetFloat32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetFloat64);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetBigInt64);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncGetBigUint64);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetInt8);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetInt16);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetInt32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetUint8);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetUint16);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetUint32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetFloat32);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetFloat64);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetBigInt64);
static JSC_DECLARE_HOST_FUNCTION(dataViewProtoFuncSetBigUint64);
static JSC_DECLARE_CUSTOM_GETTER(dataViewProtoGetterBuffer);
static JSC_DECLARE_CUSTOM_GETTER(dataViewProtoGetterByteLength);
static JSC_DECLARE_CUSTOM_GETTER(dataViewProtoGetterByteOffset);

}

#include "JSDataViewPrototype.lut.h"

namespace JSC {

const ClassInfo JSDataViewPrototype::s_info = {
    "DataView"_s, &Base::s_info, &dataViewTable, nullptr,
    CREATE_METHOD_TABLE(JSDataViewPrototype)
};

JSDataViewPrototype::JSDataViewPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

JSDataViewPrototype* JSDataViewPrototype::create(VM& vm, Structure* structure)
{
    JSDataViewPrototype* prototype =
        new (NotNull, allocateCell<JSDataViewPrototype>(vm))
        JSDataViewPrototype(vm, structure);
    prototype->finishCreation(vm);
    return prototype;
}

void JSDataViewPrototype::finishCreation(JSC::VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

Structure* JSDataViewPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

template<typename Adaptor>
EncodedJSValue getData(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* dataView = jsDynamicCast<JSDataView*>(callFrame->thisValue());
    if (!dataView)
        return throwVMTypeError(globalObject, scope, "Receiver of DataView method must be a DataView"_s);
    
    size_t byteOffset = callFrame->argument(0).toIndex(globalObject, "byteOffset");
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    bool littleEndian = false;
    size_t elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && callFrame->argumentCount() >= 2) {
        littleEndian = callFrame->uncheckedArgument(1).toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_relaxed> getter;
    auto byteLengthValue = dataView->viewByteLength(getter);
    if (UNLIKELY(!byteLengthValue))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t byteLength = byteLengthValue.value();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMRangeError(globalObject, scope, "Out of bounds access"_s);

    const unsigned dataSize = sizeof(typename Adaptor::Type);
    union {
        typename Adaptor::Type value;
        uint8_t rawBytes[dataSize];
    } u = { };

    uint8_t* dataPtr = static_cast<uint8_t*>(dataView->vector()) + byteOffset;

    if (needToFlipBytesIfLittleEndian(littleEndian)) {
        for (unsigned i = dataSize; i--;)
            u.rawBytes[i] = *dataPtr++;
    } else {
        for (unsigned i = 0; i < dataSize; i++)
            u.rawBytes[i] = *dataPtr++;
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(Adaptor::toJSValue(globalObject, u.value)));
}

template<typename Adaptor>
EncodedJSValue setData(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* dataView = jsDynamicCast<JSDataView*>(callFrame->thisValue());
    if (!dataView)
        return throwVMTypeError(globalObject, scope, "Receiver of DataView method must be a DataView"_s);
    
    size_t byteOffset = callFrame->argument(0).toIndex(globalObject, "byteOffset");
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    const unsigned dataSize = sizeof(typename Adaptor::Type);
    union {
        typename Adaptor::Type value;
        uint8_t rawBytes[dataSize];
    } u;

    u.value = toNativeFromValue<Adaptor>(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    bool littleEndian = false;
    size_t elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && callFrame->argumentCount() >= 3) {
        littleEndian = callFrame->uncheckedArgument(2).toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_relaxed> getter;
    auto byteLengthValue = dataView->viewByteLength(getter);
    if (UNLIKELY(!byteLengthValue))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t byteLength = byteLengthValue.value();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMRangeError(globalObject, scope, "Out of bounds access"_s);

    uint8_t* dataPtr = static_cast<uint8_t*>(dataView->vector()) + byteOffset;

    if (needToFlipBytesIfLittleEndian(littleEndian)) {
        for (unsigned i = dataSize; i--;)
            *dataPtr++ = u.rawBytes[i];
    } else {
        for (unsigned i = 0; i < dataSize; i++)
            *dataPtr++ = u.rawBytes[i];
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_CUSTOM_GETTER(dataViewProtoGetterBuffer, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(JSValue::decode(thisValue));
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.buffer expects |this| to be a DataView object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(view->possiblySharedJSBuffer(globalObject)));
}

JSC_DEFINE_CUSTOM_GETTER(dataViewProtoGetterByteLength, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(JSValue::decode(thisValue));
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.byteLength expects |this| to be a DataView object"_s);

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    auto byteLengthValue = view->viewByteLength(getter);
    if (UNLIKELY(!byteLengthValue))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(jsNumber(byteLengthValue.value()));
}

JSC_DEFINE_CUSTOM_GETTER(dataViewProtoGetterByteOffset, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(JSValue::decode(thisValue));
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.byteOffset expects |this| to be a DataView object"_s);

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    auto byteLengthValue = view->viewByteLength(getter);
    if (UNLIKELY(!byteLengthValue))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(jsNumber(view->byteOffsetRaw()));
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetInt8, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Int8Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetInt16, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Int16Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetInt32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Int32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetUint8, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Uint8Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetUint16, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Uint16Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetUint32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Uint32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetFloat32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Float32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetFloat64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<Float64Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetBigInt64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<BigInt64Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncGetBigUint64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return getData<BigUint64Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetInt8, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Int8Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetInt16, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Int16Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetInt32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Int32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetUint8, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Uint8Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetUint16, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Uint16Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetUint32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Uint32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetFloat32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Float32Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetFloat64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<Float64Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetBigInt64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<BigInt64Adaptor>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(dataViewProtoFuncSetBigUint64, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return setData<BigUint64Adaptor>(globalObject, callFrame);
}

} // namespace JSC
