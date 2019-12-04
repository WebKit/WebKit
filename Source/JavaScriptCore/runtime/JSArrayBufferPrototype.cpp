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
#include "JSArrayBufferPrototype.h"

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSFunction.h"
#include "JSCInlines.h"
#include "TypedArrayAdaptors.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL arrayBufferProtoFuncSlice(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArrayBuffer* thisObject = jsDynamicCast<JSArrayBuffer*>(vm, callFrame->thisValue());
    if (!thisObject || thisObject->impl()->isShared())
        return throwVMTypeError(globalObject, scope, "Receiver of slice must be an ArrayBuffer."_s);

    double begin = callFrame->argument(0).toInteger(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    
    double end;
    if (!callFrame->argument(1).isUndefined()) {
        end = callFrame->uncheckedArgument(1).toInteger(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    } else
        end = thisObject->impl()->byteLength();
    
    auto newBuffer = thisObject->impl()->slice(begin, end);
    if (!newBuffer)
        return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
    
    Structure* structure = globalObject->arrayBufferStructure(newBuffer->sharingMode());
    
    JSArrayBuffer* result = JSArrayBuffer::create(vm, structure, WTFMove(newBuffer));
    
    return JSValue::encode(result);
}

// http://tc39.github.io/ecmascript_sharedmem/shmem.html#sec-get-arraybuffer.prototype.bytelength
static EncodedJSValue JSC_HOST_CALL arrayBufferProtoGetterFuncByteLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be an array buffer but was not an object"_s);

    auto* thisObject = jsDynamicCast<JSArrayBuffer*>(vm, thisValue);
    if (!thisObject)
        return throwVMTypeError(globalObject, scope, "Receiver should be an array buffer"_s);
    if (thisObject->isShared())
        return throwVMTypeError(globalObject, scope, "Receiver should not be a shared array buffer"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(thisObject->impl()->byteLength())));
}

// http://tc39.github.io/ecmascript_sharedmem/shmem.html#StructuredData.SharedArrayBuffer.prototype.get_byteLength
static EncodedJSValue JSC_HOST_CALL sharedArrayBufferProtoGetterFuncByteLength(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Receiver should be an array buffer but was not an object"_s);

    auto* thisObject = jsDynamicCast<JSArrayBuffer*>(vm, thisValue);
    if (!thisObject)
        return throwVMTypeError(globalObject, scope, "Receiver should be an array buffer"_s);
    if (!thisObject->isShared())
        return throwVMTypeError(globalObject, scope, "Receiver should be a shared array buffer"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(thisObject->impl()->byteLength())));
}

const ClassInfo JSArrayBufferPrototype::s_info = {
    "ArrayBufferPrototype", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArrayBufferPrototype)
};

JSArrayBufferPrototype::JSArrayBufferPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSArrayBufferPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject, ArrayBufferSharingMode sharingMode)
{
    Base::finishCreation(vm);
    
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayBufferProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(vm, arrayBufferSharingModeName(sharingMode)), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    if (sharingMode == ArrayBufferSharingMode::Default)
        JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, arrayBufferProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    else
        JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->byteLength, sharedArrayBufferProtoGetterFuncByteLength, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

JSArrayBufferPrototype* JSArrayBufferPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, ArrayBufferSharingMode sharingMode)
{
    JSArrayBufferPrototype* prototype =
        new (NotNull, allocateCell<JSArrayBufferPrototype>(vm.heap))
        JSArrayBufferPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject, sharingMode);
    return prototype;
}

Structure* JSArrayBufferPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC

