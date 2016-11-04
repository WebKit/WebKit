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
#include "WebAssemblyModuleConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyModule.h"
#include "WasmPlan.h"
#include "WebAssemblyModulePrototype.h"

#include "WebAssemblyModuleConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyModuleConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyModule, CREATE_METHOD_TABLE(WebAssemblyModuleConstructor) };

/* Source for WebAssemblyModuleConstructor.lut.h
 @begin constructorTableWebAssemblyModule
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyModule(ExecState* state)
{
    auto& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue val = state->argument(0);

    // If the given bytes argument is not a BufferSource, a TypeError exception is thrown.
    JSArrayBuffer* arrayBuffer = val.getObject() ? jsDynamicCast<JSArrayBuffer*>(val.getObject()) : nullptr;
    JSArrayBufferView* arrayBufferView = val.getObject() ? jsDynamicCast<JSArrayBufferView*>(val.getObject()) : nullptr;
    if (!(arrayBuffer || arrayBufferView))
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("first argument to WebAssembly.Module must be an ArrayBufferView or an ArrayBuffer"), defaultSourceAppender, runtimeTypeForValue(val))));

    if (arrayBufferView ? arrayBufferView->isNeutered() : arrayBuffer->impl()->isNeutered())
        return JSValue::encode(throwException(state, scope, createTypeError(state, ASCIILiteral("underlying TypedArray has been detatched from the ArrayBuffer"), defaultSourceAppender, runtimeTypeForValue(val))));

    size_t byteOffset = arrayBufferView ? arrayBufferView->byteOffset() : 0;
    size_t byteSize = arrayBufferView ? arrayBufferView->length() : arrayBuffer->impl()->byteLength();
    const auto* base = arrayBufferView ? static_cast<uint8_t*>(arrayBufferView->vector()) : static_cast<uint8_t*>(arrayBuffer->impl()->data());

    Wasm::Plan plan(vm, base + byteOffset, byteSize);
    // On failure, a new WebAssembly.CompileError is thrown.
    if (plan.failed())
        return JSValue::encode(throwException(state, scope, createWebAssemblyCompileError(state, plan.errorMessage())));

    // On success, a new WebAssembly.Module object is returned with [[Module]] set to the validated Ast.module.
    auto* structure = InternalFunction::createSubclassStructure(state, state->newTarget(), asInternalFunction(state->callee())->globalObject()->WebAssemblyModuleStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(JSWebAssemblyModule::create(vm, structure, plan.getModuleInformation(), plan.getCompiledFunctions()));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyModule(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(state, scope, "WebAssembly.Module"));
}

WebAssemblyModuleConstructor* WebAssemblyModuleConstructor::create(VM& vm, Structure* structure, WebAssemblyModulePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyModuleConstructor>(vm.heap)) WebAssemblyModuleConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyModuleConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyModuleConstructor::finishCreation(VM& vm, WebAssemblyModulePrototype* prototype)
{
    Base::finishCreation(vm, ASCIILiteral("Module"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

WebAssemblyModuleConstructor::WebAssemblyModuleConstructor(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ConstructType WebAssemblyModuleConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructJSWebAssemblyModule;
    return ConstructType::Host;
}

CallType WebAssemblyModuleConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callJSWebAssemblyModule;
    return CallType::Host;
}

void WebAssemblyModuleConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<WebAssemblyModuleConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

