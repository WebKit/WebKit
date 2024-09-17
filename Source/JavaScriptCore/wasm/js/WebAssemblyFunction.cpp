/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "WebAssemblyFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "JITOpaqueByproducts.h"
#include "JSCJSValueInlines.h"
#include "JSObject.h"
#include "JSObjectInlines.h"
#include "JSToWasm.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyRuntimeError.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "NativeExecutable.h"
#include "ProtoCallFrameInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmMemoryInformation.h"
#include "WasmModuleInformation.h"
#include "WasmOperations.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/StackPointer.h>
#include <wtf/SystemTracing.h>

namespace JSC {

const ClassInfo WebAssemblyFunction::s_info = { "WebAssemblyFunction"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunction) };

static JSC_DECLARE_HOST_FUNCTION(callWebAssemblyFunction);

JSC_DEFINE_HOST_FUNCTION(callWebAssemblyFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    WebAssemblyFunction* wasmFunction = jsCast<WebAssemblyFunction*>(callFrame->jsCallee());

    // Note: we specifically use the WebAssemblyFunction as the callee to begin with in the ProtoCallFrame.
    // The reason for this is that calling into the llint may stack overflow, and the stack overflow
    // handler might read the global object from the callee.
    // For wasm, we setup |codeBlock| and |this| differently.
    //     |codeBlock| : JS entry wrapper expects a JSWebAssemblyInstance* as the |codeBlock| value argument.
    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, globalObject, wasmFunction, JSValue(), callFrame->argumentCountIncludingThis(), bitwise_cast<EncodedJSValue*>(callFrame->addressOfArgumentsStart()));
    protoCallFrame.setWasmInstance(wasmFunction->instance());

    return vmEntryToWasm(wasmFunction->jsEntrypoint(MustCheckArity).taggedPtr(), &vm, &protoCallFrame);
}

WebAssemblyFunction* WebAssemblyFunction::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, unsigned length, const String& name, JSWebAssemblyInstance* instance, Wasm::JSEntrypointCallee& jsEntrypoint, Wasm::Callee* wasmCallee, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::TypeIndex typeIndex, RefPtr<const Wasm::RTT> rtt)
{
    NativeExecutable* base = vm.getHostFunction(callWebAssemblyFunction, ImplementationVisibility::Public, WasmFunctionIntrinsic, callHostFunctionAsConstructor, nullptr, String());
    // Since ClosureCall uses this executable as an identity for Wasm CallIC thunk, we need to make it diversified.
    NativeExecutable* executable = NativeExecutable::create(vm, base->generatedJITCodeForCall(), callWebAssemblyFunction, base->generatedJITCodeForConstruct(), callHostFunctionAsConstructor, ImplementationVisibility::Public, name);
    WebAssemblyFunction* function = new (NotNull, allocateCell<WebAssemblyFunction>(vm)) WebAssemblyFunction(vm, executable, globalObject, structure, instance, jsEntrypoint, wasmCallee, wasmToWasmEntrypointLoadLocation, typeIndex, rtt);
    function->finishCreation(vm, executable, length, name);
    return function;
}

Structure* WebAssemblyFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

WebAssemblyFunction::WebAssemblyFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSWebAssemblyInstance* instance, Wasm::JSEntrypointCallee& jsEntrypoint, Wasm::Callee* wasmCallee, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::TypeIndex typeIndex, RefPtr<const Wasm::RTT> rtt)
    : Base { vm, executable, globalObject, structure, instance, Wasm::WasmToWasmImportableFunction { typeIndex, wasmToWasmEntrypointLoadLocation, &m_boxedWasmCallee, rtt.get() } }
    , m_boxedWasmCallee(reinterpret_cast<uint64_t>(CalleeBits::boxNativeCalleeIfExists(wasmCallee)))
    , m_jsToWasmCallee { jsEntrypoint }
{ }

template<typename Visitor>
void WebAssemblyFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WebAssemblyFunction* thisObject = jsCast<WebAssemblyFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(WebAssemblyFunction);

void WebAssemblyFunction::destroy(JSCell* cell)
{
    static_cast<WebAssemblyFunction*>(cell)->WebAssemblyFunction::~WebAssemblyFunction();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
