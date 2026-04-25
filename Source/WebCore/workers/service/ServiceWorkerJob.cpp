/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ServiceWorkerJob);

Ref<ServiceWorkerJob> ServiceWorkerJob::create(ServiceWorkerJobClient& client, Ref<DeferredPromise>&& promise, ServiceWorkerJobData&& jobData)
{
    return adoptRef(*new ServiceWorkerJob(client, WTF::move(promise), WTF::move(jobData)));
}

ServiceWorkerJob::ServiceWorkerJob(ServiceWorkerJobClient& client, Ref<DeferredPromise>&& promise, ServiceWorkerJobData&& jobData)
    : m_client(client)
    , m_jobData(WTF::move(jobData))
    , m_promise(WTF::move(promise))
    , m_contextIdentifier(client.contextIdentifier())
{
}

ServiceWorkerJob::~ServiceWorkerJob()
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
}

Ref<DeferredPromise> ServiceWorkerJob::takePromise()
{
    return WTF::move(m_promise);
}

void ServiceWorkerJob::failedWithException(const Exception& exception)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);

    m_completed = true;
    if (RefPtr client = m_client.get())
        client->jobFailedWithException(*this, exception);
}

void ServiceWorkerJob::resolvedWithRegistration(ServiceWorkerRegistrationData&& data, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);

    m_completed = true;
    if (RefPtr client = m_client.get())
        client->jobResolvedWithRegistration(*this, WTF::move(data), shouldNotifyWhenResolved);
}

void ServiceWorkerJob::resolvedWithUnregistrationResult(bool unregistrationResult)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);

    m_completed = true;
    if (RefPtr client = m_client.get())
        client->jobResolvedWithUnregistrationResult(*this, unregistrationResult);
}

void ServiceWorkerJob::startScriptFetch(FetchOptions::Cache cachePolicy)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);

    if (RefPtr client = m_client.get())
        client->startScriptFetchForJob(*this, cachePolicy);
}

static ResourceRequest scriptResourceRequest(ScriptExecutionContext& context, const URL& url)
{
    ResourceRequest request { URL { url } };
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
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);

    auto source = m_jobData.workerType == WorkerType::Module ? WorkerScriptLoader::Source::ModuleScript : WorkerScriptLoader::Source::ClassicWorkerScript;

    Ref scriptLoader = WorkerScriptLoader::create();
    m_scriptLoader = scriptLoader.copyRef();
    auto request = scriptResourceRequest(context, m_jobData.scriptURL);
    request.addHTTPHeaderField(HTTPHeaderName::ServiceWorker, "script"_s);

    scriptLoader->loadAsynchronously(context, WTF::move(request), source, scriptFetchOptions(cachePolicy, FetchOptions::Destination::Serviceworker), ContentSecurityPolicyEnforcement::DoNotEnforce, ServiceWorkersMode::None, *this, WorkerRunLoop::defaultMode());
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

void ServiceWorkerJob::didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse& response)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(!m_completed);
    ASSERT(m_scriptLoader);

    auto error = validateServiceWorkerResponse(m_jobData, response);
    if (error.isNull())
        return;

    Ref { *m_scriptLoader }->cancel();
    m_scriptLoader = nullptr;

    if (RefPtr client = m_client.get()) {
        Exception exception { ExceptionCode::SecurityError, error.localizedDescription() };
        client->jobFailedLoadingScript(*this, WTF::move(error), WTF::move(exception));
    }
}

void ServiceWorkerJob::notifyFinished(std::optional<ScriptExecutionContextIdentifier>)
{
    ASSERT(m_creationThread.ptr() == &Thread::currentSingleton());
    ASSERT(m_scriptLoader);

    auto scriptLoader = std::exchange(m_scriptLoader, { });

    if (!scriptLoader->failed()) {
        if (RefPtr client = m_client.get())
            client->jobFinishedLoadingScript(*this, scriptLoader->fetchResult());
        return;
    }

    if (RefPtr client = m_client.get()) {
        auto& error = scriptLoader->error();
        ASSERT(!error.isNull());
        client->jobFailedLoadingScript(*this, error, Exception { error.isAccessControl() ? ExceptionCode::SecurityError : ExceptionCode::TypeError, makeString("Script "_s, scriptLoader->url().string(), " load failed"_s) });
    }
}

bool ServiceWorkerJob::cancelPendingLoad()
{
    if (auto loader = std::exchange(m_scriptLoader, { })) {
        loader->cancel();
        return true;
    }
    return false;
}

bool ServiceWorkerJob::isRegistering() const
{
    return !m_completed && m_jobData.type == ServiceWorkerJobType::Register;
}

} // namespace WebCore
