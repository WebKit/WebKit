/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "WebAssemblyWrapperFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "WasmSignatureInlines.h"

namespace JSC {

const ClassInfo WebAssemblyWrapperFunction::s_info = { "WebAssemblyWrapperFunction", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyWrapperFunction) };

static EncodedJSValue JSC_HOST_CALL callWebAssemblyWrapperFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    WebAssemblyWrapperFunction* wasmFunction = jsCast<WebAssemblyWrapperFunction*>(callFrame->jsCallee());
    CallData callData;
    JSObject* function = wasmFunction->function();
    CallType callType = function->methodTable(vm)->getCallData(function, callData);
    RELEASE_ASSERT(callType != CallType::None);
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, function, callType, callData, jsUndefined(), ArgList(callFrame))));
}

WebAssemblyWrapperFunction::WebAssemblyWrapperFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, Wasm::WasmToWasmImportableFunction importableFunction)
    : Base(vm, executable, globalObject, structure)
    , m_importableFunction(importableFunction)
{ }

WebAssemblyWrapperFunction* WebAssemblyWrapperFunction::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* function, unsigned importIndex, JSWebAssemblyInstance* instance, Wasm::SignatureIndex signatureIndex)
{
    ASSERT_WITH_MESSAGE(!function->inherits<WebAssemblyWrapperFunction>(vm), "We should never double wrap a wrapper function.");
    String name = "";
    NativeExecutable* executable = vm.getHostFunction(callWebAssemblyWrapperFunction, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    WebAssemblyWrapperFunction* result = new (NotNull, allocateCell<WebAssemblyWrapperFunction>(vm.heap)) WebAssemblyWrapperFunction(vm, executable, globalObject, structure, Wasm::WasmToWasmImportableFunction { signatureIndex, &instance->instance().importFunctionInfo(importIndex)->wasmToEmbedderStub } );
    const Wasm::Signature& signature = Wasm::SignatureInformation::get(signatureIndex);
    result->finishCreation(vm, executable, signature.argumentCount(), name, function, instance);
    return result;
}

void WebAssemblyWrapperFunction::finishCreation(VM& vm, NativeExecutable* executable, unsigned length, const String& name, JSObject* function, JSWebAssemblyInstance* instance)
{
    Base::finishCreation(vm, executable, length, name, instance);
    RELEASE_ASSERT(JSValue(function).isFunction(vm));
    m_function.set(vm, this, function);
}

Structure* WebAssemblyWrapperFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

void WebAssemblyWrapperFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyWrapperFunction* thisObject = jsCast<WebAssemblyWrapperFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_function);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
