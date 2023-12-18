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
#include "ServiceWorkerJobData.h"

#include <wtf/CrossThreadCopier.h>

namespace WebCore {

static inline ServiceWorkerOrClientIdentifier serviceWorkerOrClientIdentifier(const ServiceWorkerOrClientIdentifier& localSourceContext)
{
    return WTF::switchOn(localSourceContext, [&](ScriptExecutionContextIdentifier contextIdentifier) -> ServiceWorkerOrClientIdentifier {
        return contextIdentifier;
    }, [&](ServiceWorkerIdentifier serviceWorkerIdentifier) -> ServiceWorkerOrClientIdentifier {
        return serviceWorkerIdentifier;
    });
}

ServiceWorkerJobData::ServiceWorkerJobData(SWServerConnectionIdentifier connectionIdentifier, const ServiceWorkerOrClientIdentifier& localSourceContext)
    : sourceContext(serviceWorkerOrClientIdentifier(localSourceContext))
    , m_identifier { connectionIdentifier, ServiceWorkerJobIdentifier::generate() }
{
}

ServiceWorkerJobData::ServiceWorkerJobData(Identifier identifier, const ServiceWorkerOrClientIdentifier& localSourceContext)
    : sourceContext(serviceWorkerOrClientIdentifier(localSourceContext))
    , m_identifier { identifier }
{
}

ServiceWorkerJobData::ServiceWorkerJobData(WebCore::ServiceWorkerJobDataIdentifier&& identifier, URL&& scriptURL, URL&& clientCreationURL, WebCore::SecurityOriginData&& topOrigin, URL&& scopeURL, WebCore::ServiceWorkerOrClientIdentifier&& sourceContext, WebCore::WorkerType workerType, WebCore::ServiceWorkerJobType type, String&& domainForCachePartition, bool isFromServiceWorkerPage, std::optional<WebCore::ServiceWorkerRegistrationOptions>&& registrationOptions)
    : scriptURL(WTFMove(scriptURL))
    , clientCreationURL(WTFMove(clientCreationURL))
    , topOrigin(WTFMove(topOrigin))
    , scopeURL(WTFMove(scopeURL))
    , sourceContext(WTFMove(sourceContext))
    , workerType(workerType)
    , type(type)
    , domainForCachePartition(WTFMove(domainForCachePartition))
    , isFromServiceWorkerPage(isFromServiceWorkerPage)
    , registrationOptions(WTFMove(registrationOptions))
    , m_identifier(WTFMove(identifier))
{
}

ServiceWorkerRegistrationKey ServiceWorkerJobData::registrationKey() const
{
    URL scope = scopeURL;
    scope.removeFragmentIdentifier();
    return { SecurityOriginData { topOrigin }, WTFMove(scope) };
}

std::optional<ScriptExecutionContextIdentifier> ServiceWorkerJobData::serviceWorkerPageIdentifier() const
{
    if (isFromServiceWorkerPage && std::holds_alternative<ScriptExecutionContextIdentifier>(sourceContext))
        return std::get<ScriptExecutionContextIdentifier>(sourceContext);
    return std::nullopt;
}

ServiceWorkerJobData ServiceWorkerJobData::isolatedCopy() const
{
    ServiceWorkerJobData result;
    result.m_identifier = identifier();
    result.sourceContext = sourceContext;
    result.workerType = workerType;
    result.type = type;
    result.isFromServiceWorkerPage = isFromServiceWorkerPage;

    result.scriptURL = scriptURL.isolatedCopy();
    result.clientCreationURL = clientCreationURL.isolatedCopy();
    result.topOrigin = topOrigin.isolatedCopy();
    result.scopeURL = scopeURL.isolatedCopy();
    result.domainForCachePartition = domainForCachePartition.isolatedCopy();
    if (registrationOptions) {
        ASSERT(type == ServiceWorkerJobType::Register);
        result.registrationOptions = crossThreadCopy(registrationOptions);
    }
    return result;
}

// https://w3c.github.io/ServiceWorker/#dfn-job-equivalent
bool ServiceWorkerJobData::isEquivalent(const ServiceWorkerJobData& job) const
{
    if (type != job.type)
        return false;

    switch (type) {
    case ServiceWorkerJobType::Register:
        ASSERT(registrationOptions && job.registrationOptions);
        return scopeURL == job.scopeURL
            && scriptURL == job.scriptURL
            && workerType == job.workerType
            && registrationOptions->updateViaCache == job.registrationOptions->updateViaCache;
    case ServiceWorkerJobType::Update:
        return scopeURL == job.scopeURL
            && scriptURL == job.scriptURL
            && workerType == job.workerType;
    case ServiceWorkerJobType::Unregister:
        return scopeURL == job.scopeURL;
    }
    return false;
}

} // namespace WebCore
