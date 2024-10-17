/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004-2023 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedResourceLoader.h"

#include "CachePolicy.h"
#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedRawResource.h"
#include "CachedResourceRequest.h"
#include "CachedSVGDocument.h"
#include "CachedSVGFont.h"
#include "CachedScript.h"
#include "CachedTextTrack.h"
#include "CachedXSLStyleSheet.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContentExtensionError.h"
#include "ContentExtensionRule.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "CookieJar.h"
#include "CrossOriginAccessControl.h"
#include "CustomHeaderFields.h"
#include "DNS.h"
#include "DateComponents.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTTPHeaderField.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include "Quirks.h"
#include "RenderElement.h"
#include "ResourceLoadInfo.h"
#include "ResourceTiming.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "ServiceWorker.h"
#include "Settings.h"
#include "StyleSheetContents.h"
#include "SubresourceLoader.h"
#include "UserContentController.h"
#include "UserStyleSheet.h"
#include <pal/SessionID.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "CachedApplicationManifest.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/Device.h>
#endif

#undef CACHEDRESOURCELOADER_RELEASE_LOG
#define PAGE_ID(frame) (frame.pageID() ? frame.pageID()->toUInt64() : 0)
#define FRAME_ID(frame) (frame.frameID().object().toUInt64())
#define CACHEDRESOURCELOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - CachedResourceLoader::" fmt, this, ##__VA_ARGS__)
#define CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME(fmt, frame, ...) RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 "] CachedResourceLoader::" fmt, this, PAGE_ID(frame), FRAME_ID(frame), ##__VA_ARGS__)

namespace WebCore {

// Timeout for link preloads to be used after window.onload
static const Seconds unusedPreloadTimeout { 3_s };

template <typename T, typename U>
static inline ResourceErrorOr<CachedResourceHandle<T>> castCachedResourceTo(ResourceErrorOr<CachedResourceHandle<U>>&& cachedResource)
{
    if (cachedResource)
        return CachedResourceHandle<T> { static_cast<T*>(cachedResource.value().get()) };
    return makeUnexpected(cachedResource.error());
}

static ScriptRequiresTelemetry scriptRequiresTelemetry(const Document* document, const ResourceRequest& request)
{
    if (UNLIKELY(!document))
        return ScriptRequiresTelemetry::No;

    RefPtr page = document->page();
    if (UNLIKELY(!page))
        return ScriptRequiresTelemetry::No;

    return page->requiresScriptTelemetryForURL(request.url()) ? ScriptRequiresTelemetry::Yes : ScriptRequiresTelemetry::No;
}

static CachedResourceHandle<CachedResource> createResource(CachedResource::Type type, CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, const Settings& settings, const Document* document)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return new CachedImage(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::Script:
        return new CachedScript(WTFMove(request), sessionID, cookieJar, scriptRequiresTelemetry(document, request.resourceRequest()));
    case CachedResource::Type::SVGDocumentResource:
        return new CachedSVGDocument(WTFMove(request), sessionID, cookieJar, settings);
    case CachedResource::Type::SVGFontResource:
        return new CachedSVGFont(WTFMove(request), sessionID, cookieJar, settings);
    case CachedResource::Type::FontResource:
        return new CachedFont(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::MediaResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::MainResource:
        return new CachedRawResource(WTFMove(request), type, sessionID, cookieJar);
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return new CachedXSLStyleSheet(WTFMove(request), sessionID, cookieJar);
#endif
    case CachedResource::Type::LinkPrefetch:
        return new CachedResource(WTFMove(request), CachedResource::Type::LinkPrefetch, sessionID, cookieJar);
    case CachedResource::Type::TextTrackResource:
        return new CachedTextTrack(WTFMove(request), sessionID, cookieJar);
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return new CachedApplicationManifest(WTFMove(request), sessionID, cookieJar);
#endif
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static CachedResourceHandle<CachedResource> createResource(CachedResourceRequest&& request, CachedResource& resource, const Document* document)
{
    switch (resource.type()) {
    case CachedResource::Type::ImageResource:
        return new CachedImage(WTFMove(request), resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::Script:
        return new CachedScript(WTFMove(request), resource.sessionID(), resource.cookieJar(), scriptRequiresTelemetry(document, request.resourceRequest()));
    case CachedResource::Type::SVGDocumentResource:
        return new CachedSVGDocument(WTFMove(request), downcast<CachedSVGDocument>(resource));
    case CachedResource::Type::SVGFontResource:
        return new CachedSVGFont(WTFMove(request), downcast<CachedSVGFont>(resource));
    case CachedResource::Type::FontResource:
        return new CachedFont(WTFMove(request), resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::MainResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
        return new CachedRawResource(WTFMove(request), resource.type(), resource.sessionID(), resource.cookieJar());
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return new CachedXSLStyleSheet(WTFMove(request), resource.sessionID(), resource.cookieJar());
#endif
    case CachedResource::Type::LinkPrefetch:
        return new CachedResource(WTFMove(request), CachedResource::Type::LinkPrefetch, resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::TextTrackResource:
        return new CachedTextTrack(WTFMove(request), resource.sessionID(), resource.cookieJar());
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return new CachedApplicationManifest(WTFMove(request), resource.sessionID(), resource.cookieJar());
#endif
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

CachedResourceLoader::CachedResourceLoader(DocumentLoader* documentLoader)
    : m_documentLoader(documentLoader)
    , m_unusedPreloadsTimer(*this, &CachedResourceLoader::warnUnusedPreloads)
    , m_garbageCollectDocumentResourcesTimer(*this, &CachedResourceLoader::garbageCollectDocumentResources)
{
}

CachedResourceLoader::~CachedResourceLoader()
{
    m_documentLoader = nullptr;
    m_document = nullptr;

    clearPreloads(ClearPreloadsMode::ClearAllPreloads);

    // Make sure no requests still point to this CachedResourceLoader
    ASSERT(m_requestCount == 0);
    m_unusedPreloadsTimer.stop();
}

CachedResource* CachedResourceLoader::cachedResource(const String& resourceURL) const
{
    ASSERT(!resourceURL.isNull());
    ASSERT(m_document);
    return m_document ? cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(m_document->completeURL(resourceURL))) : nullptr;
}

CachedResource* CachedResourceLoader::cachedResource(const URL& url) const
{
    ASSERT(!MemoryCache::shouldRemoveFragmentIdentifier(url));
    return m_documentResources.get(url.string()).get();
}

LocalFrame* CachedResourceLoader::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : nullptr;
}

ResourceErrorOr<CachedResourceHandle<CachedImage>> CachedResourceLoader::requestImage(CachedResourceRequest&& request, ImageLoading imageLoading)
{
    if (RefPtr frame = this->frame()) {
        if (frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None) {
            bool isRequestUpgradable { false };
            if (RefPtr document = frame->document()) {
                isRequestUpgradable = MixedContentChecker::shouldUpgradeInsecureContent(*frame, MixedContentChecker::IsUpgradable::Yes, request.resourceRequest().url(), request.options().mode, request.options().destination, request.options().initiator);
                request.upgradeInsecureRequestIfNeeded(*document, isRequestUpgradable ? ContentSecurityPolicy::AlwaysUpgradeRequest::Yes : ContentSecurityPolicy::AlwaysUpgradeRequest::No);
            }
            URL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(CachedResource::Type::ImageResource, requestURL, request.options(), ForPreload::No, isRequestUpgradable ? MixedContentChecker::IsUpgradable::Yes : MixedContentChecker::IsUpgradable::No))
                PingLoader::loadImage(*frame, requestURL);
            return CachedResourceHandle<CachedImage> { };
        }
    }

    if (imageLoading == ImageLoading::Immediate)
        imageLoading = clientDefersImage(request.resourceRequest().url());
    return castCachedResourceTo<CachedImage>(requestResource(CachedResource::Type::ImageResource, WTFMove(request), ForPreload::No, imageLoading));
}

ResourceErrorOr<CachedResourceHandle<CachedFont>> CachedResourceLoader::requestFont(CachedResourceRequest&& request, bool isSVG)
{
    if (isSVG)
        return castCachedResourceTo<CachedFont>(requestResource(CachedResource::Type::SVGFontResource, WTFMove(request)));
    return castCachedResourceTo<CachedFont>(requestResource(CachedResource::Type::FontResource, WTFMove(request)));
}

#if ENABLE(VIDEO)
ResourceErrorOr<CachedResourceHandle<CachedTextTrack>> CachedResourceLoader::requestTextTrack(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedTextTrack>(requestResource(CachedResource::Type::TextTrackResource, WTFMove(request)));
}
#endif

ResourceErrorOr<CachedResourceHandle<CachedCSSStyleSheet>> CachedResourceLoader::requestCSSStyleSheet(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedCSSStyleSheet>(requestResource(CachedResource::Type::CSSStyleSheet, WTFMove(request)));
}

CachedResourceHandle<CachedCSSStyleSheet> CachedResourceLoader::requestUserCSSStyleSheet(Page& page, CachedResourceRequest&& request)
{
    request.setDestinationIfNotSet(FetchOptions::Destination::Style);

    ASSERT(document());
    if (RefPtr document = this->document())
        request.setDomainForCachePartition(*document);

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching()) {
        if (CachedResourceHandle existing = memoryCache.resourceForRequest(request.resourceRequest(), page.sessionID())) {
            if (auto* cssStyleSheet = dynamicDowncast<CachedCSSStyleSheet>(*existing))
                return cssStyleSheet;
            memoryCache.remove(*existing);
        }
    }

    request.removeFragmentIdentifierIfNeeded();

    CachedResourceHandle userSheet = new CachedCSSStyleSheet(WTFMove(request), page.sessionID(), page.protectedCookieJar().ptr());

    if (userSheet->allowsCaching())
        memoryCache.add(*userSheet);

    userSheet->load(*this);
    return userSheet;
}

ResourceErrorOr<CachedResourceHandle<CachedScript>> CachedResourceLoader::requestScript(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedScript>(requestResource(CachedResource::Type::Script, WTFMove(request)));
}

#if ENABLE(XSLT)
ResourceErrorOr<CachedResourceHandle<CachedXSLStyleSheet>> CachedResourceLoader::requestXSLStyleSheet(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedXSLStyleSheet>(requestResource(CachedResource::Type::XSLStyleSheet, WTFMove(request)));
}
#endif

ResourceErrorOr<CachedResourceHandle<CachedSVGDocument>> CachedResourceLoader::requestSVGDocument(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedSVGDocument>(requestResource(CachedResource::Type::SVGDocumentResource, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::requestLinkResource(CachedResource::Type type, CachedResourceRequest&& request)
{
    ASSERT(frame());
    ASSERT(type == CachedResource::Type::LinkPrefetch);
    return requestResource(type, WTFMove(request));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestMedia(CachedResourceRequest&& request)
{
    auto resourceType = CachedResource::Type::MediaResource;
    switch (request.options().destination) {
    case FetchOptions::Destination::Audio:
    case FetchOptions::Destination::Video:
        break;
    case FetchOptions::Destination::Environmentmap:
#if ENABLE(MODEL_ELEMENT)
        resourceType = CachedResource::Type::EnvironmentMapResource;
        break;
#endif
    case FetchOptions::Destination::Model:
#if ENABLE(MODEL_ELEMENT)
        resourceType = CachedResource::Type::ModelResource;
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }

    return castCachedResourceTo<CachedRawResource>(requestResource(resourceType, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestIcon(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::Icon, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestRawResource(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::RawResource, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestBeaconResource(CachedResourceRequest&& request)
{
    ASSERT(request.options().destination == FetchOptions::Destination::EmptyString);
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::Beacon, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestPingResource(CachedResourceRequest&& request)
{
    ASSERT(request.options().destination == FetchOptions::Destination::EmptyString || request.options().destination == FetchOptions::Destination::Report);
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::Ping, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestMainResource(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::MainResource, WTFMove(request)));
}

#if ENABLE(APPLICATION_MANIFEST)
ResourceErrorOr<CachedResourceHandle<CachedApplicationManifest>> CachedResourceLoader::requestApplicationManifest(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedApplicationManifest>(requestResource(CachedResource::Type::ApplicationManifest, WTFMove(request)));
}
#endif // ENABLE(APPLICATION_MANIFEST)

#if ENABLE(MODEL_ELEMENT)
ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestEnvironmentMapResource(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::EnvironmentMapResource, WTFMove(request)));
}

ResourceErrorOr<CachedResourceHandle<CachedRawResource>> CachedResourceLoader::requestModelResource(CachedResourceRequest&& request)
{
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::ModelResource, WTFMove(request)));
}
#endif

static MixedContentChecker::ContentType contentTypeFromResourceType(CachedResource::Type type)
{
    switch (type) {
    // https://w3c.github.io/webappsec-mixed-content/#category-optionally-blockable
    // Editor's Draft, 11 February 2016
    // 3.1. Optionally-blockable Content
    case CachedResource::Type::ImageResource:
    case CachedResource::Type::MediaResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
        return MixedContentChecker::ContentType::ActiveCanWarn;

    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::Script:
    case CachedResource::Type::FontResource:
        return MixedContentChecker::ContentType::Active;

    case CachedResource::Type::SVGFontResource:
        return MixedContentChecker::ContentType::Active;

    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::SVGDocumentResource:
        return MixedContentChecker::ContentType::Active;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return MixedContentChecker::ContentType::Active;
#endif

    case CachedResource::Type::LinkPrefetch:
        return MixedContentChecker::ContentType::Active;

#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
        return MixedContentChecker::ContentType::Active;
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return MixedContentChecker::ContentType::Active;
#endif
    default:
        ASSERT_NOT_REACHED();
        return MixedContentChecker::ContentType::Active;
    }
}

static MixedContentChecker::IsUpgradable isUpgradableTypeFromResourceType(CachedResource::Type type)
{
    // https://www.w3.org/TR/mixed-content/#category-upgradeable
    // Editor’s Draft, 23 February 2023
    // 3.1. Upgradeable Content
    if (type == CachedResource::Type::ImageResource
#if ENABLE(MODEL_ELEMENT)
        || type == CachedResource::Type::EnvironmentMapResource
        || type == CachedResource::Type::ModelResource
#endif
        || type == CachedResource::Type::MediaResource)
        return MixedContentChecker::IsUpgradable::Yes;

    return MixedContentChecker::IsUpgradable::No;
}

bool CachedResourceLoader::checkInsecureContent(CachedResource::Type type, const URL& url, MixedContentChecker::IsUpgradable isRequestUpgradable) const
{
    if (!canRequestInContentDispositionAttachmentSandbox(type, url))
        return false;

    if (m_document && !m_document->settings().upgradeMixedContentEnabled()) {
        // These resource can inject script into the current document (Script,
        // XSL) or exfiltrate the content of the current document (CSS).
        // This block is a special-case for maintaining backwards compatibility.
        if (type == CachedResource::Type::Script
#if ENABLE(XSLT)
            || type == CachedResource::Type::XSLStyleSheet
#endif
            || type == CachedResource::Type::SVGDocumentResource
            || type == CachedResource::Type::CSSStyleSheet) {

            if (RefPtr frame = this->frame())
                return MixedContentChecker::frameAndAncestorsCanRunInsecureContent(*frame, m_document->protectedSecurityOrigin(), url);
        }
    }

    switch (type) {
    case CachedResource::Type::Script:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::CSSStyleSheet:
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
#endif
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::FontResource: {
        if (RefPtr frame = this->frame()) {
            if (MixedContentChecker::shouldBlockRequestForDisplayableContent(*frame, url, contentTypeFromResourceType(type), isRequestUpgradable))
                return false;
        }
        break;
    }
    case CachedResource::Type::MainResource:
    case CachedResource::Type::LinkPrefetch:
        // Prefetch cannot affect the current document.
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        break;
    }
    return true;
}

bool CachedResourceLoader::allowedByContentSecurityPolicy(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, ContentSecurityPolicy::RedirectResponseReceived redirectResponseReceived, const URL& preRedirectURL) const
{
    if (options.contentSecurityPolicyImposition == ContentSecurityPolicyImposition::SkipPolicyCheck)
        return true;

    RefPtr document = m_document.get();
    ASSERT(document);
    if (!document)
        return true;

    ASSERT(document->contentSecurityPolicy());
    CheckedPtr contentSecurityPolicy = document->contentSecurityPolicy();
    if (!contentSecurityPolicy)
        return true;

    // All content loaded through embed or object elements goes through object-src: https://www.w3.org/TR/CSP3/#directive-object-src.
    if (options.loadedFromPluginElement == LoadedFromPluginElement::Yes
        && !contentSecurityPolicy->allowObjectFromSource(url, redirectResponseReceived, preRedirectURL))
        return false;

    switch (type) {
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::Script:
        if (!contentSecurityPolicy->allowScriptFromSource(url, redirectResponseReceived, preRedirectURL, options.integrity, options.nonce))
            return false;
        break;
    case CachedResource::Type::CSSStyleSheet:
        if (!contentSecurityPolicy->allowStyleFromSource(url, redirectResponseReceived, preRedirectURL, options.nonce))
            return false;
        break;
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
        if (!contentSecurityPolicy->allowImageFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::LinkPrefetch:
        if (!contentSecurityPolicy->allowPrefetchFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
        if (!contentSecurityPolicy->allowFontFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::MediaResource:
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
#endif
        if (!contentSecurityPolicy->allowMediaFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::RawResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
        return true;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        if (!contentSecurityPolicy->allowManifestFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

static inline bool isSameOriginDataURL(const URL& url, const ResourceLoaderOptions& options)
{
    // FIXME: Remove same-origin data URL flag since it was removed from fetch spec (https://github.com/whatwg/fetch/issues/381).
    return url.protocolIsData() && options.sameOriginDataURLFlag == SameOriginDataURLFlag::Set;
}

RefPtr<LocalFrame> CachedResourceLoader::protectedFrame() const
{
    return frame();
}

// Security checks defined in https://fetch.spec.whatwg.org/#main-fetch.
bool CachedResourceLoader::canRequest(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, ForPreload forPreload, MixedContentChecker::IsUpgradable isRequestUpgradable)
{
    if (m_document) {
        if (!m_document->protectedSecurityOrigin()->canDisplay(url, OriginAccessPatternsForWebProcess::singleton())) {
            if (forPreload == ForPreload::No)
                FrameLoader::reportLocalLoadFailed(protectedFrame().get(), url.stringCenterEllipsizedToLength());
            LOG(ResourceLoading, "CachedResourceLoader::requestResource URL was not allowed by SecurityOrigin::canDisplay");
            return false;
        }

        if (options.mode == FetchOptions::Mode::SameOrigin && !m_document->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton()) && !isSameOriginDataURL(url, options)) {
            printAccessDeniedMessage(url);
            return false;
        }

        if (options.mode == FetchOptions::Mode::NoCors && !m_document->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton()) && options.redirect != FetchOptions::Redirect::Follow && type != CachedResource::Type::Ping) {
            ASSERT(type != CachedResource::Type::MainResource);
            if (RefPtr frame = this->frame()) {
                if (RefPtr frameDocument = frame->document())
                    frameDocument->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "No-Cors mode requires follow redirect mode"_s);
            }
            return false;
        }

        if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::No))
            return false;
    }

    // SVG Images have unique security rules that prevent all subresource requests except for data urls.
    if (type != CachedResource::Type::MainResource && frame()) {
        if (RefPtr page = frame()->page()) {
            if (page->chrome().client().isSVGImageChromeClient() && !url.protocolIsData())
                return false;
        }
    }

    // Last of all, check for insecure content. We do this last so that when folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.

    // FIXME: Should we consider whether the request is for preload here?
    if (!checkInsecureContent(type, url, isRequestUpgradable))
        return false;

    return true;
}

// FIXME: Should we find a way to know whether the redirection is for a preload request like we do for CachedResourceLoader::canRequest?
bool CachedResourceLoader::canRequestAfterRedirection(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, const URL& preRedirectURL) const
{
    if (m_document) {
        if (!m_document->protectedSecurityOrigin()->canDisplay(url, OriginAccessPatternsForWebProcess::singleton())) {
            FrameLoader::reportLocalLoadFailed(protectedFrame().get(), url.stringCenterEllipsizedToLength());
            LOG(ResourceLoading, "CachedResourceLoader::canRequestAfterRedirection URL was not allowed by SecurityOrigin::canDisplay");
            CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by SecurityOrigin::canDisplay");
            return false;
        }

        // FIXME: According to https://fetch.spec.whatwg.org/#http-redirect-fetch, we should check that the URL is HTTP(s) except if in navigation mode.
        // But we currently allow at least data URLs to be loaded.

        if (options.mode == FetchOptions::Mode::SameOrigin && !m_document->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton())) {
            CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by SecurityOrigin::canRequest");
            printAccessDeniedMessage(url);
            return false;
        }

        if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::Yes, preRedirectURL)) {
            CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by content policy");
            return false;
        }
    }

    // Last of all, check for insecure content. We do this last so that when folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.
    if (!checkInsecureContent(type, url, MixedContentChecker::IsUpgradable::No)) {
        CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed because content is insecure");
        return false;
    }

    return true;
}

// Created by binding generator.
String convertEnumerationToString(FetchOptions::Destination);
String convertEnumerationToString(FetchOptions::Mode);

const String& convertEnumerationToString(FetchMetadataSite enumerationValue)
{
    static NeverDestroyed<const String> none(MAKE_STATIC_STRING_IMPL("none"));
    static NeverDestroyed<const String> sameOrigin(MAKE_STATIC_STRING_IMPL("same-origin"));
    static NeverDestroyed<const String> sameSite(MAKE_STATIC_STRING_IMPL("same-site"));
    static NeverDestroyed<const String> crossSite(MAKE_STATIC_STRING_IMPL("cross-site"));

    switch (enumerationValue) {
    case FetchMetadataSite::None:
        return none;
    case FetchMetadataSite::SameOrigin:
        return sameOrigin;
    case FetchMetadataSite::SameSite:
        return sameSite;
    case FetchMetadataSite::CrossSite:
        return crossSite;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

static void updateRequestFetchMetadataHeaders(ResourceRequest& request, const ResourceLoaderOptions& options, FetchMetadataSite site)
{
    // Implementing step 13 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch as of 22 Feb 2022
    // https://w3c.github.io/webappsec-fetch-metadata/#fetch-integration
    Ref requestOrigin = SecurityOrigin::create(request.url());
    if (!requestOrigin->isPotentiallyTrustworthy())
        return;

    String destinationString;
    // The Fetch IDL documents this as "" while FetchMetadata expects "empty".
    if (options.destination == FetchOptions::Destination::EmptyString || options.loadedFromFetch == LoadedFromFetch::Yes)
        destinationString = "empty"_s;
    else
        destinationString = convertEnumerationToString(options.destination);

    request.setHTTPHeaderField(HTTPHeaderName::SecFetchDest, WTFMove(destinationString));
    request.setHTTPHeaderField(HTTPHeaderName::SecFetchMode, convertEnumerationToString(options.mode));
    request.setHTTPHeaderField(HTTPHeaderName::SecFetchSite, convertEnumerationToString(site));
}

static FetchMetadataSite computeFetchMetadataSiteInternal(const ResourceRequest& request, CachedResource::Type type, FetchOptions::Mode mode, const SecurityOrigin* originalOrigin, const LocalFrame* frame, FetchMetadataSite originalSite, bool isDirectlyUserInitiatedRequest)
{
    ASSERT(frame || originalOrigin);

    // This is true when a user causes a request, such as entering a URL.
    if (mode == FetchOptions::Mode::Navigate && type == CachedResource::Type::MainResource && isDirectlyUserInitiatedRequest)
        return FetchMetadataSite::None;

    Ref contextOrigin = originalOrigin ? *originalOrigin : frame->document()->securityOrigin();
    if (type == CachedResource::Type::MainResource && frame && frame->loader().activeDocumentLoader()) {
        if (auto& request = frame->loader().activeDocumentLoader()->triggeringAction().requester())
            contextOrigin = request->securityOrigin;
    }

    // If this is a redirect we start with the old value.
    // The value can never get more "secure" so a full redirect
    // chain only degrades towards cross-site.
    Ref requestOrigin = SecurityOrigin::create(request.url());
    if ((originalSite == FetchMetadataSite::SameOrigin || originalSite == FetchMetadataSite::None) && contextOrigin->isSameOriginAs(requestOrigin))
        return FetchMetadataSite::SameOrigin;
    if (originalSite != FetchMetadataSite::CrossSite && contextOrigin->isSameSiteAs(requestOrigin))
        return FetchMetadataSite::SameSite;
    return FetchMetadataSite::CrossSite;
}

FetchMetadataSite CachedResourceLoader::computeFetchMetadataSite(const ResourceRequest& request, CachedResource::Type type, FetchOptions::Mode mode, const LocalFrame& frame, bool isDirectlyUserInitiatedRequest)
{
    return computeFetchMetadataSiteInternal(request, type, mode, nullptr, &frame, FetchMetadataSite::SameOrigin, isDirectlyUserInitiatedRequest);
}

FetchMetadataSite CachedResourceLoader::computeFetchMetadataSiteAfterRedirection(const ResourceRequest& request, CachedResource::Type type, FetchOptions::Mode mode, const SecurityOrigin& originalOrigin, FetchMetadataSite originalSite, bool isDirectlyUserInitiatedRequest)
{
    return computeFetchMetadataSiteInternal(request, type, mode, &originalOrigin, nullptr, originalSite, isDirectlyUserInitiatedRequest);
}

static bool shouldPerformHTTPSUpgrade(const URL& originalURL, const URL& newURL, const LocalFrame& frame, CachedResource::Type type, bool isHTTPSByDefaultEnabled, OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections, HTTPSByDefaultMode httpsByDefaultMode)
{
    if (!frame.isMainFrame() || type != CachedResource::Type::MainResource)
        return false;

    const bool isSameSiteBypassEnabled = (originalURL.isEmpty()
        || RegistrableDomain(newURL) == RegistrableDomain(originalURL))
        && (advancedPrivacyProtections.contains(AdvancedPrivacyProtections::HTTPSOnlyExplicitlyBypassedForDomain) || frame.protectedLoader()->shouldSkipHTTPSUpgradeForSameSiteNavigation() || originalURL.protocolIs("http"_s));

    return (isHTTPSByDefaultEnabled || httpsByDefaultMode != HTTPSByDefaultMode::Disabled)
        && newURL.protocolIs("http"_s)
        && !isSameSiteBypassEnabled
        && !frame.protectedLoader()->isHTTPFallbackInProgress();
}

bool CachedResourceLoader::updateRequestAfterRedirection(CachedResource::Type type, ResourceRequest& request, const ResourceLoaderOptions& options, FetchMetadataSite site, const URL& preRedirectURL)
{
    ASSERT(m_documentLoader);
    if (!m_documentLoader)
        return true;

    if (RefPtr document = m_documentLoader->cachedResourceLoader().document()) {
        bool alwaysUpgradeMixedContent = document->frame() ? MixedContentChecker::shouldUpgradeInsecureContent(*document->frame(), isUpgradableTypeFromResourceType(type), request.url(), options.mode, options.destination, options.initiator) : false;
        upgradeInsecureResourceRequestIfNeeded(request, *document, alwaysUpgradeMixedContent ? ContentSecurityPolicy::AlwaysUpgradeRequest::Yes : ContentSecurityPolicy::AlwaysUpgradeRequest::No);
    }

    RefPtr frame = m_documentLoader->frame();
    if (frame) {
        bool isHTTPSByDefaultEnabled = frame->page() ? frame->page()->protectedSettings()->httpsByDefault() : false;
        if (shouldPerformHTTPSUpgrade(preRedirectURL, request.url(), *frame, type, isHTTPSByDefaultEnabled, m_documentLoader->advancedPrivacyProtections(), m_documentLoader->httpsByDefaultMode())) {
            auto portsForUpgradingInsecureScheme = frame->page() ? std::optional { frame->page()->portsForUpgradingInsecureSchemeForTesting() } : std::nullopt;
            auto upgradePort = (portsForUpgradingInsecureScheme && request.url().port() == portsForUpgradingInsecureScheme->first) ? std::optional { portsForUpgradingInsecureScheme->second } : std::nullopt;
            request.upgradeInsecureRequestIfNeeded(ShouldUpgradeLocalhostAndIPAddress::No, upgradePort);
        }
    }

    // FIXME: We might want to align the checks done here with the ones done in CachedResourceLoader::requestResource, content extensions blocking in particular.

    Ref requestOrigin = SecurityOrigin::create(request.url());
    Ref preRedirectOrigin = SecurityOrigin::create(preRedirectURL);

    // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis#section-5.2
    // Any cross-site redirect makes a request not same-site.
    if (!requestOrigin->isSameSiteAs(preRedirectOrigin))
        request.setIsSameSite(false);

    if (frame && (!frame->document() || !frame->document()->quirks().shouldDisableFetchMetadata())) {
        // In the case of a protocol downgrade we strip all FetchMetadata headers.
        // Otherwise we add or update FetchMetadata.
        if (!requestOrigin->isPotentiallyTrustworthy()) {
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchDest);
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchMode);
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchSite);
        } else
            updateRequestFetchMetadataHeaders(request, options, site);
    }

    return canRequestAfterRedirection(type, request.url(), options, preRedirectURL);
}

bool CachedResourceLoader::canRequestInContentDispositionAttachmentSandbox(CachedResource::Type type, const URL& url) const
{
    RefPtr<Document> document;

    // FIXME: Do we want to expand this to all resource types that the mixed content checker would consider active content?
    switch (type) {
    case CachedResource::Type::MainResource:
        if (RefPtr ownerElement = frame() ? frame()->ownerElement() : nullptr) {
            document = &ownerElement->document();
            break;
        }
        return true;
    case CachedResource::Type::CSSStyleSheet:
        document = m_document.get();
        break;
    default:
        return true;
    }

    ASSERT(document);
    if (!document)
        return true;

    if (!document->shouldEnforceContentDispositionAttachmentSandbox() || document->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton()))
        return true;

    auto message = makeString("Unsafe attempt to load URL "_s, url.stringCenterEllipsizedToLength(), " from document with Content-Disposition: attachment at URL "_s, document->url().stringCenterEllipsizedToLength(), '.');
    document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
    return false;
}

bool CachedResourceLoader::shouldContinueAfterNotifyingLoadedFromMemoryCache(const CachedResourceRequest& request, CachedResource& resource, ResourceError& error)
{
    if (!frame() || resource.status() != CachedResource::Cached)
        return true;

    ResourceRequest newRequest = ResourceRequest(resource.url());
    newRequest.setRequester(request.resourceRequest().requester());
    newRequest.setInitiatorIdentifier(request.resourceRequest().initiatorIdentifier());
    if (auto inspectorInitiatorNodeIdentifier = request.resourceRequest().inspectorInitiatorNodeIdentifier())
        newRequest.setInspectorInitiatorNodeIdentifier(*inspectorInitiatorNodeIdentifier);
    if (request.resourceRequest().hiddenFromInspector())
        newRequest.setHiddenFromInspector(true);
    protectedFrame()->protectedLoader()->loadedResourceFromMemoryCache(resource, newRequest, error);

    // FIXME <http://webkit.org/b/113251>: If the delegate modifies the request's
    // URL, it is no longer appropriate to use this CachedResource.
    return !newRequest.isNull();
}

bool CachedResourceLoader::shouldUpdateCachedResourceWithCurrentRequest(const CachedResource& resource, const CachedResourceRequest& request)
{
    // WebKit is not supporting CORS for fonts (https://bugs.webkit.org/show_bug.cgi?id=86817), no need to update the resource before reusing it.
    if (resource.type() == CachedResource::Type::FontResource)
        return false;

    if (resource.type() == CachedResource::Type::SVGFontResource)
        return false;

#if ENABLE(XSLT)
    // Load is same-origin, we do not check for CORS.
    if (resource.type() == CachedResource::Type::XSLStyleSheet)
        return false;
#endif

    // FIXME: We should enable resource reuse for these resource types
    switch (resource.type()) {
    case CachedResource::Type::SVGDocumentResource:
        return false;
    case CachedResource::Type::MainResource:
        return false;
    case CachedResource::Type::LinkPrefetch:
        return false;
    default:
        break;
    }

    if (resource.options().mode != request.options().mode || !serializedOriginsMatch(request.protectedOrigin().get(), resource.protectedOrigin().get()))
        return true;

    if (resource.options().redirect != request.options().redirect && resource.hasRedirections())
        return true;

    return false;
}

static inline bool isResourceSuitableForDirectReuse(const CachedResource& resource, const CachedResourceRequest& request)
{
    // FIXME: For being loaded requests, the response tainting may not be correctly computed if the fetch mode is not the same.
    // Even if the fetch mode is the same, we are not sure that the resource can be reused (Vary: Origin header for instance).
    // We should find a way to improve this.
    if (resource.status() != CachedResource::Cached)
        return false;

    // If the cached resource has not followed redirections, it is incomplete and we should not use it.
    // Let's make sure the memory cache has no such resource.
    ASSERT(resource.response().type() != ResourceResponse::Type::Opaqueredirect);

    // We could support redirect modes other than Follow in case of a redirected resource.
    // This case is rare and is not worth optimizing currently.
    if (request.options().redirect != FetchOptions::Redirect::Follow && resource.hasRedirections())
        return false;

    // FIXME: Implement reuse of cached raw resources.
    if (resource.type() == CachedResource::Type::RawResource || resource.type() == CachedResource::Type::MediaResource)
        return false;

    if (resource.type() == CachedResource::Type::Beacon || resource.type() == CachedResource::Type::Ping)
        return false;

    return true;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::updateCachedResourceWithCurrentRequest(const CachedResource& resource, CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar& cookieJar, const Settings& settings)
{
    if (!isResourceSuitableForDirectReuse(resource, request)) {
        request.setCachingPolicy(CachingPolicy::DisallowCaching);
        return loadResource(resource.type(), sessionID, WTFMove(request), cookieJar, settings, MayAddToMemoryCache::Yes);
    }

    CachedResourceHandle resourceHandle = createResource(resource.type(), WTFMove(request), sessionID, &cookieJar, settings, protectedDocument().get());
    resourceHandle->loadFrom(resource);
    return resourceHandle;
}

void CachedResourceLoader::prepareFetch(CachedResource::Type type, CachedResourceRequest& request)
{
    // Implementing step 1.1 to 1.6 of https://fetch.spec.whatwg.org/#concept-fetch.
    if (RefPtr document = this->document()) {
        if (!request.origin())
            request.setOrigin(document->securityOrigin());
        request.setClientIdentifierIfNeeded(document->identifier());
        if (RefPtr activeServiceWorker = document->activeServiceWorker())
            request.setSelectedServiceWorkerRegistrationIdentifierIfNeeded(activeServiceWorker->registrationIdentifier());
    }

    request.setAcceptHeaderIfNone(type);

    // Accept-Language value is handled in underlying port-specific code.
    // FIXME: Decide whether to support client hints
}

void CachedResourceLoader::updateHTTPRequestHeaders(FrameLoader& frameLoader, CachedResource::Type type, CachedResourceRequest& request)
{
    // Implementing steps 11 to 19 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch as of 22 Feb 2022.

    // FIXME: We should reconcile handling of MainResource with other resources.
    if (type != CachedResource::Type::MainResource)
        request.updateReferrerAndOriginHeaders(frameLoader);
    // FetchMetadata depends on PSL to determine same-site relationships and without this
    // ability it is best to not set any FetchMetadata headers as sites generally expect
    // all of them or none.
    if (frameLoader.frame().document() && !frameLoader.frame().protectedDocument()->quirks().shouldDisableFetchMetadata()) {
        auto site = computeFetchMetadataSite(request.resourceRequest(), type, request.options().mode, frameLoader.frame(), frameLoader.frame().isMainFrame() && m_documentLoader && m_documentLoader->isRequestFromClientOrUserInput());
        updateRequestFetchMetadataHeaders(request.resourceRequest(), request.options(), site);
    }
    request.updateUserAgentHeader(frameLoader);

    if (frameLoader.frame().protectedLoader()->loadType() == FrameLoadType::ReloadFromOrigin)
        request.updateCacheModeIfNeeded(cachePolicy(type, request.resourceRequest().url()));
    request.updateAccordingCacheMode();
    request.updateAcceptEncodingHeader();
}

static FetchOptions::Destination destinationForType(CachedResource::Type type, LocalFrame& frame)
{
    switch (type) {
    case CachedResource::Type::MainResource:
        return frame.isMainFrame() ? FetchOptions::Destination::Document : FetchOptions::Destination::Iframe;
    case CachedResource::Type::SVGDocumentResource:
        return FetchOptions::Destination::Document;
    case CachedResource::Type::ImageResource:
    case CachedResource::Type::Icon:
        return FetchOptions::Destination::Image;
    case CachedResource::Type::CSSStyleSheet:
        return FetchOptions::Destination::Style;
    case CachedResource::Type::Script:
        return FetchOptions::Destination::Script;
    case CachedResource::Type::FontResource:
    case CachedResource::Type::SVGFontResource:
        return FetchOptions::Destination::Font;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return FetchOptions::Destination::Xslt;
#endif
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
        return FetchOptions::Destination::Track;
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return FetchOptions::Destination::Manifest;
#endif
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::MediaResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
        // The caller is responsible for setting the appropriate destination.
        return FetchOptions::Destination::EmptyString;
    }
    ASSERT_NOT_REACHED();
    return FetchOptions::Destination::EmptyString;
}

static inline bool isSVGImageCachedResource(const CachedResource* resource)
{
    auto* cachedImage = dynamicDowncast<CachedImage>(resource);
    return cachedImage && is<SVGImage>(cachedImage->image());
}

static inline SVGImage* cachedResourceSVGImage(CachedResource* resource)
{
    auto* cachedImage = dynamicDowncast<CachedImage>(resource);
    return cachedImage ? dynamicDowncast<SVGImage>(cachedImage->image()) : nullptr;
}

static bool computeMayAddToMemoryCache(const CachedResourceRequest& newRequest, const CachedResource* existingResource)
{
    return !existingResource || !existingResource->isPreloaded() || newRequest.options().serviceWorkersMode != ServiceWorkersMode::None || existingResource->options().serviceWorkersMode == ServiceWorkersMode::None;
}

RefPtr<DocumentLoader> CachedResourceLoader::protectedDocumentLoader() const
{
    return m_documentLoader.get();
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::requestResource(CachedResource::Type type, CachedResourceRequest&& request, ForPreload forPreload, ImageLoading imageLoading)
{
    URL url = request.resourceRequest().url();
    if (!frame() || !frame()->page()) {
        CACHEDRESOURCELOADER_RELEASE_LOG("requestResource: failed because no frame or page");
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Invalid loader state"_s });
    }

    Ref frame = *this->frame();
    if (!url.isValid()) {
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: URL is invalid", frame.get());
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "URL is invalid"_s });
    }

    LOG(ResourceLoading, "CachedResourceLoader::requestResource '%.255s', charset '%s', priority=%d, forPreload=%u", url.stringCenterEllipsizedToLength().latin1().data(), request.charset().latin1().data(), request.priority() ? static_cast<int>(request.priority().value()) : -1, forPreload == ForPreload::Yes);

    request.setDestinationIfNotSet(destinationForType(type, frame));

    // Entry point to https://fetch.spec.whatwg.org/#main-fetch.
    std::unique_ptr<ResourceRequest> originalRequest;
    if (CachedResource::shouldUsePingLoad(type) || request.options().destination == FetchOptions::Destination::EmptyString) {
        originalRequest = makeUnique<ResourceRequest>(request.resourceRequest());
        originalRequest->clearHTTPReferrer();
        originalRequest->clearHTTPOrigin();
    }

    prepareFetch(type, request);

    if (request.options().destination == FetchOptions::Destination::Document || request.options().destination == FetchOptions::Destination::Iframe) {
        // FIXME: Identify HSTS cases and avoid adding the header. <https://bugs.webkit.org/show_bug.cgi?id=157885>
        if (!url.protocolIs("https"_s))
            request.resourceRequest().setHTTPHeaderField(HTTPHeaderName::UpgradeInsecureRequests, "1"_s);
    }

    bool isRequestUpgradable { false };
    if (RefPtr document = this->document()) {
        isRequestUpgradable = MixedContentChecker::shouldUpgradeInsecureContent(frame, isUpgradableTypeFromResourceType(type), url, request.options().mode, request.options().destination, request.options().initiator);
        request.upgradeInsecureRequestIfNeeded(*document, isRequestUpgradable ? ContentSecurityPolicy::AlwaysUpgradeRequest::Yes : ContentSecurityPolicy::AlwaysUpgradeRequest::No);
        url = request.resourceRequest().url();
    }

    Ref page = *frame->page();
    URL committedDocumentURL { frame->document() ? frame->document()->url() : URL { } };
    if (RefPtr documentLoader = m_documentLoader.get()) {
        if (shouldPerformHTTPSUpgrade(committedDocumentURL, request.resourceRequest().url(), frame, type, page->protectedSettings()->httpsByDefault(), documentLoader->advancedPrivacyProtections(), documentLoader->httpsByDefaultMode())) {
            auto portsForUpgradingInsecureScheme = page->portsForUpgradingInsecureSchemeForTesting();
            auto upgradePort = (portsForUpgradingInsecureScheme && request.resourceRequest().url().port() == portsForUpgradingInsecureScheme->first) ? std::optional { portsForUpgradingInsecureScheme->second } : std::nullopt;
            request.resourceRequest().upgradeInsecureRequestIfNeeded(ShouldUpgradeLocalhostAndIPAddress::No, upgradePort);
            url = request.resourceRequest().url();
        }
    }

    // We are passing url as well as request, as request url may contain a fragment identifier.
    if (!canRequest(type, url, request.options(), forPreload, isRequestUpgradable ? MixedContentChecker::IsUpgradable::Yes : MixedContentChecker::IsUpgradable::No)) {
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: Not allowed to request resource", frame.get());
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Not allowed to request resource"_s, ResourceError::Type::AccessControl });
    }

    if (!portAllowed(url)) {
        if (forPreload == ForPreload::No)
            FrameLoader::reportBlockedLoadFailed(frame, url);
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("CachedResourceLoader::requestResource URL has a blocked port", frame.get());
        return makeUnexpected(frame->protectedLoader()->blockedError(request.resourceRequest()));
    }

    if (isIPAddressDisallowed(url)) {
        FrameLoader::reportBlockedLoadFailed(frame, url);
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("CachedResourceLoader::requestResource URL has a disallowed address", frame.get());
        return makeUnexpected(frame->protectedLoader()->blockedError(request.resourceRequest()));
    }

    request.updateReferrerPolicy(document() ? document()->referrerPolicy() : ReferrerPolicy::Default);

    if (InspectorInstrumentation::willIntercept(frame.ptr(), request.resourceRequest()))
        request.setCachingPolicy(CachingPolicy::DisallowCaching);

    if (RefPtr documentLoader = m_documentLoader.get()) {
        bool madeHTTPS { request.resourceRequest().wasSchemeOptimisticallyUpgraded() };
#if ENABLE(CONTENT_EXTENSIONS)
        const auto& resourceRequest = request.resourceRequest();
        if (request.options().shouldEnableContentExtensionsCheck == ShouldEnableContentExtensionsCheck::Yes) {
            RegistrableDomain originalDomain { resourceRequest.url() };
            auto results = page->protectedUserContentProvider()->processContentRuleListsForLoad(page, resourceRequest.url(), ContentExtensions::toResourceType(type, request.resourceRequest().requester()), *documentLoader);
            bool blockedLoad = results.summary.blockedLoad;
            madeHTTPS = results.summary.madeHTTPS;
            request.applyResults(WTFMove(results), page.ptr());
            if (blockedLoad) {
                CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: Resource blocked by content blocker", frame.get());
                if (type == CachedResource::Type::MainResource) {
                    auto resource = createResource(type, WTFMove(request), page->sessionID(), page->protectedCookieJar().ptr(), page->protectedSettings(), protectedDocument().get());
                    ASSERT(resource);
                    resource->error(CachedResource::Status::LoadError);
                    resource->setResourceError(ResourceError(ContentExtensions::WebKitContentBlockerDomain, 0, resourceRequest.url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description")));
                    return resource;
                }
                if (RefPtr document = this->document()) {
                    if (auto message = ContentExtensions::customTrackerBlockingMessageForConsole(results, resourceRequest.url()))
                        document->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, WTFMove(*message));
                }
                return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Resource blocked by content blocker"_s, ResourceError::Type::AccessControl });
            }
            if (type == CachedResource::Type::MainResource && RegistrableDomain { resourceRequest.url() } != originalDomain) {
                frame->loader().load(FrameLoadRequest { frame, { resourceRequest.url() } });
                return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Loading in a new process"_s, ResourceError::Type::Cancellation });
            }
        }
#endif
        // FIXME: Propagate a better error code
        bool isHTTPSOnlyActive = (m_documentLoader->httpsByDefaultMode() == HTTPSByDefaultMode::UpgradeWithUserMediatedFallback || m_documentLoader->httpsByDefaultMode() == HTTPSByDefaultMode::UpgradeAndNoFallback)
            && !m_documentLoader->advancedPrivacyProtections().contains(AdvancedPrivacyProtections::HTTPSOnlyExplicitlyBypassedForDomain)
            && !frame->loader().isHTTPFallbackInProgress()
            && !(committedDocumentURL.protocolIs("http"_s) && request.resourceRequest().isSameSite());
        if (!madeHTTPS && !LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(request.resourceRequest().url().protocol()) && type == CachedResource::Type::MainResource && isHTTPSOnlyActive)
            return makeUnexpected(protectedDocumentLoader()->protectedFrameLoader()->client().httpNavigationWithHTTPSOnlyError(request.resourceRequest()));

        if (madeHTTPS
            && type == CachedResource::Type::MainResource
            && m_documentLoader->isLoadingMainResource()) {
            // This is to make sure the correct 'new' URL shows in the location bar.
            protectedDocumentLoader()->protectedFrameLoader()->client().dispatchDidChangeProvisionalURL();
        }
        url = request.resourceRequest().url(); // The content extension could have changed it from http to https.
        url = MemoryCache::removeFragmentIdentifierIfNeeded(url); // Might need to remove fragment identifier again.

        if (!m_documentLoader->customHeaderFields().isEmpty()) {
            bool sameOriginRequest = false;
            Ref requestedOrigin = SecurityOrigin::create(url);
            if (type == CachedResource::Type::MainResource) {
                RefPtr localMainFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
                if (frame->isMainFrame())
                    sameOriginRequest = true;
                else if (RefPtr topDocument = localMainFrame ? localMainFrame->document() : nullptr)
                    sameOriginRequest = topDocument->protectedSecurityOrigin()->isSameSchemeHostPort(requestedOrigin.get());
            } else if (RefPtr document = this->document()) {
                sameOriginRequest = document->protectedTopOrigin()->isSameSchemeHostPort(requestedOrigin.get())
                    && document->protectedSecurityOrigin()->isSameSchemeHostPort(requestedOrigin.get());
            }
            for (auto& fields : m_documentLoader->customHeaderFields()) {
                if (sameOriginRequest || fields.thirdPartyDomainsMatch(url)) {
                    for (auto& field : fields.fields)
                        request.resourceRequest().setHTTPHeaderField(field.name(), field.value());
                }
            }
        }
    }

    ResourceLoadTiming loadTiming;
    loadTiming.markStartTime();
    InitiatorContext initiatorContext = request.options().initiatorContext;

    if (request.resourceRequest().url().protocolIsInHTTPFamily())
        updateHTTPRequestHeaders(frame->protectedLoader().get(), type, request);
    request.disableCachingIfNeeded();

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching() && memoryCache.disabled())
        m_documentResources.remove(url.string());

    // See if we can use an existing resource from the cache.
    CachedResourceHandle<CachedResource> resource;
    if (RefPtr document = this->document())
        request.setDomainForCachePartition(*document);

    if (request.allowsCaching())
        resource = memoryCache.resourceForRequest(request.resourceRequest(), page->sessionID());

    if (resource && request.isLinkPreload() && !resource->isLinkPreload())
        resource->setLinkPreload();

    Ref cookieJar = page->cookieJar();

    FrameLoader::addSameSiteInfoToRequestIfNeeded(request.resourceRequest(), document(), frame->page());

    auto mayAddToMemoryCache = computeMayAddToMemoryCache(request, resource.get()) ? MayAddToMemoryCache::Yes : MayAddToMemoryCache::No;
    RevalidationPolicy policy = determineRevalidationPolicy(type, request, resource.get(), forPreload, imageLoading);
    switch (policy) {
    case Reload:
        if (mayAddToMemoryCache == MayAddToMemoryCache::Yes)
            memoryCache.remove(*resource);
        FALLTHROUGH;
    case Load:
        if (resource && mayAddToMemoryCache == MayAddToMemoryCache::Yes)
            memoryCache.remove(*resource);
        resource = loadResource(type, page->sessionID(), WTFMove(request), cookieJar, page->protectedSettings(), mayAddToMemoryCache);
        break;
    case Revalidate:
        resource = revalidateResource(WTFMove(request), *resource);
        break;
    case Use:
        ASSERT(resource);
        if (request.options().mode == FetchOptions::Mode::Navigate && !frame->isMainFrame()) {
            RefPtr localParentFrame = dynamicDowncast<LocalFrame>(frame->tree().parent());
            if (RefPtr parentDocument = localParentFrame ? localParentFrame->document() : nullptr) {
                auto coep = parentDocument->crossOriginEmbedderPolicy().value;
                if (auto error = validateCrossOriginResourcePolicy(coep, parentDocument->protectedSecurityOrigin(), request.resourceRequest().url(), resource->response(), ForNavigation::Yes, OriginAccessPatternsForWebProcess::singleton()))
                    return makeUnexpected(WTFMove(*error));
            }
        }
        // Per the Fetch specification, the "cross-origin resource policy check" should only occur in the HTTP Fetch case (https://fetch.spec.whatwg.org/#concept-http-fetch).
        // However, per https://fetch.spec.whatwg.org/#main-fetch, if the request URL's protocol is "data:", then we should perform a scheme fetch which would end up
        // returning a response WITHOUT performing an HTTP fetch (and thus no CORP check).
        if (request.options().mode == FetchOptions::Mode::NoCors && !url.protocolIsData()) {
            auto coep = document() ? document()->crossOriginEmbedderPolicy().value : CrossOriginEmbedderPolicyValue::UnsafeNone;
            if (auto error = validateCrossOriginResourcePolicy(coep, *request.protectedOrigin(), request.resourceRequest().url(), resource->response(), ForNavigation::No, OriginAccessPatternsForWebProcess::singleton()))
                return makeUnexpected(WTFMove(*error));
        }
        if (request.options().mode == FetchOptions::Mode::NoCors) {
            if (auto error = validateRangeRequestedFlag(request.resourceRequest(), resource->response()))
                return makeUnexpected(WTFMove(*error));
        }
        if (shouldUpdateCachedResourceWithCurrentRequest(*resource, request)) {
            resource = updateCachedResourceWithCurrentRequest(*resource, WTFMove(request), page->sessionID(), cookieJar, page->protectedSettings());
            if (resource->status() != CachedResource::Status::Cached)
                policy = Load;
        } else {
            ResourceError error;
            if (!shouldContinueAfterNotifyingLoadedFromMemoryCache(request, *resource, error))
                return makeUnexpected(WTFMove(error));
            loadTiming.markEndTime();

            memoryCache.resourceAccessed(*resource);

            if (document() && !resource->isLoading()) {
                Box<NetworkLoadMetrics> metrics;
                RefPtr documentLoader = document()->loader();
                // Use the actual network load metrics if this is the first time loading this resource and it was loaded
                // for this document because it may have been speculatively preloaded.
                if (auto metricsFromResource = resource->takeNetworkLoadMetrics(); metricsFromResource && documentLoader && metricsFromResource->redirectStart >= documentLoader->timing().timeOrigin())
                    metrics = WTFMove(metricsFromResource);
                auto resourceTiming = ResourceTiming::fromMemoryCache(url, request.initiatorType(), loadTiming, resource->response(), metrics ? *metrics : NetworkLoadMetrics::emptyMetrics(), *request.protectedOrigin());
                if (initiatorContext == InitiatorContext::Worker)
                    downcast<CachedRawResource>(resource.get())->finishedTimingForWorkerLoad(WTFMove(resourceTiming));
                else {
                    ASSERT(initiatorContext == InitiatorContext::Document);
                    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, request.initiatorType(), frame.ptr());
                    m_resourceTimingInfo.addResourceTiming(*resource.get(), *protectedDocument(), WTFMove(resourceTiming));
                }
            }

            if (forPreload == ForPreload::No)
                resource->setLoadPriority(request.priority(), request.fetchPriorityHint());
        }
        break;
    }
    ASSERT(resource);
    resource->setOriginalRequest(WTFMove(originalRequest));

    if (RefPtr subresourceLoader = resource->loader(); forPreload == ForPreload::No && subresourceLoader && resource->ignoreForRequestCount()) {
        subresourceLoader->clearRequestCountTracker();
        resource->setIgnoreForRequestCount(false);
        subresourceLoader->resetRequestCountTracker(*this, *resource);
    }

    if ((policy != Use || resource->stillNeedsLoad()) && imageLoading == ImageLoading::Immediate) {
        resource->load(*this);

        // We don't support immediate loads, but we do support immediate failure.
        if (resource->errorOccurred()) {
            if (resource->allowsCaching() && resource->inCache())
                memoryCache.remove(*resource);

            auto resourceError = resource->resourceError();
            // Synchronous cancellations are likely due to access control.
            if (resourceError.isNull() || resourceError.isCancellation())
                return makeUnexpected(ResourceError { String(), 0, url, String(), ResourceError::Type::AccessControl });
            return makeUnexpected(resourceError);
        }
    }

    if (document() && !document()->loadEventFinished() && !resource->resourceRequest().url().protocolIsData())
        m_validatedURLs.add(resource->resourceRequest().url());

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url().string(), resource);
    frame->protectedLoader()->client().didLoadFromRegistrableDomain(RegistrableDomain(resource->resourceRequest().url()));
    return resource;
}

void CachedResourceLoader::documentDidFinishLoadEvent()
{
    m_validatedURLs.clear();

    // If m_preloads is not empty here, it's full of link preloads,
    // as speculative preloads were cleared at DCL.
    if (m_preloads && !m_preloads->isEmptyIgnoringNullReferences() && !m_unusedPreloadsTimer.isActive())
        m_unusedPreloadsTimer.startOneShot(unusedPreloadTimeout);
}

void CachedResourceLoader::stopUnusedPreloadsTimer()
{
    m_unusedPreloadsTimer.stop();
}

CachedResourceHandle<CachedResource> CachedResourceLoader::revalidateResource(CachedResourceRequest&& request, CachedResource& resource)
{
    ASSERT(resource.inCache());
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!memoryCache.disabled());
    ASSERT(resource.canUseCacheValidator());
    ASSERT(!resource.resourceToRevalidate());
    ASSERT(resource.allowsCaching());

    CachedResourceHandle newResource = createResource(WTFMove(request), resource, protectedDocument().get());

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), &resource);
    newResource->setResourceToRevalidate(&resource);

    memoryCache.remove(resource);
    memoryCache.add(*newResource);

    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(newResource, newResource->initiatorType(), protectedFrame().get());

    return newResource;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::loadResource(CachedResource::Type type, PAL::SessionID sessionID, CachedResourceRequest&& request, const CookieJar& cookieJar, const Settings& settings, MayAddToMemoryCache mayAddToMemoryCache)
{
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!request.allowsCaching() || mayAddToMemoryCache == MayAddToMemoryCache::No || !memoryCache.resourceForRequest(request.resourceRequest(), sessionID)
        || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData);

    LOG(ResourceLoading, "Loading CachedResource for '%s'.", request.resourceRequest().url().stringCenterEllipsizedToLength().latin1().data());

    CachedResourceHandle resource = createResource(type, WTFMove(request), sessionID, &cookieJar, settings, protectedDocument().get());

    if (resource->allowsCaching() && mayAddToMemoryCache == MayAddToMemoryCache::Yes)
        memoryCache.add(*resource);

    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, resource->initiatorType(), protectedFrame().get());

    return resource;
}

static inline bool mustReloadFromServiceWorkerOptions(const ResourceLoaderOptions& options, const ResourceLoaderOptions& cachedOptions)
{
    // FIXME: We should validate/specify this behavior.
    if (options.serviceWorkerRegistrationIdentifier != cachedOptions.serviceWorkerRegistrationIdentifier)
        return true;

    if (options.serviceWorkersMode == cachedOptions.serviceWorkersMode)
        return false;

    return cachedOptions.mode == FetchOptions::Mode::Navigate || cachedOptions.destination == FetchOptions::Destination::Worker || cachedOptions.destination == FetchOptions::Destination::Sharedworker;
}

CachedResourceLoader::RevalidationPolicy CachedResourceLoader::determineRevalidationPolicy(CachedResource::Type type, CachedResourceRequest& cachedResourceRequest, CachedResource* existingResource, ForPreload forPreload, ImageLoading imageLoading) const
{
    auto& request = cachedResourceRequest.resourceRequest();

    if (!existingResource)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData)
        return Reload;

    if (mustReloadFromServiceWorkerOptions(cachedResourceRequest.options(), existingResource->options())) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading because selected service worker may differ");
        return Reload;
    }

    // We already have a preload going for this URL.
    if (forPreload == ForPreload::Yes && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to type mismatch.");
        return Reload;
    }

    if (!existingResource->varyHeaderValuesMatch(request))
        return Reload;

    RefPtr textDecoder = existingResource->textResourceDecoder();
    if (textDecoder && !textDecoder->hasEqualEncodingForCharset(cachedResourceRequest.charset())) {
        if (!existingResource->hasUnknownEncoding())
            return Reload;
        existingResource->setHasUnknownEncoding(false);
        existingResource->setEncoding(cachedResourceRequest.charset());
    }

    // FIXME: We should use the same cache policy for all resource types. The raw resource policy is overly strict
    //        while the normal subresource policy is too loose.
    if (existingResource->isMainOrMediaOrIconOrRawResource() && frame()) {
        bool strictPolicyDisabled = frame()->loader().isStrictRawResourceValidationPolicyDisabledForTesting();
        bool canReuseRawResource = strictPolicyDisabled || downcast<CachedRawResource>(*existingResource).canReuse(request);
        if (!canReuseRawResource)
            return Reload;
    }

    // Conditional requests should have failed canReuse check.
    ASSERT(!request.isConditional());

    // Do not load from cache if images are not enabled. The load for this image will be blocked in CachedImage::load.
    if (imageLoading == ImageLoading::DeferredUntilVisible)
        return Reload;

    // Don't reload resources while pasting or if cache mode allows stale resources.
    if (m_allowStaleResources || cachedResourceRequest.options().cache == FetchOptions::Cache::ForceCache || cachedResourceRequest.options().cache == FetchOptions::Cache::OnlyIfCached)
        return Use;

    ASSERT(cachedResourceRequest.options().cache == FetchOptions::Cache::Default || cachedResourceRequest.options().cache == FetchOptions::Cache::NoCache);

    // Always use preloads.
    if (existingResource->isPreloaded())
        return Use;

    // We can find resources that are being validated from cache only when validation is just successfully completing.
    if (existingResource->validationCompleting())
        return Use;
    ASSERT(!existingResource->validationInProgress());

    if (CachedResourceHandle cachedImage = dynamicDowncast<CachedImage>(*existingResource); cachedImage && cachedImage->canSkipRevalidation(*this, cachedResourceRequest))
        return Use;

    auto cachePolicy = this->cachePolicy(type, request.url());

    // Validate the redirect chain.
    bool cachePolicyIsHistoryBuffer = cachePolicy == CachePolicy::HistoryBuffer;
    if (!existingResource->redirectChainAllowsReuse(cachePolicyIsHistoryBuffer ? ReuseExpiredRedirection : DoNotReuseExpiredRedirection)) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to not cached or expired redirections.");
        return Reload;
    }

    // CachePolicy::HistoryBuffer uses the cache except if this is a main resource with "cache-control: no-store".
    if (cachePolicyIsHistoryBuffer) {
        // FIXME: Ignoring "cache-control: no-cache" for sub-resources on history navigation but not the main
        // resource is inconsistent. We should probably harmonize this.
        if (!existingResource->response().cacheControlContainsNoStore() || type != CachedResource::Type::MainResource)
            return Use;
    }

    // Don't reuse resources with Cache-control: no-store.
    if (existingResource->response().cacheControlContainsNoStore()) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to Cache-control: no-store.");
        return Reload;
    }

    // If credentials were sent with the previous request and won't be
    // with this one, or vice versa, re-fetch the resource.
    //
    // This helps with the case where the server sends back
    // "Access-Control-Allow-Origin: *" all the time, but some of the
    // client's requests are made without CORS and some with.
    if (existingResource->resourceRequest().allowCookies() != request.allowCookies() || existingResource->options().credentials != cachedResourceRequest.options().credentials) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to difference in credentials settings.");
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document, even if the cache policies would tell us to.
    if (document() && !document()->loadEventFinished() && m_validatedURLs.contains(existingResource->url()))
        return Use;

    // CachePolicy::Reload always reloads
    if (cachePolicy == CachePolicy::Reload) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to CachePolicyReload.");
        return Reload;
    }
    
    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicye reloading due to resource being in the error state");
        return Reload;
    }

    if (existingResource->isLoading()) {
        // Do not use cached main resources that are still loading because sharing
        // loading CachedResources in this case causes issues with regards to cancellation.
        // If one of the DocumentLoader clients decides to cancel the load, then the load
        // would be cancelled for all other DocumentLoaders as well.
        if (type == CachedResource::Type::MainResource)
            return Reload;
        // For cached subresources that are still loading we ignore the cache policy.
        return Use;
    }

    auto revalidationDecision = existingResource->makeRevalidationDecision(cachePolicy);

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (revalidationDecision != CachedResource::RevalidationDecision::No) {
        // See if the resource has usable ETag or Last-modified headers.
        if (existingResource->canUseCacheValidator()) {
            // Revalidating will mean exposing headers to the service worker, let's reload given the service worker already handled it.
            if (cachedResourceRequest.options().serviceWorkerRegistrationIdentifier)
                return Reload;
            return Revalidate;
        }
        
        // No, must reload.
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to missing cache validators.");
        return Reload;
    }

    // FIXME: We should be able to USE the data here, but cannot currently due to a bug
    // concerning redirect and URL fragments. https://bugs.webkit.org/show_bug.cgi?id=258934
    if (cachedResourceRequest.hasFragmentIdentifier() && existingResource->hasRedirections())
        return Load;

    return Use;
}

void CachedResourceLoader::printAccessDeniedMessage(const URL& url) const
{
    if (url.isNull())
        return;

    RefPtr frame = this->frame();
    if (!frame)
        return;

    String message;
    if (!m_document || m_document->url().isNull())
        message = makeString("Unsafe attempt to load URL "_s, url.stringCenterEllipsizedToLength(), '.');
    else
        message = makeString("Unsafe attempt to load URL "_s, url.stringCenterEllipsizedToLength(), " from origin "_s, m_document->protectedSecurityOrigin()->toString(), ". Domains, protocols and ports must match.\n"_s);

    if (RefPtr frameDocument = frame->document())
        frameDocument->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
}

void CachedResourceLoader::setAutoLoadImages(bool enable)
{
    if (enable == m_autoLoadImages)
        return;

    m_autoLoadImages = enable;

    if (!m_autoLoadImages)
        return;

    reloadImagesIfNotDeferred();
}

void CachedResourceLoader::setImagesEnabled(bool enable)
{
    if (enable == m_imagesEnabled)
        return;

    m_imagesEnabled = enable;

    if (!m_imagesEnabled)
        return;

    reloadImagesIfNotDeferred();
}

ImageLoading CachedResourceLoader::clientDefersImage(const URL&) const
{
    return m_imagesEnabled ? ImageLoading::Immediate : ImageLoading::DeferredUntilVisible;
}

bool CachedResourceLoader::shouldPerformImageLoad(const URL& url) const
{
    return m_autoLoadImages || url.protocolIsData();
}

bool CachedResourceLoader::shouldDeferImageLoad(const URL& url) const
{
    return clientDefersImage(url) == ImageLoading::DeferredUntilVisible || !shouldPerformImageLoad(url);
}

void CachedResourceLoader::reloadImagesIfNotDeferred()
{
    for (auto& resource : m_documentResources.values()) {
        CachedResourceHandle image = dynamicDowncast<CachedImage>(*resource);
        if (image && resource->stillNeedsLoad() && clientDefersImage(resource->url()) == ImageLoading::Immediate)
            image->load(*this);
    }
}

CachePolicy CachedResourceLoader::cachePolicy(CachedResource::Type type, const URL& url) const
{
    RefPtr frame = this->frame();
    if (!frame)
        return CachePolicy::Verify;

    if (type != CachedResource::Type::MainResource)
        return frame->protectedLoader()->subresourceCachePolicy(url);

    if (RefPtr page = frame->page()) {
        if (page->isResourceCachingDisabledByWebInspector())
            return CachePolicy::Reload;
    }

    switch (frame->loader().loadType()) {
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::Reload:
        return CachePolicy::Reload;
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
        // Do not revalidate cached main resource on back/forward navigation.
        return CachePolicy::HistoryBuffer;
    default:
        return CachePolicy::Verify;
    }
}

void CachedResourceLoader::clearDocumentLoader()
{
    m_documentLoader = nullptr;
}

void CachedResourceLoader::loadDone(LoadCompletionType type, bool shouldPerformPostLoadActions)
{
    RefPtr protectedDocumentLoader { m_documentLoader.get() };
    RefPtr protectedDocument { m_document.get() };

    ASSERT(shouldPerformPostLoadActions || type == LoadCompletionType::Cancel);

    if (RefPtr frame = this->frame())
        frame->protectedLoader()->loadDone(type);

    if (shouldPerformPostLoadActions)
        performPostLoadActions();

    if (!m_garbageCollectDocumentResourcesTimer.isActive())
        m_garbageCollectDocumentResourcesTimer.startOneShot(0_s);
}

// Garbage collecting m_documentResources is a workaround for the
// CachedResourceHandles on the RHS being strong references. Ideally this
// would be a weak map, however CachedResourceHandles perform additional
// bookkeeping on CachedResources, so instead pseudo-GC them -- when the
// reference count reaches 1, m_documentResources is the only reference, so
// remove it from the map.
void CachedResourceLoader::garbageCollectDocumentResources()
{
    typedef Vector<String, 10> StringVector;
    StringVector resourcesToDelete;

    for (auto& resourceEntry : m_documentResources) {
        auto& resource = *resourceEntry.value;

        if (resource.hasOneHandle() && !resource.loader() && !resource.isPreloaded()) {
            resourcesToDelete.append(resourceEntry.key);
            m_resourceTimingInfo.removeResourceTiming(resource);
        }
    }

    LOG_WITH_STREAM(ResourceLoading, stream << "CachedResourceLoader " << this << " garbageCollectDocumentResources - deleting " << resourcesToDelete.size() << " resources");

    for (auto& resource : resourcesToDelete)
        m_documentResources.remove(resource);
}

void CachedResourceLoader::performPostLoadActions()
{
    platformStrategies()->loaderStrategy()->servePendingRequests();
}

void CachedResourceLoader::incrementRequestCount(const CachedResource& resource)
{
    if (resource.ignoreForRequestCount())
        return;

    ++m_requestCount;
}

void CachedResourceLoader::decrementRequestCount(const CachedResource& resource)
{
    if (resource.ignoreForRequestCount())
        return;

    --m_requestCount;
    ASSERT(m_requestCount > -1);
}

void CachedResourceLoader::notifyFinished(const CachedResource& resource)
{
    if (isSVGImageCachedResource(&resource))
        m_cachedSVGImagesURLs.add(resource.url());
}

Vector<Ref<SVGImage>> CachedResourceLoader::allCachedSVGImages() const
{
    Vector<Ref<SVGImage>> allCachedSVGImages;

    for (auto& cachedSVGImageURL : m_cachedSVGImagesURLs) {
        CachedResourceHandle resource = cachedResource(cachedSVGImageURL);
        if (RefPtr image = cachedResourceSVGImage(resource.get()))
            allCachedSVGImages.append(image.releaseNonNull());
    }
        
    return allCachedSVGImages;
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::preload(CachedResource::Type type, CachedResourceRequest&& request)
{
    if (InspectorInstrumentation::willIntercept(protectedFrame().get(), request.resourceRequest()))
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, request.resourceRequest().url(), "Inspector intercept"_s });

    ASSERT(m_document);
    if (request.charset().isEmpty() && m_document && (type == CachedResource::Type::Script || type == CachedResource::Type::CSSStyleSheet))
        request.setCharset(m_document->charset());

    auto resource = requestResource(type, WTFMove(request), ForPreload::Yes);
    if (resource && (!m_preloads || !m_preloads->contains(*resource.value().get()))) {
        CachedResourceHandle resourceValue = resource.value();
        // Fonts need special treatment since just creating the resource doesn't trigger a load.
        if (CachedResourceHandle cachedFont = dynamicDowncast<CachedFont>(resourceValue.get()))
            cachedFont->beginLoadIfNeeded(*this);
        resourceValue->increasePreloadCount();

        if (!m_preloads)
            m_preloads = makeUnique<WeakListHashSet<CachedResource>>();
        m_preloads->add(*resourceValue.get());
    }
    return resource;
}

void CachedResourceLoader::warnUnusedPreloads()
{
    if (!m_preloads)
        return;

    RefPtr document = this->document();
    if (!document)
        return;

    for (const auto& resource : *m_preloads) {
        if (resource.isLinkPreload() && resource.preloadResult() == CachedResource::PreloadResult::PreloadNotReferenced) {
            document->addConsoleMessage(MessageSource::Other, MessageLevel::Warning,
                makeString("The resource "_s, resource.url().string(),
                " was preloaded using link preload but not used within a few seconds from the window's load event. Please make sure it wasn't preloaded for nothing."_s));
        }
    }
}

bool CachedResourceLoader::isPreloaded(const String& urlString) const
{
    RefPtr document = m_document.get();
    ASSERT(document);
    if (!document)
        return false;

    const URL& url = document->completeURL(urlString);

    if (!m_preloads)
        return false;

    for (auto& resource : *m_preloads) {
        if (resource.url() == url)
            return true;
    }
    return false;
}

void CachedResourceLoader::clearPreloads(ClearPreloadsMode mode)
{
    if (!m_preloads)
        return;

    std::unique_ptr<WeakListHashSet<CachedResource>> remainingLinkPreloads;
    for (auto& resource : *m_preloads) {
        if (mode == ClearPreloadsMode::ClearSpeculativePreloads && resource.isLinkPreload()) {
            if (!remainingLinkPreloads)
                remainingLinkPreloads = makeUnique<WeakListHashSet<CachedResource>>();
            remainingLinkPreloads->add(resource);
            continue;
        }
        resource.decreasePreloadCount();
        bool deleted = resource.deleteIfPossible();
        if (!deleted && resource.preloadResult() == CachedResource::PreloadResult::PreloadNotReferenced)
            MemoryCache::singleton().remove(resource);
    }
    m_preloads = WTFMove(remainingLinkPreloads);
}

Vector<CachedResourceHandle<CachedResource>> CachedResourceLoader::visibleResourcesToPrioritize()
{
    if (!document())
        return { };

    Vector<CachedResourceHandle<CachedResource>> toPrioritize;

    for (auto& resource : m_documentResources.values()) {
        CachedResourceHandle cachedImage = dynamicDowncast<CachedImage>(*resource);
        if (!cachedImage)
            continue;
        if (!cachedImage->isLoading())
            continue;
        if (!cachedImage->url().protocolIsInHTTPFamily())
            continue;
        if (!cachedImage->loader())
            continue;
        if (!cachedImage->isVisibleInViewport(*protectedDocument()))
            continue;
        toPrioritize.append(WTFMove(cachedImage));
    }

    return toPrioritize;
}

const ResourceLoaderOptions& CachedResourceLoader::defaultCachedResourceOptions()
{
    static NeverDestroyed<ResourceLoaderOptions> options(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::SniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::Use,
        ClientCredentialPolicy::MayAskClientForCredentials,
        FetchOptions::Credentials::Include,
        SecurityCheckPolicy::DoSecurityCheck,
        FetchOptions::Mode::NoCors,
        CertificateInfoPolicy::DoNotIncludeCertificateInfo,
        ContentSecurityPolicyImposition::DoPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::AllowCaching);
    return options;
}

}

#undef PAGE_ID
#undef FRAME_ID
#undef CACHEDRESOURCELOADER_RELEASE_LOG
#undef CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME
