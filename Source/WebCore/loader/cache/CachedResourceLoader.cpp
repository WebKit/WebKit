/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "CrossOriginAccessControl.h"
#include "CustomHeaderFields.h"
#include "DOMWindow.h"
#include "DateComponents.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTTPHeaderField.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "MixedContentChecker.h"
#include "Page.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include "RenderElement.h"
#include "ResourceLoadInfo.h"
#include "ResourceTiming.h"
#include "RuntimeApplicationChecks.h"
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
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "CachedApplicationManifest.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "Device.h"
#endif

#undef CACHEDRESOURCELOADER_RELEASE_LOG
#define PAGE_ID(frame) (valueOrDefault(frame.pageID()).toUInt64())
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

static CachedResourceHandle<CachedResource> createResource(CachedResource::Type type, CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, const Settings& settings)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return new CachedImage(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::Script:
        return new CachedScript(WTFMove(request), sessionID, cookieJar);
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

static CachedResourceHandle<CachedResource> createResource(CachedResourceRequest&& request, CachedResource& resource)
{
    switch (resource.type()) {
    case CachedResource::Type::ImageResource:
        return new CachedImage(WTFMove(request), resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), resource.sessionID(), resource.cookieJar());
    case CachedResource::Type::Script:
        return new CachedScript(WTFMove(request), resource.sessionID(), resource.cookieJar());
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
    return cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(m_document->completeURL(resourceURL)));
}

CachedResource* CachedResourceLoader::cachedResource(const URL& url) const
{
    ASSERT(!MemoryCache::shouldRemoveFragmentIdentifier(url));
    return m_documentResources.get(url.string()).get();
}

Frame* CachedResourceLoader::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : nullptr;
}

ResourceErrorOr<CachedResourceHandle<CachedImage>> CachedResourceLoader::requestImage(CachedResourceRequest&& request, ImageLoading imageLoading)
{
    if (Frame* frame = this->frame()) {
        if (frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None) {
            if (Document* document = frame->document())
                request.upgradeInsecureRequestIfNeeded(*document);
            URL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(CachedResource::Type::ImageResource, requestURL, request.options(), ForPreload::No))
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
    request.setDomainForCachePartition(*document());

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching()) {
        if (CachedResource* existing = memoryCache.resourceForRequest(request.resourceRequest(), page.sessionID())) {
            if (is<CachedCSSStyleSheet>(*existing))
                return downcast<CachedCSSStyleSheet>(existing);
            memoryCache.remove(*existing);
        }
    }

    request.removeFragmentIdentifierIfNeeded();

    CachedResourceHandle<CachedCSSStyleSheet> userSheet = new CachedCSSStyleSheet(WTFMove(request), page.sessionID(), &page.cookieJar());

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

bool CachedResourceLoader::checkInsecureContent(CachedResource::Type type, const URL& url) const
{
    if (!canRequestInContentDispositionAttachmentSandbox(type, url))
        return false;

    switch (type) {
    case CachedResource::Type::Script:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::CSSStyleSheet:
        // These resource can inject script into the current document (Script,
        // XSL) or exfiltrate the content of the current document (CSS).
        if (Frame* frame = this->frame()) {
            if (!MixedContentChecker::canRunInsecureContent(*frame, m_document->securityOrigin(), url))
                return false;
            auto& top = frame->tree().top();
            auto* localTop = dynamicDowncast<LocalFrame>(top);
            if (&top != frame && localTop && !MixedContentChecker::canRunInsecureContent(*localTop, localTop->document()->securityOrigin(), url))
                return false;
        }
        break;
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
#endif
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
#endif
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource: {
        // These resources can corrupt only the frame's pixels.
        if (Frame* frame = this->frame()) {
            if (!MixedContentChecker::canDisplayInsecureContent(*frame, m_document->securityOrigin(), contentTypeFromResourceType(type), url, MixedContentChecker::AlwaysDisplayInNonStrictMode::Yes))
                return false;
            auto& topFrame = frame->tree().top();
            auto* localTopFrame = dynamicDowncast<LocalFrame>(topFrame);
            if (!localTopFrame || !MixedContentChecker::canDisplayInsecureContent(*localTopFrame, localTopFrame->document()->securityOrigin(), contentTypeFromResourceType(type), url))
                return false;
        }
        break;
    }
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
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

    ASSERT(m_document);
    ASSERT(m_document->contentSecurityPolicy());

    // All content loaded through embed or object elements goes through object-src: https://www.w3.org/TR/CSP3/#directive-object-src.
    if (options.loadedFromPluginElement == LoadedFromPluginElement::Yes
        && !m_document->contentSecurityPolicy()->allowObjectFromSource(url, redirectResponseReceived, preRedirectURL))
        return false;

    switch (type) {
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::Script:
        if (!m_document->contentSecurityPolicy()->allowScriptFromSource(url, redirectResponseReceived, preRedirectURL, options.integrity, options.nonce))
            return false;
        break;
    case CachedResource::Type::CSSStyleSheet:
        if (!m_document->contentSecurityPolicy()->allowStyleFromSource(url, redirectResponseReceived, preRedirectURL, options.nonce))
            return false;
        break;
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
        if (!m_document->contentSecurityPolicy()->allowImageFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::LinkPrefetch:
        if (!m_document->contentSecurityPolicy()->allowPrefetchFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
        if (!m_document->contentSecurityPolicy()->allowFontFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::MediaResource:
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
#endif
        if (!m_document->contentSecurityPolicy()->allowMediaFromSource(url, redirectResponseReceived, preRedirectURL))
            return false;
        break;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::RawResource:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
#endif
        return true;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        if (!m_document->contentSecurityPolicy()->allowManifestFromSource(url, redirectResponseReceived, preRedirectURL))
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

// Security checks defined in https://fetch.spec.whatwg.org/#main-fetch step 2 and 4.
bool CachedResourceLoader::canRequest(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, ForPreload forPreload)
{
    if (document() && !document()->securityOrigin().canDisplay(url)) {
        if (forPreload == ForPreload::No)
            FrameLoader::reportLocalLoadFailed(frame(), url.stringCenterEllipsizedToLength());
        LOG(ResourceLoading, "CachedResourceLoader::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return false;
    }

    if (options.mode == FetchOptions::Mode::SameOrigin && !m_document->securityOrigin().canRequest(url) && !isSameOriginDataURL(url, options)) {
        printAccessDeniedMessage(url);
        return false;
    }

    if (options.mode == FetchOptions::Mode::NoCors && !m_document->securityOrigin().canRequest(url) && options.redirect != FetchOptions::Redirect::Follow && type != CachedResource::Type::Ping) {
        ASSERT(type != CachedResource::Type::MainResource);
        frame()->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "No-Cors mode requires follow redirect mode"_s);
        return false;
    }

    if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::No))
        return false;

    // SVG Images have unique security rules that prevent all subresource requests except for data urls.
    if (type != CachedResource::Type::MainResource && frame() && frame()->page()) {
        if (frame()->page()->chrome().client().isSVGImageChromeClient() && !url.protocolIsData())
            return false;
    }

    // Last of all, check for insecure content. We do this last so that when folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.

    // FIXME: Should we consider whether the request is for preload here?
    if (!checkInsecureContent(type, url))
        return false;

    return true;
}

// FIXME: Should we find a way to know whether the redirection is for a preload request like we do for CachedResourceLoader::canRequest?
bool CachedResourceLoader::canRequestAfterRedirection(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, const URL& preRedirectURL) const
{
    if (document() && !document()->securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame(), url.stringCenterEllipsizedToLength());
        LOG(ResourceLoading, "CachedResourceLoader::canRequestAfterRedirection URL was not allowed by SecurityOrigin::canDisplay");
        CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by SecurityOrigin::canDisplay");
        return false;
    }

    // FIXME: According to https://fetch.spec.whatwg.org/#http-redirect-fetch, we should check that the URL is HTTP(s) except if in navigation mode.
    // But we currently allow at least data URLs to be loaded.

    if (options.mode == FetchOptions::Mode::SameOrigin && !m_document->securityOrigin().canRequest(url)) {
        CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by SecurityOrigin::canRequest");
        printAccessDeniedMessage(url);
        return false;
    }

    if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::Yes, preRedirectURL)) {
        CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed by content policy");
        return false;
    }

    // Last of all, check for insecure content. We do this last so that when folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.
    if (!checkInsecureContent(type, url)) {
        CACHEDRESOURCELOADER_RELEASE_LOG("canRequestAfterRedirection: URL was not allowed because content is insecure");
        return false;
    }

    return true;
}

// Created by binding generator.
String convertEnumerationToString(FetchOptions::Destination);
String convertEnumerationToString(FetchOptions::Mode);

static const String& convertEnumerationToString(FetchMetadataSite enumerationValue)
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
    auto requestOrigin = SecurityOrigin::create(request.url());
    if (!requestOrigin->isPotentiallyTrustworthy())
        return;

    String destinationString;
    // The Fetch IDL documents this as "" while FetchMetadata expects "empty".
    if (options.destination == FetchOptions::Destination::EmptyString)
        destinationString = "empty"_s;
    else
        destinationString = convertEnumerationToString(options.destination);

    request.setHTTPHeaderField(HTTPHeaderName::SecFetchDest, WTFMove(destinationString));
    request.setHTTPHeaderField(HTTPHeaderName::SecFetchMode, convertEnumerationToString(options.mode));
    request.setHTTPHeaderField(HTTPHeaderName::SecFetchSite, convertEnumerationToString(site));
}

FetchMetadataSite CachedResourceLoader::computeFetchMetadataSite(const ResourceRequest& request, CachedResource::Type type, FetchOptions::Mode mode, const SecurityOrigin& originalOrigin, FetchMetadataSite originalSite)
{
    // This is true when a user causes a request, such as entering a URL.
    if (mode == FetchOptions::Mode::Navigate && type == CachedResource::Type::MainResource && !request.hasHTTPReferrer())
        return FetchMetadataSite::None;

    // If this is a redirect we start with the old value.
    // The value can never get more "secure" so a full redirect
    // chain only degrades towards cross-site.
    Ref<SecurityOrigin> requestOrigin = SecurityOrigin::create(request.url());
    if ((originalSite == FetchMetadataSite::SameOrigin || originalSite == FetchMetadataSite::None) && originalOrigin.isSameOriginAs(requestOrigin))
        return FetchMetadataSite::SameOrigin;
    if (originalSite != FetchMetadataSite::CrossSite && originalOrigin.isSameSiteAs(requestOrigin))
        return FetchMetadataSite::SameSite;
    return FetchMetadataSite::CrossSite;
}

bool CachedResourceLoader::updateRequestAfterRedirection(CachedResource::Type type, ResourceRequest& request, const ResourceLoaderOptions& options, FetchMetadataSite site, const URL& preRedirectURL)
{
    ASSERT(m_documentLoader);
    if (auto* document = m_documentLoader->cachedResourceLoader().document())
        upgradeInsecureResourceRequestIfNeeded(request, *document);

    // FIXME: We might want to align the checks done here with the ones done in CachedResourceLoader::requestResource, content extensions blocking in particular.

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (m_documentLoader->frame()->settings().fetchMetadataEnabled()) {
        auto requestOrigin = SecurityOrigin::create(request.url());

        // In the case of a protocol downgrade we strip all FetchMetadata headers.
        // Otherwise we add or update FetchMetadata.
        if (!requestOrigin->isPotentiallyTrustworthy()) {
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchDest);
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchMode);
            request.removeHTTPHeaderField(HTTPHeaderName::SecFetchSite);
        } else
            updateRequestFetchMetadataHeaders(request, options, site);
    }
#endif // ENABLE(PUBLIC_SUFFIX_LIST)

    return canRequestAfterRedirection(type, request.url(), options, preRedirectURL);
}

bool CachedResourceLoader::canRequestInContentDispositionAttachmentSandbox(CachedResource::Type type, const URL& url) const
{
    Document* document;

    // FIXME: Do we want to expand this to all resource types that the mixed content checker would consider active content?
    switch (type) {
    case CachedResource::Type::MainResource:
        if (auto ownerElement = frame() ? frame()->ownerElement() : nullptr) {
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

    if (!document->shouldEnforceContentDispositionAttachmentSandbox() || document->securityOrigin().canRequest(url))
        return true;

    String message = "Unsafe attempt to load URL " + url.stringCenterEllipsizedToLength() + " from document with Content-Disposition: attachment at URL " + document->url().stringCenterEllipsizedToLength() + ".";
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
    frame()->loader().loadedResourceFromMemoryCache(resource, newRequest, error);

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

    if (resource.options().mode != request.options().mode || !serializedOriginsMatch(request.origin(), resource.origin()))
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
        return loadResource(resource.type(), sessionID, WTFMove(request), cookieJar, settings);
    }

    auto resourceHandle = createResource(resource.type(), WTFMove(request), sessionID, &cookieJar, settings);
    resourceHandle->loadFrom(resource);
    return resourceHandle;
}

static inline void logMemoryCacheResourceRequest(Frame* frame, const String& key, const String& description)
{
    if (!frame || !frame->page())
        return;
    frame->page()->diagnosticLoggingClient().logDiagnosticMessage(key, description, ShouldSample::Yes);
}

void CachedResourceLoader::prepareFetch(CachedResource::Type type, CachedResourceRequest& request)
{
    // Implementing step 1.1 to 1.6 of https://fetch.spec.whatwg.org/#concept-fetch.
    if (auto* document = this->document()) {
        if (!request.origin())
            request.setOrigin(document->securityOrigin());
#if ENABLE(SERVICE_WORKER)
        request.setClientIdentifierIfNeeded(document->identifier());
        if (auto* activeServiceWorker = document->activeServiceWorker())
            request.setSelectedServiceWorkerRegistrationIdentifierIfNeeded(activeServiceWorker->registrationIdentifier());
#endif
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
#if ENABLE(PUBLIC_SUFFIX_LIST)
    // FetchMetadata depends on PSL to determine same-site relationships and without this
    // ability it is best to not set any FetchMetadata headers as sites generally expect
    // all of them or none.
    if (frameLoader.frame().settings().fetchMetadataEnabled()) {
        auto site = computeFetchMetadataSite(request.resourceRequest(), type, request.options().mode, frameLoader.frame().document()->securityOrigin());
        updateRequestFetchMetadataHeaders(request.resourceRequest(), request.options(), site);
    }
#endif // ENABLE(PUBLIC_SUFFIX_LIST)
    request.updateUserAgentHeader(frameLoader);

    request.updateAccordingCacheMode();
    request.updateAcceptEncodingHeader();
}

static FetchOptions::Destination destinationForType(CachedResource::Type type, Frame& frame)
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
    if (!resource || !is<CachedImage>(*resource))
        return false;
    return downcast<CachedImage>(*resource).hasSVGImage();
}

static inline SVGImage* cachedResourceSVGImage(CachedResource* resource)
{
    if (!isSVGImageCachedResource(resource))
        return nullptr;
    return downcast<SVGImage>(downcast<CachedImage>(*resource).image());
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::requestResource(CachedResource::Type type, CachedResourceRequest&& request, ForPreload forPreload, ImageLoading imageLoading)
{
    URL url = request.resourceRequest().url();
    if (!frame() || !frame()->page()) {
        CACHEDRESOURCELOADER_RELEASE_LOG("requestResource: failed because no frame or page");
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Invalid loader state"_s });
    }

    auto& frame = *this->frame();
    if (!url.isValid()) {
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: URL is invalid", frame);
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

    if (Document* document = this->document()) {
        request.upgradeInsecureRequestIfNeeded(*document);
        url = request.resourceRequest().url();
    }

    // We are passing url as well as request, as request url may contain a fragment identifier.
    if (!canRequest(type, url, request.options(), forPreload)) {
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: Not allowed to request resource", frame);
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Not allowed to request resource"_s, ResourceError::Type::AccessControl });
    }

    if (!portAllowed(url)) {
        if (forPreload == ForPreload::No)
            FrameLoader::reportBlockedLoadFailed(frame, url);
        CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("CachedResourceLoader::requestResource URL has a blocked port", frame);
        return makeUnexpected(frame.loader().blockedError(request.resourceRequest()));
    }

    request.updateReferrerPolicy(document() ? document()->referrerPolicy() : ReferrerPolicy::Default);

    if (InspectorInstrumentation::willIntercept(&frame, request.resourceRequest()))
        request.setCachingPolicy(CachingPolicy::DisallowCaching);

    auto& page = *frame.page();

#if ENABLE(CONTENT_EXTENSIONS)
    if (m_documentLoader) {
        const auto& resourceRequest = request.resourceRequest();
        auto results = page.userContentProvider().processContentRuleListsForLoad(page, resourceRequest.url(), ContentExtensions::toResourceType(type, request.resourceRequest().requester()), *m_documentLoader);
        bool blockedLoad = results.summary.blockedLoad;
        bool madeHTTPS = results.summary.madeHTTPS;
        request.applyResults(WTFMove(results), &page);
        if (blockedLoad) {
            CACHEDRESOURCELOADER_RELEASE_LOG_WITH_FRAME("requestResource: Resource blocked by content blocker", frame);
            if (type == CachedResource::Type::MainResource) {
                auto resource = createResource(type, WTFMove(request), page.sessionID(), &page.cookieJar(), page.settings());
                ASSERT(resource);
                resource->error(CachedResource::Status::LoadError);
                resource->setResourceError(ResourceError(ContentExtensions::WebKitContentBlockerDomain, 0, resourceRequest.url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description")));
                return resource;
            }
            if (RefPtr document = this->document()) {
                if (auto message = ContentExtensions::customLoadBlockingMessageForConsole(results, resourceRequest.url()))
                    document->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, WTFMove(*message));
            }
            return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Resource blocked by content blocker"_s, ResourceError::Type::AccessControl });
        }
        if (madeHTTPS
            && type == CachedResource::Type::MainResource
            && m_documentLoader->isLoadingMainResource()) {
            // This is to make sure the correct 'new' URL shows in the location bar.
            m_documentLoader->frameLoader()->client().dispatchDidChangeProvisionalURL();
        }
        url = request.resourceRequest().url(); // The content extension could have changed it from http to https.
        url = MemoryCache::removeFragmentIdentifierIfNeeded(url); // Might need to remove fragment identifier again.
    }
#endif

    if (m_documentLoader && !m_documentLoader->customHeaderFields().isEmpty()) {
        bool sameOriginRequest = false;
        auto requestedOrigin = SecurityOrigin::create(url);
        if (type == CachedResource::Type::MainResource) {
            if (frame.isMainFrame())
                sameOriginRequest = true;
            else if (auto* topDocument = frame.mainFrame().document())
                sameOriginRequest = topDocument->securityOrigin().isSameSchemeHostPort(requestedOrigin.get());
        } else if (document()) {
            sameOriginRequest = document()->topOrigin().isSameSchemeHostPort(requestedOrigin.get())
                && document()->securityOrigin().isSameSchemeHostPort(requestedOrigin.get());
        }
        for (auto& fields : m_documentLoader->customHeaderFields()) {
            if (sameOriginRequest || fields.thirdPartyDomainsMatch(url)) {
                for (auto& field : fields.fields)
                    request.resourceRequest().setHTTPHeaderField(field.name(), field.value());
            }
        }
    }

    ResourceLoadTiming loadTiming;
    loadTiming.markStartTime();
    InitiatorContext initiatorContext = request.options().initiatorContext;

    if (request.resourceRequest().url().protocolIsInHTTPFamily())
        updateHTTPRequestHeaders(frame.loader(), type, request);

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching() && memoryCache.disabled())
        m_documentResources.remove(url.string());

    // See if we can use an existing resource from the cache.
    CachedResourceHandle<CachedResource> resource;
    if (document())
        request.setDomainForCachePartition(*document());

    if (request.allowsCaching())
        resource = memoryCache.resourceForRequest(request.resourceRequest(), page.sessionID());

    if (resource && request.isLinkPreload() && !resource->isLinkPreload())
        resource->setLinkPreload();

    logMemoryCacheResourceRequest(&frame, DiagnosticLoggingKeys::memoryCacheUsageKey(), resource ? DiagnosticLoggingKeys::inMemoryCacheKey() : DiagnosticLoggingKeys::notInMemoryCacheKey());

    auto& cookieJar = page.cookieJar();

    RevalidationPolicy policy = determineRevalidationPolicy(type, request, resource.get(), forPreload, imageLoading);
    switch (policy) {
    case Reload:
        memoryCache.remove(*resource);
        FALLTHROUGH;
    case Load:
        if (resource) {
            logMemoryCacheResourceRequest(&frame, DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::unusedKey());
            memoryCache.remove(*resource);
        }
        resource = loadResource(type, page.sessionID(), WTFMove(request), cookieJar, page.settings());
        break;
    case Revalidate:
        if (resource)
            logMemoryCacheResourceRequest(&frame, DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::revalidatingKey());
        resource = revalidateResource(WTFMove(request), *resource);
        break;
    case Use:
        ASSERT(resource);
        if (request.options().mode == FetchOptions::Mode::Navigate && !frame.isMainFrame()) {
            auto* localParentFrame = dynamicDowncast<LocalFrame>(frame.tree().parent());
            if (auto* parentDocument = localParentFrame ? localParentFrame->document() : nullptr) {
                auto coep = parentDocument->crossOriginEmbedderPolicy().value;
                if (auto error = validateCrossOriginResourcePolicy(coep, parentDocument->securityOrigin(), request.resourceRequest().url(), resource->response(), ForNavigation::Yes))
                    return makeUnexpected(WTFMove(*error));
            }
        }
        // Per the Fetch specification, the "cross-origin resource policy check" should only occur in the HTTP Fetch case (https://fetch.spec.whatwg.org/#concept-http-fetch).
        // However, per https://fetch.spec.whatwg.org/#main-fetch, if the request URL's protocol is "data:", then we should perform a scheme fetch which would end up
        // returning a response WITHOUT performing an HTTP fetch (and thus no CORP check).
        if (request.options().mode == FetchOptions::Mode::NoCors && !url.protocolIsData()) {
            auto coep = document() ? document()->crossOriginEmbedderPolicy().value : CrossOriginEmbedderPolicyValue::UnsafeNone;
            if (auto error = validateCrossOriginResourcePolicy(coep, *request.origin(), request.resourceRequest().url(), resource->response(), ForNavigation::No))
                return makeUnexpected(WTFMove(*error));
        }
        if (request.options().mode == FetchOptions::Mode::NoCors) {
            if (auto error = validateRangeRequestedFlag(request.resourceRequest(), resource->response()))
                return makeUnexpected(WTFMove(*error));
        }
        if (shouldUpdateCachedResourceWithCurrentRequest(*resource, request)) {
            resource = updateCachedResourceWithCurrentRequest(*resource, WTFMove(request), page.sessionID(), cookieJar, page.settings());
            if (resource->status() != CachedResource::Status::Cached)
                policy = Load;
        } else {
            ResourceError error;
            if (!shouldContinueAfterNotifyingLoadedFromMemoryCache(request, *resource, error))
                return makeUnexpected(WTFMove(error));
            logMemoryCacheResourceRequest(&frame, DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::usedKey());
            loadTiming.markEndTime();

            memoryCache.resourceAccessed(*resource);

            if (document() && !resource->isLoading()) {
                Box<NetworkLoadMetrics> metrics;
                auto* documentLoader = document()->loader();
                // Use the actual network load metrics if this is the first time loading this resource and it was loaded
                // for this document because it may have been speculatively preloaded.
                if (auto metricsFromResource = resource->takeNetworkLoadMetrics(); metricsFromResource && documentLoader && metricsFromResource->redirectStart >= documentLoader->timing().timeOrigin())
                    metrics = WTFMove(metricsFromResource);
                auto resourceTiming = ResourceTiming::fromMemoryCache(url, request.initiatorName(), loadTiming, resource->response(), metrics ? *metrics : NetworkLoadMetrics::emptyMetrics(), *request.origin());
                if (initiatorContext == InitiatorContext::Worker) {
                    ASSERT(is<CachedRawResource>(resource.get()));
                    downcast<CachedRawResource>(resource.get())->finishedTimingForWorkerLoad(WTFMove(resourceTiming));
                } else {
                    ASSERT(initiatorContext == InitiatorContext::Document);
                    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, request.initiatorName(), &frame);
                    m_resourceTimingInfo.addResourceTiming(*resource.get(), *document(), WTFMove(resourceTiming));
                }
            }

            if (forPreload == ForPreload::No)
                resource->setLoadPriority(request.priority());
        }
        break;
    }
    ASSERT(resource);
    resource->setOriginalRequest(WTFMove(originalRequest));

    if (auto* subresourceLoader = resource->loader(); forPreload == ForPreload::No && subresourceLoader && resource->ignoreForRequestCount()) {
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
#if ENABLE(TRACKING_PREVENTION)
    frame.loader().client().didLoadFromRegistrableDomain(RegistrableDomain(resource->resourceRequest().url()));
#endif
    return resource;
}

void CachedResourceLoader::documentDidFinishLoadEvent()
{
    m_validatedURLs.clear();

    // If m_preloads is not empty here, it's full of link preloads,
    // as speculative preloads were cleared at DCL.
    if (m_preloads && m_preloads->size() && !m_unusedPreloadsTimer.isActive())
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

    auto newResource = createResource(WTFMove(request), resource);

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), &resource);
    newResource->setResourceToRevalidate(&resource);

    memoryCache.remove(resource);
    memoryCache.add(*newResource);

    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(newResource, newResource->initiatorName(), frame());

    return newResource;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::loadResource(CachedResource::Type type, PAL::SessionID sessionID, CachedResourceRequest&& request, const CookieJar& cookieJar, const Settings& settings)
{
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!request.allowsCaching() || !memoryCache.resourceForRequest(request.resourceRequest(), sessionID)
        || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData);

    LOG(ResourceLoading, "Loading CachedResource for '%s'.", request.resourceRequest().url().stringCenterEllipsizedToLength().latin1().data());

    auto resource = createResource(type, WTFMove(request), sessionID, &cookieJar, settings);

    if (resource->allowsCaching())
        memoryCache.add(*resource);

    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, resource->initiatorName(), frame());

    return resource;
}

static void logRevalidation(const String& reason, DiagnosticLoggingClient& logClient)
{
    logClient.logDiagnosticMessage(DiagnosticLoggingKeys::cachedResourceRevalidationReasonKey(), reason, ShouldSample::Yes);
}

static void logResourceRevalidationDecision(CachedResource::RevalidationDecision reason, const Frame* frame)
{
    if (!frame || !frame->page())
        return;
    auto& logClient = frame->page()->diagnosticLoggingClient();
    switch (reason) {
    case CachedResource::RevalidationDecision::No:
        break;
    case CachedResource::RevalidationDecision::YesDueToExpired:
        logRevalidation(DiagnosticLoggingKeys::isExpiredKey(), logClient);
        break;
    case CachedResource::RevalidationDecision::YesDueToNoStore:
        logRevalidation(DiagnosticLoggingKeys::noStoreKey(), logClient);
        break;
    case CachedResource::RevalidationDecision::YesDueToNoCache:
        logRevalidation(DiagnosticLoggingKeys::noCacheKey(), logClient);
        break;
    case CachedResource::RevalidationDecision::YesDueToCachePolicy:
        logRevalidation(DiagnosticLoggingKeys::reloadKey(), logClient);
        break;
    }
}

#if ENABLE(SERVICE_WORKER)
static inline bool mustReloadFromServiceWorkerOptions(const ResourceLoaderOptions& options, const ResourceLoaderOptions& cachedOptions)
{
    // FIXME: We should validate/specify this behavior.
    if (options.serviceWorkerRegistrationIdentifier != cachedOptions.serviceWorkerRegistrationIdentifier)
        return true;

    if (options.serviceWorkersMode == cachedOptions.serviceWorkersMode)
        return false;

    return cachedOptions.mode == FetchOptions::Mode::Navigate || cachedOptions.destination == FetchOptions::Destination::Worker || cachedOptions.destination == FetchOptions::Destination::Sharedworker;
}
#endif

CachedResourceLoader::RevalidationPolicy CachedResourceLoader::determineRevalidationPolicy(CachedResource::Type type, CachedResourceRequest& cachedResourceRequest, CachedResource* existingResource, ForPreload forPreload, ImageLoading imageLoading) const
{
    auto& request = cachedResourceRequest.resourceRequest();

    if (!existingResource)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData)
        return Reload;

#if ENABLE(SERVICE_WORKER)
    if (mustReloadFromServiceWorkerOptions(cachedResourceRequest.options(), existingResource->options())) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading because selected service worker may differ");
        return Reload;
    }
#endif

    // We already have a preload going for this URL.
    if (forPreload == ForPreload::Yes && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to type mismatch.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonTypeMismatchKey());
        return Reload;
    }

    if (!existingResource->varyHeaderValuesMatch(request))
        return Reload;

    auto* textDecoder = existingResource->textResourceDecoder();
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

    if (is<CachedImage>(*existingResource) && downcast<CachedImage>(*existingResource).canSkipRevalidation(*this, cachedResourceRequest))
        return Use;

    auto cachePolicy = this->cachePolicy(type, request.url());

    // Validate the redirect chain.
    bool cachePolicyIsHistoryBuffer = cachePolicy == CachePolicy::HistoryBuffer;
    if (!existingResource->redirectChainAllowsReuse(cachePolicyIsHistoryBuffer ? ReuseExpiredRedirection : DoNotReuseExpiredRedirection)) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to not cached or expired redirections.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonRedirectChainKey());
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
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonNoStoreKey());
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
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonCredentialSettingsKey());
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document, even if the cache policies would tell us to.
    if (document() && !document()->loadEventFinished() && m_validatedURLs.contains(existingResource->url()))
        return Use;

    // CachePolicy::Reload always reloads
    if (cachePolicy == CachePolicy::Reload) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to CachePolicyReload.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonReloadKey());
        return Reload;
    }
    
    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicye reloading due to resource being in the error state");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonErrorKey());
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
    logResourceRevalidationDecision(revalidationDecision, frame());

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (revalidationDecision != CachedResource::RevalidationDecision::No) {
        // See if the resource has usable ETag or Last-modified headers.
        if (existingResource->canUseCacheValidator()) {
#if ENABLE(SERVICE_WORKER)
            // Revalidating will mean exposing headers to the service worker, let's reload given the service worker already handled it.
            if (cachedResourceRequest.options().serviceWorkerRegistrationIdentifier)
                return Reload;
#endif
            return Revalidate;
        }
        
        // No, must reload.
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to missing cache validators.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonMustRevalidateNoValidatorKey());
        return Reload;
    }

    return Use;
}

void CachedResourceLoader::printAccessDeniedMessage(const URL& url) const
{
    if (url.isNull())
        return;

    if (!frame())
        return;

    String message;
    if (!m_document || m_document->url().isNull())
        message = makeString("Unsafe attempt to load URL ", url.stringCenterEllipsizedToLength(), '.');
    else
        message = makeString("Unsafe attempt to load URL ", url.stringCenterEllipsizedToLength(), " from origin ", m_document->securityOrigin().toString(), ". Domains, protocols and ports must match.\n");

    frame()->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
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
        if (is<CachedImage>(*resource) && resource->stillNeedsLoad() && clientDefersImage(resource->url()) == ImageLoading::Immediate)
            downcast<CachedImage>(*resource).load(*this);
    }
}

CachePolicy CachedResourceLoader::cachePolicy(CachedResource::Type type, const URL& url) const
{
    Frame* frame = this->frame();
    if (!frame)
        return CachePolicy::Verify;

    if (type != CachedResource::Type::MainResource)
        return frame->loader().subresourceCachePolicy(url);

    if (Page* page = frame->page()) {
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

void CachedResourceLoader::loadDone(LoadCompletionType type, bool shouldPerformPostLoadActions)
{
    RefPtr<DocumentLoader> protectDocumentLoader(m_documentLoader);
    RefPtr<Document> protectDocument(m_document.get());

    ASSERT(shouldPerformPostLoadActions || type == LoadCompletionType::Cancel);

    if (frame())
        frame()->loader().loadDone(type);

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
        auto* resource = cachedResource(cachedSVGImageURL);
        if (auto* image = cachedResourceSVGImage(resource))
            allCachedSVGImages.append(*image);
    }
        
    return allCachedSVGImages;
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::preload(CachedResource::Type type, CachedResourceRequest&& request)
{
    if (InspectorInstrumentation::willIntercept(frame(), request.resourceRequest()))
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, request.resourceRequest().url(), "Inspector intercept"_s });

    if (request.charset().isEmpty() && (type == CachedResource::Type::Script || type == CachedResource::Type::CSSStyleSheet))
        request.setCharset(m_document->charset());

    auto resource = requestResource(type, WTFMove(request), ForPreload::Yes);
    if (resource && (!m_preloads || !m_preloads->contains(resource.value().get()))) {
        auto resourceValue = resource.value();
        // Fonts need special treatment since just creating the resource doesn't trigger a load.
        if (type == CachedResource::Type::FontResource)
            downcast<CachedFont>(resourceValue.get())->beginLoadIfNeeded(*this);
        resourceValue->increasePreloadCount();

        if (!m_preloads)
            m_preloads = makeUnique<ListHashSet<CachedResource*>>();
        m_preloads->add(resourceValue.get());
    }
    return resource;
}

void CachedResourceLoader::warnUnusedPreloads()
{
    if (!m_preloads)
        return;
    for (const auto& resource : *m_preloads) {
        if (resource && resource->isLinkPreload() && resource->preloadResult() == CachedResource::PreloadResult::PreloadNotReferenced && document()) {
            document()->addConsoleMessage(MessageSource::Other, MessageLevel::Warning,
                "The resource " + resource->url().string() +
                " was preloaded using link preload but not used within a few seconds from the window's load event. Please make sure it wasn't preloaded for nothing.");
        }
    }
}

bool CachedResourceLoader::isPreloaded(const String& urlString) const
{
    const URL& url = m_document->completeURL(urlString);

    if (m_preloads) {
        for (auto& resource : *m_preloads) {
            if (resource->url() == url)
                return true;
        }
    }
    return false;
}

void CachedResourceLoader::clearPreloads(ClearPreloadsMode mode)
{
    if (!m_preloads)
        return;

    std::unique_ptr<ListHashSet<CachedResource*>> remainingLinkPreloads;
    for (auto* resource : *m_preloads) {
        ASSERT(resource);
        if (mode == ClearPreloadsMode::ClearSpeculativePreloads && resource->isLinkPreload()) {
            if (!remainingLinkPreloads)
                remainingLinkPreloads = makeUnique<ListHashSet<CachedResource*>>();
            remainingLinkPreloads->add(resource);
            continue;
        }
        resource->decreasePreloadCount();
        bool deleted = resource->deleteIfPossible();
        if (!deleted && resource->preloadResult() == CachedResource::PreloadResult::PreloadNotReferenced)
            MemoryCache::singleton().remove(*resource);
    }
    m_preloads = WTFMove(remainingLinkPreloads);
}

Vector<CachedResource*> CachedResourceLoader::visibleResourcesToPrioritize()
{
    if (!document())
        return { };

    Vector<CachedResource*> toPrioritize;

    for (auto& resource : m_documentResources.values()) {
        if (!is<CachedImage>(resource.get()))
            continue;
        auto& cachedImage = downcast<CachedImage>(*resource);
        if (!cachedImage.isLoading())
            continue;
        if (!cachedImage.url().protocolIsInHTTPFamily())
            continue;
        if (!cachedImage.loader())
            continue;
        if (!cachedImage.isVisibleInViewport(*document()))
            continue;
        toPrioritize.append(&cachedImage);
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
