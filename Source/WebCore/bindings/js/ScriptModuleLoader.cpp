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
#include "Document.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "LoadableModuleScript.h"
#include "MIMETypeRegistry.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
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
        const_cast<CachedModuleScriptLoader&>(loader.get()).clearClient();
}


static bool isRootModule(JSC::JSValue importerModuleKey)
{
    return importerModuleKey.isSymbol() || importerModuleKey.isUndefined();
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
        promise->resolve<IDLAny>(toJS(exec, &globalObject, asSymbol(moduleNameValue)->privateName()));
        return jsPromise.promise();
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier

    if (!moduleNameValue.isString()) {
        promise->reject(TypeError, ASCIILiteral("Module specifier is not Symbol or String."));
        return jsPromise.promise();
    }

    String specifier = asString(moduleNameValue)->value(exec);

    // 1. Apply the URL parser to specifier. If the result is not failure, return the result.
    URL absoluteURL(URL(), specifier);
    if (absoluteURL.isValid()) {
        promise->resolve<IDLDOMString>(absoluteURL.string());
        return jsPromise.promise();
    }

    // 2. If specifier does not start with the character U+002F SOLIDUS (/), the two-character sequence U+002E FULL STOP, U+002F SOLIDUS (./),
    //    or the three-character sequence U+002E FULL STOP, U+002E FULL STOP, U+002F SOLIDUS (../), return failure and abort these steps.
    if (!specifier.startsWith('/') && !specifier.startsWith("./") && !specifier.startsWith("../")) {
        promise->reject(TypeError, ASCIILiteral("Module specifier does not start with \"/\", \"./\", or \"../\"."));
        return jsPromise.promise();
    }

    // 3. Return the result of applying the URL parser to specifier with script's base URL as the base URL.

    URL completedURL;

    if (isRootModule(importerModuleKey))
        completedURL = m_document.completeURL(specifier);
    else if (importerModuleKey.isString()) {
        URL importerModuleRequestURL(URL(), asString(importerModuleKey)->value(exec));
        if (!importerModuleRequestURL.isValid()) {
            promise->reject(TypeError, ASCIILiteral("Importer module key is an invalid URL."));
            return jsPromise.promise();
        }

        URL importerModuleResponseURL = m_requestURLToResponseURLMap.get(importerModuleRequestURL);
        if (!importerModuleResponseURL.isValid()) {
            promise->reject(TypeError, ASCIILiteral("Importer module has an invalid response URL."));
            return jsPromise.promise();
        }

        completedURL = m_document.completeURL(specifier, importerModuleResponseURL);
    } else {
        promise->reject(TypeError, ASCIILiteral("Importer module key is not Symbol or String."));
        return jsPromise.promise();
    }

    if (!completedURL.isValid()) {
        promise->reject(TypeError, ASCIILiteral("Module name constructs an invalid URL."));
        return jsPromise.promise();
    }

    promise->resolve<IDLDOMString>(completedURL.string());
    return jsPromise.promise();
}

JSC::JSInternalPromise* ScriptModuleLoader::fetch(JSC::JSGlobalObject* jsGlobalObject, JSC::ExecState* exec, JSC::JSModuleLoader*, JSC::JSValue moduleKeyValue, JSC::JSValue scriptFetcher)
{
    ASSERT(JSC::jsDynamicCast<JSC::JSScriptFetcher*>(scriptFetcher));

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

    if (auto* frame = m_document.frame()) {
        auto loader = CachedModuleScriptLoader::create(*this, deferred.get());
        m_loaders.add(loader.copyRef());
        if (!loader->load(m_document, *static_cast<LoadableScript*>(JSC::jsCast<JSC::JSScriptFetcher*>(scriptFetcher)->fetcher()), completedURL)) {
            loader->clearClient();
            m_loaders.remove(WTFMove(loader));
            deferred->reject(frame->script().moduleLoaderAlreadyReportedErrorSymbol());
            return jsPromise.promise();
        }
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
    auto* moduleRecord = jsDynamicDowncast<JSC::JSModuleRecord*>(moduleRecordValue);
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

void ScriptModuleLoader::notifyFinished(CachedModuleScriptLoader& loader, RefPtr<DeferredPromise> promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script

    if (!m_loaders.remove(&loader))
        return;
    loader.clearClient();

    auto& cachedScript = *loader.cachedScript();

    bool failed = false;

    if (cachedScript.resourceError().isAccessControl()) {
        promise->reject(TypeError, ASCIILiteral("Cross-origin script load denied by Cross-Origin Resource Sharing policy."));
        return;
    }

    if (cachedScript.errorOccurred())
        failed = true;
    else if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(cachedScript.response().mimeType())) {
        // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
        // The result of extracting a MIME type from response's header list (ignoring parameters) is not a JavaScript MIME type.
        // For historical reasons, fetching a classic script does not include MIME type checking. In contrast, module scripts will fail to load if they are not of a correct MIME type.
        promise->reject(TypeError, makeString("'", cachedScript.response().mimeType(), "' is not a valid JavaScript MIME type."));
        return;
    }

    auto* frame = m_document.frame();
    if (!frame)
        return;

    if (failed) {
        // Reject a promise to propagate the error back all the way through module promise chains to the script element.
        promise->reject(frame->script().moduleLoaderAlreadyReportedErrorSymbol());
        return;
    }

    if (cachedScript.wasCanceled()) {
        promise->reject(frame->script().moduleLoaderFetchingIsCanceledSymbol());
        return;
    }

    m_requestURLToResponseURLMap.add(cachedScript.url(), cachedScript.response().url());
    ScriptSourceCode scriptSourceCode(&cachedScript, JSC::SourceProviderSourceType::Module);
    promise->resolveWithCallback([] (JSC::ExecState& state, JSDOMGlobalObject&, JSC::SourceCode sourceCode) {
        return JSC::JSSourceCode::create(state.vm(), WTFMove(sourceCode));
    }, scriptSourceCode.jsSourceCode());
}

}
