/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WorkerModuleScriptLoader.h"

#include "CachedScriptFetcher.h"
#include "ContentSecurityPolicy.h"
#include "DOMWrapperWorld.h"
#include "JSDOMBinding.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "ModuleFetchParameters.h"
#include "ResourceLoaderOptions.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "ScriptSourceCode.h"
#include "ServiceWorkerGlobalScope.h"
#include "WorkerScriptFetcher.h"
#include "WorkerScriptLoader.h"

namespace WebCore {

Ref<WorkerModuleScriptLoader> WorkerModuleScriptLoader::create(ModuleScriptLoaderClient& client, DeferredPromise& promise, WorkerScriptFetcher& scriptFetcher, RefPtr<JSC::ScriptFetchParameters>&& parameters)
{
    return adoptRef(*new WorkerModuleScriptLoader(client, promise, scriptFetcher, WTFMove(parameters)));
}

WorkerModuleScriptLoader::WorkerModuleScriptLoader(ModuleScriptLoaderClient& client, DeferredPromise& promise, WorkerScriptFetcher& scriptFetcher, RefPtr<JSC::ScriptFetchParameters>&& parameters)
    : ModuleScriptLoader(client, promise, scriptFetcher, WTFMove(parameters))
    , m_scriptLoader(WorkerScriptLoader::create())
{
}

WorkerModuleScriptLoader::~WorkerModuleScriptLoader()
{
    protectedScriptLoader()->cancel();
}

void WorkerModuleScriptLoader::load(ScriptExecutionContext& context, URL&& sourceURL)
{
    m_sourceURL = WTFMove(sourceURL);

    if (auto* globalScope = dynamicDowncast<ServiceWorkerGlobalScope>(context)) {
        if (auto* scriptResource = globalScope->scriptResource(m_sourceURL)) {
            m_script = scriptResource->script;
            m_responseURL = scriptResource->responseURL;
            m_responseMIMEType = scriptResource->mimeType;
            m_retrievedFromServiceWorkerCache = true;
            notifyClientFinished();
            return;
        }
    }

    ResourceRequest request { m_sourceURL };

    FetchOptions fetchOptions;
    fetchOptions.mode = FetchOptions::Mode::Cors;
    fetchOptions.cache = FetchOptions::Cache::Default;
    fetchOptions.redirect = FetchOptions::Redirect::Follow;
    fetchOptions.credentials = static_cast<WorkerScriptFetcher&>(scriptFetcher()).credentials();
    fetchOptions.destination = static_cast<WorkerScriptFetcher&>(scriptFetcher()).destination();
    fetchOptions.referrerPolicy = static_cast<WorkerScriptFetcher&>(scriptFetcher()).referrerPolicy();

    bool cspCheckFailed = false;
    ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;
    if (!context.shouldBypassMainWorldContentSecurityPolicy()) {
        CheckedPtr contentSecurityPolicy = context.contentSecurityPolicy();
        if (fetchOptions.destination == FetchOptions::Destination::Script) {
            cspCheckFailed = contentSecurityPolicy && !contentSecurityPolicy->allowScriptFromSource(m_sourceURL);
            contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective;
        } else {
            cspCheckFailed = contentSecurityPolicy && !contentSecurityPolicy->allowWorkerFromSource(m_sourceURL);
            contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective;
        }
    }

    if (cspCheckFailed) {
        protectedScriptLoader()->notifyError();
        ASSERT(!m_failed);
        notifyFinished();
        ASSERT(m_failed);
        return;
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
    // If destination is "worker" or "sharedworker" and the top-level module fetch flag is set, then set request's mode to "same-origin".
    if (fetchOptions.destination == FetchOptions::Destination::Worker || fetchOptions.destination == FetchOptions::Destination::Serviceworker) {
        if (parameters() && parameters()->isTopLevelModule())
            fetchOptions.mode = FetchOptions::Mode::SameOrigin;
    }

    protectedScriptLoader()->loadAsynchronously(context, WTFMove(request), WorkerScriptLoader::Source::ModuleScript, WTFMove(fetchOptions), contentSecurityPolicyEnforcement, ServiceWorkersMode::All, *this, taskMode());
}

Ref<WorkerScriptLoader> WorkerModuleScriptLoader::protectedScriptLoader()
{
    return m_scriptLoader;
}

ReferrerPolicy WorkerModuleScriptLoader::referrerPolicy()
{
    if (auto policy = parseReferrerPolicy(m_scriptLoader->referrerPolicy(), ReferrerPolicySource::HTTPHeader))
        return *policy;
    return ReferrerPolicy::EmptyString;
}

void WorkerModuleScriptLoader::notifyFinished()
{
    ASSERT(m_promise);

    if (m_scriptLoader->failed())
        m_failed = true;
    else {
        m_script = m_scriptLoader->script();
        m_responseURL = m_scriptLoader->responseURL();
        m_responseMIMEType = m_scriptLoader->responseMIMEType();
    }

    notifyClientFinished();
}

void WorkerModuleScriptLoader::notifyClientFinished()
{
    Ref protectedThis { *this };

    if (m_client)
        m_client->notifyFinished(*this, WTFMove(m_sourceURL), m_promise.releaseNonNull());
}

String WorkerModuleScriptLoader::taskMode()
{
    return "loadModulesInWorkerOrWorkletMode"_s;
}

}
