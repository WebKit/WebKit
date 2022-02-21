/*
 * Copyright (C) 2009-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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
#include "WorkerScriptLoader.h"

#include "ContentSecurityPolicy.h"
#include "Exception.h"
#include "FetchIdioms.h"
#include "MIMETypeRegistry.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorker.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerGlobalScope.h"
#include "TextResourceDecoder.h"
#include "WorkerFetchResult.h"
#include "WorkerGlobalScope.h"
#include "WorkerScriptLoaderClient.h"
#include "WorkerThreadableLoader.h"
#include <wtf/Ref.h>

namespace WebCore {

WorkerScriptLoader::WorkerScriptLoader()
    : m_script(ScriptBuffer::empty())
{
}

WorkerScriptLoader::~WorkerScriptLoader() = default;

std::optional<Exception> WorkerScriptLoader::loadSynchronously(ScriptExecutionContext* scriptExecutionContext, const URL& url, Source source, FetchOptions::Mode mode, FetchOptions::Cache cachePolicy, ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement, const String& initiatorIdentifier)
{
    ASSERT(scriptExecutionContext);
    auto& workerGlobalScope = downcast<WorkerGlobalScope>(*scriptExecutionContext);

    m_url = url;
    m_lastRequestURL = url;
    m_source = source;
    m_destination = FetchOptions::Destination::Script;
    m_isCOEPEnabled = scriptExecutionContext->settingsValues().crossOriginEmbedderPolicyEnabled;

#if ENABLE(SERVICE_WORKER)
    bool isServiceWorkerGlobalScope = is<ServiceWorkerGlobalScope>(workerGlobalScope);

    if (isServiceWorkerGlobalScope) {
        if (auto* scriptResource = downcast<ServiceWorkerGlobalScope>(workerGlobalScope).scriptResource(url)) {
            m_script = scriptResource->script;
            m_responseURL = scriptResource->responseURL;
            m_responseMIMEType = scriptResource->mimeType;
            return std::nullopt;
        }
    }
#endif

    std::unique_ptr<ResourceRequest> request(createResourceRequest(initiatorIdentifier));
    if (!request)
        return std::nullopt;

    ASSERT_WITH_SECURITY_IMPLICATION(is<WorkerGlobalScope>(scriptExecutionContext));

    // Only used for importScripts that prescribes NoCors mode.
    ASSERT(mode == FetchOptions::Mode::NoCors);
    request->setRequester(ResourceRequest::Requester::ImportScripts);

    ThreadableLoaderOptions options;
    options.credentials = FetchOptions::Credentials::Include;
    options.mode = mode;
    options.cache = cachePolicy;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.contentSecurityPolicyEnforcement = contentSecurityPolicyEnforcement;
    options.destination = m_destination;
#if ENABLE(SERVICE_WORKER)
    options.serviceWorkersMode = isServiceWorkerGlobalScope ? ServiceWorkersMode::None : ServiceWorkersMode::All;
    if (auto* activeServiceWorker = workerGlobalScope.activeServiceWorker())
        options.serviceWorkerRegistrationIdentifier = activeServiceWorker->registrationIdentifier();
#endif
    WorkerThreadableLoader::loadResourceSynchronously(workerGlobalScope, WTFMove(*request), *this, options);

    // If the fetching attempt failed, throw a NetworkError exception and abort all these steps.
    if (failed())
        return Exception { NetworkError, m_error.sanitizedDescription() };

#if ENABLE(SERVICE_WORKER)
    if (isServiceWorkerGlobalScope) {
        if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(responseMIMEType()))
            return Exception { NetworkError, "mime type is not a supported JavaScript mime type"_s };

        downcast<ServiceWorkerGlobalScope>(workerGlobalScope).setScriptResource(url, ServiceWorkerContextData::ImportedScript { script(), m_responseURL, m_responseMIMEType });
    }
#endif
    return std::nullopt;
}

void WorkerScriptLoader::loadAsynchronously(ScriptExecutionContext& scriptExecutionContext, ResourceRequest&& scriptRequest, Source source, FetchOptions&& fetchOptions, ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement, ServiceWorkersMode serviceWorkerMode, WorkerScriptLoaderClient& client, String&& taskMode)
{
    m_client = &client;
    m_url = scriptRequest.url();
    m_lastRequestURL = scriptRequest.url();
    m_source = source;
    m_destination = fetchOptions.destination;
    m_isCOEPEnabled = scriptExecutionContext.settingsValues().crossOriginEmbedderPolicyEnabled;

    ASSERT(scriptRequest.httpMethod() == "GET");

    auto request = makeUnique<ResourceRequest>(WTFMove(scriptRequest));
    if (!request)
        return;

    ThreadableLoaderOptions options { WTFMove(fetchOptions) };
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.contentSecurityPolicyEnforcement = contentSecurityPolicyEnforcement;
    if (fetchOptions.destination == FetchOptions::Destination::Serviceworker)
        options.certificateInfoPolicy = CertificateInfoPolicy::IncludeCertificateInfo;

    // FIXME: We should drop the sameOriginDataURLFlag flag and implement the latest Fetch specification.
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    // A service worker job can be executed from a worker context or a document context.
    options.serviceWorkersMode = serviceWorkerMode;
#if ENABLE(SERVICE_WORKER)
    if (auto* activeServiceWorker = scriptExecutionContext.activeServiceWorker())
        options.serviceWorkerRegistrationIdentifier = activeServiceWorker->registrationIdentifier();
#endif

    // During create, callbacks may happen which remove the last reference to this object.
    Ref<WorkerScriptLoader> protectedThis(*this);
    m_threadableLoader = ThreadableLoader::create(scriptExecutionContext, *this, WTFMove(*request), options, { }, WTFMove(taskMode));
}

const URL& WorkerScriptLoader::responseURL() const
{
    ASSERT(!failed());
    return m_responseURL;
}

std::unique_ptr<ResourceRequest> WorkerScriptLoader::createResourceRequest(const String& initiatorIdentifier)
{
    auto request = makeUnique<ResourceRequest>(m_url);
    request->setHTTPMethod("GET"_s);
    request->setInitiatorIdentifier(initiatorIdentifier);
    return request;
}

static ResourceError constructJavaScriptMIMETypeError(const ResourceResponse& response)
{
    auto message = makeString("Refused to execute ", response.url().stringCenterEllipsizedToLength(), " as script because ", response.mimeType(), " is not a script MIME type.");
    return { errorDomainWebKitInternal, 0, response.url(), WTFMove(message), ResourceError::Type::AccessControl };
}

ResourceError WorkerScriptLoader::validateWorkerResponse(const ResourceResponse& response, Source source, FetchOptions::Destination destination)
{
    if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode())
        return { errorDomainWebKitInternal, 0, response.url(), "Response is not 2xx"_s, ResourceError::Type::General };

    if (!isScriptAllowedByNosniff(response)) {
        auto message = makeString("Refused to execute ", response.url().stringCenterEllipsizedToLength(), " as script because \"X-Content-Type-Options: nosniff\" was given and its Content-Type is not a script MIME type.");
        return { errorDomainWebKitInternal, 0, response.url(), WTFMove(message), ResourceError::Type::General };
    }

    switch (source) {
    case Source::ClassicWorkerScript:
        // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-worker-script (Step 5)
        // This is the result a dedicated / shared / service worker script fetch.
        if (response.url().protocolIsInHTTPFamily() && !MIMETypeRegistry::isSupportedJavaScriptMIMEType(response.mimeType()))
            return constructJavaScriptMIMETypeError(response);
        break;
    case Source::ClassicWorkerImport:
        // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-worker-imported-script (Step 5).
        // This is the result of an importScripts() call.
        if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(response.mimeType()))
            return constructJavaScriptMIMETypeError(response);
        break;
    case Source::ModuleScript:
        if (shouldBlockResponseDueToMIMEType(response, destination))
            return constructJavaScriptMIMETypeError(response);
        break;
    }

    return { };
}

void WorkerScriptLoader::redirectReceived(const URL& redirectURL)
{
    m_lastRequestURL = redirectURL;
}

void WorkerScriptLoader::didReceiveResponse(ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    m_error = validateWorkerResponse(response, m_source, m_destination);
    if (!m_error.isNull()) {
        m_failed = true;
        return;
    }

    m_responseURL = response.url();
    m_certificateInfo = response.certificateInfo() ? *response.certificateInfo() : CertificateInfo();
    m_responseMIMEType = response.mimeType();
    m_responseSource = response.source();
    m_isRedirected = response.isRedirected();
    m_contentSecurityPolicy = ContentSecurityPolicyResponseHeaders { response };
    if (m_isCOEPEnabled)
        m_crossOriginEmbedderPolicy = obtainCrossOriginEmbedderPolicy(response, nullptr);
    m_referrerPolicy = response.httpHeaderField(HTTPHeaderName::ReferrerPolicy);
    if (m_client)
        m_client->didReceiveResponse(identifier, response);
}

void WorkerScriptLoader::didReceiveData(const SharedBuffer& buffer)
{
    if (m_failed)
        return;

    if (!m_decoder)
        m_decoder = TextResourceDecoder::create("text/javascript"_s, "UTF-8");

    if (buffer.isEmpty())
        return;

    m_script.append(m_decoder->decode(buffer.data(), buffer.size()));
}

void WorkerScriptLoader::didFinishLoading(ResourceLoaderIdentifier identifier, const NetworkLoadMetrics&)
{
    if (m_failed) {
        notifyError();
        return;
    }

    if (m_decoder)
        m_script.append(m_decoder->flush());

    m_identifier = identifier;
    notifyFinished();
}

void WorkerScriptLoader::didFail(const ResourceError& error)
{
    m_error = error;
    notifyError();
}

void WorkerScriptLoader::notifyError()
{
    m_failed = true;
    if (m_error.isNull())
        m_error = { errorDomainWebKitInternal, 0, url(), "Failed to load script", ResourceError::Type::General };
    notifyFinished();
}

void WorkerScriptLoader::notifyFinished()
{
    m_threadableLoader = nullptr;
    if (!m_client || m_finishing)
        return;

    m_finishing = true;
    m_client->notifyFinished();
}

void WorkerScriptLoader::cancel()
{
    if (!m_threadableLoader)
        return;

    m_client = nullptr;
    m_threadableLoader->cancel();
    m_threadableLoader = nullptr;
}

WorkerFetchResult WorkerScriptLoader::fetchResult() const
{
    if (m_failed)
        return workerFetchError(error());
    return { script(), lastRequestURL(), certificateInfo(), contentSecurityPolicy(), crossOriginEmbedderPolicy(), referrerPolicy(), { } };
}

} // namespace WebCore
