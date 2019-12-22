/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "Error.h"
#include "JSArrayBuffer.h"
#include "JSDataView.h"
#include "Lookup.h"
#include "JSCInlines.h"
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
  setInt8               dataViewProtoFuncSetInt8             DontEnum|Function       2  DataViewSetInt8
  setUint8              dataViewProtoFuncSetUint8            DontEnum|Function       2  DataViewSetUint8
  setInt16              dataViewProtoFuncSetInt16            DontEnum|Function       2  DataViewSetInt16
  setUint16             dataViewProtoFuncSetUint16           DontEnum|Function       2  DataViewSetUint16
  setInt32              dataViewProtoFuncSetInt32            DontEnum|Function       2  DataViewSetInt32
  setUint32             dataViewProtoFuncSetUint32           DontEnum|Function       2  DataViewSetUint32
  setFloat32            dataViewProtoFuncSetFloat32          DontEnum|Function       2  DataViewSetFloat32
  setFloat64            dataViewProtoFuncSetFloat64          DontEnum|Function       2  DataViewSetFloat64
  buffer                dataViewProtoGetterBuffer            DontEnum|Accessor       0
  byteLength            dataViewProtoGetterByteLength        DontEnum|Accessor       0
  byteOffset            dataViewProtoGetterByteOffset        DontEnum|Accessor       0
@end
*/

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt8(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt16(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint8(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint16(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat64(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt8(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt16(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint8(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint16(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat32(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat64(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoGetterBuffer(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoGetterByteLength(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL dataViewProtoGetterByteOffset(JSGlobalObject*, CallFrame*);

}

#include "JSDataViewPrototype.lut.h"

namespace JSC {

const ClassInfo JSDataViewPrototype::s_info = {
    "DataViewPrototype", &Base::s_info, &dataViewTable, nullptr,
    CREATE_METHOD_TABLE(JSDataViewPrototype)
};

JSDataViewPrototype::JSDataViewPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

JSDataViewPrototype* JSDataViewPrototype::create(VM& vm, Structure* structure)
{
    JSDataViewPrototype* prototype =
        new (NotNull, allocateCell<JSDataViewPrototype>(vm.heap))
        JSDataViewPrototype(vm, structure);
    prototype->finishCreation(vm);
    return prototype;
}

void JSDataViewPrototype::finishCreation(JSC::VM& vm)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsNontrivialString(vm, "DataView"_s), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
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

    JSDataView* dataView = jsDynamicCast<JSDataView*>(vm, callFrame->thisValue());
    if (!dataView)
        return throwVMTypeError(globalObject, scope, "Receiver of DataView method must be a DataView"_s);
    
    unsigned byteOffset = callFrame->argument(0).toIndex(globalObject, "byteOffset");
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    bool littleEndian = false;
    unsigned elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && callFrame->argumentCount() >= 2) {
        littleEndian = callFrame->uncheckedArgument(1).toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    
    unsigned byteLength = dataView->length();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMError(globalObject, scope, createRangeError(globalObject, "Out of bounds access"_s));

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

    return JSValue::encode(Adaptor::toJSValue(u.value));
}

template<typename Adaptor>
EncodedJSValue setData(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* dataView = jsDynamicCast<JSDataView*>(vm, callFrame->thisValue());
    if (!dataView)
        return throwVMTypeError(globalObject, scope, "Receiver of DataView method must be a DataView"_s);
    
    unsigned byteOffset = callFrame->argument(0).toIndex(globalObject, "byteOffset");
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    const unsigned dataSize = sizeof(typename Adaptor::Type);
    union {
        typename Adaptor::Type value;
        uint8_t rawBytes[dataSize];
    } u;

    u.value = toNativeFromValue<Adaptor>(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    bool littleEndian = false;
    unsigned elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && callFrame->argumentCount() >= 3) {
        littleEndian = callFrame->uncheckedArgument(2).toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    
    unsigned byteLength = dataView->length();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMError(globalObject, scope, createRangeError(globalObject, "Out of bounds access"_s));

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

IGNORE_CLANG_WARNINGS_BEGIN("missing-prototypes")

EncodedJSValue JSC_HOST_CALL dataViewProtoGetterBuffer(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(vm, callFrame->thisValue());
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.buffer expects |this| to be a DataView object");

    return JSValue::encode(view->possiblySharedJSBuffer(globalObject));
}

EncodedJSValue JSC_HOST_CALL dataViewProtoGetterByteLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(vm, callFrame->thisValue());
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.buffer expects |this| to be a DataView object");

    return JSValue::encode(jsNumber(view->length()));
}

EncodedJSValue JSC_HOST_CALL dataViewProtoGetterByteOffset(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSDataView* view = jsDynamicCast<JSDataView*>(vm, callFrame->thisValue());
    if (!view)
        return throwVMTypeError(globalObject, scope, "DataView.prototype.buffer expects |this| to be a DataView object");

    return JSValue::encode(jsNumber(view->byteOffset()));
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt8(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Int8Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt16(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Int16Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Int32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint8(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Uint8Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint16(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Uint16Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Uint32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Float32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat64(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return getData<Float64Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt8(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Int8Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt16(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Int16Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Int32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint8(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Uint8Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint16(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Uint16Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Uint32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat32(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Float32Adaptor>(globalObject, callFrame);
}

EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat64(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return setData<Float64Adaptor>(globalObject, callFrame);
}
IGNORE_CLANG_WARNINGS_END

} // namespace JSC
