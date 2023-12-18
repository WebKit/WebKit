/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerJob.h"

#include "HTTPHeaderNames.h"
#include "JSDOMPromiseDeferred.h"
#include "MIMETypeRegistry.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerFetchResult.h"
#include "WorkerRunLoop.h"

namespace WebCore {

ServiceWorkerJob::ServiceWorkerJob(ServiceWorkerJobClient& client, RefPtr<DeferredPromise>&& promise, ServiceWorkerJobData&& jobData)
    : m_client(client)
    , m_jobData(WTFMove(jobData))
    , m_promise(WTFMove(promise))
    , m_contextIdentifier(client.contextIdentifier())
{
}

ServiceWorkerJob::~ServiceWorkerJob()
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
}

RefPtr<DeferredPromise> ServiceWorkerJob::takePromise()
{
    return WTFMove(m_promise);
}

void ServiceWorkerJob::failedWithException(const Exception& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);

    m_completed = true;
    m_client.jobFailedWithException(*this, exception);
}

void ServiceWorkerJob::resolvedWithRegistration(ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);

    m_completed = true;
    m_client.jobResolvedWithRegistration(*this, WTFMove(data), shouldNotifyWhenResolved);
}

void ServiceWorkerJob::resolvedWithUnregistrationResult(bool unregistrationResult)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);

    m_completed = true;
    m_client.jobResolvedWithUnregistrationResult(*this, unregistrationResult);
}

void ServiceWorkerJob::startScriptFetch(FetchOptions::Cache cachePolicy)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);

    m_client.startScriptFetchForJob(*this, cachePolicy);
}

static ResourceRequest scriptResourceRequest(ScriptExecutionContext& context, const URL& url)
{
    ResourceRequest request { url };
    request.setInitiatorIdentifier(context.resourceRequestIdentifier());
    return request;
}

static FetchOptions scriptFetchOptions(FetchOptions::Cache cachePolicy, FetchOptions::Destination destination)
{
    FetchOptions options;
    options.mode = FetchOptions::Mode::SameOrigin;
    options.cache = cachePolicy;
    options.redirect = FetchOptions::Redirect::Error;
    options.destination = destination;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    return options;
}

void ServiceWorkerJob::fetchScriptWithContext(ScriptExecutionContext& context, FetchOptions::Cache cachePolicy)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);

    auto source = m_jobData.workerType == WorkerType::Module ? WorkerScriptLoader::Source::ModuleScript : WorkerScriptLoader::Source::ClassicWorkerScript;

    m_scriptLoader = WorkerScriptLoader::create();
    auto request = scriptResourceRequest(context, m_jobData.scriptURL);
    request.addHTTPHeaderField(HTTPHeaderName::ServiceWorker, "script"_s);

    m_scriptLoader->loadAsynchronously(context, WTFMove(request), source, scriptFetchOptions(cachePolicy, FetchOptions::Destination::Serviceworker), ContentSecurityPolicyEnforcement::DoNotEnforce, ServiceWorkersMode::None, *this, WorkerRunLoop::defaultMode());
}

ResourceError ServiceWorkerJob::validateServiceWorkerResponse(const ServiceWorkerJobData& jobData, const ResourceResponse& response)
{
    // Extract a MIME type from the response's header list. If this MIME type (ignoring parameters) is not a JavaScript MIME type, then:
    if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(response.mimeType()))
        return { errorDomainWebKitInternal, 0, response.url(), "MIME Type is not a JavaScript MIME type"_s };

    auto serviceWorkerAllowed = response.httpHeaderField(HTTPHeaderName::ServiceWorkerAllowed);
    String maxScopeString;
    if (serviceWorkerAllowed.isNull()) {
        auto path = jobData.scriptURL.path();
        // Last part of the path is the script's filename.
        maxScopeString = path.left(path.reverseFind('/') + 1).toString();
    } else {
        auto maxScope = URL(jobData.scriptURL, serviceWorkerAllowed);
        if (SecurityOrigin::create(maxScope)->isSameOriginAs(SecurityOrigin::create(jobData.scriptURL)))
            maxScopeString = maxScope.path().toString();
    }

    auto scopeString = jobData.scopeURL.path();
    if (maxScopeString.isNull() || !scopeString.startsWith(maxScopeString))
        return { errorDomainWebKitInternal, 0, response.url(), "Scope URL should start with the given script URL"_s };

    return { };
}

void ServiceWorkerJob::didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse& response)
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(!m_completed);
    ASSERT(m_scriptLoader);

    auto error = validateServiceWorkerResponse(m_jobData, response);
    if (error.isNull())
        return;

    m_scriptLoader->cancel();
    m_scriptLoader = nullptr;

    Exception exception { ExceptionCode::SecurityError, error.localizedDescription() };
    m_client.jobFailedLoadingScript(*this, WTFMove(error), WTFMove(exception));
}

void ServiceWorkerJob::notifyFinished()
{
    ASSERT(m_creationThread.ptr() == &Thread::current());
    ASSERT(m_scriptLoader);

    auto scriptLoader = std::exchange(m_scriptLoader, { });

    if (!scriptLoader->failed()) {
        m_client.jobFinishedLoadingScript(*this, scriptLoader->fetchResult());
        return;
    }

    auto& error = scriptLoader->error();
    ASSERT(!error.isNull());

    m_client.jobFailedLoadingScript(*this, error, Exception { error.isAccessControl() ? ExceptionCode::SecurityError : ExceptionCode::TypeError, makeString("Script ", scriptLoader->url().string(), " load failed") });
}

bool ServiceWorkerJob::cancelPendingLoad()
{
    if (auto loader = std::exchange(m_scriptLoader, { })) {
        loader->cancel();
        return true;
    }
    return false;
}

} // namespace WebCore
