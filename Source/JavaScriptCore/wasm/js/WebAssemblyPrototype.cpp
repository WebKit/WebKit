/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include "JSPromiseDeferred.h"
#include "JSToWasm.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
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

static EncodedJSValue JSC_HOST_CALL webAssemblyCompileFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    JSPromiseDeferred* promise = JSPromiseDeferred::create(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);

        Vector<Strong<JSCell>> dependencies;
        dependencies.append(Strong<JSCell>(vm, globalObject));
        vm.promiseDeferredTimer->addPendingPromise(promise, WTFMove(dependencies));

        Vector<uint8_t> source = createSourceBufferFromValue(vm, exec, exec->argument(0));

        if (UNLIKELY(catchScope.exception()))
            reject(exec, catchScope, promise);
        else {
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

        return JSValue::encode(promise->promise());
    }
}

enum class Resolve { WithInstance, WithModuleAndInstance };
static void resolve(VM& vm, ExecState* exec, JSPromiseDeferred* promise, JSWebAssemblyInstance* instance, JSWebAssemblyModule* module, Ref<Wasm::CodeBlock>&& codeBlock, Resolve resolveKind)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);
    instance->finalizeCreation(vm, exec, WTFMove(codeBlock));
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    if (resolveKind == Resolve::WithInstance)
        promise->resolve(exec, instance);
    else {
        JSObject* result = constructEmptyObject(exec);
        result->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("module")), module);
        result->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("instance")), instance);
        promise->resolve(exec, result);
    }
    CLEAR_AND_RETURN_IF_EXCEPTION(scope, void());
}

static void instantiate(VM& vm, ExecState* exec, JSPromiseDeferred* promise, JSWebAssemblyModule* module, JSObject* importObject, Resolve resolveKind)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);
    // In order to avoid potentially recompiling a module. We first gather all the import/memory information prior to compiling code.
    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::create(vm, exec, module, importObject, exec->lexicalGlobalObject()->WebAssemblyInstanceStructure(), Ref<Wasm::Module>(module->module()));
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    Vector<Strong<JSCell>> dependencies;
    // The instance keeps the module alive.
    dependencies.append(Strong<JSCell>(vm, instance));
    vm.promiseDeferredTimer->addPendingPromise(promise, WTFMove(dependencies));
    // Note: This completion task may or may not get called immediately.
    module->module().compileAsync(&vm.wasmContext, instance->memoryMode(), createSharedTask<Wasm::CodeBlock::CallbackType>([promise, instance, module, resolveKind, &vm] (Ref<Wasm::CodeBlock>&& refCodeBlock) mutable {
        RefPtr<Wasm::CodeBlock> codeBlock = WTFMove(refCodeBlock);
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, instance, module, resolveKind, &vm, codeBlock = WTFMove(codeBlock)] () mutable {
            ExecState* exec = instance->globalObject()->globalExec();
            resolve(vm, exec, promise, instance, module, codeBlock.releaseNonNull(), resolveKind);
        });
    }), &Wasm::createJSToWasmWrapper, &Wasm::wasmToJSException);
}

static void compileAndInstantiate(VM& vm, ExecState* exec, JSPromiseDeferred* promise, JSValue buffer, JSObject* importObject)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto* globalObject = exec->lexicalGlobalObject();

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, importObject));
    vm.promiseDeferredTimer->addPendingPromise(promise, WTFMove(dependencies));

    Vector<uint8_t> source = createSourceBufferFromValue(vm, exec, buffer);
    RETURN_IF_EXCEPTION(scope, reject(exec, scope, promise));

    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, importObject, globalObject, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.promiseDeferredTimer->scheduleWorkSoon(promise, [promise, importObject, globalObject, result = WTFMove(result), &vm] () mutable {
            auto scope = DECLARE_CATCH_SCOPE(vm);
            ExecState* exec = globalObject->globalExec();
            JSWebAssemblyModule* module = JSWebAssemblyModule::createStub(vm, exec, globalObject->WebAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception()))
                return reject(exec, scope, promise);

            instantiate(vm, exec, promise, module, importObject, Resolve::WithModuleAndInstance);
        });
    }));
}

static EncodedJSValue JSC_HOST_CALL webAssemblyInstantiateFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    JSPromiseDeferred* promise = JSPromiseDeferred::create(exec, globalObject);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);

        JSValue importArgument = exec->argument(1);
        JSObject* importObject = importArgument.getObject();
        if (UNLIKELY(!importArgument.isUndefined() && !importObject)) {
            promise->reject(exec, createTypeError(exec,
                ASCIILiteral("second argument to WebAssembly.instantiate must be undefined or an Object"), defaultSourceAppender, runtimeTypeForValue(importArgument)));
            CLEAR_AND_RETURN_IF_EXCEPTION(catchScope, JSValue::encode(promise->promise()));
        } else {
            JSValue firstArgument = exec->argument(0);
            if (auto* module = jsDynamicCast<JSWebAssemblyModule*>(vm, firstArgument))
                instantiate(vm, exec, promise, module, importObject, Resolve::WithInstance);
            else
                compileAndInstantiate(vm, exec, promise, firstArgument, importObject);
        }

        return JSValue::encode(promise->promise());
    }
}

static EncodedJSValue JSC_HOST_CALL webAssemblyValidateFunc(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t byteOffset;
    size_t byteSize;
    uint8_t* base = getWasmBufferFromValue(exec, exec->argument(0), byteOffset, byteSize);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    BBQPlan plan(&vm.wasmContext, base + byteOffset, byteSize, BBQPlan::Validation, Plan::dontFinalize());
    // FIXME: We might want to throw an OOM exception here if we detect that something will OOM.
    // https://bugs.webkit.org/show_bug.cgi?id=166015
    return JSValue::encode(jsBoolean(plan.parseAndValidateModule()));
}

WebAssemblyPrototype* WebAssemblyPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyPrototype>(vm.heap)) WebAssemblyPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

WebAssemblyPrototype::WebAssemblyPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
