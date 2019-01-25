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
#include "Error.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferPrototype.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL arrayBufferFuncIsView(ExecState*);
static EncodedJSValue JSC_HOST_CALL callArrayBuffer(ExecState*);

template<>
const ClassInfo JSArrayBufferConstructor::s_info = {
    "Function", &Base::s_info, nullptr, nullptr,
    CREATE_METHOD_TABLE(JSArrayBufferConstructor)
};

template<>
const ClassInfo JSSharedArrayBufferConstructor::s_info = {
    "Function", &Base::s_info, nullptr, nullptr,
    CREATE_METHOD_TABLE(JSSharedArrayBufferConstructor)
};

template<ArrayBufferSharingMode sharingMode>
JSGenericArrayBufferConstructor<sharingMode>::JSGenericArrayBufferConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callArrayBuffer, JSGenericArrayBufferConstructor<sharingMode>::constructArrayBuffer)
{
}

template<ArrayBufferSharingMode sharingMode>
void JSGenericArrayBufferConstructor<sharingMode>::finishCreation(VM& vm, JSArrayBufferPrototype* prototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, arrayBufferSharingModeName(sharingMode));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessor(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    if (sharingMode == ArrayBufferSharingMode::Default) {
        JSGlobalObject* globalObject = this->globalObject(vm);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->isView, arrayBufferFuncIsView, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().isViewPrivateName(), arrayBufferFuncIsView, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    }
}

template<ArrayBufferSharingMode sharingMode>
EncodedJSValue JSC_HOST_CALL JSGenericArrayBufferConstructor<sharingMode>::constructArrayBuffer(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGenericArrayBufferConstructor* constructor = jsCast<JSGenericArrayBufferConstructor*>(exec->jsCallee());

    Structure* arrayBufferStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), constructor->globalObject(vm)->arrayBufferStructure(sharingMode));
    RETURN_IF_EXCEPTION(scope, { });

    unsigned length;
    if (exec->argumentCount()) {
        length = exec->uncheckedArgument(0).toIndex(exec, "length");
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    } else {
        // Although the documentation doesn't say so, it is in fact correct to say
        // "new ArrayBuffer()". The result is the same as allocating an array buffer
        // with a zero length.
        length = 0;
    }

    auto buffer = ArrayBuffer::tryCreate(length, 1);
    if (!buffer)
        return JSValue::encode(throwOutOfMemoryError(exec, scope));
    
    if (sharingMode == ArrayBufferSharingMode::Shared)
        buffer->makeShared();
    ASSERT(sharingMode == buffer->sharingMode());

    JSArrayBuffer* result = JSArrayBuffer::create(vm, arrayBufferStructure, WTFMove(buffer));
    return JSValue::encode(result);
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

static EncodedJSValue JSC_HOST_CALL callArrayBuffer(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, scope, "ArrayBuffer"));
}

// ------------------------------ Functions --------------------------------

// ECMA 24.1.3.1
EncodedJSValue JSC_HOST_CALL arrayBufferFuncIsView(ExecState* exec)
{
    return JSValue::encode(jsBoolean(jsDynamicCast<JSArrayBufferView*>(exec->vm(), exec->argument(0))));
}

// Instantiate JSGenericArrayBufferConstructors.
template class JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Shared>;
template class JSGenericArrayBufferConstructor<ArrayBufferSharingMode::Default>;

} // namespace JSC

