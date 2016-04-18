/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "LinkLoader.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContainerNode.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "LinkRelAttribute.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleResolver.h"

namespace WebCore {

LinkLoader::LinkLoader(LinkLoaderClient& client)
    : m_client(client)
    , m_linkLoadTimer(*this, &LinkLoader::linkLoadTimerFired)
    , m_linkLoadingErrorTimer(*this, &LinkLoader::linkLoadingErrorTimerFired)
{
}

LinkLoader::~LinkLoader()
{
    if (m_cachedLinkResource)
        m_cachedLinkResource->removeClient(this);
}

void LinkLoader::linkLoadTimerFired()
{
    m_client.linkLoaded();
}

void LinkLoader::linkLoadingErrorTimerFired()
{
    m_client.linkLoadingErrored();
}

void LinkLoader::notifyFinished(CachedResource* resource)
{
    ASSERT_UNUSED(resource, m_cachedLinkResource.get() == resource);

    if (m_cachedLinkResource->errorOccurred())
        m_linkLoadingErrorTimer.startOneShot(0);
    else 
        m_linkLoadTimer.startOneShot(0);

    m_cachedLinkResource->removeClient(this);
    m_cachedLinkResource = nullptr;
}

Optional<CachedResource::Type> LinkLoader::resourceTypeFromAsAttribute(const String& as)
{
    if (as.isEmpty())
        return CachedResource::LinkPreload;
    if (equalLettersIgnoringASCIICase(as, "image"))
        return CachedResource::ImageResource;
    if (equalLettersIgnoringASCIICase(as, "script"))
        return CachedResource::Script;
    if (equalLettersIgnoringASCIICase(as, "style"))
        return CachedResource::CSSStyleSheet;
    if (equalLettersIgnoringASCIICase(as, "media"))
        return CachedResource::MediaResource;
    if (equalLettersIgnoringASCIICase(as, "font"))
        return CachedResource::FontResource;
#if ENABLE(VIDEO_TRACK)
    if (equalLettersIgnoringASCIICase(as, "track"))
        return CachedResource::TextTrackResource;
#endif
    return Nullopt;
}

static void preloadIfNeeded(const LinkRelAttribute& relAttribute, const URL& href, Document& document, const String& as, const String& crossOriginMode)
{
    if (!document.loader() || !relAttribute.isLinkPreload)
        return;

    ASSERT(RuntimeEnabledFeatures::sharedFeatures().linkPreloadEnabled());
    if (!href.isValid()) {
        document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, String("<link rel=preload> has an invalid `href` value"));
        return;
    }
    auto type = LinkLoader::resourceTypeFromAsAttribute(as);
    if (!type) {
        document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, String("<link rel=preload> must have a valid `as` value"));
        return;
    }

    ResourceRequest resourceRequest(document.completeURL(href));
    CachedResourceRequest linkRequest(resourceRequest, CachedResource::defaultPriorityForResourceType(type.value()));
    linkRequest.setInitiator("link");

    if (!crossOriginMode.isNull()) {
        StoredCredentials allowCredentials = equalLettersIgnoringASCIICase(crossOriginMode, "use-credentials") ? AllowStoredCredentials : DoNotAllowStoredCredentials;
        updateRequestForAccessControl(linkRequest.mutableResourceRequest(), document.securityOrigin(), allowCredentials);
    }
    linkRequest.setForPreload(true);
    document.cachedResourceLoader().preload(type.value(), linkRequest, emptyString());
}

bool LinkLoader::loadLink(const LinkRelAttribute& relAttribute, const URL& href, const String& as, const String& crossOrigin, Document& document)
{
    // We'll record this URL per document, even if we later only use it in top level frames
    if (relAttribute.iconType != InvalidIcon && href.isValid() && !href.isEmpty()) {
        if (!m_client.shouldLoadLink())
            return false;
        if (Frame* frame = document.frame())
            frame->loader().client().dispatchDidChangeIcons(relAttribute.iconType);
    }

    if (relAttribute.isDNSPrefetch) {
        Settings* settings = document.settings();
        // FIXME: The href attribute of the link element can be in "//hostname" form, and we shouldn't attempt
        // to complete that as URL <https://bugs.webkit.org/show_bug.cgi?id=48857>.
        if (settings && settings->dnsPrefetchingEnabled() && href.isValid() && !href.isEmpty() && document.frame())
            document.frame()->loader().client().prefetchDNS(href.host());
    }

    if (m_client.shouldLoadLink())
        preloadIfNeeded(relAttribute, href, document, as, crossOrigin);

#if ENABLE(LINK_PREFETCH)
    if ((relAttribute.isLinkPrefetch || relAttribute.isLinkSubresource) && href.isValid() && document.frame()) {
        if (!m_client.shouldLoadLink())
            return false;

        Optional<ResourceLoadPriority> priority;
        CachedResource::Type type = CachedResource::LinkPrefetch;
        if (relAttribute.isLinkSubresource) {
            // We only make one request to the cached resource loader if multiple rel types are specified;
            // this is the higher priority, which should overwrite the lower priority.
            priority = ResourceLoadPriority::Low;
            type = CachedResource::LinkSubresource;
        }
        CachedResourceRequest linkRequest(ResourceRequest(document.completeURL(href)), priority);

        if (m_cachedLinkResource) {
            m_cachedLinkResource->removeClient(this);
            m_cachedLinkResource = nullptr;
        }
        m_cachedLinkResource = document.cachedResourceLoader().requestLinkResource(type, linkRequest);
        if (m_cachedLinkResource)
            m_cachedLinkResource->addClient(this);
    }
#endif

    return true;
}

}
