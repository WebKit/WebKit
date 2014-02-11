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
#include "JSArrayBufferPrototype.h"

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBuffer.h"
#include "JSFunction.h"
#include "JSCInlines.h"
#include "TypedArrayAdaptors.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL arrayBufferProtoFuncSlice(ExecState* exec)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    
    JSArrayBuffer* thisObject = jsDynamicCast<JSArrayBuffer*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of slice must be an array buffer."));
    
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Slice requires at least one argument."));
    
    int32_t begin = exec->argument(0).toInt32(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    int32_t end;
    if (exec->argumentCount() >= 2) {
        end = exec->uncheckedArgument(1).toInt32(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    } else
        end = thisObject->impl()->byteLength();
    
    RefPtr<ArrayBuffer> newBuffer = thisObject->impl()->slice(begin, end);
    if (!newBuffer)
        return throwVMError(exec, createOutOfMemoryError(callee->globalObject()));
    
    Structure* structure = callee->globalObject()->arrayBufferStructure();
    
    JSArrayBuffer* result = JSArrayBuffer::create(exec->vm(), structure, newBuffer);
    
    return JSValue::encode(result);
}

const ClassInfo JSArrayBufferPrototype::s_info = {
    "ArrayBufferPrototype", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSArrayBufferPrototype)
};

JSArrayBufferPrototype::JSArrayBufferPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSArrayBufferPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    
    JSC_NATIVE_FUNCTION(vm.propertyNames->slice, arrayBufferProtoFuncSlice, DontEnum, 2);
}

JSArrayBufferPrototype* JSArrayBufferPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSArrayBufferPrototype* prototype =
        new (NotNull, allocateCell<JSArrayBufferPrototype>(vm.heap))
        JSArrayBufferPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

Structure* JSArrayBufferPrototype::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC

