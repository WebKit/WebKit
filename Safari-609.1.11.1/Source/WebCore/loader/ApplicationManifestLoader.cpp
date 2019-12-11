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
#include "ApplicationManifestLoader.h"

#if ENABLE(APPLICATION_MANIFEST)

#include "CachedApplicationManifest.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "CustomHeaderFields.h"
#include "DocumentLoader.h"
#include "Frame.h"

namespace WebCore {

ApplicationManifestLoader::ApplicationManifestLoader(DocumentLoader& documentLoader, const URL& url, bool useCredentials)
    : m_documentLoader(documentLoader)
    , m_url(url)
    , m_useCredentials(useCredentials)
{
}

ApplicationManifestLoader::~ApplicationManifestLoader()
{
    stopLoading();
}

bool ApplicationManifestLoader::startLoading()
{
    ASSERT(!m_resource);
    auto* frame = m_documentLoader.frame();
    if (!frame)
        return false;

    ResourceRequest resourceRequest = m_url;
    resourceRequest.setPriority(ResourceLoadPriority::Low);
#if !ERROR_DISABLED
    // Copy this because we may want to access it after transferring the
    // `resourceRequest` to the `request`. If we don't, then the LOG_ERROR
    // below won't print a URL.
    auto resourceRequestURL = resourceRequest.url();
#endif

    auto credentials = m_useCredentials ? FetchOptions::Credentials::Include : FetchOptions::Credentials::Omit;
    auto options = ResourceLoaderOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::SniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::DoNotUse,
        ClientCredentialPolicy::CannotAskClientForCredentials,
        credentials,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::AllowCaching);
    options.destination = FetchOptions::Destination::Manifest;
    CachedResourceRequest request(WTFMove(resourceRequest), options);

    auto cachedResource = frame->document()->cachedResourceLoader().requestApplicationManifest(WTFMove(request));
    m_resource = cachedResource.value_or(nullptr);
    if (m_resource)
        m_resource->addClient(*this);
    else {
        LOG_ERROR("Failed to start load for application manifest at url %s (error: %s)", resourceRequestURL.string().ascii().data(), cachedResource.error().localizedDescription().utf8().data());
        return false;
    }

    return true;
}

void ApplicationManifestLoader::stopLoading()
{
    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }
}

Optional<ApplicationManifest>& ApplicationManifestLoader::processManifest()
{
    if (!m_processedManifest && m_resource) {
        auto manifestURL = m_url;
        auto documentURL = m_documentLoader.url();
        auto frame = m_documentLoader.frame();
        auto document = frame ? frame->document() : nullptr;
        m_processedManifest = m_resource->process(manifestURL, documentURL, document);
    }
    return m_processedManifest;
}

void ApplicationManifestLoader::notifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    m_documentLoader.finishedLoadingApplicationManifest(*this);
}

} // namespace WebCore

#endif // ENABLE(APPLICATION_MANIFEST)
