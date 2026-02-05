/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "DeferredWorkTimer.h"
#include "Exception.h"
#include "GlobalObjectMethodTable.h"
#include "JSArrayBufferViewInlines.h"
#include "JSCBuiltins.h"
#include "JSGlobalObjectInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyTag.h"
#include "ObjectConstructor.h"
#include "Options.h"
#include "StrongInlines.h"
#include "StructureInlines.h"
#include "ThrowScope.h"
#include "TopExceptionScope.h"
#include "WebAssemblyCompileOptions.h"
#include "WebAssemblyModuleRecord.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSWebAssembly);

// Uses UNUSED_FUNCTION because some constructors, e.g., for Arrays are currently not exposed.
#define DEFINE_CALLBACK_FOR_CONSTRUCTOR(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
static UNUSED_FUNCTION JSValue create##capitalName(VM&, JSObject* object) \
{ \
    JSWebAssembly* webAssembly = jsCast<JSWebAssembly*>(object); \
    JSGlobalObject* globalObject = webAssembly->globalObject(); \
    return globalObject->properName##Constructor(); \
}

FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(DEFINE_CALLBACK_FOR_CONSTRUCTOR)

#undef DEFINE_CALLBACK_FOR_CONSTRUCTOR

static JSC_DECLARE_HOST_FUNCTION(webAssemblyCompileFunc);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyInstantiateFunc);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyValidateFunc);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyGetterJSTag);

}

#include "JSWebAssembly.lut.h"

namespace JSC {

const ClassInfo JSWebAssembly::s_info = { "WebAssembly"_s, &Base::s_info, &webAssemblyTable, nullptr, CREATE_METHOD_TABLE(JSWebAssembly) };

/* Source for JSWebAssembly.lut.h
@begin webAssemblyTable
  CompileError    createWebAssemblyCompileError  DontEnum|PropertyCallback
  Exception       createWebAssemblyException     DontEnum|PropertyCallback
  Global          createWebAssemblyGlobal        DontEnum|PropertyCallback
  Instance        createWebAssemblyInstance      DontEnum|PropertyCallback
  LinkError       createWebAssemblyLinkError     DontEnum|PropertyCallback
  Memory          createWebAssemblyMemory        DontEnum|PropertyCallback
  Module          createWebAssemblyModule        DontEnum|PropertyCallback
  RuntimeError    createWebAssemblyRuntimeError  DontEnum|PropertyCallback
  Table           createWebAssemblyTable         DontEnum|PropertyCallback
  Tag             createWebAssemblyTag           DontEnum|PropertyCallback
  compile         webAssemblyCompileFunc         Function 1
  instantiate     webAssemblyInstantiateFunc     Function 1
  validate        webAssemblyValidateFunc        Function 1
@end
*/

JSWebAssembly* JSWebAssembly::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<JSWebAssembly>(vm)) JSWebAssembly(vm, structure);
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
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
    if (globalObject->globalObjectMethodTable()->compileStreaming)
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("compileStreaming"_s, webAssemblyCompileStreamingCodeGenerator, static_cast<unsigned>(0));
    if (globalObject->globalObjectMethodTable()->instantiateStreaming)
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("instantiateStreaming"_s, webAssemblyInstantiateStreamingCodeGenerator, static_cast<unsigned>(0));
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION("JSTag"_s, webAssemblyGetterJSTag, PropertyAttribute::ReadOnly);
}

JSWebAssembly::JSWebAssembly(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

/**
 * namespace WebAssembly {
 *     Promise<Module> compile(BufferSource bytes, optional WebAssemblyCompileOptions options);
 * }
 *
 * See https://webassembly.github.io/js-string-builtins/js-api/#dom-webassembly-compile
 */
JSC_DEFINE_HOST_FUNCTION(webAssemblyCompileFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    RETURN_IF_EXCEPTION(scope, { });

    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(globalObject, scope)));

    JSObject* compileOptionsObject = nullptr;
    if (Options::useWasmJSStringBuiltins()) {
        JSValue compileOptionsArgument = callFrame->argument(1);
        compileOptionsObject = compileOptionsArgument.getObject();
        if (!compileOptionsArgument.isUndefined() && !compileOptionsObject) [[unlikely]] {
            auto error = createTypeError(globalObject, "second argument to WebAssembly.compile must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(compileOptionsArgument));
            RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::rejectedPromise(globalObject, error)));
        }
    }
    auto compileOptions = WebAssemblyCompileOptions::tryCreate(globalObject, compileOptionsObject);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    JSWebAssembly::webAssemblyModuleValidateAsync(globalObject, promise, WTF::move(source), WTF::move(compileOptions));
    return JSValue::encode(promise);
}

void JSWebAssembly::webAssemblyModuleValidateAsync(JSGlobalObject* globalObject, JSPromise* promise, Vector<uint8_t>&& source, std::optional<WebAssemblyCompileOptions>&& compileOptions)
{
    VM& vm = globalObject->vm();

    Vector<JSCell*> dependencies;
    dependencies.append(globalObject);

    auto ticket = vm.deferredWorkTimer->addPendingWork(DeferredWorkTimer::WorkType::ImminentlyScheduled, vm, promise, WTF::move(dependencies));
    Wasm::Module::validateAsync(vm, WTF::move(source), createSharedTask<Wasm::Module::CallbackType>([ticket, promise, globalObject, compileOptions = WTF::move(compileOptions), &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.deferredWorkTimer->scheduleWorkSoon(ticket, [promise, globalObject, result = WTF::move(result), compileOptions = WTF::move(compileOptions), &vm](DeferredWorkTimer::Ticket) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (!result.has_value()) [[unlikely]] {
                throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            if (compileOptions) {
                auto errorMessage = compileOptions->validateBuiltinsAndImportedStrings(result.value());
                if (errorMessage.has_value()) {
                    throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, errorMessage.value()));
                    promise->rejectWithCaughtException(globalObject, scope);
                    return;
                }
                result.value()->applyCompileOptions(compileOptions.value());
            }

            JSValue module = JSWebAssemblyModule::create(vm, globalObject->webAssemblyModuleStructure(), WTF::move(result.value()));

            scope.release();
            promise->resolve(globalObject, module);
        });
    }));
}

enum class Resolve { WithInstance, WithModuleRecord, WithModuleAndInstance };
static void instantiate(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, JSWebAssemblyModule* module, JSObject* importObject, RefPtr<SourceProvider>&& provider, const Identifier& moduleKey, Resolve resolveKind, Wasm::CreationMode creationMode, bool alwaysAsync)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    // In order to avoid potentially recompiling a module. We first gather all the import/memory information prior to compiling code.
    // When called via the module loader, the memory is not available yet at this step, so we skip initializing the memory here.
    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::tryCreate(vm, globalObject->webAssemblyInstanceStructure(), globalObject, moduleKey, module, importObject, creationMode, WTF::move(provider));
    if (scope.exception()) [[unlikely]] {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    instance->initializeImports(globalObject, importObject, creationMode);
    if (scope.exception()) [[unlikely]] {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    Vector<JSCell*> dependencies;
    // The instance keeps the module alive.
    dependencies.append(promise);

    scope.release();
    auto ticket = vm.deferredWorkTimer->addPendingWork(DeferredWorkTimer::WorkType::ImminentlyScheduled, vm, instance, WTF::move(dependencies));
    // Note: This completion task may or may not get called immediately.
    module->module().compileAsync(vm, instance->memoryMode(), createSharedTask<Wasm::CalleeGroup::CallbackType>([ticket, promise, instance, module, resolveKind, creationMode, &vm, alwaysAsync] (Ref<Wasm::CalleeGroup>&& calleeGroup, bool isAsync) mutable {
        auto callback = [promise, instance, module, resolveKind, creationMode, &vm, calleeGroup = WTF::move(calleeGroup)](DeferredWorkTimer::Ticket) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);
            JSGlobalObject* globalObject = instance->globalObject();
            instance->finalizeCreation(vm, globalObject, WTF::move(calleeGroup), creationMode);
            if (scope.exception()) [[unlikely]] {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            scope.release();
            switch (resolveKind) {
            case Resolve::WithInstance: {
                promise->resolve(globalObject, instance);
                break;
            }
            case Resolve::WithModuleRecord: {
                auto* moduleRecord = instance->moduleRecord();
                if (Options::dumpModuleRecord()) [[unlikely]]
                    moduleRecord->dump();
                promise->resolve(globalObject, moduleRecord);
                break;
            }
            case Resolve::WithModuleAndInstance: {
                JSObject* result = constructEmptyObject(globalObject);
                result->putDirect(vm, Identifier::fromString(vm, "module"_s), module);
                result->putDirect(vm, Identifier::fromString(vm, "instance"_s), instance);
                promise->resolve(globalObject, result);
                break;
            }
            }
        };

        if (alwaysAsync || isAsync) {
            vm.deferredWorkTimer->scheduleWorkSoon(ticket, WTF::move(callback));
            return;
        }
        vm.deferredWorkTimer->cancelPendingWork(ticket);
        callback(ticket);
    }));
}

static void compileAndInstantiate(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, const Identifier& moduleKey, JSValue buffer, JSObject* importObject, RefPtr<SourceProvider>&& sourceProvider, std::optional<WebAssemblyCompileOptions>&& compileOptions, Resolve resolveKind, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, buffer);
    if (scope.exception()) [[unlikely]] {
        promise->rejectWithCaughtException(globalObject, scope);
        return;
    }

    JSCell* moduleKeyCell = identifierToJSValue(vm, moduleKey).asCell();
    Vector<JSCell*> dependencies;
    if (importObject)
        dependencies.append(importObject);
    dependencies.append(moduleKeyCell);
    auto ticket = vm.deferredWorkTimer->addPendingWork(DeferredWorkTimer::WorkType::ImminentlyScheduled, vm, promise, WTF::move(dependencies));
    Wasm::Module::validateAsync(vm, WTF::move(source), createSharedTask<Wasm::Module::CallbackType>([ticket, promise, importObject, sourceProvider = WTF::move(sourceProvider), compileOptions = WTF::move(compileOptions), moduleKeyCell, globalObject, resolveKind, creationMode, &vm] (Wasm::Module::ValidationResult&& result) mutable {
        vm.deferredWorkTimer->scheduleWorkSoon(ticket, [promise, importObject, sourceProvider = WTF::move(sourceProvider), compileOptions = WTF::move(compileOptions), moduleKeyCell, globalObject, result = WTF::move(result), resolveKind, creationMode, &vm](DeferredWorkTimer::Ticket) mutable {
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (!result.has_value()) [[unlikely]] {
                throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            if (compileOptions) {
                auto errorMessage = compileOptions->validateBuiltinsAndImportedStrings(result.value());
                if (errorMessage.has_value()) {
                    throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, errorMessage.value()));
                    promise->rejectWithCaughtException(globalObject, scope);
                    return;
                }
                result.value()->applyCompileOptions(compileOptions.value());
            }

            JSWebAssemblyModule* module = JSWebAssemblyModule::create(vm, globalObject->webAssemblyModuleStructure(), WTF::move(result.value()));

            const Identifier moduleKey = JSValue(moduleKeyCell).toPropertyKey(globalObject);
            if (scope.exception()) [[unlikely]] {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            instantiate(vm, globalObject, promise, module, importObject, WTF::move(sourceProvider), moduleKey, resolveKind, creationMode, /* alwaysAsync */ false);
            if (scope.exception()) [[unlikely]] {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
        });
    }));
}

JSValue JSWebAssembly::instantiate(JSGlobalObject* globalObject, JSPromise* promise, RefPtr<SourceProvider>&& sourceProvider, const Identifier& moduleKey, JSValue argument)
{
    VM& vm = globalObject->vm();
    compileAndInstantiate(vm, globalObject, promise, moduleKey, argument, nullptr, WTF::move(sourceProvider), std::nullopt, Resolve::WithModuleRecord, Wasm::CreationMode::FromModuleLoader);
    return promise;
}

void JSWebAssembly::instantiateForStreaming(VM& vm, JSGlobalObject* globalObject, JSPromise* promise, JSWebAssemblyModule* module, JSObject* importObject, RefPtr<SourceProvider>&& sourceProvider)
{
    JSC::instantiate(vm, globalObject, promise, module, importObject, WTF::move(sourceProvider), JSWebAssemblyInstance::createPrivateModuleKey(), Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS, /* alwaysAsync */ true);
}

/**
 * This implements two standard APIs:
 *
 * namespace WebAssembly {
 *     Promise<WebAssemblyInstantiatedSource> instantiate(BufferSource bytes, optional object importObject, optional WebAssemblyCompileOptions options);
 *     Promise<Instance> instantiate(Module moduleObject, optional object importObject);
 * }
 *
 * See:
 *   - https://webassembly.github.io/js-string-builtins/js-api/#dom-webassembly-instantiate
 *   - https://webassembly.github.io/js-string-builtins/js-api/#dom-webassembly-instantiate-moduleobject-importobject
 */
JSC_DEFINE_HOST_FUNCTION(webAssemblyInstantiateFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [taintedness, url] = sourceTaintedOriginFromStack(vm, callFrame);
    RefPtr<SourceProvider> provider = StringSourceProvider::create("[wasm code]"_s, SourceOrigin(url), String(), taintedness, TextPosition(), SourceProviderSourceType::Program);

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::rejectedPromise(globalObject, createTypeError(globalObject, "second argument to WebAssembly.instantiate must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(importArgument)))));

    JSObject* compileOptionsObject = nullptr;
    if (Options::useWasmJSStringBuiltins()) {
        JSValue compileOptionsArgument = callFrame->argument(2);
        compileOptionsObject = compileOptionsArgument.getObject();
        if (!compileOptionsArgument.isUndefined() && !compileOptionsObject) [[unlikely]] {
            auto error = createTypeError(globalObject, "third argument to WebAssembly.instantiate must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(compileOptionsArgument));
            RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::rejectedPromise(globalObject, error)));
        }
    }

    JSValue firstArgument = callFrame->argument(0);
    if (firstArgument.inherits<JSWebAssemblyModule>())
        instantiate(vm, globalObject, promise, jsCast<JSWebAssemblyModule*>(firstArgument), importObject, WTF::move(provider), JSWebAssemblyInstance::createPrivateModuleKey(), Resolve::WithInstance, Wasm::CreationMode::FromJS, /* alwaysAsync */ true);
    else {
        auto compileOptions = WebAssemblyCompileOptions::tryCreate(globalObject, compileOptionsObject);
        RETURN_IF_EXCEPTION(scope, { });
        compileAndInstantiate(vm, globalObject, promise, JSWebAssemblyInstance::createPrivateModuleKey(), firstArgument, importObject, WTF::move(provider), WTF::move(compileOptions), Resolve::WithModuleAndInstance, Wasm::CreationMode::FromJS);
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(promise));
}

/**
 * namespace WebAssembly {
 *     boolean validate(BufferSource bytes, optional WebAssemblyCompileOptions options);
 * }
 *
 * See https://webassembly.github.io/js-string-builtins/js-api/#dom-webassembly-validate
 */
JSC_DEFINE_HOST_FUNCTION(webAssemblyValidateFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: We might want to throw an OOM exception here if we detect that something will OOM.
    // https://bugs.webkit.org/show_bug.cgi?id=166015
    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSObject* compileOptionsObject = nullptr;
    if (Options::useWasmJSStringBuiltins()) {
        JSValue compileOptionsArgument = callFrame->argument(1);
        compileOptionsObject = compileOptionsArgument.getObject();
        if (!compileOptionsArgument.isUndefined() && !compileOptionsObject) [[unlikely]] {
            auto error = createTypeError(globalObject, "second argument to WebAssembly.validate must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(compileOptionsArgument));
            return JSValue::encode(throwException(globalObject, scope, error));
        }
    }

    auto validationResult = Wasm::Module::validateSync(vm, WTF::move(source));
    bool success = validationResult.has_value();
    if (success) {
        auto compileOptions = WebAssemblyCompileOptions::tryCreate(globalObject, compileOptionsObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (compileOptions) {
            auto errorMessage = compileOptions->validateBuiltinsAndImportedStrings(validationResult.value());
            success = !errorMessage.has_value();
        }
    }

    return JSValue::encode(jsBoolean(success));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyCompileStreamingInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(globalObject->globalObjectMethodTable()->compileStreaming);
    return JSValue::encode(globalObject->globalObjectMethodTable()->compileStreaming(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyInstantiateStreamingInternal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject) [[unlikely]]
        return JSValue::encode(JSPromise::rejectedPromise(globalObject, createTypeError(globalObject, "second argument to WebAssembly.instantiateStreaming must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(importArgument))));

    ASSERT(globalObject->globalObjectMethodTable()->instantiateStreaming);
    // FIXME: <http://webkit.org/b/184888> if there's an importObject and it contains a Memory, then we can compile the module with the right memory type (fast or not) by looking at the memory's type.
    return JSValue::encode(globalObject->globalObjectMethodTable()->instantiateStreaming(globalObject, callFrame->argument(0), importObject));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyGetterJSTag, (JSGlobalObject* globalObject, CallFrame*))
{
    // https://webassembly.github.io/exception-handling/js-api/#dom-webassembly-jstag
    return JSValue::encode(globalObject->webAssemblyJSTag());
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
