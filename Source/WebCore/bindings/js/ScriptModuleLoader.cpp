/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptModuleLoader.h"

#include "CachedModuleScriptLoader.h"
#include "CachedScript.h"
#include "CachedScriptFetcher.h"
#include "DocumentInlines.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "JSDOMPromiseDeferred.h"
#include "LoadableModuleScript.h"
#include "MIMETypeRegistry.h"
#include "ModuleFetchFailureKind.h"
#include "ModuleFetchParameters.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ShadowRealmGlobalScope.h"
#include "SubresourceIntegrity.h"
#include "WebAssemblyScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include "WorkerModuleScriptLoader.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkerScriptFetcher.h"
#include "WorkerScriptLoader.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/AbstractModuleRecord.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/ImportMap.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSScriptFetchParameters.h>
#include <JavaScriptCore/JSScriptFetcher.h>
#include <JavaScriptCore/JSSourceCode.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/SourceProvider.h>
#include <JavaScriptCore/Symbol.h>

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerGlobalScope.h"
#endif

namespace WebCore {

ScriptModuleLoader::ScriptModuleLoader(ScriptExecutionContext& context, OwnerType ownerType)
    : m_context(context)
    , m_ownerType(ownerType)
{
}

ScriptModuleLoader::~ScriptModuleLoader()
{
    for (auto& loader : m_loaders)
        loader->clearClient();
}

UniqueRef<ScriptModuleLoader> ScriptModuleLoader::shadowRealmLoader(JSC::JSGlobalObject* realmGlobal) const
{
    auto loader = makeUniqueRef<ScriptModuleLoader>(m_context, m_ownerType);
    loader->m_shadowRealmGlobal = realmGlobal;
    return loader;
}

static bool isRootModule(JSC::JSValue importerModuleKey)
{
    return importerModuleKey.isSymbol() || importerModuleKey.isUndefined();
}

static Expected<URL, String> resolveModuleSpecifier(ScriptExecutionContext& context, ScriptModuleLoader::OwnerType ownerType, JSC::ImportMap& importMap, const String& specifier, const URL& originalBaseURL)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier

    URL result = importMap.resolve(specifier, ownerType == ScriptModuleLoader::OwnerType::Document ? downcast<Document>(context).baseURLForComplete(originalBaseURL) : originalBaseURL);
    if (result.isNull())
        return makeUnexpected(makeString("Module name, '"_s, specifier, "' does not resolve to a valid URL."_s));
    return result;
}

JSC::Identifier ScriptModuleLoader::resolve(JSC::JSGlobalObject* jsGlobalObject, JSC::JSModuleLoader*, JSC::JSValue moduleNameValue, JSC::JSValue importerModuleKey, JSC::JSValue)
{
    JSC::VM& vm = jsGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We use a Symbol as a special purpose; It means this module is an inline module.
    // So there is no correct URL to retrieve the module source code. If the module name
    // value is a Symbol, it is used directly as a module key.
    if (moduleNameValue.isSymbol())
        return JSC::Identifier::fromUid(asSymbol(moduleNameValue)->privateName());

    if (!moduleNameValue.isString()) {
        JSC::throwTypeError(jsGlobalObject, scope, "Importer module key is not a Symbol or a String."_s);
        return { };
    }

    String specifier = asString(moduleNameValue)->value(jsGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    URL baseURL = responseURLFromRequestURL(*jsGlobalObject, importerModuleKey);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = resolveModuleSpecifier(m_context, m_ownerType, jsGlobalObject->importMap(), specifier, baseURL);
    if (!result) {
        auto* error = JSC::createTypeError(jsGlobalObject, result.error());
        ASSERT(error);
        error->putDirect(vm, builtinNames(vm).failureKindPrivateName(), JSC::jsNumber(static_cast<int32_t>(ModuleFetchFailureKind::WasResolveError)));
        JSC::throwException(jsGlobalObject, scope, error);
        return { };
    }

    return JSC::Identifier::fromString(vm, result->string());
}

static void rejectToPropagateNetworkError(DeferredPromise& deferred, ModuleFetchFailureKind failureKind, ASCIILiteral message)
{
    deferred.rejectWithCallback([&] (JSDOMGlobalObject& jsGlobalObject) {
        // We annotate exception with special private symbol. It allows us to distinguish these errors from the user thrown ones.
        JSC::VM& vm = jsGlobalObject.vm();
        // FIXME: Propagate more descriptive error.
        // https://bugs.webkit.org/show_bug.cgi?id=167553
        auto* error = JSC::createTypeError(&jsGlobalObject, message);
        ASSERT(error);
        error->putDirect(vm, builtinNames(vm).failureKindPrivateName(), JSC::jsNumber(static_cast<int32_t>(failureKind)));
        return error;
    });
}

static void rejectWithFetchError(DeferredPromise& deferred, ExceptionCode ec, const String& message)
{
    // Used to signal to the promise client that the failure was from a fetch, but not one that was propagated from another context.
    deferred.rejectWithCallback([&] (JSDOMGlobalObject& jsGlobalObject) {
        JSC::VM& vm = jsGlobalObject.vm();
        JSC::JSObject* error = jsCast<JSC::JSObject*>(createDOMException(&jsGlobalObject, ec, message));
        ASSERT(error);
        error->putDirect(vm, builtinNames(vm).failureKindPrivateName(), JSC::jsNumber(static_cast<int32_t>(ModuleFetchFailureKind::WasFetchError)));
        return error;
    });
}

JSC::JSInternalPromise* ScriptModuleLoader::fetch(JSC::JSGlobalObject* jsGlobalObject, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue parametersValue, JSC::JSValue scriptFetcher)
{
    JSC::VM& vm = jsGlobalObject->vm();
    ASSERT(JSC::jsDynamicCast<JSC::JSScriptFetcher*>(scriptFetcher));

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto* jsPromise = JSC::JSInternalPromise::create(vm, globalObject.internalPromiseStructure());
    RELEASE_ASSERT(jsPromise);
    auto deferred = DeferredPromise::create(globalObject, *jsPromise);
    if (moduleKeyValue.isSymbol()) {
        rejectWithFetchError(deferred.get(), TypeError, "Symbol module key should be already fulfilled with the inlined resource."_s);
        return jsPromise;
    }

    if (!moduleKeyValue.isString()) {
        rejectWithFetchError(deferred.get(), TypeError, "Module key is not Symbol or String."_s);
        return jsPromise;
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script

    URL completedURL { asString(moduleKeyValue)->value(jsGlobalObject) };
    if (!completedURL.isValid()) {
        rejectWithFetchError(deferred.get(), TypeError, "Module key is a valid URL."_s);
        return jsPromise;
    }

    RefPtr<JSC::ScriptFetchParameters> parameters;
    if (auto* scriptFetchParameters = JSC::jsDynamicCast<JSC::JSScriptFetchParameters*>(parametersValue))
        parameters = &scriptFetchParameters->parameters();

    if (m_ownerType == OwnerType::Document) {
        auto loader = CachedModuleScriptLoader::create(*this, deferred.get(), *static_cast<CachedScriptFetcher*>(JSC::jsCast<JSC::JSScriptFetcher*>(scriptFetcher)->fetcher()), WTFMove(parameters));
        m_loaders.add(loader.copyRef());
        if (!loader->load(downcast<Document>(m_context), WTFMove(completedURL))) {
            loader->clearClient();
            m_loaders.remove(WTFMove(loader));
            rejectToPropagateNetworkError(deferred.get(), ModuleFetchFailureKind::WasPropagatedError, "Importing a module script failed."_s);
            return jsPromise;
        }
    } else {
        auto loader = WorkerModuleScriptLoader::create(*this, deferred.get(), *static_cast<WorkerScriptFetcher*>(JSC::jsCast<JSC::JSScriptFetcher*>(scriptFetcher)->fetcher()), WTFMove(parameters));
        m_loaders.add(loader.copyRef());
        loader->load(m_context, WTFMove(completedURL));
    }

    return jsPromise;
}

URL ScriptModuleLoader::moduleURL(JSC::JSGlobalObject& jsGlobalObject, JSC::JSValue moduleKeyValue)
{
    if (moduleKeyValue.isSymbol())
        return m_context.url();

    ASSERT(moduleKeyValue.isString());
    return URL { asString(moduleKeyValue)->value(&jsGlobalObject) };
}

URL ScriptModuleLoader::responseURLFromRequestURL(JSC::JSGlobalObject& jsGlobalObject, JSC::JSValue moduleKeyValue)
{
    JSC::VM& vm = jsGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isRootModule(moduleKeyValue)) {
        if (m_ownerType == OwnerType::Document)
            return downcast<Document>(m_context).baseURL();
        return m_context.url();
    }

    ASSERT(!isRootModule(moduleKeyValue));
    ASSERT(moduleKeyValue.isString());
    String requestURL = asString(moduleKeyValue)->value(&jsGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT_WITH_MESSAGE(URL(requestURL).isValid(), "Invalid module referrer never starts importing dependent modules.");

    auto iterator = m_requestURLToResponseURLMap.find(requestURL);
    if (iterator == m_requestURLToResponseURLMap.end())
        return URL { requestURL }; // dynamic-import().
    URL result = iterator->value;
    ASSERT(result.isValid());
    return result;
}

JSC::JSValue ScriptModuleLoader::evaluate(JSC::JSGlobalObject* jsGlobalObject, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue moduleRecordValue, JSC::JSValue, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    JSC::VM& vm = jsGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Currently, we only support JSModuleRecord and WebAssemblyModuleRecord.
    // Once the reflective part of the module loader is supported, we will handle arbitrary values.
    // https://whatwg.github.io/loader/#registry-prototype-provide
    auto* moduleRecord = JSC::jsDynamicCast<JSC::AbstractModuleRecord*>(moduleRecordValue);
    if (!moduleRecord)
        return JSC::jsUndefined();

    URL sourceURL = moduleURL(*jsGlobalObject, moduleKeyValue);
    if (!sourceURL.isValid())
        return JSC::throwTypeError(jsGlobalObject, scope, "Module key is an invalid URL."_s);

    if (m_shadowRealmGlobal)
        RELEASE_AND_RETURN(scope, moduleRecord->evaluate(m_shadowRealmGlobal, awaitedValue, resumeMode));
    else if (m_ownerType == OwnerType::Document) {
        if (auto* frame = downcast<Document>(m_context).frame())
            RELEASE_AND_RETURN(scope, frame->script().evaluateModule(sourceURL, *moduleRecord, awaitedValue, resumeMode));
    } else {
        ASSERT(is<WorkerOrWorkletGlobalScope>(m_context));
        if (auto* script = downcast<WorkerOrWorkletGlobalScope>(m_context).script())
            RELEASE_AND_RETURN(scope, script->evaluateModule(*moduleRecord, awaitedValue, resumeMode));
    }
    return JSC::jsUndefined();
}

static JSC::JSInternalPromise* rejectPromise(JSDOMGlobalObject& globalObject, ExceptionCode ec, String message)
{
    auto* jsPromise = JSC::JSInternalPromise::create(globalObject.vm(), globalObject.internalPromiseStructure());
    RELEASE_ASSERT(jsPromise);
    auto deferred = DeferredPromise::create(globalObject, *jsPromise);
    rejectWithFetchError(deferred.get(), ec, WTFMove(message));
    return jsPromise;
}

static bool isWorkletOrServiceWorker(ScriptExecutionContext& context)
{
    if (is<WorkletGlobalScope>(context))
        return true;
#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerGlobalScope>(context))
        return true;
#endif
    return false;
}

JSC::JSInternalPromise* ScriptModuleLoader::importModule(JSC::JSGlobalObject* jsGlobalObject, JSC::JSModuleLoader*, JSC::JSString* moduleName, JSC::JSValue parametersValue, const JSC::SourceOrigin& sourceOrigin)
{
    JSC::VM& vm = jsGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);

    // https://html.spec.whatwg.org/multipage/webappapis.html#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)
    // If settings object's global object implements WorkletGlobalScope or ServiceWorkerGlobalScope, then:
    if (isWorkletOrServiceWorker(m_context)) {
        scope.release();
        return rejectPromise(globalObject, TypeError, "Dynamic-import is not available in Worklets or ServiceWorkers"_s);
    }

    auto reject = [&](auto& scope) {
        auto* promise = JSC::JSInternalPromise::create(vm, globalObject.internalPromiseStructure());
        return promise->rejectWithCaughtException(&globalObject, scope);
    };

    auto getTypeFromAssertions = [&]() -> JSC::ScriptFetchParameters::Type {
        auto assertions = JSC::retrieveAssertionsFromDynamicImportOptions(&globalObject, parametersValue, { vm.propertyNames->type.impl() });
        RETURN_IF_EXCEPTION(scope, { });
        if (!assertions.isEmpty())
            return JSC::ScriptFetchParameters::parseType(assertions.get(vm.propertyNames->type.impl())).value_or(JSC::ScriptFetchParameters::Type::JavaScript);
        return JSC::ScriptFetchParameters::Type::JavaScript;
    };

    // If SourceOrigin and/or CachedScriptFetcher is null, we import the module with the default fetcher.
    // SourceOrigin can be null if the source code is not coupled with the script file.
    // The examples,
    //     1. The code evaluated by the inspector.
    //     2. The other unusual code execution like the evaluation through the NPAPI.
    //     3. The code from injected bundle's script.
    //     4. The code from extension script.
    URL baseURL;
    RefPtr<JSC::ScriptFetcher> scriptFetcher;
    RefPtr<ModuleFetchParameters> parameters;
    if (sourceOrigin.isNull()) {
        auto type = getTypeFromAssertions();
        RETURN_IF_EXCEPTION(scope, reject(scope));

        parameters = ModuleFetchParameters::create(type, emptyString(), /* isTopLevelModule */ true);

        if (m_ownerType == OwnerType::Document) {
            baseURL = downcast<Document>(m_context).baseURL();
            scriptFetcher = CachedScriptFetcher::create(downcast<Document>(m_context).charset());
        } else {
            // https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options
            baseURL = m_context.url();
            scriptFetcher = WorkerScriptFetcher::create(*parameters, FetchOptions::Credentials::SameOrigin, FetchOptions::Destination::Script, ReferrerPolicy::EmptyString);
        }
    } else {
        baseURL = URL { sourceOrigin.string() };
        if (!baseURL.isValid()) {
            scope.release();
            return rejectPromise(globalObject, TypeError, "Importer module key is not a Symbol or a String."_s);
        }

        auto type = getTypeFromAssertions();
        RETURN_IF_EXCEPTION(scope, reject(scope));

        parameters = ModuleFetchParameters::create(type, emptyString(), /* isTopLevelModule */ true);

        if (sourceOrigin.fetcher()) {
            scriptFetcher = sourceOrigin.fetcher();
            // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-an-import()-module-script-graph
            // Destination should be "script" for dynamic-import.
            if (m_ownerType == OwnerType::WorkerOrWorklet) {
                auto& fetcher = static_cast<WorkerScriptFetcher&>(*scriptFetcher);
                scriptFetcher = WorkerScriptFetcher::create(*parameters, fetcher.credentials(), FetchOptions::Destination::Script, fetcher.referrerPolicy());
            }
        }

        if (!scriptFetcher) {
            if (m_ownerType == OwnerType::Document)
                scriptFetcher = CachedScriptFetcher::create(downcast<Document>(m_context).charset());
            else
                scriptFetcher = WorkerScriptFetcher::create(*parameters, FetchOptions::Credentials::SameOrigin, FetchOptions::Destination::Script, ReferrerPolicy::EmptyString);
        }
    }
    ASSERT(baseURL.isValid());
    ASSERT(scriptFetcher);
    ASSERT(parameters);

    auto specifier = moduleName->value(jsGlobalObject);
    RETURN_IF_EXCEPTION(scope, reject(scope));
    RELEASE_AND_RETURN(scope, JSC::importModule(jsGlobalObject, JSC::Identifier::fromString(vm, specifier), JSC::jsString(vm, baseURL.string()), JSC::JSScriptFetchParameters::create(vm, parameters.releaseNonNull()), JSC::JSScriptFetcher::create(vm, WTFMove(scriptFetcher))));
}

JSC::JSObject* ScriptModuleLoader::createImportMetaProperties(JSC::JSGlobalObject* jsGlobalObject, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSModuleRecord*, JSC::JSValue)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#hostgetimportmetaproperties

    auto& vm = jsGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* metaProperties = JSC::constructEmptyObject(vm, jsGlobalObject->nullPrototypeObjectStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    URL responseURL = responseURLFromRequestURL(*jsGlobalObject, moduleKeyValue);
    RETURN_IF_EXCEPTION(scope, nullptr);

    metaProperties->putDirect(vm, JSC::Identifier::fromString(vm, "url"_s), JSC::jsString(vm, responseURL.string()));
    RETURN_IF_EXCEPTION(scope, nullptr);

    String resolveName = "resolve"_s;
    OwnerType ownerType = m_ownerType;
    auto* function = JSC::JSNativeStdFunction::create(vm, jsGlobalObject, 1, resolveName, [ownerType, responseURL](JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame) -> JSC::EncodedJSValue {
        JSC::VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto specifier = callFrame->argument(0).toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        auto* domGlobalObject = jsDynamicCast<JSDOMGlobalObject*>(globalObject);
        if (UNLIKELY(!domGlobalObject))
            return JSC::throwVMTypeError(globalObject, scope);

        auto* context = domGlobalObject->scriptExecutionContext();
        if (UNLIKELY(!context))
            return JSC::throwVMTypeError(globalObject, scope);

        auto result = resolveModuleSpecifier(*context, ownerType, domGlobalObject->importMap(), specifier, responseURL);
        if (UNLIKELY(!result))
            return JSC::throwVMTypeError(globalObject, scope, result.error());

        return JSC::JSValue::encode(JSC::jsString(vm, result->string()));
    });
    metaProperties->putDirect(vm, JSC::Identifier::fromString(vm, resolveName), function);

    return metaProperties;
}

void ScriptModuleLoader::notifyFinished(ModuleScriptLoader& moduleScriptLoader, URL&& sourceURL, Ref<DeferredPromise> promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script

    if (!m_loaders.remove(&moduleScriptLoader))
        return;
    moduleScriptLoader.clearClient();

    auto canonicalizeAndRegisterResponseURL = [&] (URL responseURL, bool hasRedirections, ResourceResponse::Source source) {
        // If we do not have redirection, we must reserve the source URL's fragment explicitly here since ResourceResponse::url() is the one when we first cache it to MemoryCache.
        // FIXME: We should track fragments through redirections.
        // https://bugs.webkit.org/show_bug.cgi?id=158420
        // https://bugs.webkit.org/show_bug.cgi?id=210490
        if (!hasRedirections && source != ResourceResponse::Source::ServiceWorker) {
            if (sourceURL.hasFragmentIdentifier())
                responseURL.setFragmentIdentifier(sourceURL.fragmentIdentifier());
        }
        return responseURL;
    };

    if (m_ownerType == OwnerType::Document) {
        auto& loader = static_cast<CachedModuleScriptLoader&>(moduleScriptLoader);
        auto& cachedScript = *loader.cachedScript();

        if (cachedScript.resourceError().isAccessControl()) {
            rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasPropagatedError, "Cross-origin script load denied by Cross-Origin Resource Sharing policy."_s);
            return;
        }

        if (cachedScript.errorOccurred()) {
            rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasPropagatedError, "Importing a module script failed."_s);
            return;
        }

        if (cachedScript.wasCanceled()) {
            rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasCanceled, "Importing a module script is canceled."_s);
            return;
        }

        ModuleType type = ModuleType::Invalid;
        auto mimeType = cachedScript.response().mimeType();
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType))
            type = ModuleType::JavaScript;
#if ENABLE(WEBASSEMBLY)
        else if (context().settingsValues().webAssemblyESMIntegrationEnabled && MIMETypeRegistry::isSupportedWebAssemblyMIMEType(mimeType))
            type = ModuleType::WebAssembly;
#endif
        else if (loader.parameters() && loader.parameters()->type() == JSC::ScriptFetchParameters::JSON && MIMETypeRegistry::isSupportedJSONMIMEType(mimeType))
            type = ModuleType::JSON;
        else {
            // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
            // The result of extracting a MIME type from response's header list (ignoring parameters) is not a JavaScript MIME type.
            // For historical reasons, fetching a classic script does not include MIME type checking. In contrast, module scripts will fail to load if they are not of a correct MIME type.
            rejectWithFetchError(promise.get(), TypeError, makeString("'", cachedScript.response().mimeType(), "' is not a valid JavaScript MIME type."));
            return;
        }

        if (auto* parameters = loader.parameters()) {
            String integrity = parameters->integrity();
            if (!integrity.isEmpty()) {
                if (!matchIntegrityMetadata(cachedScript, integrity)) {
                    m_context.addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Cannot load script ", integrityMismatchDescription(cachedScript, integrity)));
                    rejectWithFetchError(promise.get(), TypeError, "Cannot load script due to integrity mismatch"_s);
                    return;
                }
            }
        }

        URL responseURL = canonicalizeAndRegisterResponseURL(cachedScript.response().url(), cachedScript.hasRedirections(), cachedScript.response().source());
        m_requestURLToResponseURLMap.add(sourceURL.string(), WTFMove(responseURL));
        promise->resolveWithCallback([&] (JSDOMGlobalObject& jsGlobalObject) {
            switch (type) {
            case ModuleType::JavaScript:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { ScriptSourceCode { &cachedScript, JSC::SourceProviderSourceType::Module, loader.scriptFetcher() }.jsSourceCode() });
                break;
#if ENABLE(WEBASSEMBLY)
            case ModuleType::WebAssembly:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { WebAssemblyScriptSourceCode { &cachedScript, loader.scriptFetcher() }.jsSourceCode() });
                break;
#endif
            case ModuleType::JSON:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { ScriptSourceCode { &cachedScript, JSC::SourceProviderSourceType::JSON, loader.scriptFetcher() }.jsSourceCode() });
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        });
    } else {
        auto& loader = static_cast<WorkerModuleScriptLoader&>(moduleScriptLoader);

        if (loader.failed()) {
            ASSERT(!loader.retrievedFromServiceWorkerCache());
            auto& workerScriptLoader = loader.scriptLoader();
            ASSERT(workerScriptLoader.failed());
            if (workerScriptLoader.error().isAccessControl()) {
                rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasPropagatedError, "Cross-origin script load denied by Cross-Origin Resource Sharing policy."_s);
                return;
            }

            if (workerScriptLoader.error().isCancellation()) {
                rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasCanceled, "Importing a module script is canceled."_s);
                return;
            }

            rejectToPropagateNetworkError(promise.get(), ModuleFetchFailureKind::WasPropagatedError, "Importing a module script failed."_s);
            return;
        }

        ModuleType type = ModuleType::Invalid;
        auto mimeType = loader.responseMIMEType();
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType))
            type = ModuleType::JavaScript;
#if ENABLE(WEBASSEMBLY)
        else if (context().settingsValues().webAssemblyESMIntegrationEnabled && MIMETypeRegistry::isSupportedWebAssemblyMIMEType(mimeType))
            type = ModuleType::WebAssembly;
#endif
        else if (loader.parameters() && loader.parameters()->type() == JSC::ScriptFetchParameters::JSON && MIMETypeRegistry::isSupportedJSONMIMEType(mimeType))
            type = ModuleType::JSON;
        else {
            // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
            // The result of extracting a MIME type from response's header list (ignoring parameters) is not a JavaScript MIME type.
            // For historical reasons, fetching a classic script does not include MIME type checking. In contrast, module scripts will fail to load if they are not of a correct MIME type.
            rejectWithFetchError(promise.get(), TypeError, makeString("'", loader.responseMIMEType(), "' is not a valid JavaScript MIME type."));
            return;
        }

        URL responseURL = loader.responseURL();
        if (!loader.retrievedFromServiceWorkerCache()) {
            auto& workerScriptLoader = loader.scriptLoader();
            if (auto* parameters = loader.parameters()) {
                // If this is top-level-module, then we extract referrer-policy and apply to the dependent modules.
                if (parameters->isTopLevelModule())
                    static_cast<WorkerScriptFetcher&>(loader.scriptFetcher()).setReferrerPolicy(loader.referrerPolicy());
            }
            responseURL = canonicalizeAndRegisterResponseURL(responseURL, workerScriptLoader.isRedirected(), workerScriptLoader.responseSource());
#if ENABLE(SERVICE_WORKER)
            if (is<ServiceWorkerGlobalScope>(m_context))
                downcast<ServiceWorkerGlobalScope>(m_context).setScriptResource(sourceURL, ServiceWorkerContextData::ImportedScript { loader.script(), responseURL, loader.responseMIMEType() });
#endif
        }
        m_requestURLToResponseURLMap.add(sourceURL.string(), responseURL);
        promise->resolveWithCallback([&] (JSDOMGlobalObject& jsGlobalObject) {
            switch (type) {
            case ModuleType::JavaScript:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { ScriptSourceCode { loader.script(), WTFMove(responseURL), { }, JSC::SourceProviderSourceType::Module, loader.scriptFetcher() }.jsSourceCode() });
                break;
#if ENABLE(WEBASSEMBLY)
            case ModuleType::WebAssembly:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { WebAssemblyScriptSourceCode { loader.script(), WTFMove(responseURL), loader.scriptFetcher() }.jsSourceCode() });
                break;
#endif
            case ModuleType::JSON:
                return JSC::JSSourceCode::create(jsGlobalObject.vm(),
                    JSC::SourceCode { ScriptSourceCode { loader.script(), WTFMove(responseURL), { }, JSC::SourceProviderSourceType::JSON, loader.scriptFetcher() }.jsSourceCode() });
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        });
    }
}

}
