/*
 * Copyright (C) 2026 Sergey Rubanov <chi187@gmail.com>. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAssemblyJSFunctionPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunctionBase.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(webAssemblyJSFunctionProtoFuncType);

const ClassInfo WebAssemblyJSFunctionPrototype::s_info = { "WebAssembly.Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyJSFunctionPrototype) };

WebAssemblyJSFunctionPrototype* WebAssemblyJSFunctionPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyJSFunctionPrototype>(vm)) WebAssemblyJSFunctionPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyJSFunctionPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyJSFunctionPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("type"_s, webAssemblyJSFunctionProtoFuncType, static_cast<unsigned>(PropertyAttribute::None), 0, ImplementationVisibility::Public);
}

WebAssemblyJSFunctionPrototype::WebAssemblyJSFunctionPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyJSFunctionProtoFuncType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* wasmFunction = dynamicDowncast<WebAssemblyFunctionBase>(callFrame->thisValue());
    if (!wasmFunction)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function.prototype.type called on non-WebAssembly function"_s);

    RefPtr<const Wasm::RTT> signature = wasmFunction->rtt();
    if (!signature)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function.prototype.type unable to produce type descriptor"_s);

    MarkedArgumentBuffer parameterList;
    parameterList.ensureCapacity(signature->argumentCount());
    for (unsigned i = 0; i < signature->argumentCount(); ++i) {
        JSString* typeString = Wasm::typeToJSAPIString(vm, signature->argumentType(i));
        if (!typeString)
            return throwVMTypeError(globalObject, scope, "WebAssembly.Function.prototype.type unable to produce type descriptor"_s);
        parameterList.append(typeString);
    }

    MarkedArgumentBuffer resultList;
    resultList.ensureCapacity(signature->returnCount());
    for (unsigned i = 0; i < signature->returnCount(); ++i) {
        JSString* typeString = Wasm::typeToJSAPIString(vm, signature->returnType(i));
        if (!typeString)
            return throwVMTypeError(globalObject, scope, "WebAssembly.Function.prototype.type unable to produce type descriptor"_s);
        resultList.append(typeString);
    }

    if (parameterList.hasOverflowed() || resultList.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    JSArray* parameters = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), parameterList);
    RETURN_IF_EXCEPTION(scope, { });
    JSArray* results = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), resultList);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* type = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);
    type->putDirect(vm, Identifier::fromString(vm, "parameters"_s), parameters);
    type->putDirect(vm, Identifier::fromString(vm, "results"_s), results);
    return JSValue::encode(type);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
