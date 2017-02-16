/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedRawResource.h"
#include "CachedResourceRequest.h"
#include "CachedSVGDocument.h"
#include "CachedSVGFont.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContentExtensionError.h"
#include "ContentExtensionRule.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLElement.h"
#include "HTMLFrameOwnerElement.h"
#include "LoaderStrategy.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include "RenderElement.h"
#include "ResourceLoadInfo.h"
#include "ResourceTiming.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "SessionID.h"
#include "Settings.h"
#include "StyleSheetContents.h"
#include "SubresourceLoader.h"
#include "UserContentController.h"
#include "UserStyleSheet.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(VIDEO_TRACK)
#include "CachedTextTrack.h"
#endif

#define PRELOAD_DEBUG 0

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - CachedResourceLoader::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

static CachedResource* createResource(CachedResource::Type type, CachedResourceRequest&& request, SessionID sessionID)
{
    switch (type) {
    case CachedResource::ImageResource:
        return new CachedImage(WTFMove(request), sessionID);
    case CachedResource::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), sessionID);
    case CachedResource::Script:
        return new CachedScript(WTFMove(request), sessionID);
    case CachedResource::SVGDocumentResource:
        return new CachedSVGDocument(WTFMove(request), sessionID);
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
        return new CachedSVGFont(WTFMove(request), sessionID);
#endif
    case CachedResource::FontResource:
        return new CachedFont(WTFMove(request), sessionID);
    case CachedResource::MediaResource:
    case CachedResource::RawResource:
    case CachedResource::MainResource:
        return new CachedRawResource(WTFMove(request), type, sessionID);
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return new CachedXSLStyleSheet(WTFMove(request), sessionID);
#endif
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return new CachedResource(WTFMove(request), CachedResource::LinkPrefetch, sessionID);
    case CachedResource::LinkSubresource:
        return new CachedResource(WTFMove(request), CachedResource::LinkSubresource, sessionID);
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
        return new CachedTextTrack(WTFMove(request), sessionID);
#endif
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

CachedResourceLoader::CachedResourceLoader(DocumentLoader* documentLoader)
    : m_document(nullptr)
    , m_documentLoader(documentLoader)
    , m_requestCount(0)
    , m_garbageCollectDocumentResourcesTimer(*this, &CachedResourceLoader::garbageCollectDocumentResources)
    , m_autoLoadImages(true)
    , m_imagesEnabled(true)
    , m_allowStaleResources(false)
{
}

CachedResourceLoader::~CachedResourceLoader()
{
    m_documentLoader = nullptr;
    m_document = nullptr;

    clearPreloads(ClearPreloadsMode::ClearAllPreloads);
    for (auto& resource : m_documentResources.values())
        resource->setOwningCachedResourceLoader(nullptr);

    // Make sure no requests still point to this CachedResourceLoader
    ASSERT(m_requestCount == 0);
}

CachedResource* CachedResourceLoader::cachedResource(const String& resourceURL) const
{
    ASSERT(!resourceURL.isNull());
    return cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(m_document->completeURL(resourceURL)));
}

CachedResource* CachedResourceLoader::cachedResource(const URL& url) const
{
    ASSERT(!MemoryCache::shouldRemoveFragmentIdentifier(url));
    return m_documentResources.get(url).get();
}

Frame* CachedResourceLoader::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : nullptr;
}

SessionID CachedResourceLoader::sessionID() const
{
    SessionID sessionID = SessionID::defaultSessionID();

    if (Frame* f = frame())
        sessionID = f->page()->sessionID();

    return sessionID;
}

CachedResourceHandle<CachedImage> CachedResourceLoader::requestImage(CachedResourceRequest&& request)
{
    if (Frame* frame = this->frame()) {
        if (frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None) {
            if (Document* document = frame->document())
                request.upgradeInsecureRequestIfNeeded(*document);
            URL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(CachedResource::ImageResource, requestURL, request, ForPreload::No))
                PingLoader::loadImage(*frame, requestURL);
            return nullptr;
        }
    }

    auto defer = clientDefersImage(request.resourceRequest().url()) ? DeferOption::DeferredByClient : DeferOption::NoDefer;
    return downcast<CachedImage>(requestResource(CachedResource::ImageResource, WTFMove(request), ForPreload::No, defer).get());
}

CachedResourceHandle<CachedFont> CachedResourceLoader::requestFont(CachedResourceRequest&& request, bool isSVG)
{
#if ENABLE(SVG_FONTS)
    if (isSVG)
        return downcast<CachedSVGFont>(requestResource(CachedResource::SVGFontResource, WTFMove(request)).get());
#else
    UNUSED_PARAM(isSVG);
#endif
    return downcast<CachedFont>(requestResource(CachedResource::FontResource, WTFMove(request)).get());
}

#if ENABLE(VIDEO_TRACK)
CachedResourceHandle<CachedTextTrack> CachedResourceLoader::requestTextTrack(CachedResourceRequest&& request)
{
    return downcast<CachedTextTrack>(requestResource(CachedResource::TextTrackResource, WTFMove(request)).get());
}
#endif

CachedResourceHandle<CachedCSSStyleSheet> CachedResourceLoader::requestCSSStyleSheet(CachedResourceRequest&& request)
{
    return downcast<CachedCSSStyleSheet>(requestResource(CachedResource::CSSStyleSheet, WTFMove(request)).get());
}

CachedResourceHandle<CachedCSSStyleSheet> CachedResourceLoader::requestUserCSSStyleSheet(CachedResourceRequest&& request)
{
    ASSERT(document());
    request.setDomainForCachePartition(*document());

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching()) {
        if (CachedResource* existing = memoryCache.resourceForRequest(request.resourceRequest(), sessionID())) {
            if (is<CachedCSSStyleSheet>(*existing))
                return downcast<CachedCSSStyleSheet>(existing);
            memoryCache.remove(*existing);
        }
    }

    request.removeFragmentIdentifierIfNeeded();

    CachedResourceHandle<CachedCSSStyleSheet> userSheet = new CachedCSSStyleSheet(WTFMove(request), sessionID());

    if (userSheet->allowsCaching())
        memoryCache.add(*userSheet);
    // FIXME: loadResource calls setOwningCachedResourceLoader() if the resource couldn't be added to cache. Does this function need to call it, too?

    userSheet->load(*this);
    return userSheet;
}

CachedResourceHandle<CachedScript> CachedResourceLoader::requestScript(CachedResourceRequest&& request)
{
    return downcast<CachedScript>(requestResource(CachedResource::Script, WTFMove(request)).get());
}

#if ENABLE(XSLT)
CachedResourceHandle<CachedXSLStyleSheet> CachedResourceLoader::requestXSLStyleSheet(CachedResourceRequest&& request)
{
    return downcast<CachedXSLStyleSheet>(requestResource(CachedResource::XSLStyleSheet, WTFMove(request)).get());
}
#endif

CachedResourceHandle<CachedSVGDocument> CachedResourceLoader::requestSVGDocument(CachedResourceRequest&& request)
{
    return downcast<CachedSVGDocument>(requestResource(CachedResource::SVGDocumentResource, WTFMove(request)).get());
}

#if ENABLE(LINK_PREFETCH)
CachedResourceHandle<CachedResource> CachedResourceLoader::requestLinkResource(CachedResource::Type type, CachedResourceRequest&& request)
{
    ASSERT(frame());
    ASSERT(type == CachedResource::LinkPrefetch || type == CachedResource::LinkSubresource);
    return requestResource(type, WTFMove(request));
}
#endif

CachedResourceHandle<CachedRawResource> CachedResourceLoader::requestMedia(CachedResourceRequest&& request)
{
    return downcast<CachedRawResource>(requestResource(CachedResource::MediaResource, WTFMove(request)).get());
}

CachedResourceHandle<CachedRawResource> CachedResourceLoader::requestRawResource(CachedResourceRequest&& request)
{
    return downcast<CachedRawResource>(requestResource(CachedResource::RawResource, WTFMove(request)).get());
}

CachedResourceHandle<CachedRawResource> CachedResourceLoader::requestMainResource(CachedResourceRequest&& request)
{
    return downcast<CachedRawResource>(requestResource(CachedResource::MainResource, WTFMove(request)).get());
}

static MixedContentChecker::ContentType contentTypeFromResourceType(CachedResource::Type type)
{
    switch (type) {
    // https://w3c.github.io/webappsec-mixed-content/#category-optionally-blockable
    // Editor's Draft, 11 February 2016
    // 3.1. Optionally-blockable Content
    case CachedResource::ImageResource:
    case CachedResource::MediaResource:
            return MixedContentChecker::ContentType::ActiveCanWarn;

    case CachedResource::CSSStyleSheet:
    case CachedResource::Script:
    case CachedResource::FontResource:
        return MixedContentChecker::ContentType::Active;

#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
        return MixedContentChecker::ContentType::Active;
#endif

    case CachedResource::RawResource:
    case CachedResource::SVGDocumentResource:
        return MixedContentChecker::ContentType::Active;
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return MixedContentChecker::ContentType::Active;
#endif

#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
        return MixedContentChecker::ContentType::Active;
#endif

#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
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
    case CachedResource::Script:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
    case CachedResource::SVGDocumentResource:
    case CachedResource::CSSStyleSheet:
        // These resource can inject script into the current document (Script,
        // XSL) or exfiltrate the content of the current document (CSS).
        if (Frame* frame = this->frame()) {
            if (!frame->loader().mixedContentChecker().canRunInsecureContent(m_document->securityOrigin(), url))
                return false;
            Frame& top = frame->tree().top();
            if (&top != frame && !top.loader().mixedContentChecker().canRunInsecureContent(top.document()->securityOrigin(), url))
                return false;
        }
        break;
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
    case CachedResource::MediaResource:
    case CachedResource::RawResource:
    case CachedResource::ImageResource:
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource: {
        // These resources can corrupt only the frame's pixels.
        if (Frame* frame = this->frame()) {
            if (!frame->loader().mixedContentChecker().canDisplayInsecureContent(m_document->securityOrigin(), contentTypeFromResourceType(type), url, MixedContentChecker::AlwaysDisplayInNonStrictMode::Yes))
                return false;
            Frame& topFrame = frame->tree().top();
            if (!topFrame.loader().mixedContentChecker().canDisplayInsecureContent(topFrame.document()->securityOrigin(), contentTypeFromResourceType(type), url))
                return false;
        }
        break;
    }
    case CachedResource::MainResource:
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
        // Prefetch cannot affect the current document.
#endif
        break;
    }
    return true;
}

bool CachedResourceLoader::allowedByContentSecurityPolicy(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, ContentSecurityPolicy::RedirectResponseReceived redirectResponseReceived) const
{
    if (options.contentSecurityPolicyImposition == ContentSecurityPolicyImposition::SkipPolicyCheck)
        return true;

    ASSERT(m_document);
    ASSERT(m_document->contentSecurityPolicy());

    switch (type) {
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
    case CachedResource::Script:
        if (!m_document->contentSecurityPolicy()->allowScriptFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::CSSStyleSheet:
        if (!m_document->contentSecurityPolicy()->allowStyleFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::SVGDocumentResource:
    case CachedResource::ImageResource:
        if (!m_document->contentSecurityPolicy()->allowImageFromSource(url, redirectResponseReceived))
            return false;
        break;
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource:
        if (!m_document->contentSecurityPolicy()->allowFontFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::MediaResource:
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
        if (!m_document->contentSecurityPolicy()->allowMediaFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::RawResource:
        return true;
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

bool CachedResourceLoader::canRequest(CachedResource::Type type, const URL& url, const CachedResourceRequest& request, ForPreload forPreload)
{
    auto& options = request.options();

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

    if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::No))
        return false;

    // SVG Images have unique security rules that prevent all subresource requests except for data urls.
    if (type != CachedResource::MainResource && frame() && frame()->page()) {
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
bool CachedResourceLoader::canRequestAfterRedirection(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options) const
{
    if (document() && !document()->securityOrigin().canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame(), url.stringCenterEllipsizedToLength());
        LOG(ResourceLoading, "CachedResourceLoader::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return false;
    }

    // FIXME: According to https://fetch.spec.whatwg.org/#http-redirect-fetch, we should check that the URL is HTTP(s) except if in navigation mode.
    // But we currently allow at least data URLs to be loaded.

    if (options.mode == FetchOptions::Mode::SameOrigin && !m_document->securityOrigin().canRequest(url)) {
        printAccessDeniedMessage(url);
        return false;
    }

    if (!allowedByContentSecurityPolicy(type, url, options, ContentSecurityPolicy::RedirectResponseReceived::Yes))
        return false;

    // Last of all, check for insecure content. We do this last so that when folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.
    if (!checkInsecureContent(type, url))
        return false;

    return true;
}

bool CachedResourceLoader::updateRequestAfterRedirection(CachedResource::Type type, ResourceRequest& request, const ResourceLoaderOptions& options)
{
    ASSERT(m_documentLoader);
    if (auto* document = m_documentLoader->cachedResourceLoader().document())
        upgradeInsecureResourceRequestIfNeeded(request, *document);

    // FIXME: We might want to align the checks done here with the ones done in CachedResourceLoader::requestResource, content extensions blocking in particular.

    return canRequestAfterRedirection(type, request.url(), options);
}

bool CachedResourceLoader::canRequestInContentDispositionAttachmentSandbox(CachedResource::Type type, const URL& url) const
{
    Document* document;

    // FIXME: Do we want to expand this to all resource types that the mixed content checker would consider active content?
    switch (type) {
    case CachedResource::MainResource:
        if (auto ownerElement = frame() ? frame()->ownerElement() : nullptr) {
            document = &ownerElement->document();
            break;
        }
        return true;
    case CachedResource::CSSStyleSheet:
        document = m_document;
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

bool CachedResourceLoader::shouldContinueAfterNotifyingLoadedFromMemoryCache(const CachedResourceRequest& request, CachedResource* resource)
{
    if (!resource || !frame() || resource->status() != CachedResource::Cached)
        return true;

    ResourceRequest newRequest = ResourceRequest(resource->url());
    newRequest.setInitiatorIdentifier(request.resourceRequest().initiatorIdentifier());
    if (request.resourceRequest().hiddenFromInspector())
        newRequest.setHiddenFromInspector(true);
    frame()->loader().loadedResourceFromMemoryCache(resource, newRequest);

    // FIXME <http://webkit.org/b/113251>: If the delegate modifies the request's
    // URL, it is no longer appropriate to use this CachedResource.
    return !newRequest.isNull();
}

bool CachedResourceLoader::shouldUpdateCachedResourceWithCurrentRequest(const CachedResource& resource, const CachedResourceRequest& request)
{
    // WebKit is not supporting CORS for fonts (https://bugs.webkit.org/show_bug.cgi?id=86817), no need to update the resource before reusing it.
    if (resource.type() == CachedResource::Type::FontResource)
        return false;

#if ENABLE(SVG_FONTS)
    if (resource.type() == CachedResource::Type::SVGFontResource)
        return false;
#endif

#if ENABLE(XSLT)
    // Load is same-origin, we do not check for CORS.
    if (resource.type() == CachedResource::XSLStyleSheet)
        return false;
#endif

    // FIXME: We should enable resource reuse for these resource types
    switch (resource.type()) {
    case CachedResource::SVGDocumentResource:
        return false;
    case CachedResource::MainResource:
        return false;
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return false;
    case CachedResource::LinkSubresource:
        return false;
#endif
    default:
        break;
    }

    if (resource.options().mode != request.options().mode || !originsMatch(request.origin(), resource.origin()))
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

    return true;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::updateCachedResourceWithCurrentRequest(const CachedResource& resource, CachedResourceRequest&& request)
{
    if (!isResourceSuitableForDirectReuse(resource, request)) {
        request.setCachingPolicy(CachingPolicy::DisallowCaching);
        return loadResource(resource.type(), WTFMove(request));
    }

    auto resourceHandle = createResource(resource.type(), WTFMove(request), sessionID());
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
    // Implementing step 1 to 7 of https://fetch.spec.whatwg.org/#fetching

    if (!request.origin() && document())
        request.setOrigin(document()->securityOrigin());

    request.setAcceptHeaderIfNone(type);

    // Accept-Language value is handled in underlying port-specific code.
    // FIXME: Decide whether to support client hints
}

void CachedResourceLoader::updateHTTPRequestHeaders(CachedResource::Type type, CachedResourceRequest& request)
{
    // Implementing steps 7 to 12 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch

    // FIXME: We should reconcile handling of MainResource with other resources.
    if (type != CachedResource::Type::MainResource) {
        // In some cases we may try to load resources in frameless documents. Such loads always fail.
        // FIXME: We shouldn't need to do the check on frame.
        if (auto* frame = this->frame())
            request.updateReferrerOriginAndUserAgentHeaders(frame->loader(), document() ? document()->referrerPolicy() : ReferrerPolicy::Default);
    }

    request.updateAccordingCacheMode();
}

CachedResourceHandle<CachedResource> CachedResourceLoader::requestResource(CachedResource::Type type, CachedResourceRequest&& request, ForPreload forPreload, DeferOption defer)
{
    if (Document* document = this->document())
        request.upgradeInsecureRequestIfNeeded(*document);

    URL url = request.resourceRequest().url();

    LOG(ResourceLoading, "CachedResourceLoader::requestResource '%s', charset '%s', priority=%d, forPreload=%u", url.stringCenterEllipsizedToLength().latin1().data(), request.charset().latin1().data(), request.priority() ? static_cast<int>(request.priority().value()) : -1, forPreload == ForPreload::Yes);

    if (!url.isValid()) {
        RELEASE_LOG_IF_ALLOWED("requestResource: URL is invalid (frame = %p)", frame());
        return nullptr;
    }

    prepareFetch(type, request);

    // We are passing url as well as request, as request url may contain a fragment identifier.
    if (!canRequest(type, url, request, forPreload)) {
        RELEASE_LOG_IF_ALLOWED("requestResource: Not allowed to request resource (frame = %p)", frame());
        return nullptr;
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (frame() && frame()->mainFrame().page() && m_documentLoader) {
        const auto& resourceRequest = request.resourceRequest();
        auto blockedStatus = frame()->mainFrame().page()->userContentProvider().processContentExtensionRulesForLoad(resourceRequest.url(), toResourceType(type), *m_documentLoader);
        request.applyBlockedStatus(blockedStatus);
        if (blockedStatus.blockedLoad) {
            RELEASE_LOG_IF_ALLOWED("requestResource: Resource blocked by content blocker (frame = %p)", frame());
            if (type == CachedResource::Type::MainResource) {
                auto resource = createResource(type, WTFMove(request), sessionID());
                ASSERT(resource);
                resource->error(CachedResource::Status::LoadError);
                resource->setResourceError(ResourceError(ContentExtensions::WebKitContentBlockerDomain, 0, resourceRequest.url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description")));
                return resource;
            }
            return nullptr;
        }
        if (blockedStatus.madeHTTPS
            && type == CachedResource::Type::MainResource
            && m_documentLoader->isLoadingMainResource()) {
            // This is to make sure the correct 'new' URL shows in the location bar.
            m_documentLoader->frameLoader()->client().dispatchDidChangeProvisionalURL();
        }
        url = request.resourceRequest().url(); // The content extension could have changed it from http to https.
        url = MemoryCache::removeFragmentIdentifierIfNeeded(url); // Might need to remove fragment identifier again.
    }
#endif

#if ENABLE(WEB_TIMING)
    LoadTiming loadTiming;
    loadTiming.markStartTimeAndFetchStart();
    InitiatorContext initiatorContext = request.options().initiatorContext;
#endif

    if (request.resourceRequest().url().protocolIsInHTTPFamily())
        updateHTTPRequestHeaders(type, request);

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching() && memoryCache.disabled()) {
        DocumentResourceMap::iterator it = m_documentResources.find(url.string());
        if (it != m_documentResources.end()) {
            it->value->setOwningCachedResourceLoader(nullptr);
            m_documentResources.remove(it);
        }
    }

    // See if we can use an existing resource from the cache.
    CachedResourceHandle<CachedResource> resource;
    if (document())
        request.setDomainForCachePartition(*document());

    if (request.allowsCaching())
        resource = memoryCache.resourceForRequest(request.resourceRequest(), sessionID());

    if (resource && request.isLinkPreload() && !resource->isLinkPreload())
        resource->setLinkPreload();

    logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheUsageKey(), resource ? DiagnosticLoggingKeys::inMemoryCacheKey() : DiagnosticLoggingKeys::notInMemoryCacheKey());

    RevalidationPolicy policy = determineRevalidationPolicy(type, request, resource.get(), forPreload, defer);
    switch (policy) {
    case Reload:
        memoryCache.remove(*resource);
        FALLTHROUGH;
    case Load:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::unusedKey());
        resource = loadResource(type, WTFMove(request));
        break;
    case Revalidate:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::revalidatingKey());
        resource = revalidateResource(WTFMove(request), *resource);
        break;
    case Use:
        ASSERT(resource);
        if (shouldUpdateCachedResourceWithCurrentRequest(*resource, request)) {
            resource = updateCachedResourceWithCurrentRequest(*resource, WTFMove(request));
            if (resource->status() != CachedResource::Status::Cached)
                policy = Load;
        } else {
            if (!shouldContinueAfterNotifyingLoadedFromMemoryCache(request, resource.get()))
                return nullptr;
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::usedKey());
            memoryCache.resourceAccessed(*resource);
#if ENABLE(WEB_TIMING)
            loadTiming.setResponseEnd(MonotonicTime::now());

            if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled()) {
                ResourceTiming resourceTiming = ResourceTiming::fromCache(url, request.initiatorName(), loadTiming);
                if (initiatorContext == InitiatorContext::Worker) {
                    ASSERT(is<CachedRawResource>(resource.get()));
                    downcast<CachedRawResource>(resource.get())->finishedTimingForWorkerLoad(WTFMove(resourceTiming));
                } else if (document()) {
                    ASSERT(initiatorContext == InitiatorContext::Document);
                    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, request.initiatorName(), frame());
                    m_resourceTimingInfo.addResourceTiming(*resource.get(), *document(), WTFMove(resourceTiming));
                }
            }
#endif
            if (forPreload == ForPreload::No)
                resource->setLoadPriority(request.priority());
        }
        break;
    }

    if (!resource)
        return nullptr;

    if (forPreload == ForPreload::No && resource->loader() && resource->resourceRequest().ignoreForRequestCount()) {
        resource->resourceRequest().setIgnoreForRequestCount(false);
        incrementRequestCount(*resource);
    }

    if ((policy != Use || resource->stillNeedsLoad()) && defer == DeferOption::NoDefer) {
        resource->load(*this);

        // We don't support immediate loads, but we do support immediate failure.
        if (resource->errorOccurred()) {
            if (resource->allowsCaching() && resource->inCache())
                memoryCache.remove(*resource);
            return nullptr;
        }
    }

    if (document() && !document()->loadEventFinished() && !resource->resourceRequest().url().protocolIsData())
        m_validatedURLs.add(resource->resourceRequest().url());

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url(), resource);
    return resource;
}

void CachedResourceLoader::documentDidFinishLoadEvent()
{
    m_validatedURLs.clear();
}

CachedResourceHandle<CachedResource> CachedResourceLoader::revalidateResource(CachedResourceRequest&& request, CachedResource& resource)
{
    ASSERT(resource.inCache());
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!memoryCache.disabled());
    ASSERT(resource.canUseCacheValidator());
    ASSERT(!resource.resourceToRevalidate());
    ASSERT(resource.sessionID() == sessionID());
    ASSERT(resource.allowsCaching());

    CachedResourceHandle<CachedResource> newResource = createResource(resource.type(), WTFMove(request), resource.sessionID());

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), &resource);
    newResource->setResourceToRevalidate(&resource);

    memoryCache.remove(resource);
    memoryCache.add(*newResource);
#if ENABLE(WEB_TIMING)
    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        m_resourceTimingInfo.storeResourceTimingInitiatorInformation(newResource, newResource->initiatorName(), frame());
#endif
    return newResource;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::loadResource(CachedResource::Type type, CachedResourceRequest&& request)
{
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!request.allowsCaching() || !memoryCache.resourceForRequest(request.resourceRequest(), sessionID())
        || request.resourceRequest().cachePolicy() == DoNotUseAnyCache || request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData || request.resourceRequest().cachePolicy() == RefreshAnyCacheData);

    LOG(ResourceLoading, "Loading CachedResource for '%s'.", request.resourceRequest().url().stringCenterEllipsizedToLength().latin1().data());

    CachedResourceHandle<CachedResource> resource = createResource(type, WTFMove(request), sessionID());

    if (resource->allowsCaching() && !memoryCache.add(*resource))
        resource->setOwningCachedResourceLoader(this);
#if ENABLE(WEB_TIMING)
    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, resource->initiatorName(), frame());
#endif
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

CachedResourceLoader::RevalidationPolicy CachedResourceLoader::determineRevalidationPolicy(CachedResource::Type type, CachedResourceRequest& cachedResourceRequest, CachedResource* existingResource, ForPreload forPreload, DeferOption defer) const
{
    auto& request = cachedResourceRequest.resourceRequest();

    if (!existingResource)
        return Load;

    if (request.cachePolicy() == DoNotUseAnyCache || request.cachePolicy() == ReloadIgnoringCacheData)
        return Load;

    if (request.cachePolicy() == RefreshAnyCacheData)
        return Reload;

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
    if (textDecoder && !textDecoder->hasEqualEncodingForCharset(cachedResourceRequest.charset()))
        return Reload;

    // FIXME: We should use the same cache policy for all resource types. The raw resource policy is overly strict
    //        while the normal subresource policy is too loose.
    if (existingResource->isMainOrMediaOrRawResource() && frame()) {
        bool strictPolicyDisabled = frame()->loader().isStrictRawResourceValidationPolicyDisabledForTesting();
        bool canReuseRawResource = strictPolicyDisabled || downcast<CachedRawResource>(*existingResource).canReuse(request);
        if (!canReuseRawResource)
            return Reload;
    }

    // Conditional requests should have failed canReuse check.
    ASSERT(!request.isConditional());

    // Do not load from cache if images are not enabled. The load for this image will be blocked in CachedImage::load.
    if (defer == DeferOption::DeferredByClient)
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

    // Validate the redirect chain.
    bool cachePolicyIsHistoryBuffer = cachePolicy(type) == CachePolicyHistoryBuffer;
    if (!existingResource->redirectChainAllowsReuse(cachePolicyIsHistoryBuffer ? ReuseExpiredRedirection : DoNotReuseExpiredRedirection)) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to not cached or expired redirections.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonRedirectChainKey());
        return Reload;
    }

    // CachePolicyHistoryBuffer uses the cache except if this is a main resource with "cache-control: no-store".
    if (cachePolicyIsHistoryBuffer) {
        // FIXME: Ignoring "cache-control: no-cache" for sub-resources on history navigation but not the main
        // resource is inconsistent. We should probably harmonize this.
        if (!existingResource->response().cacheControlContainsNoStore() || type != CachedResource::MainResource)
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
    if (existingResource->resourceRequest().allowCookies() != request.allowCookies()) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to difference in credentials settings.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonCredentialSettingsKey());
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document, even if the cache policies would tell us to.
    if (document() && !document()->loadEventFinished() && m_validatedURLs.contains(existingResource->url()))
        return Use;

    // CachePolicyReload always reloads
    if (cachePolicy(type) == CachePolicyReload) {
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

    auto revalidationDecision = existingResource->makeRevalidationDecision(cachePolicy(type));
    logResourceRevalidationDecision(revalidationDecision, frame());

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (revalidationDecision != CachedResource::RevalidationDecision::No) {
        // See if the resource has usable ETag or Last-modified headers.
        if (existingResource->canUseCacheValidator())
            return Revalidate;
        
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
        message = "Unsafe attempt to load URL " + url.stringCenterEllipsizedToLength() + '.';
    else
        message = "Unsafe attempt to load URL " + url.stringCenterEllipsizedToLength() + " from frame with URL " + m_document->url().stringCenterEllipsizedToLength() + ". Domains, protocols and ports must match.\n";

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

bool CachedResourceLoader::clientDefersImage(const URL&) const
{
    return !m_imagesEnabled;
}

bool CachedResourceLoader::shouldPerformImageLoad(const URL& url) const
{
    return m_autoLoadImages || url.protocolIsData();
}

bool CachedResourceLoader::shouldDeferImageLoad(const URL& url) const
{
    return clientDefersImage(url) || !shouldPerformImageLoad(url);
}

void CachedResourceLoader::reloadImagesIfNotDeferred()
{
    for (auto& resource : m_documentResources.values()) {
        if (is<CachedImage>(*resource) && resource->stillNeedsLoad() && !clientDefersImage(resource->url()))
            downcast<CachedImage>(*resource).load(*this);
    }
}

CachePolicy CachedResourceLoader::cachePolicy(CachedResource::Type type) const
{
    Frame* frame = this->frame();
    if (!frame)
        return CachePolicyVerify;

    if (type != CachedResource::MainResource)
        return frame->loader().subresourceCachePolicy();

    if (Page* page = frame->page()) {
        if (page->isResourceCachingDisabled())
            return CachePolicyReload;
    }

    switch (frame->loader().loadType()) {
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::Reload:
        return CachePolicyReload;
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
        // Do not revalidate cached main resource on back/forward navigation.
        return CachePolicyHistoryBuffer;
    default:
        return CachePolicyVerify;
    }
}

void CachedResourceLoader::removeCachedResource(CachedResource& resource)
{
#ifndef NDEBUG
    DocumentResourceMap::iterator it = m_documentResources.find(resource.url());
    if (it != m_documentResources.end())
        ASSERT(it->value.get() == &resource);
#endif
    m_documentResources.remove(resource.url());
}

void CachedResourceLoader::loadDone(bool shouldPerformPostLoadActions)
{
    RefPtr<DocumentLoader> protectDocumentLoader(m_documentLoader);
    RefPtr<Document> protectDocument(m_document);

    if (frame())
        frame()->loader().loadDone();

    if (shouldPerformPostLoadActions)
        performPostLoadActions();

    if (!m_garbageCollectDocumentResourcesTimer.isActive())
        m_garbageCollectDocumentResourcesTimer.startOneShot(0);
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

    for (auto& resource : m_documentResources) {
        if (resource.value->hasOneHandle()) {
            resourcesToDelete.append(resource.key);
            resource.value->setOwningCachedResourceLoader(nullptr);
        }
    }

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

CachedResourceHandle<CachedResource> CachedResourceLoader::preload(CachedResource::Type type, CachedResourceRequest&& request)
{
    if (request.charset().isEmpty() && (type == CachedResource::Script || type == CachedResource::CSSStyleSheet))
        request.setCharset(m_document->charset());

    CachedResourceHandle<CachedResource> resource = requestResource(type, WTFMove(request), ForPreload::Yes);
    if (!resource || (m_preloads && m_preloads->contains(resource.get())))
        return nullptr;
    // Fonts need special treatment since just creating the resource doesn't trigger a load.
    if (type == CachedResource::FontResource)
        downcast<CachedFont>(resource.get())->beginLoadIfNeeded(*this);
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = std::make_unique<ListHashSet<CachedResource*>>();
    m_preloads->add(resource.get());

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().latin1().data());
#endif
    return resource;
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
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    if (!m_preloads)
        return;

    std::unique_ptr<ListHashSet<CachedResource*>> remainingLinkPreloads;
    for (auto* resource : *m_preloads) {
        ASSERT(resource);
        if (mode == ClearPreloadsMode::ClearSpeculativePreloads && resource->isLinkPreload()) {
            if (!remainingLinkPreloads)
                remainingLinkPreloads = std::make_unique<ListHashSet<CachedResource*>>();
            remainingLinkPreloads->add(resource);
            continue;
        }
        resource->decreasePreloadCount();
        bool deleted = resource->deleteIfPossible();
        if (!deleted && resource->preloadResult() == CachedResource::PreloadNotReferenced)
            MemoryCache::singleton().remove(*resource);
    }
    m_preloads = WTFMove(remainingLinkPreloads);
}

#if PRELOAD_DEBUG
void CachedResourceLoader::printPreloadStats()
{
    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    for (auto& resource : m_preloads) {
        if (resource->preloadResult() == CachedResource::PreloadNotReferenced)
            printf("!! UNREFERENCED PRELOAD %s\n", resource->url().latin1().data());
        else if (resource->preloadResult() == CachedResource::PreloadReferencedWhileComplete)
            printf("HIT COMPLETE PRELOAD %s\n", resource->url().latin1().data());
        else if (resource->preloadResult() == CachedResource::PreloadReferencedWhileLoading)
            printf("HIT LOADING PRELOAD %s\n", resource->url().latin1().data());

        if (resource->type() == CachedResource::Script) {
            scripts++;
            if (resource->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                scriptMisses++;
        } else if (resource->type() == CachedResource::CSSStyleSheet) {
            stylesheets++;
            if (resource->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                stylesheetMisses++;
        } else {
            images++;
            if (resource->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                imageMisses++;
        }

        if (resource->errorOccurred() && resource->preloadResult() == CachedResource::PreloadNotReferenced)
            MemoryCache::singleton().remove(resource);

        resource->decreasePreloadCount();
    }
    m_preloads = nullptr;

    if (scripts)
        printf("SCRIPTS: %d (%d hits, hit rate %d%%)\n", scripts, scripts - scriptMisses, (scripts - scriptMisses) * 100 / scripts);
    if (stylesheets)
        printf("STYLESHEETS: %d (%d hits, hit rate %d%%)\n", stylesheets, stylesheets - stylesheetMisses, (stylesheets - stylesheetMisses) * 100 / stylesheets);
    if (images)
        printf("IMAGES:  %d (%d hits, hit rate %d%%)\n", images, images - imageMisses, (images - imageMisses) * 100 / images);
}
#endif

const ResourceLoaderOptions& CachedResourceLoader::defaultCachedResourceOptions()
{
    static NeverDestroyed<ResourceLoaderOptions> options(SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, ClientCredentialPolicy::MayAskClientForCredentials, FetchOptions::Credentials::Include, DoSecurityCheck, FetchOptions::Mode::NoCors, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, CachingPolicy::AllowCaching);
    return options;
}

bool CachedResourceLoader::isAlwaysOnLoggingAllowed() const
{
    return m_documentLoader ? m_documentLoader->isAlwaysOnLoggingAllowed() : true;
}

}
