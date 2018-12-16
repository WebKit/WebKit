/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Oleksandr Skachkov <gskachkov@gmail.com>.
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
#include "WebAssemblyPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "CatchScope.h"
#include "Exception.h"
#include "FunctionPrototype.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSPromiseDeferred.h"
#include "JSToWasm.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
#include "Options.h"
#include "PromiseDeferredTimer.h"
#include "StrongInlines.h"
#include "ThrowScope.h"
#include "WasmBBQPlan.h"
#include "WasmToJS.h"
#include "WasmWorklist.h"
#include "WebAssemblyInstanceConstructor.h"
#include "WebAssemblyModuleConstructor.h"

using JSC::Wasm::Plan;
using JSC::Wasm::BBQPlan;

namespace JSC {
static EncodedJSValue JSC_HOST_CALL webAssemblyCompileFunc(ExecState*);
static EncodedJSValue JSC_HOST_CALL webAssemblyInstantiateFunc(ExecState*);
static EncodedJSValue JSC_HOST_CALL webAssemblyValidateFunc(ExecState*);
}

#include "WebAssemblyPrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyPrototype::s_info = { "WebAssembly", &Base::s_info, &prototypeTableWebAssembly, nullptr, CREATE_METHOD_TABLE(WebAssemblyPrototype) };

/* Source for WebAssemblyPrototype.lut.h
 @begin prototypeTableWebAssembly
 compile     webAssemblyCompileFunc     DontEnum|Function 1
 instantiate webAssemblyInstantiateFunc DontEnum|Function 1
 validate    webAssemblyValidateFunc    DontEnum|Function 1
 @end
 */

static void reject(ExecState* exec, CatchScope& catchScope, JSPromiseDeferred* promise)
{
    Exception* exception = catchScope.exception();
    ASSERT(exception);
    catchScope.clearException();
    promise->reject(exec, exception->value());
    CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, void());
}

static void webAssemblyModuleValidateAsyncInternal(ExecState* exec, JSPromiseDeferred* promise, Vector<uint8_t>&& source)
{
    VM& vm = exec->vm();
    auto* globalObject = exec->lexicalGlobalObject();

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, globalObject));

    vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));

    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, globalObject, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, globalObject, result = WTFMove(result), &vm] () mutable {
            auto scope = DECLARE_CATCH_SCOPE(vm);
            ExecState* exec = globalObject->globalExec();
            JSValue module = JSWebAssemblyModule::createStub(vm, exec, globalObject->WebAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception())) {
                reject(exec, scope, promise);
                return;
            }

            promise->resolve(exec, module);
            CLEAR_AND_RETURN_IF_EXCEPTION(scope, void());
        });
    }));
}

static EncodedJSValue JSC_HOST_CALL webAssemblyCompileFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    JSPromiseDeferred* promise = JSPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        Vector<uint8_t> source = createSourceBufferFromValue(vm, exec, exec->argument(0));

        if (UNLIKELY(catchScope.exception()))
            reject(exec, catchScope, promise);
        else
            webAssemblyModuleValidateAsyncInternal(exec, promise, WTFMove(source));

        return JSValue::encode(promise->promise());
    }
}

enum class Resolve { WithInstance, WithModuleRecord, WithModuleAndInstance };
static void resolve(VM& vm, ExecState* exec, JSPromiseDeferred* promise, JSWebAssemblyInstance* instance, JSWebAssemblyModule* module, JSObject* importObject, Ref<Wasm::CodeBlock>&& codeBlock, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);
    instance->finalizeCreation(vm, exec, WTFMove(codeBlock), importObject, creationMode);
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    if (resolveKind == Resolve::WithInstance)
        promise->resolve(exec, instance);
    else if (resolveKind == Resolve::WithModuleRecord) {
        auto* moduleRecord = instance->moduleNamespaceObject()->moduleRecord();
        if (Options::dumpModuleRecord())
            moduleRecord->dump();
        promise->resolve(exec, moduleRecord);
    } else {
        JSObject* result = constructEmptyObject(exec);
        result->putDirect(vm, Identifier::fromString(&vm, "module"_s), module);
        result->putDirect(vm, Identifier::fromString(&vm, "instance"_s), instance);
        promise->resolve(exec, result);
    }
    CLEAR_AND_RETURN_IF_EXCEPTION(scope, void());
}

void WebAssemblyPrototype::webAssemblyModuleValidateAsync(ExecState* exec, JSPromiseDeferred* promise, Vector<uint8_t>&& source)
{
    VM& vm = exec->vm();
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    webAssemblyModuleValidateAsyncInternal(exec, promise, WTFMove(source));
    CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, void());
}

static void instantiate(VM& vm, ExecState* exec, JSPromiseDeferred* promise, JSWebAssemblyModule* module, JSObject* importObject, const Identifier& moduleKey, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);
    // In order to avoid potentially recompiling a module. We first gather all the import/memory information prior to compiling code.
    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::create(vm, exec, moduleKey, module, importObject, exec->lexicalGlobalObject()->WebAssemblyInstanceStructure(), Ref<Wasm::Module>(module->module()), creationMode);
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    Vector<Strong<JSCell>> dependencies;
    // The instance keeps the module alive.
    dependencies.append(Strong<JSCell>(vm, instance));
    dependencies.append(Strong<JSCell>(vm, importObject));
    vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));
    // Note: This completion task may or may not get called immediately.
    module->module().compileAsync(&vm.wasmContext, instance->memoryMode(), createSharedTask<Wasm::CodeBlock::CallbackType>([promise, instance, module, importObject, resolveKind, creationMode, &vm] (Ref<Wasm::CodeBlock>&& refCodeBlock) mutable {
        RefPtr<Wasm::CodeBlock> codeBlock = WTFMove(refCodeBlock);
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, instance, module, importObject, resolveKind, creationMode, &vm, codeBlock = WTFMove(codeBlock)] () mutable {
            ExecState* exec = instance->globalObject(vm)->globalExec();
            resolve(vm, exec, promise, instance, module, importObject, codeBlock.releaseNonNull(), resolveKind, creationMode);
        });
    }), &Wasm::createJSToWasmWrapper, &Wasm::wasmToJSException);
}

static void compileAndInstantiate(VM& vm, ExecState* exec, JSPromiseDeferred* promise, const Identifier& moduleKey, JSValue buffer, JSObject* importObject, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto* globalObject = exec->lexicalGlobalObject();

    JSCell* moduleKeyCell = identifierToJSValue(vm, moduleKey).asCell();
    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, importObject));
    dependencies.append(Strong<JSCell>(vm, moduleKeyCell));
    vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));

    Vector<uint8_t> source = createSourceBufferFromValue(vm, exec, buffer);
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, importObject, moduleKeyCell, globalObject, resolveKind, creationMode, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, importObject, moduleKeyCell, globalObject, result = WTFMove(result), resolveKind, creationMode, &vm] () mutable {
            auto scope = DECLARE_CATCH_SCOPE(vm);
            ExecState* exec = globalObject->globalExec();
            JSWebAssemblyModule* module = JSWebAssemblyModule::createStub(vm, exec, globalObject->WebAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception()))
                return reject(exec, scope, promise);

            const Identifier moduleKey = JSValue(moduleKeyCell).toPropertyKey(exec);
            if (UNLIKELY(scope.exception()))
                return reject(exec, scope, promise);

            instantiate(vm, exec, promise, module, importObject, moduleKey, resolveKind, creationMode);
        });
    }));
}

JSValue WebAssemblyPrototype::instantiate(ExecState* exec, JSPromiseDeferred* promise, const Identifier& moduleKey, JSValue argument)
{
    VM& vm = exec->vm();
    compileAndInstantiate(vm, exec, promise, moduleKey, argument, nullptr, Resolve::WithModuleRecord, Wasm::CreationMode::FromModuleLoader);
    return promise->promise();
}

static void webAssemblyModuleInstantinateAsyncInternal(ExecState* exec, JSPromiseDeferred* promise, Vector<uint8_t>&& source, JSObject* importObject)
{
    auto* globalObject = exec->lexicalGlobalObject();
    VM& vm = exec->vm();

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, importObject));
    dependencies.append(Strong<JSCell>(vm, globalObject));
    vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));

    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, importObject, globalObject, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, importObject, globalObject, result = WTFMove(result), &vm] () mutable {
            auto scope = DECLARE_CATCH_SCOPE(vm);
            ExecState* exec = globalObject->globalExec();
            JSWebAssemblyModule* module = JSWebAssemblyModule::createStub(vm, exec, globalObject->WebAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception()))
                return reject(exec, scope, promise);

            instantiate(vm, exec, promise, module, importObject, JSWebAssemblyInstance::createPrivateModuleKey(),  Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);
            CLEAR_AND_RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));
        });
    }));
}

void WebAssemblyPrototype::webAssemblyModuleInstantinateAsync(ExecState* exec, JSPromiseDeferred* promise, Vector<uint8_t>&& source, JSObject* importedObject)
{
    VM& vm = exec->vm();
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    webAssemblyModuleInstantinateAsyncInternal(exec, promise, WTFMove(source), importedObject);
    CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, void());
}

static EncodedJSValue JSC_HOST_CALL webAssemblyInstantiateFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    JSPromiseDeferred* promise = JSPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);

        JSValue importArgument = exec->argument(1);
        JSObject* importObject = importArgument.getObject();
        if (UNLIKELY(!importArgument.isUndefined() && !importObject)) {
            promise->reject(exec, createTypeError(exec,
                "second argument to WebAssembly.instantiate must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(vm, importArgument)));
            CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, JSValue::encode(promise->promise()));
        } else {
            JSValue firstArgument = exec->argument(0);
            if (auto* module = jsDynamicCast<JSWebAssemblyModule*>(vm, firstArgument))
                instantiate(vm, exec, promise, module, importObject, JSWebAssemblyInstance::createPrivateModuleKey(), Resolve::WithInstance, Wasm::CreationMode::FromJS);
            else
                compileAndInstantiate(vm, exec, promise, JSWebAssemblyInstance::createPrivateModuleKey(), firstArgument, importObject, Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);
        }

        return JSValue::encode(promise->promise());
    }
}

static EncodedJSValue JSC_HOST_CALL webAssemblyValidateFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const uint8_t* base;
    size_t byteSize;
    std::tie(base, byteSize) = getWasmBufferFromValue(exec, exec->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    BBQPlan plan(&vm.wasmContext, BBQPlan::Validation, Plan::dontFinalize());
    // FIXME: We might want to throw an OOM exception here if we detect that something will OOM.
    // https://bugs.webkit.org/show_bug.cgi?id=166015
    return JSValue::encode(jsBoolean(plan.parseAndValidateModule(base, byteSize)));
}

EncodedJSValue JSC_HOST_CALL webAssemblyCompileStreamingInternal(ExecState* exec)
{
    VM& vm = exec->vm();
    auto* globalObject = exec->lexicalGlobalObject();
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    JSPromiseDeferred* promise = JSPromiseDeferred::tryCreate(exec, globalObject);

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, globalObject));
    vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));

    if (globalObject->globalObjectMethodTable()->compileStreaming)
        globalObject->globalObjectMethodTable()->compileStreaming(globalObject, exec, promise, exec->argument(0));
    else {
        // CompileStreaming is not supported in jsc, only in browser environment
        ASSERT_NOT_REACHED();
    }

    CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, JSValue::encode(promise->promise()));

    return JSValue::encode(promise->promise());
}

EncodedJSValue JSC_HOST_CALL webAssemblyInstantiateStreamingInternal(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    JSPromiseDeferred* promise = JSPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);

        JSValue importArgument = exec->argument(1);
        JSObject* importObject = importArgument.getObject();
        if (UNLIKELY(!importArgument.isUndefined() && !importObject)) {
            promise->reject(exec, createTypeError(exec,
                "second argument to WebAssembly.instantiateStreaming must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(vm, importArgument)));
            CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, JSValue::encode(promise->promise()));
        } else {
            if (globalObject->globalObjectMethodTable()->instantiateStreaming) {
                Vector<Strong<JSCell>> dependencies;
                dependencies.append(Strong<JSCell>(vm, globalObject));
                dependencies.append(Strong<JSCell>(vm, importObject));
                vm.promiseDeferredTimer->addPendingPromise(vm, promise, WTFMove(dependencies));

                // FIXME: <http://webkit.org/b/184888> if there's an importObject and it contains a Memory, then we can compile the module with the right memory type (fast or not) by looking at the memory's type.
                globalObject->globalObjectMethodTable()->instantiateStreaming(globalObject, exec, promise, exec->argument(0), importObject);
            } else {
                // InstantiateStreaming is not supported in jsc, only in browser environment.
                ASSERT_NOT_REACHED();
            }
        }
        CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, JSValue::encode(promise->promise()));

        return JSValue::encode(promise->promise());
    }
}

WebAssemblyPrototype* WebAssemblyPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyPrototype>(vm.heap)) WebAssemblyPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);

    if (Options::useWebAssemblyStreamingApi()) {
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("compileStreaming", webAssemblyPrototypeCompileStreamingCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("instantiateStreaming", webAssemblyPrototypeInstantiateStreamingCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }
}

WebAssemblyPrototype::WebAssemblyPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
