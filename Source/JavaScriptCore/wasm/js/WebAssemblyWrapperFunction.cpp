/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "CallLinkInfo.h"
#include "IteratorOperations.h"
#include "JSArray.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntData.h"
#include "ObjectConstructor.h"
#include "WasmTypeDefinitionInlines.h"

namespace JSC {

const ClassInfo WebAssemblyWrapperFunction::s_info = { "WebAssemblyWrapperFunction"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyWrapperFunction) };

static JSC_DECLARE_HOST_FUNCTION(callWebAssemblyWrapperFunction);
static JSC_DECLARE_HOST_FUNCTION(callWebAssemblyWrapperFunctionIncludingInvalidValues);
static JSC_DECLARE_HOST_FUNCTION(callWebAssemblyJSFunction);

WebAssemblyWrapperFunction::WebAssemblyWrapperFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSObject* function, Wasm::WasmOrJSImportableFunction&& importableFunction, WasmOrJSImportableFunctionCallLinkInfo* callLinkInfo)
    : Base(vm, executable, globalObject, structure, WTF::move(importableFunction), callLinkInfo)
    , m_function(function, WriteBarrierEarlyInit)
{ }

WebAssemblyWrapperFunction* WebAssemblyWrapperFunction::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* function, unsigned importIndex, JSWebAssemblyInstance* instance, Ref<const Wasm::RTT>&& signature)
{
    ASSERT_WITH_MESSAGE(!function->inherits<WebAssemblyWrapperFunction>(), "We should never double wrap a wrapper function.");

    String name = emptyString();
    NativeExecutable* executable = nullptr;
    unsigned length = signature->argumentCount();
    if (signature->argumentsOrResultsIncludeV128() || signature->argumentsOrResultsIncludeExnref()) [[unlikely]]
        executable = vm.getHostFunction(callWebAssemblyWrapperFunctionIncludingInvalidValues, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, length, name);
    else
        executable = vm.getHostFunction(callWebAssemblyWrapperFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, length, name);

    RELEASE_ASSERT(JSValue(function).isCallable());
    WebAssemblyWrapperFunction* result = new (NotNull, allocateCell<WebAssemblyWrapperFunction>(vm)) WebAssemblyWrapperFunction(vm, executable, globalObject, structure, function,
        Wasm::WasmOrJSImportableFunction {
            {
                {
                    CalleeBits::encodeNativeCallee(&Wasm::WasmToJSCallee::singleton()),
                    { instance, WriteBarrierEarlyInit },
                    &instance->importFunctionInfo(importIndex)->importFunctionStub
                },
                signature.ptr()
            },
            { },
            { }
        },
        instance->importFunctionInfo(importIndex));
    result->m_importableFunction.importFunction.set(vm, globalObject, function);
    result->finishCreation(vm);
    return result;
}

WebAssemblyWrapperFunction* WebAssemblyWrapperFunction::createFromJSFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure, JSObject* function, Ref<const Wasm::RTT>&& signature)
{
    ASSERT_WITH_MESSAGE(!function->inherits<WebAssemblyWrapperFunction>(), "We should never double wrap a wrapper function.");

    String name = emptyString();
    NativeExecutable* executable = nullptr;
    unsigned length = signature->argumentCount();
    if (signature->argumentsOrResultsIncludeV128() || signature->argumentsOrResultsIncludeExnref()) [[unlikely]]
        executable = vm.getHostFunction(callWebAssemblyWrapperFunctionIncludingInvalidValues, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, length, name);
    else
        executable = vm.getHostFunction(callWebAssemblyJSFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, length, name);

    RELEASE_ASSERT(JSValue(function).isCallable());
    auto ownedCallLinkInfo = makeUnique<Wasm::WasmOrJSImportableFunctionCallLinkInfo>();
    ownedCallLinkInfo->boxedCallee = CalleeBits::encodeNativeCallee(&Wasm::WasmToJSCallee::singleton());
    ownedCallLinkInfo->entrypointLoadLocation = &ownedCallLinkInfo->importFunctionStub;
    ownedCallLinkInfo->rtt = signature.ptr();
    ownedCallLinkInfo->importFunctionStub = LLInt::getCodeRef<WasmEntryPtrTag>(wasm_to_js_wrapper_entry).code();
    ownedCallLinkInfo->importFunction.set(vm, globalObject, function);
    auto callLinkInfo = makeUnique<DataOnlyCallLinkInfo>();
    callLinkInfo->initialize(vm, nullptr, CallLinkInfo::CallType::Call, CodeOrigin { });
    WTF::storeStoreFence();
    ownedCallLinkInfo->callLinkInfo = WTF::move(callLinkInfo);

    Wasm::WasmOrJSImportableFunction importable = *ownedCallLinkInfo;
    WebAssemblyWrapperFunction* result = new (NotNull, allocateCell<WebAssemblyWrapperFunction>(vm)) WebAssemblyWrapperFunction(vm, executable, globalObject, structure, function, WTF::move(importable), ownedCallLinkInfo.get());
    result->m_ownedRTT = WTF::move(signature);
    result->m_ownedCallLinkInfo = WTF::move(ownedCallLinkInfo);
    result->finishCreation(vm);
    vm.writeBarrier(result);
    return result;
}

Structure* WebAssemblyWrapperFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

template<typename Visitor>
void WebAssemblyWrapperFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WebAssemblyWrapperFunction* thisObject = uncheckedDowncast<WebAssemblyWrapperFunction>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_function);
}

DEFINE_VISIT_CHILDREN(WebAssemblyWrapperFunction);

void WebAssemblyWrapperFunction::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* wrapper = static_cast<WebAssemblyWrapperFunction*>(cell);
    wrapper->clearJSCallICs(wrapper->vm());
    wrapper->WebAssemblyWrapperFunction::~WebAssemblyWrapperFunction();
}

void WebAssemblyWrapperFunction::clearJSCallICs(VM& vm)
{
    if (auto* owned = m_ownedCallLinkInfo.get()) {
        if (auto* callLinkInfo = owned->callLinkInfo.get())
            callLinkInfo->unlinkOrUpgrade(vm, nullptr, nullptr);
    }
}

JSC_DEFINE_HOST_FUNCTION(callWebAssemblyWrapperFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    WebAssemblyWrapperFunction* wasmFunction = uncheckedDowncast<WebAssemblyWrapperFunction>(callFrame->jsCallee());
    JSObject* function = wasmFunction->function();
    auto callData = JSC::getCallDataInline(function);
    RELEASE_ASSERT(callData.type != CallData::Type::None);
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, function, callData, jsUndefined(), ArgList(callFrame))));
}

JSC_DEFINE_HOST_FUNCTION(callWebAssemblyWrapperFunctionIncludingInvalidValues, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, Wasm::errorMessageForExceptionType(Wasm::ExceptionType::TypeErrorInvalidValueUse));
}

JSC_DEFINE_HOST_FUNCTION(callWebAssemblyJSFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* wasmFunction = uncheckedDowncast<WebAssemblyWrapperFunction>(callFrame->jsCallee());
    JSObject* function = wasmFunction->function();
    RefPtr<const Wasm::RTT> signature = wasmFunction->rtt();
    RELEASE_ASSERT(signature);

    MarkedArgumentBuffer args;
    args.ensureCapacity(signature->argumentCount());
    for (unsigned i = 0; i < signature->argumentCount(); ++i) {
        uint64_t bits = toWebAssemblyValue(globalObject, signature->argumentType(i), callFrame->argument(i));
        RETURN_IF_EXCEPTION(scope, { });
        args.append(toJSValue(globalObject, signature->argumentType(i), bits));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (args.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallDataInline(function);
    RELEASE_ASSERT(callData.type != CallData::Type::None);
    JSValue result = call(globalObject, function, callData, jsUndefined(), args);
    RETURN_IF_EXCEPTION(scope, { });

    if (!signature->returnCount())
        return JSValue::encode(jsUndefined());

    if (signature->returnCount() == 1) {
        uint64_t bits = toWebAssemblyValue(globalObject, signature->returnType(0), result);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(toJSValue(globalObject, signature->returnType(0), bits)));
    }

    unsigned iterationCount = 0;
    MarkedArgumentBuffer jsValues;
    jsValues.ensureCapacity(signature->returnCount());
    forEachInIterable(globalObject, result, [&](VM&, JSGlobalObject*, JSValue value) {
        if (jsValues.size() < signature->returnCount())
            jsValues.append(value);
        ++iterationCount;
    });
    RETURN_IF_EXCEPTION(scope, { });
    if (jsValues.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    if (iterationCount != signature->returnCount())
        return throwVMTypeError(globalObject, scope, "Incorrect number of values returned to Wasm from JS"_s);

    MarkedArgumentBuffer converted;
    converted.ensureCapacity(signature->returnCount());
    for (unsigned i = 0; i < signature->returnCount(); ++i) {
        uint64_t bits = toWebAssemblyValue(globalObject, signature->returnType(i), jsValues.at(i));
        RETURN_IF_EXCEPTION(scope, { });
        converted.append(toJSValue(globalObject, signature->returnType(i), bits));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (converted.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), converted)));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
