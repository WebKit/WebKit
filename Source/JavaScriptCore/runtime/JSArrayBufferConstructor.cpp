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
#include "JSArrayBufferConstructor.h"

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferPrototype.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL arrayBufferFuncIsView(ExecState*);

const ClassInfo JSArrayBufferConstructor::s_info = {
    "Function", &Base::s_info, 0, 0,
    CREATE_METHOD_TABLE(JSArrayBufferConstructor)
};

JSArrayBufferConstructor::JSArrayBufferConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSArrayBufferConstructor::finishCreation(VM& vm, JSArrayBufferPrototype* prototype)
{
    Base::finishCreation(vm, "ArrayBuffer");
    putDirectPrototypePropertyWithoutTransitions(vm, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), DontEnum | DontDelete | ReadOnly);

    JSGlobalObject* globalObject = this->globalObject();
    JSC_NATIVE_FUNCTION(vm.propertyNames->isView, arrayBufferFuncIsView, DontEnum, 1);
}

JSArrayBufferConstructor* JSArrayBufferConstructor::create(VM& vm, Structure* structure, JSArrayBufferPrototype* prototype)
{
    JSArrayBufferConstructor* result =
        new (NotNull, allocateCell<JSArrayBufferConstructor>(vm.heap))
        JSArrayBufferConstructor(vm, structure);
    result->finishCreation(vm, prototype);
    return result;
}

Structure* JSArrayBufferConstructor::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL constructArrayBuffer(ExecState* exec)
{
    JSArrayBufferConstructor* constructor =
        jsCast<JSArrayBufferConstructor*>(exec->callee());
    
    unsigned length;
    if (exec->argumentCount()) {
        length = exec->uncheckedArgument(0).toUInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    } else {
        // Although the documentation doesn't say so, it is in fact correct to say
        // "new ArrayBuffer()". The result is the same as allocating an array buffer
        // with a zero length.
        length = 0;
    }
    
    RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(length, 1);
    if (!buffer)
        return throwVMError(exec, createOutOfMemoryError(constructor->globalObject()));
    
    JSArrayBuffer* result = JSArrayBuffer::create(
        exec->vm(), constructor->globalObject()->arrayBufferStructure(), buffer);
    
    return JSValue::encode(result);
}

ConstructType JSArrayBufferConstructor::getConstructData(
    JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructArrayBuffer;
    return ConstructTypeHost;
}

CallType JSArrayBufferConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructArrayBuffer;
    return CallTypeHost;
}

// ------------------------------ Functions --------------------------------

// ECMA 24.1.3.1
EncodedJSValue JSC_HOST_CALL arrayBufferFuncIsView(ExecState* exec)
{
    return JSValue::encode(jsBoolean(jsDynamicCast<JSArrayBufferView*>(exec->argument(0))));
}
    

} // namespace JSC

