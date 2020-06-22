/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HTMLSrcsetParser.h"
#include "LinkHeader.h"
#include "LinkPreloadResourceClients.h"
#include "LinkRelAttribute.h"
#include "LoaderStrategy.h"
#include "MIMETypeRegistry.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "PlatformStrategies.h"
#include "ResourceError.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "SizesAttributeParser.h"
#include "StyleResolver.h"

namespace WebCore {

LinkLoader::LinkLoader(LinkLoaderClient& client)
    : m_client(client)
{
}

LinkLoader::~LinkLoader()
{
    if (m_cachedLinkResource)
        m_cachedLinkResource->removeClient(*this);
    if (m_preloadResourceClient)
        m_preloadResourceClient->clear();
}

void LinkLoader::triggerEvents(const CachedResource& resource)
{
    if (resource.errorOccurred())
        m_client.linkLoadingErrored();
    else
        m_client.linkLoaded();
}

void LinkLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT_UNUSED(resource, m_cachedLinkResource.get() == &resource);

    triggerEvents(*m_cachedLinkResource);

    m_cachedLinkResource->removeClient(*this);
    m_cachedLinkResource = nullptr;
}

void LinkLoader::loadLinksFromHeader(const String& headerValue, const URL& baseURL, Document& document, MediaAttributeCheck mediaAttributeCheck)
{
    if (headerValue.isEmpty())
        return;
    LinkHeaderSet headerSet(headerValue);
    for (auto& header : headerSet) {
        if (!header.valid() || header.url().isEmpty() || header.rel().isEmpty())
            continue;
        if ((mediaAttributeCheck == MediaAttributeCheck::MediaAttributeNotEmpty && !header.isViewportDependent())
            || (mediaAttributeCheck == MediaAttributeCheck::MediaAttributeEmpty && header.isViewportDependent())) {
                continue;
        }

        LinkRelAttribute relAttribute(document, header.rel());
        URL url(baseURL, header.url());
        // Sanity check to avoid re-entrancy here.
        if (equalIgnoringFragmentIdentifier(url, baseURL))
            continue;

        LinkLoadParameters params { relAttribute, url, header.as(), header.media(), header.mimeType(), header.crossOrigin(), header.imageSrcSet(), header.imageSizes(), ReferrerPolicy::EmptyString };
        preconnectIfNeeded(params, document);
        preloadIfNeeded(params, document, nullptr);
    }
}

Optional<CachedResource::Type> LinkLoader::resourceTypeFromAsAttribute(const String& as)
{
    if (equalLettersIgnoringASCIICase(as, "fetch"))
        return CachedResource::Type::RawResource;
    if (equalLettersIgnoringASCIICase(as, "image"))
        return CachedResource::Type::ImageResource;
    if (equalLettersIgnoringASCIICase(as, "script"))
        return CachedResource::Type::Script;
    if (equalLettersIgnoringASCIICase(as, "style"))
        return CachedResource::Type::CSSStyleSheet;
    if (RuntimeEnabledFeatures::sharedFeatures().mediaPreloadingEnabled() && (equalLettersIgnoringASCIICase(as, "video") || equalLettersIgnoringASCIICase(as, "audio")))
        return CachedResource::Type::MediaResource;
    if (equalLettersIgnoringASCIICase(as, "font"))
        return CachedResource::Type::FontResource;
    if (equalLettersIgnoringASCIICase(as, "track"))
        return CachedResource::Type::TextTrackResource;
    return WTF::nullopt;
}

static std::unique_ptr<LinkPreloadResourceClient> createLinkPreloadResourceClient(CachedResource& resource, LinkLoader& loader)
{
    switch (resource.type()) {
    case CachedResource::Type::ImageResource:
        return makeUnique<LinkPreloadImageResourceClient>(loader, downcast<CachedImage>(resource));
    case CachedResource::Type::Script:
        return makeUnique<LinkPreloadDefaultResourceClient>(loader, downcast<CachedScript>(resource));
    case CachedResource::Type::CSSStyleSheet:
        return makeUnique<LinkPreloadStyleResourceClient>(loader, downcast<CachedCSSStyleSheet>(resource));
    case CachedResource::Type::FontResource:
        return makeUnique<LinkPreloadFontResourceClient>(loader, downcast<CachedFont>(resource));
    case CachedResource::Type::TextTrackResource:
        return makeUnique<LinkPreloadDefaultResourceClient>(loader, downcast<CachedTextTrack>(resource));
    case CachedResource::Type::MediaResource:
        ASSERT(RuntimeEnabledFeatures::sharedFeatures().mediaPreloadingEnabled());
        FALLTHROUGH;
    case CachedResource::Type::RawResource:
        return makeUnique<LinkPreloadRawResourceClient>(loader, downcast<CachedRawResource>(resource));
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Icon:
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
    case CachedResource::Type::SVGDocumentResource:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::LinkPrefetch:
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        // None of these values is currently supported as an `as` value.
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

bool LinkLoader::isSupportedType(CachedResource::Type resourceType, const String& mimeType)
{
    if (mimeType.isEmpty())
        return true;
    switch (resourceType) {
    case CachedResource::Type::ImageResource:
        return MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(mimeType);
    case CachedResource::Type::Script:
        return MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType);
    case CachedResource::Type::CSSStyleSheet:
        return MIMETypeRegistry::isSupportedStyleSheetMIMEType(mimeType);
    case CachedResource::Type::FontResource:
        return MIMETypeRegistry::isSupportedFontMIMEType(mimeType);
    case CachedResource::Type::MediaResource:
        if (!RuntimeEnabledFeatures::sharedFeatures().mediaPreloadingEnabled())
            ASSERT_NOT_REACHED();
        return MIMETypeRegistry::isSupportedMediaMIMEType(mimeType);
    case CachedResource::Type::TextTrackResource:
        return MIMETypeRegistry::isSupportedTextTrackMIMEType(mimeType);
    case CachedResource::Type::RawResource:
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        return true;
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void LinkLoader::preconnectIfNeeded(const LinkLoadParameters& params, Document& document)
{
    const URL href = params.href;
    if (!params.relAttribute.isLinkPreconnect || !href.isValid() || !params.href.protocolIsInHTTPFamily() || !document.frame())
        return;
    ASSERT(document.settings().linkPreconnectEnabled());
    StoredCredentialsPolicy storageCredentialsPolicy = StoredCredentialsPolicy::Use;
    if (equalIgnoringASCIICase(params.crossOrigin, "anonymous") && document.securityOrigin().canAccess(SecurityOrigin::create(href)))
        storageCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;
    ASSERT(document.frame()->loader().networkingContext());
    platformStrategies()->loaderStrategy()->preconnectTo(document.frame()->loader(), href, storageCredentialsPolicy, [weakDocument = makeWeakPtr(document), href](ResourceError error) {
        if (!weakDocument)
            return;

        if (!error.isNull())
            weakDocument->addConsoleMessage(MessageSource::Network, MessageLevel::Error, makeString("Failed to preconnect to "_s, href.string(), ". Error: "_s, error.localizedDescription()));
        else
            weakDocument->addConsoleMessage(MessageSource::Network, MessageLevel::Info, makeString("Successfuly preconnected to "_s, href.string()));
    });
}

std::unique_ptr<LinkPreloadResourceClient> LinkLoader::preloadIfNeeded(const LinkLoadParameters& params, Document& document, LinkLoader* loader)
{
    if (!document.loader() || !params.relAttribute.isLinkPreload)
        return nullptr;

    ASSERT(RuntimeEnabledFeatures::sharedFeatures().linkPreloadEnabled());
    auto type = LinkLoader::resourceTypeFromAsAttribute(params.as);
    if (!type) {
        document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> must have a valid `as` value"_s);
        return nullptr;
    }
    URL url;
    if (RuntimeEnabledFeatures::sharedFeatures().linkPreloadResponsiveImagesEnabled() && type == CachedResource::Type::ImageResource && !params.imageSrcSet.isEmpty()) {
        auto sourceSize = SizesAttributeParser(params.imageSizes, document).length();
        auto candidate = bestFitSourceForImageAttributes(document.deviceScaleFactor(), params.href.string(), params.imageSrcSet, sourceSize);
        url = document.completeURL(URL({ }, candidate.string.toString()).string());
    } else
        url = document.completeURL(params.href.string());

    if (!url.isValid()) {
        if (params.imageSrcSet.isEmpty())
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> has an invalid `href` value"_s);
        else
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> has an invalid `imagesrcset` value"_s);
        return nullptr;
    }
    if (!MediaQueryEvaluator::mediaAttributeMatches(document, params.media))
        return nullptr;
    if (!isSupportedType(type.value(), params.mimeType))
        return nullptr;

    auto options = CachedResourceLoader::defaultCachedResourceOptions();
    options.referrerPolicy = params.referrerPolicy;
    auto linkRequest = createPotentialAccessControlRequest(url, WTFMove(options), document, params.crossOrigin);
    linkRequest.setPriority(CachedResource::defaultPriorityForResourceType(type.value()));
    linkRequest.setInitiator("link");
    linkRequest.setIgnoreForRequestCount(true);
    linkRequest.setIsLinkPreload();

    auto cachedLinkResource = document.cachedResourceLoader().preload(type.value(), WTFMove(linkRequest)).value_or(nullptr);

    if (cachedLinkResource && cachedLinkResource->type() != *type)
        return nullptr;

    if (cachedLinkResource && loader)
        return createLinkPreloadResourceClient(*cachedLinkResource, *loader);
    return nullptr;
}

void LinkLoader::prefetchIfNeeded(const LinkLoadParameters& params, Document& document)
{
    if (!params.href.isValid() || !document.frame())
        return;

    ASSERT(RuntimeEnabledFeatures::sharedFeatures().linkPrefetchEnabled());
    Optional<ResourceLoadPriority> priority;
    CachedResource::Type type = CachedResource::Type::LinkPrefetch;

    if (m_cachedLinkResource) {
        m_cachedLinkResource->removeClient(*this);
        m_cachedLinkResource = nullptr;
    }
    // FIXME: Add further prefetch restrictions/limitations:
    // - third-party iframes cannot trigger prefetches
    // - Number of prefetches of a given page is limited (to 1 maybe?)
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;
    options.certificateInfoPolicy = CertificateInfoPolicy::IncludeCertificateInfo;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.redirect = FetchOptions::Redirect::Manual;
    options.mode = FetchOptions::Mode::Navigate;
    options.serviceWorkersMode = ServiceWorkersMode::None;
    options.cachingPolicy = CachingPolicy::DisallowCaching;
    options.referrerPolicy = params.referrerPolicy;
    m_cachedLinkResource = document.cachedResourceLoader().requestLinkResource(type, CachedResourceRequest(ResourceRequest { document.completeURL(params.href.string()) }, options, priority)).value_or(nullptr);
    if (m_cachedLinkResource)
        m_cachedLinkResource->addClient(*this);
}

void LinkLoader::cancelLoad()
{
    if (m_preloadResourceClient)
        m_preloadResourceClient->clear();
}

void LinkLoader::loadLink(const LinkLoadParameters& params, Document& document)
{
    if (params.relAttribute.isDNSPrefetch) {
        // FIXME: The href attribute of the link element can be in "//hostname" form, and we shouldn't attempt
        // to complete that as URL <https://bugs.webkit.org/show_bug.cgi?id=48857>.
        if (document.settings().dnsPrefetchingEnabled() && params.href.isValid() && !params.href.isEmpty() && document.frame())
            document.frame()->loader().client().prefetchDNS(params.href.host().toString());
    }

    preconnectIfNeeded(params, document);

    if (params.relAttribute.isLinkPrefetch) {
        prefetchIfNeeded(params, document);
        return;
    }

    if (m_client.shouldLoadLink()) {
        auto resourceClient = preloadIfNeeded(params, document, this);
        if (m_preloadResourceClient)
            m_preloadResourceClient->clear();
        if (resourceClient)
            m_preloadResourceClient = WTFMove(resourceClient);
    }
}

}
