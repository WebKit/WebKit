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
#include "DefaultResourceLoadPriority.h"
#include "DocumentInlines.h"
#include "FetchRequestDestination.h"
#include "FrameLoader.h"
#include "HTMLSrcsetParser.h"
#include "JSFetchRequestDestination.h"
#include "LinkHeader.h"
#include "LinkPreloadResourceClients.h"
#include "LinkRelAttribute.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "MIMETypeRegistry.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "NodeRenderStyle.h"
#include "PlatformStrategies.h"
#include "RenderElement.h"
#include "ResourceError.h"
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
    if (CachedResourceHandle cachedLinkResource = m_cachedLinkResource)
        cachedLinkResource->removeClient(*this);
    if (m_preloadResourceClient)
        m_preloadResourceClient->clear();
}

void LinkLoader::triggerEvents(const CachedResource& resource)
{
    if (resource.errorOccurred())
        m_client->linkLoadingErrored();
    else
        m_client->linkLoaded();
}

void LinkLoader::triggerError()
{
    m_client->linkLoadingErrored();
}

void LinkLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    ASSERT_UNUSED(resource, m_cachedLinkResource.get() == &resource);

    CachedResourceHandle cachedLinkResource = m_cachedLinkResource;
    triggerEvents(*cachedLinkResource);

    cachedLinkResource->removeClient(*this);
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

        RequestPriority fetchPriorityHint = RequestPriority::Auto;
        if (document.settings().fetchPriorityEnabled())
            fetchPriorityHint = parseEnumerationFromString<RequestPriority>(header.fetchPriorityHint()).value_or(RequestPriority::Auto);

        LinkLoadParameters params { relAttribute, url, header.as(), header.media(), header.mimeType(), header.crossOrigin(), header.imageSrcSet(), header.imageSizes(), header.nonce(),
            parseReferrerPolicy(header.referrerPolicy(), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString), fetchPriorityHint };

        preconnectIfNeeded(params, document);
        preloadIfNeeded(params, document, nullptr);
    }
}

std::optional<CachedResource::Type> LinkLoader::resourceTypeFromAsAttribute(const String& as, Document& document, ShouldLog shouldLogError)
{
    if (equalLettersIgnoringASCIICase(as, "fetch"_s))
        return CachedResource::Type::RawResource;
    auto destination = parseEnumerationFromString<FetchRequestDestination>(as);
    if (!destination) {
        if (shouldLogError == ShouldLog::Yes)
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> must have a valid `as` value"_s);
        return std::nullopt;
    }
    switch (*destination) {
    case FetchRequestDestination::EmptyString:
        if (shouldLogError == ShouldLog::Yes)
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> cannot have the empty string as `as` value"_s);
        return std::nullopt;
    case FetchRequestDestination::Audio:
        if (document.settings().mediaPreloadingEnabled())
            return CachedResource::Type::MediaResource;
        return std::nullopt;
    case FetchRequestDestination::Audioworklet:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Document:
        return std::nullopt;
    case FetchRequestDestination::Embed:
        return std::nullopt;
    case FetchRequestDestination::Font:
        return CachedResource::Type::FontResource;
    case FetchRequestDestination::Image:
        return CachedResource::Type::ImageResource;
    case FetchRequestDestination::Iframe:
        return std::nullopt;
    case FetchRequestDestination::Manifest:
        return std::nullopt;
    case FetchRequestDestination::Model:
        return std::nullopt;
    case FetchRequestDestination::Object:
        return std::nullopt;
    case FetchRequestDestination::Paintworklet:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Report:
        return std::nullopt;
    case FetchRequestDestination::Script:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Serviceworker:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Sharedworker:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Style:
        return CachedResource::Type::CSSStyleSheet;
    case FetchRequestDestination::Track:
        return CachedResource::Type::TextTrackResource;
    case FetchRequestDestination::Video:
        if (document.settings().mediaPreloadingEnabled())
            return CachedResource::Type::MediaResource;
        return std::nullopt;
    case FetchRequestDestination::Worker:
        return CachedResource::Type::Script;
    case FetchRequestDestination::Xslt:
        return std::nullopt;
    }
    return std::nullopt;
}

static std::unique_ptr<LinkPreloadResourceClient> createLinkPreloadResourceClient(CachedResource& resource, LinkLoader& loader, Document& document)
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
        ASSERT_UNUSED(document, document.settings().mediaPreloadingEnabled());
        FALLTHROUGH;
    case CachedResource::Type::RawResource:
        return makeUnique<LinkPreloadRawResourceClient>(loader, downcast<CachedRawResource>(resource));
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::SVGFontResource:
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
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
#endif
        // None of these values is currently supported as an `as` value.
        ASSERT_NOT_REACHED();
    }
    return nullptr;
}

bool LinkLoader::isSupportedType(CachedResource::Type resourceType, const String& mimeType, Document& document)
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
        if (!document.settings().mediaPreloadingEnabled())
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
    if (equalLettersIgnoringASCIICase(params.crossOrigin, "anonymous"_s) && !document.securityOrigin().isSameOriginDomain(SecurityOrigin::create(href)))
        storageCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;
    ASSERT(document.frame()->loader().networkingContext());
    platformStrategies()->loaderStrategy()->preconnectTo(document.protectedFrame()->checkedLoader(), href, storageCredentialsPolicy, LoaderStrategy::ShouldPreconnectAsFirstParty::No, [weakDocument = WeakPtr { document }, href](ResourceError error) {
        RefPtr document = weakDocument.get();
        if (!document)
            return;

        if (!error.isNull())
            document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, makeString("Failed to preconnect to "_s, href.string(), ". Error: "_s, error.localizedDescription()));
        else
            document->addConsoleMessage(MessageSource::Network, MessageLevel::Info, makeString("Successfully preconnected to "_s, href.string()));
    });
}

std::unique_ptr<LinkPreloadResourceClient> LinkLoader::preloadIfNeeded(const LinkLoadParameters& params, Document& document, LinkLoader* loader)
{
    std::optional<CachedResource::Type> type;
    if (!document.loader())
        return nullptr;

    if (params.relAttribute.isLinkModulePreload) {
        type = LinkLoader::resourceTypeFromAsAttribute(params.as, document, ShouldLog::No);
        if (!type)
            type = CachedResource::Type::Script;
        if (type && type != CachedResource::Type::Script) {
            if (loader)
                loader->triggerError();
            return nullptr;
        }
    } else if (params.relAttribute.isLinkPreload) {
        ASSERT(document.settings().linkPreloadEnabled());
        type = LinkLoader::resourceTypeFromAsAttribute(params.as, document, ShouldLog::Yes);
    }

    if (!type)
        return nullptr;

    URL url;
    if (document.settings().linkPreloadResponsiveImagesEnabled() && type == CachedResource::Type::ImageResource && !params.imageSrcSet.isEmpty()) {
        auto sourceSize = SizesAttributeParser(params.imageSizes, document).length();
        auto candidate = bestFitSourceForImageAttributes(document.deviceScaleFactor(), AtomString { params.href.string() }, params.imageSrcSet, sourceSize);
        url = document.completeURL(URL({ }, candidate.string.toString()).string());
    } else
        url = document.completeURL(params.href.string());

    if (!url.isValid()) {
        if (params.relAttribute.isLinkModulePreload)
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=modulepreload> has an invalid `href` value"_s);
        else if (params.imageSrcSet.isEmpty())
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> has an invalid `href` value"_s);
        else
            document.addConsoleMessage(MessageSource::Other, MessageLevel::Error, "<link rel=preload> has an invalid `imagesrcset` value"_s);
        return nullptr;
    }
    auto queries = MQ::MediaQueryParser::parse(params.media, { document });
    if (!MQ::MediaQueryEvaluator { screenAtom(), document, document.renderStyle() }.evaluate(queries))
        return nullptr;
    if (!isSupportedType(type.value(), params.mimeType, document))
        return nullptr;

    auto options = CachedResourceLoader::defaultCachedResourceOptions();
    options.referrerPolicy = params.referrerPolicy;
    options.fetchPriorityHint = params.fetchPriorityHint;
    options.nonce = params.nonce;

    auto linkRequest = [&]() {
        if (params.relAttribute.isLinkModulePreload) {
            options.mode = FetchOptions::Mode::Cors;
            options.credentials = equalLettersIgnoringASCIICase(params.crossOrigin, "use-credentials"_s) ? FetchOptions::Credentials::Include : FetchOptions::Credentials::SameOrigin;
            CachedResourceRequest cachedRequest { ResourceRequest { url }, WTFMove(options) };
            cachedRequest.setOrigin(document.securityOrigin());
            updateRequestForAccessControl(cachedRequest.resourceRequest(), document.securityOrigin(), options.storedCredentialsPolicy);
            return cachedRequest;
        }
        return createPotentialAccessControlRequest(url, WTFMove(options), document, params.crossOrigin);
    }();
    linkRequest.setPriority(DefaultResourceLoadPriority::forResourceType(type.value()));
    linkRequest.setInitiatorType("link"_s);
    linkRequest.setIgnoreForRequestCount(true);
    linkRequest.setIsLinkPreload();

    auto cachedLinkResource = document.protectedCachedResourceLoader()->preload(type.value(), WTFMove(linkRequest)).value_or(nullptr);

    if (cachedLinkResource && cachedLinkResource->type() != *type)
        return nullptr;

    if (cachedLinkResource && loader)
        return createLinkPreloadResourceClient(*cachedLinkResource, *loader, document);

    if (loader)
        loader->triggerError();
    return nullptr;
}

void LinkLoader::prefetchIfNeeded(const LinkLoadParameters& params, Document& document)
{
    if (!params.href.isValid() || !document.frame())
        return;

    ASSERT(document.settings().linkPrefetchEnabled());
    std::optional<ResourceLoadPriority> priority;
    CachedResource::Type type = CachedResource::Type::LinkPrefetch;

    if (m_cachedLinkResource) {
        m_cachedLinkResource->removeClient(*this);
        m_cachedLinkResource = nullptr;
    }
    // FIXME: Add further prefetch restrictions/limitations:
    // - third-party iframes cannot trigger prefetches
    // - Number of prefetches of a given page is limited (to 1 maybe?)
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::DoPolicyCheck;
    options.certificateInfoPolicy = CertificateInfoPolicy::IncludeCertificateInfo;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.redirect = FetchOptions::Redirect::Manual;
    options.mode = FetchOptions::Mode::Navigate;
    options.serviceWorkersMode = ServiceWorkersMode::None;
    options.cachingPolicy = CachingPolicy::DisallowCaching;
    options.referrerPolicy = params.referrerPolicy;
    options.nonce = params.nonce;
    m_cachedLinkResource = document.protectedCachedResourceLoader()->requestLinkResource(type, CachedResourceRequest(ResourceRequest { document.completeURL(params.href.string()) }, options, priority)).value_or(nullptr);
    if (CachedResourceHandle cachedLinkResource = m_cachedLinkResource)
        cachedLinkResource->addClient(*this);
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
            document.protectedFrame()->checkedLoader()->client().prefetchDNS(params.href.host().toString());
    }

    preconnectIfNeeded(params, document);

    if (params.relAttribute.isLinkPrefetch) {
        prefetchIfNeeded(params, document);
        return;
    }

    if (m_client->shouldLoadLink()) {
        auto resourceClient = preloadIfNeeded(params, document, this);
        if (m_preloadResourceClient)
            m_preloadResourceClient->clear();
        if (resourceClient)
            m_preloadResourceClient = WTFMove(resourceClient);
    }
}

}
