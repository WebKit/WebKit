/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "LoadableModuleScript.h"
#include "MIMETypeRegistry.h"
#include "ModuleFetchFailureKind.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include <runtime/Completion.h>
#include <runtime/JSInternalPromise.h>
#include <runtime/JSInternalPromiseDeferred.h>
#include <runtime/JSModuleRecord.h>
#include <runtime/JSScriptFetcher.h>
#include <runtime/JSSourceCode.h>
#include <runtime/JSString.h>
#include <runtime/Symbol.h>

namespace WebCore {

ScriptModuleLoader::ScriptModuleLoader(Document& document)
    : m_document(document)
{
}

ScriptModuleLoader::~ScriptModuleLoader()
{
    for (auto& loader : m_loaders)
        loader->clearClient();
}

static bool isRootModule(JSC::JSValue importerModuleKey)
{
    return importerModuleKey.isSymbol() || importerModuleKey.isUndefined();
}

static Expected<URL, ASCIILiteral> resolveModuleSpecifier(Document& document, const String& specifier, const URL& baseURL)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier

    URL absoluteURL(URL(), specifier);
    if (absoluteURL.isValid())
        return absoluteURL;

    if (!specifier.startsWith('/') && !specifier.startsWith("./") && !specifier.startsWith("../"))
        return makeUnexpected(ASCIILiteral("Module specifier does not start with \"/\", \"./\", or \"../\"."));

    auto result = document.completeURL(specifier, baseURL);
    if (!result.isValid())
        return makeUnexpected(ASCIILiteral("Module name does not resolve to a valid URL."));
    return result;
}

JSC::JSInternalPromise* ScriptModuleLoader::resolve(JSC::JSGlobalObject* jsGlobalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleNameValue, JSC::JSValue importerModuleKey, JSC::JSValue)
{
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto& jsPromise = *JSC::JSInternalPromiseDeferred::create(exec, &globalObject);
    auto promise = DeferredPromise::create(globalObject, jsPromise);

    // We use a Symbol as a special purpose; It means this module is an inline module.
    // So there is no correct URL to retrieve the module source code. If the module name
    // value is a Symbol, it is used directly as a module key.
    if (moduleNameValue.isSymbol()) {
        promise->resolve<IDLAny>(JSC::Symbol::create(exec->vm(), asSymbol(moduleNameValue)->privateName().uid()));
        return jsPromise.promise();
    }

    if (!moduleNameValue.isString()) {
        promise->reject(TypeError, ASCIILiteral("Importer module key is not a Symbol or a String."));
        return jsPromise.promise();
    }

    String specifier = asString(moduleNameValue)->value(exec);
    URL baseURL;
    if (isRootModule(importerModuleKey))
        baseURL = m_document.baseURL();
    else {
        ASSERT(importerModuleKey.isString());
        URL importerModuleRequestURL(URL(), asString(importerModuleKey)->value(exec));
        ASSERT_WITH_MESSAGE(importerModuleRequestURL.isValid(), "Invalid module referrer never starts importing dependent modules.");

        auto iterator = m_requestURLToResponseURLMap.find(importerModuleRequestURL);
        ASSERT_WITH_MESSAGE(iterator != m_requestURLToResponseURLMap.end(), "Module referrer must register itself to the map before starting importing dependent modules.");
        baseURL = iterator->value;
    }

    auto result = resolveModuleSpecifier(m_document, specifier, baseURL);
    if (!result) {
        promise->reject(TypeError, result.error());
        return jsPromise.promise();
    }

    promise->resolve<IDLDOMString>(result->string());
    return jsPromise.promise();
}

static void rejectToPropagateNetworkError(DeferredPromise& deferred, ModuleFetchFailureKind failureKind, ASCIILiteral message)
{
    deferred.rejectWithCallback([&] (JSC::ExecState& state, JSDOMGlobalObject&) {
        // We annotate exception with special private symbol. It allows us to distinguish these errors from the user thrown ones.
        JSC::VM& vm = state.vm();
        // FIXME: Propagate more descriptive error.
        // https://bugs.webkit.org/show_bug.cgi?id=167553
        auto* error = JSC::createTypeError(&state, message);
        ASSERT(error);
        error->putDirect(vm, static_cast<JSVMClientData&>(*vm.clientData).builtinNames().failureKindPrivateName(), JSC::jsNumber(static_cast<int32_t>(failureKind)));
        return error;
    });
}

JSC::JSInternalPromise* ScriptModuleLoader::fetch(JSC::JSGlobalObject* jsGlobalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue scriptFetcher)
{
    ASSERT(JSC::jsDynamicCast<JSC::JSScriptFetcher*>(exec->vm(), scriptFetcher));

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto& jsPromise = *JSC::JSInternalPromiseDeferred::create(exec, &globalObject);
    auto deferred = DeferredPromise::create(globalObject, jsPromise);
    if (moduleKeyValue.isSymbol()) {
        deferred->reject(TypeError, ASCIILiteral("Symbol module key should be already fulfilled with the inlined resource."));
        return jsPromise.promise();
    }

    if (!moduleKeyValue.isString()) {
        deferred->reject(TypeError, ASCIILiteral("Module key is not Symbol or String."));
        return jsPromise.promise();
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script

    URL completedURL(URL(), asString(moduleKeyValue)->value(exec));
    if (!completedURL.isValid()) {
        deferred->reject(TypeError, ASCIILiteral("Module key is a valid URL."));
        return jsPromise.promise();
    }

    auto loader = CachedModuleScriptLoader::create(*this, deferred.get(), *static_cast<CachedScriptFetcher*>(JSC::jsCast<JSC::JSScriptFetcher*>(scriptFetcher)->fetcher()));
    m_loaders.add(loader.copyRef());
    if (!loader->load(m_document, completedURL)) {
        loader->clearClient();
        m_loaders.remove(WTFMove(loader));
        rejectToPropagateNetworkError(deferred.get(), ModuleFetchFailureKind::WasErrored, ASCIILiteral("Importing a module script failed."));
        return jsPromise.promise();
    }

    return jsPromise.promise();
}

JSC::JSValue ScriptModuleLoader::evaluate(JSC::JSGlobalObject*, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue moduleRecordValue, JSC::JSValue)
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Currently, we only support JSModuleRecord.
    // Once the reflective part of the module loader is supported, we will handle arbitrary values.
    // https://whatwg.github.io/loader/#registry-prototype-provide
    auto* moduleRecord = jsDynamicDowncast<JSC::JSModuleRecord*>(vm, moduleRecordValue);
    if (!moduleRecord)
        return JSC::jsUndefined();

    URL sourceURL;
    if (moduleKeyValue.isSymbol())
        sourceURL = m_document.url();
    else if (moduleKeyValue.isString())
        sourceURL = URL(URL(), asString(moduleKeyValue)->value(exec));
    else
        return JSC::throwTypeError(exec, scope, ASCIILiteral("Module key is not Symbol or String."));

    if (!sourceURL.isValid())
        return JSC::throwTypeError(exec, scope, ASCIILiteral("Module key is an invalid URL."));

    if (auto* frame = m_document.frame())
        return frame->script().evaluateModule(sourceURL, *moduleRecord);
    return JSC::jsUndefined();
}

static JSC::JSInternalPromise* rejectPromise(JSC::ExecState& state, JSDOMGlobalObject& globalObject, ExceptionCode ec, ASCIILiteral message)
{
    auto& jsPromise = *JSC::JSInternalPromiseDeferred::create(&state, &globalObject);
    auto deferred = DeferredPromise::create(globalObject, jsPromise);
    deferred->reject(ec, WTFMove(message));
    return jsPromise.promise();
}

JSC::JSInternalPromise* ScriptModuleLoader::importModule(JSC::JSGlobalObject* jsGlobalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSString* moduleName, const JSC::SourceOrigin& sourceOrigin)
{
    auto& state = *exec;
    JSC::VM& vm = exec->vm();
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);

    // If SourceOrigin and/or CachedScriptFetcher is null, we import the module with the default fetcher.
    // SourceOrigin can be null if the source code is not coupled with the script file.
    // The examples,
    //     1. The code evaluated by the inspector.
    //     2. The other unusual code execution like the evaluation through the NPAPI.
    //     3. The code from injected bundle's script.
    //     4. The code from extension script.
    URL baseURL;
    RefPtr<JSC::ScriptFetcher> scriptFetcher;
    if (sourceOrigin.isNull()) {
        baseURL = m_document.baseURL();
        scriptFetcher = CachedScriptFetcher::create(m_document.charset());
    } else {
        baseURL = URL(URL(), sourceOrigin.string());
        if (!baseURL.isValid())
            return rejectPromise(state, globalObject, TypeError, ASCIILiteral("Importer module key is not a Symbol or a String."));

        if (sourceOrigin.fetcher())
            scriptFetcher = sourceOrigin.fetcher();
        else
            scriptFetcher = CachedScriptFetcher::create(m_document.charset());
    }
    ASSERT(baseURL.isValid());
    ASSERT(scriptFetcher);

    auto specifier = moduleName->value(exec);
    auto result = resolveModuleSpecifier(m_document, specifier, baseURL);
    if (!result)
        return rejectPromise(state, globalObject, TypeError, result.error());

    return JSC::importModule(exec, JSC::Identifier::fromString(&vm, result->string()), JSC::JSScriptFetcher::create(vm, WTFMove(scriptFetcher) ));
}

void ScriptModuleLoader::notifyFinished(CachedModuleScriptLoader& loader, RefPtr<DeferredPromise> promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script

    if (!m_loaders.remove(&loader))
        return;
    loader.clearClient();

    auto& cachedScript = *loader.cachedScript();

    if (cachedScript.resourceError().isAccessControl()) {
        promise->reject(TypeError, ASCIILiteral("Cross-origin script load denied by Cross-Origin Resource Sharing policy."));
        return;
    }

    if (cachedScript.errorOccurred()) {
        rejectToPropagateNetworkError(*promise, ModuleFetchFailureKind::WasErrored, ASCIILiteral("Importing a module script failed."));
        return;
    }

    if (cachedScript.wasCanceled()) {
        rejectToPropagateNetworkError(*promise, ModuleFetchFailureKind::WasCanceled, ASCIILiteral("Importing a module script is canceled."));
        return;
    }

    if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(cachedScript.response().mimeType())) {
        // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
        // The result of extracting a MIME type from response's header list (ignoring parameters) is not a JavaScript MIME type.
        // For historical reasons, fetching a classic script does not include MIME type checking. In contrast, module scripts will fail to load if they are not of a correct MIME type.
        promise->reject(TypeError, makeString("'", cachedScript.response().mimeType(), "' is not a valid JavaScript MIME type."));
        return;
    }

    m_requestURLToResponseURLMap.add(cachedScript.url(), cachedScript.response().url());
    promise->resolveWithCallback([&] (JSC::ExecState& state, JSDOMGlobalObject&) {
        return JSC::JSSourceCode::create(state.vm(),
            JSC::SourceCode { ScriptSourceCode { &cachedScript, JSC::SourceProviderSourceType::Module, loader.scriptFetcher() }.jsSourceCode() });
    });
}

}
