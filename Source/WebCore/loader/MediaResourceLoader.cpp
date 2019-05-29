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
#include "HTMLMediaElement.h"
#include "InspectorInstrumentation.h"
#include "SecurityOrigin.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MediaResourceLoader::MediaResourceLoader(Document& document, HTMLMediaElement& mediaElement, const String& crossOriginMode)
    : ContextDestructionObserver(&document)
    , m_document(makeWeakPtr(document))
    , m_mediaElement(makeWeakPtr(mediaElement))
    , m_crossOriginMode(crossOriginMode)
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
    m_mediaElement = nullptr;
}

RefPtr<PlatformMediaResource> MediaResourceLoader::requestResource(ResourceRequest&& request, LoadOptions options)
{
    if (!m_document)
        return nullptr;

    DataBufferingPolicy bufferingPolicy = options & LoadOption::BufferData ? DataBufferingPolicy::BufferData : DataBufferingPolicy::DoNotBufferData;
    auto cachingPolicy = options & LoadOption::DisallowCaching ? CachingPolicy::DisallowCaching : CachingPolicy::AllowCaching;

    request.setRequester(ResourceRequest::Requester::Media);

    if (m_mediaElement)
        request.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(*m_mediaElement));

#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && PLATFORM(MAC)
    // FIXME: Workaround for <rdar://problem/26071607>. We are not able to do CORS checking on 304 responses because they are usually missing the headers we need.
    if (!m_crossOriginMode.isNull())
        request.makeUnconditional();
#endif

    ContentSecurityPolicyImposition contentSecurityPolicyImposition = m_mediaElement && m_mediaElement->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
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
    loaderOptions.destination = m_mediaElement && !m_mediaElement->isVideo() ? FetchOptions::Destination::Audio : FetchOptions::Destination::Video;
    auto cachedRequest = createPotentialAccessControlRequest(WTFMove(request), *m_document, m_crossOriginMode, WTFMove(loaderOptions));
    if (m_mediaElement)
        cachedRequest.setInitiator(*m_mediaElement);

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
    if (m_responsesForTesting.size() > maximumResponsesForTesting)
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
        static NeverDestroyed<const String> consoleMessage("Cross-origin media resource load denied by Cross-Origin Resource Sharing policy.");
        m_loader->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage.get());
        m_didPassAccessControlCheck = false;
        if (m_client)
            m_client->accessControlCheckFailed(*this, ResourceError(errorDomainWebKitInternal, 0, response.url(), consoleMessage.get()));
        stop();
        return;
    }

    m_didPassAccessControlCheck = m_resource->options().mode == FetchOptions::Mode::Cors;
    if (m_client)
        m_client->responseReceived(*this, response, [this, protectedThis = makeRef(*this), completionHandler = completionHandlerCaller.release()] (ShouldContinue shouldContinue) mutable {
            if (completionHandler)
                completionHandler();
            if (shouldContinue == ShouldContinue::No)
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

void MediaResource::dataReceived(CachedResource& resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->dataReceived(*this, data, dataLength);
}

void MediaResource::notifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client) {
        if (m_resource->loadFailedOrCanceled())
            m_client->loadFailed(*this, m_resource->resourceError());
        else
            m_client->loadFinished(*this);
    }
    stop();
}

} // namespace WebCore

#endif
