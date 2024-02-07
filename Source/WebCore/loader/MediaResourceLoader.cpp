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
#include "DocumentInlines.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "InspectorInstrumentation.h"
#include "LocalFrameLoaderClient.h"
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
    assertIsMainThread();
}

MediaResourceLoader::~MediaResourceLoader()
{
    assertIsMainThread();

    ASSERT(m_resources.isEmptyIgnoringNullReferences());
}

void MediaResourceLoader::contextDestroyed()
{
    assertIsMainThread();

    ContextDestructionObserver::contextDestroyed();
    m_document = nullptr;
    m_element = nullptr;
}

void MediaResourceLoader::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    assertIsMainThread();

    if (!m_document || !m_document->frame())
        return completionHandler(makeUnexpected(internalError(url)));

    m_document->protectedFrame()->checkedLoader()->client().sendH2Ping(url, WTFMove(completionHandler));
}

RefPtr<PlatformMediaResource> MediaResourceLoader::requestResource(ResourceRequest&& request, LoadOptions options)
{
    assertIsMainThread();

    if (!m_document)
        return nullptr;

    DataBufferingPolicy bufferingPolicy = options & LoadOption::BufferData ? DataBufferingPolicy::BufferData : DataBufferingPolicy::DoNotBufferData;
    auto cachingPolicy = options & LoadOption::DisallowCaching ? CachingPolicy::DisallowCaching : CachingPolicy::AllowCaching;

    request.setRequester(ResourceRequestRequester::Media);

    if (RefPtr element = m_element.get())
        request.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(*element));

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
    if (RefPtr element = m_element.get())
        cachedRequest.setInitiator(*element);

    auto resource = m_document->protectedCachedResourceLoader()->requestMedia(WTFMove(cachedRequest)).value_or(nullptr);
    if (!resource)
        return nullptr;

    Ref mediaResource = MediaResource::create(*this, WTFMove(resource));
    m_resources.add(mediaResource.get());

    return mediaResource;
}

void MediaResourceLoader::removeResource(MediaResource& mediaResource)
{
    assertIsMainThread();

    ASSERT(m_resources.contains(mediaResource));
    m_resources.remove(mediaResource);
}

void MediaResourceLoader::addResponseForTesting(const ResourceResponse& response)
{
    assertIsMainThread();

    const auto maximumResponsesForTesting = 5;
    if (!shouldRecordResponsesForTesting || m_responsesForTesting.size() > maximumResponsesForTesting)
        return;
    m_responsesForTesting.append(response);
}

Document* MediaResourceLoader::document()
{
    assertIsMainThread();

    return m_document.get();
}

RefPtr<Document> MediaResourceLoader::protectedDocument()
{
    return document();
}

const String& MediaResourceLoader::crossOriginMode() const
{
    assertIsMainThread();

    return m_crossOriginMode;
}

Vector<ResourceResponse> MediaResourceLoader::responsesForTesting() const
{
    assertIsMainThread();

    return m_responsesForTesting;
}

Ref<MediaResource> MediaResource::create(MediaResourceLoader& loader, CachedResourceHandle<CachedRawResource>&& resource)
{
    return adoptRef(*new MediaResource(loader, WTFMove(resource)));
}

MediaResource::MediaResource(MediaResourceLoader& loader, CachedResourceHandle<CachedRawResource>&& resource)
    : m_loader(loader)
    , m_resource(WTFMove(resource))
{
    assertIsMainThread();

    ASSERT(resource);
    protectedResource()->addClient(*this);
}

Ref<MediaResourceLoader> MediaResource::protectedLoader() const
{
    return m_loader;
}

CachedResourceHandle<CachedRawResource> MediaResource::protectedResource() const
{
    return m_resource;
}

MediaResource::~MediaResource()
{
    assertIsMainThread();

    protectedLoader()->removeResource(*this);
}

void MediaResource::shutdown()
{
    assertIsMainThread();

    setClient(nullptr);

    if (CachedResourceHandle resource = std::exchange(m_resource, nullptr))
        resource->removeClient(*this);
}

void MediaResource::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

    if (!m_loader->document())
        return;

    Ref protectedThis { *this };
    if (m_resource->resourceError().isAccessControl()) {
        static NeverDestroyed<const String> consoleMessage("Cross-origin media resource load denied by Cross-Origin Resource Sharing policy."_s);
        m_loader->protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage.get());
        m_didPassAccessControlCheck.store(false);
        if (RefPtr client = this->client())
            client->accessControlCheckFailed(*this, ResourceError(errorDomainWebKitInternal, 0, response.url(), consoleMessage.get()));
        ensureShutdown();
        return;
    }

    m_didPassAccessControlCheck.store(m_resource->options().mode == FetchOptions::Mode::Cors);
    if (RefPtr client = this->client()) {
        client->responseReceived(*this, response, [this, protectedThis = Ref { *this }, completionHandler = completionHandlerCaller.release()] (auto shouldContinue) mutable {
            if (completionHandler)
                completionHandler();
            if (shouldContinue == ShouldContinuePolicyCheck::No)
                ensureShutdown();
        });
    }

    protectedLoader()->addResponseForTesting(response);
}

bool MediaResource::shouldCacheResponse(CachedResource& resource, const ResourceResponse& response)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    if (RefPtr client = this->client())
        return client->shouldCacheResponse(*this, response);
    return true;
}

void MediaResource::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    if (RefPtr client = this->client())
        client->redirectReceived(*this, WTFMove(request), response, WTFMove(completionHandler));
    else
        completionHandler(WTFMove(request));
}

void MediaResource::dataSent(CachedResource& resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    if (RefPtr client = this->client())
        client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void MediaResource::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    if (RefPtr client = this->client())
        client->dataReceived(*this, buffer);
}

void MediaResource::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics)
{
    assertIsMainThread();

    ASSERT_UNUSED(resource, &resource == m_resource);

    Ref protectedThis { *this };
    if (RefPtr client = this->client()) {
        if (m_resource->loadFailedOrCanceled())
            client->loadFailed(*this, m_resource->resourceError());
        else
            client->loadFinished(*this, metrics);
    }
    ensureShutdown();
}

void MediaResource::ensureShutdown()
{
    ensureOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->shutdown();
    });
}

} // namespace WebCore

#endif
