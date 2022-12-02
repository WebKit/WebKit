/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSArrayBufferConstructor.h"

#include "BuiltinNames.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferPrototype.h"
#include "JSArrayBufferView.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(arrayBufferFuncIsView);
static JSC_DECLARE_HOST_FUNCTION(callArrayBuffer);
static JSC_DECLARE_HOST_FUNCTION(constructArrayBuffer);
static JSC_DECLARE_HOST_FUNCTION(constructSharedArrayBuffer);

template<>
const ClassInfo JSArrayBufferConstructor::s_info = {
    "Function"_s, &Base::s_info, nullptr, nullptr,
    CREATE_METHOD_TABLE(JSArrayBufferConstructor)
};

template<>
const ClassInfo JSSharedArrayBufferConstructor::s_info = {
    "Function"_s, &Base::s_info, nullptr, nullptr,
    CREATE_METHOD_TABLE(JSSharedArrayBufferConstructor)
};

template<ArrayBufferSharingMode sharingMode>
JSGenericArrayBufferConstructor<sharingMode>::JSGenericArrayBufferConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callArrayBuffer, sharingMode == ArrayBufferSharingMode::Default ? constructArrayBuffer : constructSharedArrayBuffer)
{
}

template<ArrayBufferSharingMode sharingMode>
void JSGenericArrayBufferConstructor<sharingMode>::finishCreation(VM& vm, JSArrayBufferPrototype* prototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, 1, arrayBufferSharingModeName(sharingMode), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    if (sharingMode == ArrayBufferSharingMode::Default) {
        JSGlobalObject* globalObject = this->globalObject();
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->isView, arrayBufferFuncIsView, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().isViewPrivateName(), arrayBufferFuncIsView, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }
}

template<ArrayBufferSharingMode sharingMode>
EncodedJSValue JSGenericArrayBufferConstructor<sharingMode>::constructImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, arrayBufferStructureWithSharingMode<sharingMode>, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = 0;
    std::optional<size_t> maxByteLength;
    if (callFrame->argumentCount()) {
        length = callFrame->uncheckedArgument(0).toTypedArrayIndex(globalObject, "length"_s);
        RETURN_IF_EXCEPTION(scope, { });

        if (Options::useResizableArrayBuffer()) {
            JSValue options = callFrame->argument(1);
            if (options.isObject()) {
                JSValue maxByteLengthValue = asObject(options)->get(globalObject, vm.propertyNames->maxByteLength);
                RETURN_IF_EXCEPTION(scope, { });
                if (!maxByteLengthValue.isUndefined()) {
                    maxByteLength = maxByteLengthValue.toTypedArrayIndex(globalObject, "maxByteLength"_s);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }
    }

    // https://tc39.es/proposal-resizablearraybuffer/#sec-allocatesharedarraybuffer
    RefPtr<ArrayBuffer> buffer;
    if (maxByteLength) {
        if (maxByteLength.value() < length)
            return throwVMRangeError(globalObject, scope, "ArrayBuffer length exceeds maxByteLength option"_s);
        if constexpr (sharingMode == ArrayBufferSharingMode::Shared) {
            buffer = ArrayBuffer::tryCreateShared(vm, length, 1, maxByteLength.value());
            if (!buffer)
                return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
        }
    }

    if (!buffer) {
        buffer = ArrayBuffer::tryCreate(length, 1, maxByteLength);
        if (!buffer)
            return JSValue::encode(throwOutOfMemoryError(globalObject, scope));
        if constexpr (sharingMode == ArrayBufferSharingMode::Shared)
            buffer->makeShared();
    }

    ASSERT(sharingMode == buffer->sharingMode());

    return JSValue::encode(JSArrayBuffer::create(vm, structure, WTFMove(buffer)));
}

template<ArrayBufferSharingMode sharingMode>
Structure* JSGenericArrayBufferConstructor<sharingMode>::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

template<ArrayBufferSharingMode sharingMode>
const ClassInfo* JSGenericArrayBufferConstructor<sharingMode>::info()
{
    return &JSGenericArrayBufferConstructor<sharingMode>::s_info;
}

JSC_DEFINE_HOST_FUNCTION(callArrayBuffer, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "ArrayBuffer"));
}

JSC_DEFINE_HOST_FUNCTION(constructArrayBuffer, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Default>::constructImpl(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(constructSharedArrayBuffer, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Shared>::constructImpl(globalObject, callFrame);
}

// ------------------------------ Functions --------------------------------

// ECMA 24.1.3.1
JSC_DEFINE_HOST_FUNCTION(arrayBufferFuncIsView, (JSGlobalObject*, CallFrame* callFrame))
{
    return JSValue::encode(jsBoolean(jsDynamicCast<JSArrayBufferView*>(callFrame->argument(0))));
}

// Instantiate JSGenericArrayBufferConstructors.
template class JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Shared>;
template class JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Default>;

} // namespace JSC

