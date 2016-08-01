/*
 * Copyright (C) 2014 Igalia S.L
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "SecurityOrigin.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MediaResourceLoader::MediaResourceLoader(Document& document, const String& crossOriginMode)
    : ContextDestructionObserver(&document)
    , m_document(&document)
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
}

RefPtr<PlatformMediaResource> MediaResourceLoader::requestResource(const ResourceRequest& request, LoadOptions options)
{
    if (!m_document)
        return nullptr;

    DataBufferingPolicy bufferingPolicy = options & LoadOption::BufferData ? WebCore::BufferData : WebCore::DoNotBufferData;
    auto cachingPolicy = options & LoadOption::DisallowCaching ? CachingPolicy::DisallowCaching : CachingPolicy::AllowCaching;

    // FIXME: We should try to remove the copy of request when crearing cacheRequest.
    // FIXME: Skip Content Security Policy check if the element that inititated this request is in a user-agent shadow tree. See <https://bugs.webkit.org/show_bug.cgi?id=155505>.
    CachedResourceRequest cacheRequest(ResourceRequest(request), ResourceLoaderOptions(SendCallbacks, DoNotSniffContent, bufferingPolicy, AllowStoredCredentials, ClientCredentialPolicy::MayAskClientForCredentials, FetchOptions::Credentials::Include, DoSecurityCheck, FetchOptions::Mode::NoCors, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, cachingPolicy));

    cacheRequest.setAsPotentiallyCrossOrigin(m_crossOriginMode, *m_document);

    cacheRequest.mutableResourceRequest().setRequester(ResourceRequest::Requester::Media);
#if HAVE(AVFOUNDATION_LOADER_DELEGATE) && PLATFORM(MAC)
    // FIXME: Workaround for <rdar://problem/26071607>. We are not able to do CORS checking on 304 responses because they
    // are usually missing the headers we need.
    if (cacheRequest.options().mode == FetchOptions::Mode::Cors)
        cacheRequest.mutableResourceRequest().makeUnconditional();
#endif

    CachedResourceHandle<CachedRawResource> resource = m_document->cachedResourceLoader().requestMedia(cacheRequest);
    if (!resource)
        return nullptr;

    Ref<MediaResource> mediaResource = MediaResource::create(*this, resource);
    m_resources.add(mediaResource.ptr());

    return WTFMove(mediaResource);
}

void MediaResourceLoader::removeResource(MediaResource& mediaResource)
{
    ASSERT(m_resources.contains(&mediaResource));
    m_resources.remove(&mediaResource);
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
    resource->addClient(this);
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

    m_resource->removeClient(this);
    m_resource = nullptr;
}

void MediaResource::setDefersLoading(bool defersLoading)
{
    if (m_resource)
        m_resource->setDefersLoading(defersLoading);
}

void MediaResource::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    if (!m_loader->document())
        return;

    RefPtr<MediaResource> protectedThis(this);
    if (!m_loader->crossOriginMode().isNull() && !resource->passesSameOriginPolicyCheck(*m_loader->document()->securityOrigin())) {
        static NeverDestroyed<const String> consoleMessage("Cross-origin media resource load denied by Cross-Origin Resource Sharing policy.");
        m_loader->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage.get());
        m_didPassAccessControlCheck = false;
        if (m_client)
            m_client->accessControlCheckFailed(*this, ResourceError(errorDomainWebKitInternal, 0, response.url(), consoleMessage.get()));
        stop();
        return;
    }

    m_didPassAccessControlCheck = !m_loader->crossOriginMode().isNull();
    if (m_client)
        m_client->responseReceived(*this, response);
}

bool MediaResource::shouldCacheResponse(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        return m_client->shouldCacheResponse(*this, response);
    return true;
}

void MediaResource::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->redirectReceived(*this, request, response);
}

void MediaResource::dataSent(CachedResource* resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void MediaResource::dataReceived(CachedResource* resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client)
        m_client->dataReceived(*this, data, dataLength);
}

void MediaResource::notifyFinished(CachedResource* resource)
{
    ASSERT(resource == m_resource);

    RefPtr<MediaResource> protectedThis(this);
    if (m_client) {
        if (resource->loadFailedOrCanceled())
            m_client->loadFailed(*this, resource->resourceError());
        else
            m_client->loadFinished(*this);
    }
    stop();
}

#if USE(SOUP)
char* MediaResource::getOrCreateReadBuffer(CachedResource* resource, size_t requestedSize, size_t& actualSize)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    return m_client ? m_client->getOrCreateReadBuffer(*this, requestedSize, actualSize) : nullptr;
}
#endif

} // namespace WebCore

#endif
