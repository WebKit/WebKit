/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "JSWebAssembly.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "CatchScope.h"
#include "DeferredWorkTimer.h"
#include "Exception.h"
#include "JSCBuiltins.h"
#include "JSGlobalObjectInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
#include "Options.h"
#include "StrongInlines.h"
#include "StructureInlines.h"
#include "ThrowScope.h"
#include "WebAssemblyModuleRecord.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSWebAssembly);

#define DEFINE_CALLBACK_FOR_CONSTRUCTOR(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
static JSValue create##capitalName(VM& vm, JSObject* object) \
{ \
    JSWebAssembly* webAssembly = jsCast<JSWebAssembly*>(object); \
    JSGlobalObject* globalObject = webAssembly->globalObject(vm); \
    return globalObject->properName##Constructor(); \
}

FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DEFINE_CALLBACK_FOR_CONSTRUCTOR)

#undef DEFINE_CALLBACK_FOR_CONSTRUCTOR

static JSC_DECLARE_HOST_FUNCTION(webAssemblyCompileFunc);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyInstantiateFunc);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyValidateFunc);

}

#include "JSWebAssembly.lut.h"

namespace JSC {

const ClassInfo JSWebAssembly::s_info = { "WebAssembly", &Base::s_info, &webAssemblyTable, nullptr, CREATE_METHOD_TABLE(JSWebAssembly) };

/* Source for JSWebAssembly.lut.h
@begin webAssemblyTable
  CompileError    createWebAssemblyCompileError  DontEnum|PropertyCallback
  Global          createWebAssemblyGlobal        DontEnum|PropertyCallback
  Instance        createWebAssemblyInstance      DontEnum|PropertyCallback
  LinkError       createWebAssemblyLinkError     DontEnum|PropertyCallback
  Memory          createWebAssemblyMemory        DontEnum|PropertyCallback
  Module          createWebAssemblyModule        DontEnum|PropertyCallback
  RuntimeError    createWebAssemblyRuntimeError  DontEnum|PropertyCallback
  Table           createWebAssemblyTable         DontEnum|PropertyCallback
  compile         webAssemblyCompileFunc         Function 1
  instantiate     webAssemblyInstantiateFunc     Function 1
  validate        webAssemblyValidateFunc        Function 1
@end
*/

JSWebAssembly* JSWebAssembly::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<JSWebAssembly>(vm.heap)) JSWebAssembly(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* JSWebAssembly::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void JSWebAssembly::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
    if (Options::useWebAssemblyStreaming()) {
        if (globalObject->globalObjectMethodTable()->compileStreaming)
            JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("compileStreaming", webAssemblyCompileStreamingCodeGenerator, static_cast<unsigned>(0));
        if (globalObject->globalObjectMethodTable()->instantiateStreaming)
            JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("instantiateStreaming", webAssemblyInstantiateStreamingCodeGenerator, static_cast<unsigned>(0));
    }
}

JSWebAssembly::JSWebAssembly(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyCompileFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    RETURN_IF_EXCEPTION(scope, { });

    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    scope.release();
    JSWebAssembly::webAssemblyModuleValidateAsync(globalObject, promise, WTFMove(source));
    return JSValue::encode(promise);
}

enum class Resolve { WithInstance, WithModuleRecord, WithModuleAndInstance };
static void resolve(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, JSWebAssemblyInstance* instance, JSWebAssemblyModule* module, JSObject* importObject, Ref<Wasm::CodeBlock>&& codeBlock, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    instance->finalizeCreation(vm, globalObject, WTFMove(codeBlock), importObject, creationMode);
    if (UNLIKELY(scope.exception())) {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    scope.release();
    if (resolveKind == Resolve::WithInstance)
        promise->resolve(globalObject, instance);
    else if (resolveKind == Resolve::WithModuleRecord) {
        auto* moduleRecord = instance->moduleRecord();
        if (UNLIKELY(Options::dumpModuleRecord()))
            moduleRecord->dump();
        promise->resolve(globalObject, moduleRecord);
    } else {
        JSObject* result = constructEmptyObject(globalObject);
        result->putDirect(vm, Identifier::fromString(vm, "module"_s), module);
        result->putDirect(vm, Identifier::fromString(vm, "instance"_s), instance);
        promise->resolve(globalObject, result);
    }
}

void JSWebAssembly::webAssemblyModuleValidateAsync(JSGlobalObject* globalObject, JSPromise* promise, Vector<uint8_t>&& source)
{
    VM& vm = globalObject->vm();

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, globalObject));

    vm.deferredWorkTimer->addPendingWork(vm, promise, WTFMove(dependencies));
    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, globalObject, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.deferredWorkTimer->scheduleWorkSoon(promise, [promise, globalObject, result = WTFMove(result), &vm](DeferredWorkTimer::Ticket, DeferredWorkTimer::TicketData&&) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSValue module = JSWebAssemblyModule::createStub(vm, globalObject, globalObject->webAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            scope.release();
            promise->resolve(globalObject, module);
        });
    }));
}

static void instantiate(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, JSWebAssemblyModule* module, JSObject* importObject, const Identifier& moduleKey, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    // In order to avoid potentially recompiling a module. We first gather all the import/memory information prior to compiling code.
    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::tryCreate(vm, globalObject, moduleKey, module, importObject, globalObject->webAssemblyInstanceStructure(), Ref<Wasm::Module>(module->module()), creationMode);
    if (UNLIKELY(scope.exception())) {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    Vector<Strong<JSCell>> dependencies;
    // The instance keeps the module alive.
    dependencies.append(Strong<JSCell>(vm, promise));
    dependencies.append(Strong<JSCell>(vm, importObject));

    vm.deferredWorkTimer->addPendingWork(vm, instance, WTFMove(dependencies));
    // Note: This completion task may or may not get called immediately.
    module->module().compileAsync(&vm.wasmContext, instance->memoryMode(), createSharedTask<Wasm::CodeBlock::CallbackType>([promise, instance, module, importObject, resolveKind, creationMode, &vm] (Ref<Wasm::CodeBlock>&& refCodeBlock) mutable {
        RefPtr<Wasm::CodeBlock> codeBlock = WTFMove(refCodeBlock);
        vm.deferredWorkTimer->scheduleWorkSoon(instance, [promise, instance, module, importObject, resolveKind, creationMode, &vm, codeBlock = WTFMove(codeBlock)](DeferredWorkTimer::Ticket, DeferredWorkTimer::TicketData&&) mutable {
            JSGlobalObject* globalObject = instance->globalObject();
            resolve(vm, globalObject, promise, instance, module, importObject, codeBlock.releaseNonNull(), resolveKind, creationMode);
        });
    }));
}

static void compileAndInstantiate(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, const Identifier& moduleKey, JSValue buffer, JSObject* importObject, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, buffer);
    if (UNLIKELY(scope.exception())) {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    JSCell* moduleKeyCell = identifierToJSValue(vm, moduleKey).asCell();
    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, importObject));
    dependencies.append(Strong<JSCell>(vm, moduleKeyCell));
    vm.deferredWorkTimer->addPendingWork(vm, promise, WTFMove(dependencies));
    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, importObject, moduleKeyCell, globalObject, resolveKind, creationMode, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.deferredWorkTimer->scheduleWorkSoon(promise, [promise, importObject, moduleKeyCell, globalObject, result = WTFMove(result), resolveKind, creationMode, &vm](DeferredWorkTimer::Ticket, DeferredWorkTimer::TicketData&&) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSWebAssemblyModule* module = JSWebAssemblyModule::createStub(vm, globalObject, globalObject->webAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            const Identifier moduleKey = JSValue(moduleKeyCell).toPropertyKey(globalObject);
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            instantiate(vm, globalObject, promise, module, importObject, moduleKey, resolveKind, creationMode);
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
        });
    }));
}

JSValue JSWebAssembly::instantiate(JSGlobalObject* globalObject, JSPromise* promise, const Identifier& moduleKey, JSValue argument)
{
    VM& vm = globalObject->vm();
    compileAndInstantiate(vm, globalObject, promise, moduleKey, argument, nullptr, Resolve::WithModuleRecord, Wasm::CreationMode::FromModuleLoader);
    return promise;
}

void JSWebAssembly::instantiateForStreaming(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, JSWebAssemblyModule* module, JSObject* importObject)
{
    JSC::instantiate(vm, globalObject, promise, module, importObject, JSWebAssemblyInstance::createPrivateModuleKey(), Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);
}

void JSWebAssembly::webAssemblyModuleInstantinateAsync(JSGlobalObject* globalObject, JSPromise* promise, Vector<uint8_t>&& source, JSObject* importObject)
{
    VM& vm = globalObject->vm();

    Vector<Strong<JSCell>> dependencies;
    dependencies.append(Strong<JSCell>(vm, importObject));
    dependencies.append(Strong<JSCell>(vm, globalObject));
    vm.deferredWorkTimer->addPendingWork(vm, promise, WTFMove(dependencies));
    Wasm::Module::validateAsync(&vm.wasmContext, WTFMove(source), createSharedTask<Wasm::Module::CallbackType>([promise, importObject, globalObject, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.deferredWorkTimer->scheduleWorkSoon(promise, [promise, importObject, globalObject, result = WTFMove(result), &vm](DeferredWorkTimer::Ticket, DeferredWorkTimer::TicketData&&) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSWebAssemblyModule* module = JSWebAssemblyModule::createStub(vm, globalObject, globalObject->webAssemblyModuleStructure(), WTFMove(result));
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            JSC::instantiate(vm, globalObject, promise, module, importObject, JSWebAssemblyInstance::createPrivateModuleKey(),  Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
        });
    }));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyInstantiateFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (UNLIKELY(!importArgument.isUndefined() && !importObject))
        return JSValue::encode(JSPromise::rejectedPromise(globalObject, createTypeError(globalObject, "second argument to WebAssembly.instantiate must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(vm, importArgument))));

    JSValue firstArgument = callFrame->argument(0);
    if (firstArgument.inherits<JSWebAssemblyModule>(vm))
        instantiate(vm, globalObject, promise, jsCast<JSWebAssemblyModule*>(firstArgument), importObject, JSWebAssemblyInstance::createPrivateModuleKey(), Resolve::WithInstance, Wasm::CreationMode::FromJS);
    else
        compileAndInstantiate(vm, globalObject, promise, JSWebAssemblyInstance::createPrivateModuleKey(), firstArgument, importObject, Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);

    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyValidateFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: We might want to throw an OOM exception here if we detect that something will OOM.
    // https://bugs.webkit.org/show_bug.cgi?id=166015
    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto validationResult = Wasm::Module::validateSync(&vm.wasmContext, WTFMove(source));
    return JSValue::encode(jsBoolean(validationResult.has_value()));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyCompileStreamingInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(globalObject->globalObjectMethodTable()->compileStreaming);
    return JSValue::encode(globalObject->globalObjectMethodTable()->compileStreaming(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyInstantiateStreamingInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (UNLIKELY(!importArgument.isUndefined() && !importObject))
        return JSValue::encode(JSPromise::rejectedPromise(globalObject, createTypeError(globalObject, "second argument to WebAssembly.instantiateStreaming must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(vm, importArgument))));

    ASSERT(globalObject->globalObjectMethodTable()->instantiateStreaming);
    // FIXME: <http://webkit.org/b/184888> if there's an importObject and it contains a Memory, then we can compile the module with the right memory type (fast or not) by looking at the memory's type.
    return JSValue::encode(globalObject->globalObjectMethodTable()->instantiateStreaming(globalObject, callFrame->argument(0), importObject));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
