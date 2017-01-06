/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSFunctionInlines.h"
#include "JSObject.h"
#include "JSWebAssemblyCallee.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "LLIntThunks.h"
#include "ProtoCallFrame.h"
#include "VM.h"
#include "WasmFormat.h"
#include "WasmMemory.h"

namespace JSC {

const ClassInfo WebAssemblyFunction::s_info = { "WebAssemblyFunction", &Base::s_info, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunction) };

static EncodedJSValue JSC_HOST_CALL callWebAssemblyFunction(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    WebAssemblyFunction* wasmFunction = jsDynamicCast<WebAssemblyFunction*>(exec->jsCallee());
    if (!wasmFunction)
        return JSValue::encode(throwException(exec, scope, createTypeError(exec, "expected a WebAssembly function", defaultSourceAppender, runtimeTypeForValue(exec->jsCallee()))));
    Wasm::SignatureIndex signatureIndex = wasmFunction->signatureIndex();
    const Wasm::Signature* signature = Wasm::SignatureInformation::get(&vm, signatureIndex);

    // FIXME is this the right behavior? https://bugs.webkit.org/show_bug.cgi?id=164876
    if (exec->argumentCount() != signature->argumentCount())
        return JSValue::encode(throwException(exec, scope, createNotEnoughArgumentsError(exec, defaultSourceAppender)));

    // FIXME is this boxing correct? https://bugs.webkit.org/show_bug.cgi?id=164876
    Vector<JSValue> boxedArgs;
    for (unsigned argIndex = 0; argIndex < exec->argumentCount(); ++argIndex) {
        JSValue arg = exec->uncheckedArgument(argIndex);
        switch (signature->argument(argIndex)) {
        case Wasm::I32:
            arg = JSValue::decode(arg.toInt32(exec));
            break;
        case Wasm::F32:
            arg = JSValue::decode(bitwise_cast<uint32_t>(arg.toFloat(exec)));
            break;
        case Wasm::F64:
            arg = JSValue::decode(bitwise_cast<uint64_t>(arg.toNumber(exec)));
            break;
        case Wasm::Void:
        case Wasm::I64:
        case Wasm::Func:
        case Wasm::Anyfunc:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        boxedArgs.append(arg);
    }

    JSValue firstArgument = JSValue();
    int argCount = 1;
    JSValue* remainingArgs = nullptr;
    if (boxedArgs.size()) {
        remainingArgs = boxedArgs.data();
        firstArgument = *remainingArgs;
        remainingArgs++;
        argCount = boxedArgs.size();
    }

    // Note: we specifically use the WebAssemblyFunction as the callee to begin with in the ProtoCallFrame.
    // The reason for this is that calling into the llint may stack overflow, and the stack overflow
    // handler might read the global object from the callee. The JSWebAssemblyCallee doesn't have a
    // global object, but the WebAssemblyFunction does.
    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, wasmFunction, firstArgument, argCount, remainingArgs);

    return wasmFunction->call(vm, &protoCallFrame);
}

EncodedJSValue WebAssemblyFunction::call(VM& vm, ProtoCallFrame* protoCallFrame)
{
    // Setup the memory that the entrance loads.
    if (JSWebAssemblyMemory* memory = instance()->memory()) {
        Wasm::Memory* wasmMemory = memory->memory();
        vm.topWasmMemoryPointer = wasmMemory->memory();
        vm.topWasmMemorySize = wasmMemory->size();
    } else {
        vm.topWasmMemoryPointer = nullptr;
        vm.topWasmMemorySize = 0;
    }

    JSWebAssemblyInstance* prevJSWebAssemblyInstance = vm.topJSWebAssemblyInstance;
    vm.topJSWebAssemblyInstance = instance();
    ASSERT(instance());
    EncodedJSValue rawResult = vmEntryToWasm(m_jsEntrypoint->entrypoint(), &vm, protoCallFrame);
    vm.topJSWebAssemblyInstance = prevJSWebAssemblyInstance;

    // FIXME is this correct? https://bugs.webkit.org/show_bug.cgi?id=164876
    switch (m_returnType) {
    case Wasm::Void:
        return JSValue::encode(jsUndefined());
    case Wasm::I32:
        return JSValue::encode(JSValue(static_cast<int32_t>(rawResult)));
    case Wasm::F32:
        return JSValue::encode(JSValue(bitwise_cast<float>(static_cast<int32_t>(rawResult))));
    case Wasm::F64:
        return JSValue::encode(JSValue(bitwise_cast<double>(rawResult)));
    case Wasm::I64:
    case Wasm::Func:
    case Wasm::Anyfunc:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return EncodedJSValue();
}

WebAssemblyFunction* WebAssemblyFunction::create(VM& vm, JSGlobalObject* globalObject, unsigned length, const String& name, JSWebAssemblyInstance* instance, JSWebAssemblyCallee* jsEntrypoint, JSWebAssemblyCallee* wasmEntrypoint, Wasm::SignatureIndex signatureIndex)
{
    NativeExecutable* executable = vm.getHostFunction(callWebAssemblyFunction, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    Structure* structure = globalObject->webAssemblyFunctionStructure();
    WebAssemblyFunction* function = new (NotNull, allocateCell<WebAssemblyFunction>(vm.heap)) WebAssemblyFunction(vm, globalObject, structure, signatureIndex);
    function->finishCreation(vm, executable, length, name, instance, jsEntrypoint, wasmEntrypoint);
    return function;
}

Structure* WebAssemblyFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

WebAssemblyFunction::WebAssemblyFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure, Wasm::SignatureIndex signatureIndex)
    : Base(vm, globalObject, structure)
    , m_signatureIndex(signatureIndex)
{
    // Don't cache the signature pointer: it's a global on VM and can change as new WebAssembly.Module are created.
    const Wasm::Signature* signature = Wasm::SignatureInformation::get(&vm, m_signatureIndex);
    m_returnType = signature->returnType();
}

void WebAssemblyFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyFunction* thisObject = jsCast<WebAssemblyFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_instance);
    visitor.append(thisObject->m_jsEntrypoint);
    visitor.append(thisObject->m_wasmEntrypoint);
}

void WebAssemblyFunction::finishCreation(VM& vm, NativeExecutable* executable, unsigned length, const String& name, JSWebAssemblyInstance* instance, JSWebAssemblyCallee* jsEntrypoint, JSWebAssemblyCallee* wasmEntrypoint)
{
    Base::finishCreation(vm, executable, length, name);
    ASSERT(inherits(info()));
    m_instance.set(vm, this, instance);
    ASSERT(jsEntrypoint != wasmEntrypoint);
    m_jsEntrypoint.set(vm, this, jsEntrypoint);
    m_wasmEntrypoint.set(vm, this, wasmEntrypoint);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
