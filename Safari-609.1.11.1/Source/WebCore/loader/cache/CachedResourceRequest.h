/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#pragma once

#include "CachedResource.h"
#include "Element.h"
#include "ResourceLoadPriority.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerIdentifier.h"
#include <wtf/RefPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct ContentRuleListResults;
class Document;
class FrameLoader;
struct ServiceWorkerRegistrationData;
enum class ReferrerPolicy : uint8_t;

bool isRequestCrossOrigin(SecurityOrigin*, const URL& requestURL, const ResourceLoaderOptions&);

class CachedResourceRequest {
public:
    CachedResourceRequest(ResourceRequest&&, const ResourceLoaderOptions&, Optional<ResourceLoadPriority> = WTF::nullopt, String&& charset = String());

    ResourceRequest&& releaseResourceRequest() { return WTFMove(m_resourceRequest); }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }
    ResourceRequest& resourceRequest() { return m_resourceRequest; }

    const String& charset() const { return m_charset; }
    void setCharset(const String& charset) { m_charset = charset; }

    const ResourceLoaderOptions& options() const { return m_options; }
    void setOptions(const ResourceLoaderOptions& options) { m_options = options; }

    const Optional<ResourceLoadPriority>& priority() const { return m_priority; }
    void setPriority(Optional<ResourceLoadPriority>&& priority) { m_priority = WTFMove(priority); }

    void setInitiator(Element&);
    void setInitiator(const AtomString& name);
    const AtomString& initiatorName() const;

    bool allowsCaching() const { return m_options.cachingPolicy == CachingPolicy::AllowCaching; }
    void setCachingPolicy(CachingPolicy policy) { m_options.cachingPolicy = policy;  }

    // Whether this request should impact request counting and delay window.onload.
    bool ignoreForRequestCount() const { return m_ignoreForRequestCount; }
    void setIgnoreForRequestCount(bool ignoreForRequestCount) { m_ignoreForRequestCount = ignoreForRequestCount; }

    void setDestinationIfNotSet(FetchOptions::Destination);

    void deprecatedSetAsPotentiallyCrossOrigin(const String&, Document&); // Use WebCore::createPotentialAccessControlRequest() instead.

    void updateForAccessControl(Document&);

    void updateReferrerPolicy(ReferrerPolicy);
    void updateReferrerOriginAndUserAgentHeaders(FrameLoader&);
    void upgradeInsecureRequestIfNeeded(Document&);
    void setAcceptHeaderIfNone(CachedResource::Type);
    void updateAccordingCacheMode();
    void updateAcceptEncodingHeader();

    void removeFragmentIdentifierIfNeeded();
#if ENABLE(CONTENT_EXTENSIONS)
    void applyResults(ContentRuleListResults&&, Page*);
#endif
    void setDomainForCachePartition(Document&);
    void setDomainForCachePartition(const String&);
    bool isLinkPreload() const { return m_isLinkPreload; }
    void setIsLinkPreload() { m_isLinkPreload = true; }

    void setOrigin(Ref<SecurityOrigin>&& origin) { m_origin = WTFMove(origin); }
    RefPtr<SecurityOrigin> releaseOrigin() { return WTFMove(m_origin); }
    SecurityOrigin* origin() const { return m_origin.get(); }

    String&& releaseFragmentIdentifier() { return WTFMove(m_fragmentIdentifier); }
    void clearFragmentIdentifier() { m_fragmentIdentifier = { }; }

    static String splitFragmentIdentifierFromRequestURL(ResourceRequest&);

#if ENABLE(SERVICE_WORKER)
    void setClientIdentifierIfNeeded(DocumentIdentifier);
    void setSelectedServiceWorkerRegistrationIdentifierIfNeeded(ServiceWorkerRegistrationIdentifier);
    void setNavigationServiceWorkerRegistrationData(const Optional<ServiceWorkerRegistrationData>&);
#endif

private:
    ResourceRequest m_resourceRequest;
    String m_charset;
    ResourceLoaderOptions m_options;
    Optional<ResourceLoadPriority> m_priority;
    RefPtr<Element> m_initiatorElement;
    AtomString m_initiatorName;
    RefPtr<SecurityOrigin> m_origin;
    String m_fragmentIdentifier;
    bool m_isLinkPreload { false };
    bool m_ignoreForRequestCount { false };
};

void upgradeInsecureResourceRequestIfNeeded(ResourceRequest&, Document&);

} // namespace WebCore
