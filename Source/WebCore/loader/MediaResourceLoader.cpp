/*
 * Copyright (C) 2014 Igalia S.L
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "MediaResourceLoader.h"

#if ENABLE(VIDEO)

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "SecurityOrigin.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static bool shouldRecordResponsesForTesting = false;

void MediaResourceLoader::recordResponsesForTesting()
{
    shouldRecordResponsesForTesting = true;
}

MediaResourceLoader::MediaResourceLoader(Document& document, Element& element, const String& crossOriginMode, FetchOptions::Destination destination)
    : ContextDestructionObserver(&document)
    , m_document(document)
    , m_element(element)
    , m_crossOriginMode(crossOriginMode)
    , m_destination(destination)
{
}

MediaResourceLoader::~MediaResourceLoader()
{
    ASSERT(m_resources.isEmpty());
}

void MediaResourceLoader::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
    m_document = nullptr;
    m_element = nullptr;
}

void MediaResourceLoader::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    if (!m_document || !m_document->frame())
        return completionHandler(makeUnexpected(internalError(url)));

    m_document->frame()->loader().client().sendH2Ping(url, WTFMove(completionHandler));
}


RefPtr<PlatformMediaResource> MediaResourceLoader::requestResource(ResourceRequest&& request, LoadOptions options)
{
    if (!m_document)
        return nullptr;

    DataBufferingPolicy bufferingPolicy = options & LoadOption::BufferData ? DataBufferingPolicy::BufferData : DataBufferingPolicy::DoNotBufferData;
    auto cachingPolicy = options & LoadOption::DisallowCaching ? CachingPolicy::DisallowCaching : CachingPolicy::AllowCaching;

    request.setRequester(ResourceRequestRequester::Media);

    if (m_element)
        request.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(*m_element));

#if PLATFORM(MAC)
    // FIXME: Workaround for <rdar://problem/26071607>. We are not able to do CORS checking on 304 responses because they are usually missing the headers we need.
    if (!m_crossOriginMode.isNull())
        request.makeUnconditional();
#endif

    ContentSecurityPolicyImposition contentSecurityPolicyImposition = m_element && m_element->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
    ResourceLoaderOptions loaderOptions {
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::DoNotSniffContent,
        bufferingPolicy,
        StoredCredentialsPolicy::Use,
        ClientCredentialPolicy::MayAskClientForCredentials,
        FetchOptions::Credentials::Include,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        contentSecurityPolicyImposition,
        DefersLoadingPolicy::AllowDefersLoading,
        cachingPolicy };
    loaderOptions.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    loaderOptions.destination = m_destination;
    auto cachedRequest = createPotentialAccessControlRequest(WTFMove(request), WTFMove(loaderOptions), *m_document, m_crossOriginMode);
    if (m_element)
        cachedRequest.setInitiator(*m_element);

    auto resource = m_document->cachedResourceLoader().requestMedia(WTFMove(cachedRequest)).value_or(nullptr);
    if (!resource)
        return nullptr;

    Ref<MediaResource> mediaResource = MediaResource::create(*this, resource);
    m_resources.add(mediaResource.ptr());

    return mediaResource;
}

void MediaResourceLoader::removeResource(MediaResource& mediaResource)
{
    ASSERT(m_resources.contains(&mediaResource));
    m_resources.remove(&mediaResource);
}

void MediaResourceLoader::addResponseForTesting(const ResourceResponse& response)
{
    const auto maximumResponsesForTesting = 5;
    if (!shouldRecordResponsesForTesting || m_responsesForTesting.size() > maximumResponsesForTesting)
        return;
    m_responsesForTesting.append(response);
}

Ref<MediaResource> MediaResource::create(MediaResourceLoader& loader, CachedResourceHandle<CachedRawResource> resource)
{
    return adoptRef(*new MediaResource(loader, resource));
}

MediaResource::MediaResource(MediaResourceLoader& loader, CachedResourceHandle<CachedRawResource> resource)
    : m_loader(loader)
    , m_resource(resource)
{
    ASSERT(resource);
    resource->addClient(*this);
}

MediaResource::~MediaResource()
{
    stop();
    m_loader->removeResource(*this);
}

void MediaResource::stop()
{
    if (!m_resource)
        return;

    m_resource->removeClient(*this);
    m_resource = nullptr;
}

void MediaResource::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

    if (!m_loader->document())
        return;

    RefPtr<MediaResource> protectedThis(this);
    if (m_resource->resourceError().isAccessControl()) {
        static NeverDestroyed<const String> consoleMessage("Cross-origin media resource load denied by Cross-Origin Resource Sharing policy."_s);
        m_loader->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage.get());
        m_didPassAccessControlCheck = false;
        if (m_client)
            m_client->accessControlCheckFailed(*this, ResourceError(errorDomainWebKitInternal, 0, response.url(), consoleMessage.get()));
        stop();
        return;
    }

    m_didPassAccessControlCheck = m_resource->options().mode == FetchOptions::Mode::Cors;
    if (m_client)
        m_client->responseReceived(*this, response, [this, protectedThis = Ref { *this }, completionHandler = completionHandlerCaller.release()] (auto shouldContinue) mutable {
            if (completionHandler)
                completionHandler();
            if (shouldContinue == ShouldContinuePolicyCheck::No)
                stop();
        });

    m_loader->addResponseForTesting(response);
}

bool MediaResource::shouldCacheResponse(CachedResource& resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        return m_client->shouldCacheResponse(*this, response);
    return true;
}

void MediaResource::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->redirectReceived(*this, WTFMove(request), response, WTFMove(completionHandler));
    else
        completionHandler(WTFMove(request));
}

void MediaResource::dataSent(CachedResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void MediaResource::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->dataReceived(*this, buffer);
}

void MediaResource::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client) {
        if (m_resource->loadFailedOrCanceled())
            m_client->loadFailed(*this, m_resource->resourceError());
        else
            m_client->loadFinished(*this, metrics);
    }
    stop();
}

} // namespace WebCore

#endif
