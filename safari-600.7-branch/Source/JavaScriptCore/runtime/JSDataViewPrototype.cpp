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

#include "config.h"
#include "JSDataViewPrototype.h"

#include "Error.h"
#include "JSDataView.h"
#include "Lookup.h"
#include "JSCInlines.h"
#include "ToNativeFromValue.h"
#include "TypedArrayAdaptors.h"
#include <wtf/FlipBytes.h>

namespace JSC {

const ClassInfo JSDataViewPrototype::s_info = {
    "DataViewPrototype", &Base::s_info, 0, ExecState::dataViewTable,
    CREATE_METHOD_TABLE(JSDataViewPrototype)
};

/* Source for JSDataViewPrototype.lut.h
@begin dataViewTable
  getInt8               dataViewProtoFuncGetInt8             DontEnum|Function       0
  getUint8              dataViewProtoFuncGetUint8            DontEnum|Function       0
  getInt16              dataViewProtoFuncGetInt16            DontEnum|Function       0
  getUint16             dataViewProtoFuncGetUint16           DontEnum|Function       0
  getInt32              dataViewProtoFuncGetInt32            DontEnum|Function       0
  getUint32             dataViewProtoFuncGetUint32           DontEnum|Function       0
  getFloat32            dataViewProtoFuncGetFloat32          DontEnum|Function       0
  getFloat64            dataViewProtoFuncGetFloat64          DontEnum|Function       0
  setInt8               dataViewProtoFuncSetInt8             DontEnum|Function       0
  setUint8              dataViewProtoFuncSetUint8            DontEnum|Function       0
  setInt16              dataViewProtoFuncSetInt16            DontEnum|Function       0
  setUint16             dataViewProtoFuncSetUint16           DontEnum|Function       0
  setInt32              dataViewProtoFuncSetInt32            DontEnum|Function       0
  setUint32             dataViewProtoFuncSetUint32           DontEnum|Function       0
  setFloat32            dataViewProtoFuncSetFloat32          DontEnum|Function       0
  setFloat64            dataViewProtoFuncSetFloat64          DontEnum|Function       0
@end
*/

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

Structure* JSDataViewPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

bool JSDataViewPrototype::getOwnPropertySlot(
    JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(
        exec, ExecState::dataViewTable(exec->vm()), jsCast<JSDataViewPrototype*>(object),
        propertyName, slot);
}

template<typename Adaptor>
EncodedJSValue getData(ExecState* exec)
{
    JSDataView* dataView = jsDynamicCast<JSDataView*>(exec->thisValue());
    if (!dataView)
        return throwVMError(exec, createTypeError(exec, "Receiver of DataView method must be a DataView"));
    
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Need at least one argument (the byteOffset)"));
    
    unsigned byteOffset = exec->uncheckedArgument(0).toUInt32(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    bool littleEndian = false;
    unsigned elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && exec->argumentCount() >= 2) {
        littleEndian = exec->uncheckedArgument(1).toBoolean(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    
    unsigned byteLength = dataView->length();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMError(exec, createRangeError(exec, "Out of bounds access"));

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
EncodedJSValue setData(ExecState* exec)
{
    JSDataView* dataView = jsDynamicCast<JSDataView*>(exec->thisValue());
    if (!dataView)
        return throwVMError(exec, createTypeError(exec, "Receiver of DataView method must be a DataView"));
    
    if (exec->argumentCount() < 2)
        return throwVMError(exec, createTypeError(exec, "Need at least two argument (the byteOffset and value)"));
    
    unsigned byteOffset = exec->uncheckedArgument(0).toUInt32(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    const unsigned dataSize = sizeof(typename Adaptor::Type);
    union {
        typename Adaptor::Type value;
        uint8_t rawBytes[dataSize];
    } u;

    u.value = toNativeFromValue<Adaptor>(exec, exec->uncheckedArgument(1));
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    bool littleEndian = false;
    unsigned elementSize = sizeof(typename Adaptor::Type);
    if (elementSize > 1 && exec->argumentCount() >= 3) {
        littleEndian = exec->uncheckedArgument(2).toBoolean(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    
    unsigned byteLength = dataView->length();
    if (elementSize > byteLength || byteOffset > byteLength - elementSize)
        return throwVMError(exec, createRangeError(exec, "Out of bounds access"));

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

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt8(ExecState* exec)
{
    return getData<Int8Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt16(ExecState* exec)
{
    return getData<Int16Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetInt32(ExecState* exec)
{
    return getData<Int32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint8(ExecState* exec)
{
    return getData<Uint8Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint16(ExecState* exec)
{
    return getData<Uint16Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetUint32(ExecState* exec)
{
    return getData<Uint32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat32(ExecState* exec)
{
    return getData<Float32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncGetFloat64(ExecState* exec)
{
    return getData<Float64Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt8(ExecState* exec)
{
    return setData<Int8Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt16(ExecState* exec)
{
    return setData<Int16Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetInt32(ExecState* exec)
{
    return setData<Int32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint8(ExecState* exec)
{
    return setData<Uint8Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint16(ExecState* exec)
{
    return setData<Uint16Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetUint32(ExecState* exec)
{
    return setData<Uint32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat32(ExecState* exec)
{
    return setData<Float32Adaptor>(exec);
}

static EncodedJSValue JSC_HOST_CALL dataViewProtoFuncSetFloat64(ExecState* exec)
{
    return setData<Float64Adaptor>(exec);
}

} // namespace JSC

#include "JSDataViewPrototype.lut.h"

