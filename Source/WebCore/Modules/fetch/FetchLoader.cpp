/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchLoader.h"

#include "BlobURL.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "ContentSecurityPolicy.h"
#include "FetchBody.h"
#include "FetchBodyConsumer.h"
#include "FetchLoaderClient.h"
#include "FetchRequest.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "ThreadableBlobRegistry.h"

namespace WebCore {

FetchLoader::~FetchLoader()
{
    if (!m_urlForReading.isEmpty())
        ThreadableBlobRegistry::unregisterBlobURL(m_urlForReading);
}

void FetchLoader::start(ScriptExecutionContext& context, const Blob& blob)
{
    return startLoadingBlobURL(context, blob.url());
}

void FetchLoader::startLoadingBlobURL(ScriptExecutionContext& context, const URL& blobURL)
{
    m_urlForReading = BlobURL::createPublicURL(context.securityOrigin());
    if (m_urlForReading.isEmpty()) {
        m_client.didFail({ errorDomainWebKitInternal, 0, URL(), "Could not create URL for Blob"_s });
        return;
    }

    ThreadableBlobRegistry::registerBlobURL(context.securityOrigin(), context.policyContainer(), m_urlForReading, blobURL);

    ResourceRequest request(m_urlForReading);
    request.setInitiatorIdentifier(context.resourceRequestIdentifier());
    request.setHTTPMethod("GET"_s);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.dataBufferingPolicy = DataBufferingPolicy::DoNotBufferData;
    options.preflightPolicy = PreflightPolicy::Consider;
    options.credentials = FetchOptions::Credentials::Include;
    options.mode = FetchOptions::Mode::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    m_loader = ThreadableLoader::create(context, *this, WTFMove(request), options);
    m_isStarted = m_loader;
}

void FetchLoader::start(ScriptExecutionContext& context, const FetchRequest& request, const String& initiator)
{
    ResourceLoaderOptions resourceLoaderOptions = request.fetchOptions();
    resourceLoaderOptions.preflightPolicy = PreflightPolicy::Consider;
    ThreadableLoaderOptions options(resourceLoaderOptions,
        context.shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective,
        String(initiator),
        ResponseFilteringPolicy::Disable);
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.dataBufferingPolicy = DataBufferingPolicy::DoNotBufferData;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    options.navigationPreloadIdentifier = request.navigationPreloadIdentifier();
    options.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Disable;

    ResourceRequest fetchRequest = request.resourceRequest();

    ASSERT(context.contentSecurityPolicy());
    auto& contentSecurityPolicy = *context.contentSecurityPolicy();

    contentSecurityPolicy.upgradeInsecureRequestIfNeeded(fetchRequest, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!context.shouldBypassMainWorldContentSecurityPolicy() && !contentSecurityPolicy.allowConnectToSource(fetchRequest.url())) {
        m_client.didFail({ errorDomainWebKitInternal, 0, fetchRequest.url(), "Not allowed by ContentSecurityPolicy"_s, ResourceError::Type::AccessControl });
        return;
    }

    String referrer = request.internalRequestReferrer();
    if (referrer == "no-referrer"_s) {
        options.referrerPolicy = ReferrerPolicy::NoReferrer;
        referrer = String();
    } else
        referrer = (referrer == "client"_s) ? context.url().strippedForUseAsReferrer() : URL(context.url(), referrer).strippedForUseAsReferrer();
    if (options.referrerPolicy == ReferrerPolicy::EmptyString)
        options.referrerPolicy = context.referrerPolicy();

    m_loader = ThreadableLoader::create(context, *this, WTFMove(fetchRequest), options, WTFMove(referrer));
    m_isStarted = m_loader;
}

FetchLoader::FetchLoader(FetchLoaderClient& client, FetchBodyConsumer* consumer)
    : m_client(client)
    , m_consumer(consumer)
{
}

void FetchLoader::stop()
{
    if (m_consumer)
        m_consumer->clean();
    if (m_loader)
        m_loader->cancel();
}

RefPtr<FragmentedSharedBuffer> FetchLoader::startStreaming()
{
    ASSERT(m_consumer);
    auto firstChunk = m_consumer->takeData();
    m_consumer = nullptr;
    return firstChunk;
}

void FetchLoader::didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse& response)
{
    m_client.didReceiveResponse(response);
}

void FetchLoader::didReceiveData(const SharedBuffer& buffer)
{
    if (!m_consumer) {
        m_client.didReceiveData(buffer);
        return;
    }
    m_consumer->append(buffer);
}

void FetchLoader::didFinishLoading(ResourceLoaderIdentifier, const NetworkLoadMetrics& metrics)
{
    m_client.didSucceed(metrics);
}

void FetchLoader::didFail(const ResourceError& error)
{
    m_client.didFail(error);
}

} // namespace WebCore
