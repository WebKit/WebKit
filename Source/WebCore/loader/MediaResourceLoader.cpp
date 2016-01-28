/*
 * Copyright (C) 2014 Igalia S.L
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

MediaResourceLoader::MediaResourceLoader(Document& document, const String& crossOriginMode, std::unique_ptr<PlatformMediaResourceLoaderClient> client)
    : PlatformMediaResourceLoader(WTFMove(client))
    , m_document(document)
    , m_crossOriginMode(crossOriginMode)
    , m_didPassAccessControlCheck(false)
{
}

MediaResourceLoader::~MediaResourceLoader()
{
    stop();
}

bool MediaResourceLoader::start(const ResourceRequest& request, LoadOptions options)
{
    if (m_resource)
        return false;

    DataBufferingPolicy bufferingPolicy = options & LoadOption::BufferData ? WebCore::BufferData : WebCore::DoNotBufferData;
    RequestOriginPolicy corsPolicy = !m_crossOriginMode.isNull() ? PotentiallyCrossOriginEnabled : UseDefaultOriginRestrictionsForType;
    StoredCredentials allowCredentials = m_crossOriginMode.isNull() || equalLettersIgnoringASCIICase(m_crossOriginMode, "use-credentials") ? AllowStoredCredentials : DoNotAllowStoredCredentials;

    // ContentSecurityPolicyImposition::DoPolicyCheck is a placeholder value. It does not affect the request since Content Security Policy does not apply to raw resources.
    CachedResourceRequest cacheRequest(request, ResourceLoaderOptions(SendCallbacks, DoNotSniffContent, bufferingPolicy, allowCredentials, DoNotAskClientForCrossOriginCredentials, ClientDidNotRequestCredentials, DoSecurityCheck, corsPolicy, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, CachingPolicy::AllowCaching));

    if (!m_crossOriginMode.isNull())
        updateRequestForAccessControl(cacheRequest.mutableResourceRequest(), m_document.securityOrigin(), allowCredentials);

    m_didPassAccessControlCheck = false;

    m_resource = m_document.cachedResourceLoader().requestRawResource(cacheRequest);
    if (!m_resource)
        return false;

    m_resource->addClient(this);
    return true;
}

void MediaResourceLoader::stop()
{
    if (!m_resource)
        return;

    m_resource->removeClient(this);
    m_resource = nullptr;
}

void MediaResourceLoader::setDefersLoading(bool defersLoading)
{
    if (m_resource)
        m_resource->setDefersLoading(defersLoading);
}

void MediaResourceLoader::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResourceLoader> protect(this);
    if (!m_crossOriginMode.isNull() && !resource->passesSameOriginPolicyCheck(*m_document.securityOrigin())) {
        static NeverDestroyed<const String> consoleMessage("Cross-origin media resource load denied by Cross-Origin Resource Sharing policy.");
        m_document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, consoleMessage.get());
        m_didPassAccessControlCheck = false;
        m_client->accessControlCheckFailed(ResourceError(errorDomainWebKitInternal, 0, response.url(), consoleMessage.get()));
        stop();
        return;
    }

    m_didPassAccessControlCheck = !m_crossOriginMode.isNull();
    m_client->responseReceived(response);
}

void MediaResourceLoader::dataReceived(CachedResource* resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, resource == m_resource);

    RefPtr<MediaResourceLoader> protect(this);
    m_client->dataReceived(data, dataLength);
}

void MediaResourceLoader::notifyFinished(CachedResource* resource)
{
    ASSERT(resource == m_resource);

    RefPtr<MediaResourceLoader> protect(this);
    if (resource->loadFailedOrCanceled())
        m_client->loadFailed(resource->resourceError());
    else
        m_client->loadFinished();
    stop();
}

#if USE(SOUP)
char* MediaResourceLoader::getOrCreateReadBuffer(CachedResource* resource, size_t requestedSize, size_t& actualSize)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    return m_client->getOrCreateReadBuffer(requestedSize, actualSize);
}
#endif

} // namespace WebCore

#endif
