/*
 * Copyright (C) 2006-2017 Apple Inc.  All rights reserved.
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
#include "IconLoader.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "ResourceRequest.h"
#include "SharedBuffer.h"
#include <wtf/text/CString.h>

namespace WebCore {

IconLoader::IconLoader(DocumentLoader& documentLoader, const URL& url)
    : m_documentLoader(documentLoader)
    , m_url(url)
{
}

IconLoader::~IconLoader()
{
    stopLoading();
}

void IconLoader::startLoading()
{
    if (m_resource)
        return;

    RefPtr frame = m_documentLoader->frame();
    if (!frame)
        return;

    ResourceRequest resourceRequest = m_url;
    resourceRequest.setPriority(ResourceLoadPriority::Low);
#if !ERROR_DISABLED
    // Copy this because we may want to access it after transferring the
    // `resourceRequest` to the `request`. If we don't, then the LOG_ERROR
    // below won't print a URL.
    auto resourceRequestURL = resourceRequest.url();
#endif

    CachedResourceRequest request(WTFMove(resourceRequest), ResourceLoaderOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::SniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::DoNotUse,
        ClientCredentialPolicy::CannotAskClientForCredentials,
        FetchOptions::Credentials::Omit,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::AllowCaching));

    request.setInitiatorType(cachedResourceRequestInitiatorTypes().icon);

    auto cachedResource = frame->document()->cachedResourceLoader().requestIcon(WTFMove(request));
    m_resource = cachedResource.value_or(nullptr);
    if (CachedResourceHandle resource = m_resource)
        resource->addClient(*this);
    else
        LOG_ERROR("Failed to start load for icon at url %s (error: %s)", resourceRequestURL.string().ascii().data(), cachedResource.error().localizedDescription().utf8().data());
}

void IconLoader::stopLoading()
{
    if (CachedResourceHandle resource = std::exchange(m_resource, nullptr))
        resource->removeClient(*this);
}

void IconLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    // If we got a status code indicating an invalid response, then lets
    // ignore the data and not try to decode the error page as an icon.
    RefPtr data = m_resource->resourceBuffer();
    int status = m_resource->response().httpStatusCode();
    if (status && (status < 200 || status > 299))
        data = nullptr;

    constexpr uint8_t pdfMagicNumber[] = { '%', 'P', 'D', 'F' };
    if (data && data->startsWith(std::span(pdfMagicNumber, std::size(pdfMagicNumber)))) {
        LOG(IconDatabase, "IconLoader::finishLoading() - Ignoring icon at %s because it appears to be a PDF", m_resource->url().string().ascii().data());
        data = nullptr;
    }

    LOG(IconDatabase, "IconLoader::finishLoading() - Committing iconURL %s to database", m_resource->url().string().ascii().data());

    // DocumentLoader::finishedLoadingIcon destroys this IconLoader as it finishes. This will automatically
    // trigger IconLoader::stopLoading() during destruction, so we should just return here.
    Ref { m_documentLoader.get() }->finishedLoadingIcon(*this, data.get());
}

}
