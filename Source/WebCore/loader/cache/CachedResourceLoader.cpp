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
#include "CrossOriginAccessControl.h"
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
#include "HTTPHeaderField.h"
#include "LoaderStrategy.h"
#include "LocalizedStrings.h"
#include "Logging.h"
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

#if ENABLE(VIDEO_TRACK)
#include "CachedTextTrack.h"
#endif

#undef RELEASE_LOG_IF_ALLOWED
#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - CachedResourceLoader::" fmt, this, ##__VA_ARGS__)

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

static CachedResource* createResource(CachedResource::Type type, CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return new CachedImage(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::CSSStyleSheet:
        return new CachedCSSStyleSheet(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::Script:
        return new CachedScript(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::SVGDocumentResource:
        return new CachedSVGDocument(WTFMove(request), sessionID, cookieJar);
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
        return new CachedSVGFont(WTFMove(request), sessionID, cookieJar);
#endif
    case CachedResource::Type::FontResource:
        return new CachedFont(WTFMove(request), sessionID, cookieJar);
    case CachedResource::Type::Beacon:
    case CachedResource::Type::MediaResource:
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
#if ENABLE(VIDEO_TRACK)
    case CachedResource::Type::TextTrackResource:
        return new CachedTextTrack(WTFMove(request), sessionID, cookieJar);
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return new CachedApplicationManifest(WTFMove(request), sessionID, cookieJar);
#endif
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

CachedResourceLoader::CachedResourceLoader(DocumentLoader* documentLoader)
    : m_document(nullptr)
    , m_documentLoader(documentLoader)
    , m_requestCount(0)
    , m_unusedPreloadsTimer(*this, &CachedResourceLoader::warnUnusedPreloads)
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
    return m_documentResources.get(url).get();
}

Frame* CachedResourceLoader::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : nullptr;
}

PAL::SessionID CachedResourceLoader::sessionID() const
{
    auto sessionID = PAL::SessionID::defaultSessionID();
    if (auto* frame = this->frame()) {
        if (auto* page = frame->page())
            sessionID = page->sessionID();
    }
    return sessionID;
}

ResourceErrorOr<CachedResourceHandle<CachedImage>> CachedResourceLoader::requestImage(CachedResourceRequest&& request)
{
    if (Frame* frame = this->frame()) {
        if (frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None) {
            if (Document* document = frame->document())
                request.upgradeInsecureRequestIfNeeded(*document);
            URL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(CachedResource::Type::ImageResource, requestURL, request, ForPreload::No))
                PingLoader::loadImage(*frame, requestURL);
            return CachedResourceHandle<CachedImage> { };
        }
    }

    auto defer = clientDefersImage(request.resourceRequest().url()) ? DeferOption::DeferredByClient : DeferOption::NoDefer;
    return castCachedResourceTo<CachedImage>(requestResource(CachedResource::Type::ImageResource, WTFMove(request), ForPreload::No, defer));
}

ResourceErrorOr<CachedResourceHandle<CachedFont>> CachedResourceLoader::requestFont(CachedResourceRequest&& request, bool isSVG)
{
#if ENABLE(SVG_FONTS)
    if (isSVG)
        return castCachedResourceTo<CachedFont>(requestResource(CachedResource::Type::SVGFontResource, WTFMove(request)));
#else
    UNUSED_PARAM(isSVG);
#endif
    return castCachedResourceTo<CachedFont>(requestResource(CachedResource::Type::FontResource, WTFMove(request)));
}

#if ENABLE(VIDEO_TRACK)
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
        if (CachedResource* existing = memoryCache.resourceForRequest(request.resourceRequest(), sessionID())) {
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
    // FIXME: Assert request.options().destination is FetchOptions::Destination::{Audio, Video}.
    return castCachedResourceTo<CachedRawResource>(requestResource(CachedResource::Type::MediaResource, WTFMove(request)));
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

static MixedContentChecker::ContentType contentTypeFromResourceType(CachedResource::Type type)
{
    switch (type) {
    // https://w3c.github.io/webappsec-mixed-content/#category-optionally-blockable
    // Editor's Draft, 11 February 2016
    // 3.1. Optionally-blockable Content
    case CachedResource::Type::ImageResource:
    case CachedResource::Type::MediaResource:
            return MixedContentChecker::ContentType::ActiveCanWarn;

    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::Script:
    case CachedResource::Type::FontResource:
        return MixedContentChecker::ContentType::Active;

#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
        return MixedContentChecker::ContentType::Active;
#endif

    case CachedResource::Type::Beacon:
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

#if ENABLE(VIDEO_TRACK)
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
            if (!frame->loader().mixedContentChecker().canRunInsecureContent(m_document->securityOrigin(), url))
                return false;
            Frame& top = frame->tree().top();
            if (&top != frame && !top.loader().mixedContentChecker().canRunInsecureContent(top.document()->securityOrigin(), url))
                return false;
        }
        break;
#if ENABLE(VIDEO_TRACK)
    case CachedResource::Type::TextTrackResource:
#endif
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
    case CachedResource::Type::FontResource: {
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
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Beacon:
    case CachedResource::Type::LinkPrefetch:
        // Prefetch cannot affect the current document.
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
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
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::Script:
        if (!m_document->contentSecurityPolicy()->allowScriptFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::Type::CSSStyleSheet:
        if (!m_document->contentSecurityPolicy()->allowStyleFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::ImageResource:
        if (!m_document->contentSecurityPolicy()->allowImageFromSource(url, redirectResponseReceived))
            return false;
        break;
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
    case CachedResource::Type::FontResource:
        if (!m_document->contentSecurityPolicy()->allowFontFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::Type::MediaResource:
#if ENABLE(VIDEO_TRACK)
    case CachedResource::Type::TextTrackResource:
#endif
        if (!m_document->contentSecurityPolicy()->allowMediaFromSource(url, redirectResponseReceived))
            return false;
        break;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::RawResource:
        return true;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        if (!m_document->contentSecurityPolicy()->allowManifestFromSource(url, redirectResponseReceived))
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

// Security checks defined in https://fetch.spec.whatwg.org/#main-fetch step 2 and 5.
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

    if (options.mode == FetchOptions::Mode::NoCors && options.redirect != FetchOptions::Redirect::Follow) {
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
    case CachedResource::Type::MainResource:
        if (auto ownerElement = frame() ? frame()->ownerElement() : nullptr) {
            document = &ownerElement->document();
            break;
        }
        return true;
    case CachedResource::Type::CSSStyleSheet:
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

bool CachedResourceLoader::shouldContinueAfterNotifyingLoadedFromMemoryCache(const CachedResourceRequest& request, CachedResource& resource, ResourceError& error)
{
    if (!frame() || resource.status() != CachedResource::Cached)
        return true;

    ResourceRequest newRequest = ResourceRequest(resource.url());
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

#if ENABLE(SVG_FONTS)
    if (resource.type() == CachedResource::Type::SVGFontResource)
        return false;
#endif

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

    if (resource.type() == CachedResource::Type::Beacon)
        return false;

    return true;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::updateCachedResourceWithCurrentRequest(const CachedResource& resource, CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
{
    if (!isResourceSuitableForDirectReuse(resource, request)) {
        request.setCachingPolicy(CachingPolicy::DisallowCaching);
        return loadResource(resource.type(), WTFMove(request), cookieJar);
    }

    auto resourceHandle = createResource(resource.type(), WTFMove(request), sessionID, cookieJar);
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
    auto* document = this->document();

    if (document) {
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

void CachedResourceLoader::updateHTTPRequestHeaders(CachedResource::Type type, CachedResourceRequest& request)
{
    // Implementing steps 7 to 12 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch

    // FIXME: We should reconcile handling of MainResource with other resources.
    if (type != CachedResource::Type::MainResource) {
        // In some cases we may try to load resources in frameless documents. Such loads always fail.
        // FIXME: We shouldn't need to do the check on frame.
        if (auto* frame = this->frame())
            request.updateReferrerOriginAndUserAgentHeaders(frame->loader());
    }

    request.updateAccordingCacheMode();
    request.updateAcceptEncodingHeader();
}

static FetchOptions::Destination destinationForType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::MainResource:
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
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
        return FetchOptions::Destination::Font;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
        return FetchOptions::Destination::Xslt;
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::Type::TextTrackResource:
        return FetchOptions::Destination::Track;
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return FetchOptions::Destination::Manifest;
#endif
    case CachedResource::Type::Beacon:
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::MediaResource:
        // The caller is responsible for setting the appropriate destination.
        return FetchOptions::Destination::EmptyString;
    }
    ASSERT_NOT_REACHED();
    return FetchOptions::Destination::EmptyString;
}

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::requestResource(CachedResource::Type type, CachedResourceRequest&& request, ForPreload forPreload, DeferOption defer)
{
    request.setDestinationIfNotSet(destinationForType(type));

    // Entry point to https://fetch.spec.whatwg.org/#main-fetch.
    std::unique_ptr<ResourceRequest> originalRequest;
    if (CachedResource::shouldUsePingLoad(type) || request.options().destination == FetchOptions::Destination::EmptyString) {
        originalRequest = std::make_unique<ResourceRequest>(request.resourceRequest());
        originalRequest->clearHTTPReferrer();
        originalRequest->clearHTTPOrigin();
    }

    if (Document* document = this->document())
        request.upgradeInsecureRequestIfNeeded(*document);

    request.updateReferrerPolicy(document() ? document()->referrerPolicy() : ReferrerPolicy::NoReferrerWhenDowngrade);
    URL url = request.resourceRequest().url();

    LOG(ResourceLoading, "CachedResourceLoader::requestResource '%.255s', charset '%s', priority=%d, forPreload=%u", url.stringCenterEllipsizedToLength().latin1().data(), request.charset().latin1().data(), request.priority() ? static_cast<int>(request.priority().value()) : -1, forPreload == ForPreload::Yes);

    if (!url.isValid()) {
        RELEASE_LOG_IF_ALLOWED("requestResource: URL is invalid (frame = %p)", frame());
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "URL is invalid"_s });
    }

    prepareFetch(type, request);

    // We are passing url as well as request, as request url may contain a fragment identifier.
    if (!canRequest(type, url, request, forPreload)) {
        RELEASE_LOG_IF_ALLOWED("requestResource: Not allowed to request resource (frame = %p)", frame());
        return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Not allowed to request resource"_s, ResourceError::Type::AccessControl });
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (frame() && frame()->page() && m_documentLoader) {
        const auto& resourceRequest = request.resourceRequest();
        auto* page = frame()->page();
        auto blockedStatus = page->userContentProvider().processContentExtensionRulesForLoad(resourceRequest.url(), toResourceType(type), *m_documentLoader);
        request.applyBlockedStatus(blockedStatus, page);
        if (blockedStatus.blockedLoad) {
            RELEASE_LOG_IF_ALLOWED("requestResource: Resource blocked by content blocker (frame = %p)", frame());
            if (type == CachedResource::Type::MainResource) {
                CachedResourceHandle<CachedResource> resource = createResource(type, WTFMove(request), page->sessionID(), &page->cookieJar());
                ASSERT(resource);
                resource->error(CachedResource::Status::LoadError);
                resource->setResourceError(ResourceError(ContentExtensions::WebKitContentBlockerDomain, 0, resourceRequest.url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description")));
                return WTFMove(resource);
            }
            return makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, url, "Resource blocked by content blocker"_s, ResourceError::Type::AccessControl });
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

    if (frame() && m_documentLoader && !m_documentLoader->customHeaderFields().isEmpty()) {
        bool sameOriginRequest = false;
        auto requestedOrigin = SecurityOrigin::create(url);
        if (type == CachedResource::Type::MainResource) {
            if (frame()->isMainFrame())
                sameOriginRequest = true;
            else if (auto* topDocument = frame()->mainFrame().document())
                sameOriginRequest = topDocument->securityOrigin().isSameSchemeHostPort(requestedOrigin.get());
        } else if (document()) {
            sameOriginRequest = document()->topDocument().securityOrigin().isSameSchemeHostPort(requestedOrigin.get())
                && document()->securityOrigin().isSameSchemeHostPort(requestedOrigin.get());
        }
        if (sameOriginRequest) {
            for (auto& field : m_documentLoader->customHeaderFields())
                request.resourceRequest().setHTTPHeaderField(field.name(), field.value());
        }
    }

    LoadTiming loadTiming;
    loadTiming.markStartTimeAndFetchStart();
    InitiatorContext initiatorContext = request.options().initiatorContext;

    if (request.resourceRequest().url().protocolIsInHTTPFamily())
        updateHTTPRequestHeaders(type, request);

    auto& memoryCache = MemoryCache::singleton();
    if (request.allowsCaching() && memoryCache.disabled())
        m_documentResources.remove(url.string());

    // See if we can use an existing resource from the cache.
    CachedResourceHandle<CachedResource> resource;
    if (document())
        request.setDomainForCachePartition(*document());

    if (request.allowsCaching())
        resource = memoryCache.resourceForRequest(request.resourceRequest(), sessionID());

    if (resource && request.isLinkPreload() && !resource->isLinkPreload())
        resource->setLinkPreload();

    logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheUsageKey(), resource ? DiagnosticLoggingKeys::inMemoryCacheKey() : DiagnosticLoggingKeys::notInMemoryCacheKey());

    auto* cookieJar = document() && document()->page() ? &document()->page()->cookieJar() : nullptr;

    RevalidationPolicy policy = determineRevalidationPolicy(type, request, resource.get(), forPreload, defer);
    switch (policy) {
    case Reload:
        memoryCache.remove(*resource);
        FALLTHROUGH;
    case Load:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::unusedKey());
        resource = loadResource(type, WTFMove(request), cookieJar);
        break;
    case Revalidate:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::revalidatingKey());
        resource = revalidateResource(WTFMove(request), *resource);
        break;
    case Use:
        ASSERT(resource);
        if (request.options().mode == FetchOptions::Mode::NoCors) {
            if (auto error = validateCrossOriginResourcePolicy(*request.origin(), request.resourceRequest().url(), resource->response()))
                return makeUnexpected(WTFMove(*error));
        }
        if (shouldUpdateCachedResourceWithCurrentRequest(*resource, request)) {
            resource = updateCachedResourceWithCurrentRequest(*resource, WTFMove(request), document()->page()->sessionID(), cookieJar);
            if (resource->status() != CachedResource::Status::Cached)
                policy = Load;
        } else {
            ResourceError error;
            if (!shouldContinueAfterNotifyingLoadedFromMemoryCache(request, *resource, error))
                return makeUnexpected(WTFMove(error));
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::memoryCacheEntryDecisionKey(), DiagnosticLoggingKeys::usedKey());
            loadTiming.setResponseEnd(MonotonicTime::now());

            memoryCache.resourceAccessed(*resource);

            if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled() && document() && !resource->isLoading()) {
                auto resourceTiming = ResourceTiming::fromCache(url, request.initiatorName(), loadTiming, resource->response(), *request.origin());
                if (initiatorContext == InitiatorContext::Worker) {
                    ASSERT(is<CachedRawResource>(resource.get()));
                    downcast<CachedRawResource>(resource.get())->finishedTimingForWorkerLoad(WTFMove(resourceTiming));
                } else {
                    ASSERT(initiatorContext == InitiatorContext::Document);
                    m_resourceTimingInfo.storeResourceTimingInitiatorInformation(resource, request.initiatorName(), frame());
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

    if (forPreload == ForPreload::No && resource->loader() && resource->ignoreForRequestCount()) {
        resource->setIgnoreForRequestCount(false);
        incrementRequestCount(*resource);
    }

    if ((policy != Use || resource->stillNeedsLoad()) && defer == DeferOption::NoDefer) {
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
    m_documentResources.set(resource->url(), resource);
    return WTFMove(resource);
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
    ASSERT(resource.sessionID() == sessionID());
    ASSERT(resource.allowsCaching());

    CachedResourceHandle<CachedResource> newResource = createResource(resource.type(), WTFMove(request), resource.sessionID(), resource.cookieJar());

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), &resource);
    newResource->setResourceToRevalidate(&resource);

    memoryCache.remove(resource);
    memoryCache.add(*newResource);

    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        m_resourceTimingInfo.storeResourceTimingInitiatorInformation(newResource, newResource->initiatorName(), frame());

    return newResource;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::loadResource(CachedResource::Type type, CachedResourceRequest&& request, const CookieJar* cookieJar)
{
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!request.allowsCaching() || !memoryCache.resourceForRequest(request.resourceRequest(), sessionID())
        || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData || request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData);

    LOG(ResourceLoading, "Loading CachedResource for '%s'.", request.resourceRequest().url().stringCenterEllipsizedToLength().latin1().data());

    CachedResourceHandle<CachedResource> resource = createResource(type, WTFMove(request), sessionID(), cookieJar);

    if (resource->allowsCaching())
        memoryCache.add(*resource);

    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
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

CachedResourceLoader::RevalidationPolicy CachedResourceLoader::determineRevalidationPolicy(CachedResource::Type type, CachedResourceRequest& cachedResourceRequest, CachedResource* existingResource, ForPreload forPreload, DeferOption defer) const
{
    auto& request = cachedResourceRequest.resourceRequest();

    if (!existingResource)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::DoNotUseAnyCache || request.cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData)
        return Load;

    if (request.cachePolicy() == ResourceRequestCachePolicy::RefreshAnyCacheData)
        return Reload;

#if ENABLE(SERVICE_WORKER)
    // FIXME: We should validate/specify this behavior.
    if (cachedResourceRequest.options().serviceWorkerRegistrationIdentifier != existingResource->options().serviceWorkerRegistrationIdentifier) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading because selected service worker differs");
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

    auto cachePolicy = this->cachePolicy(type, request.url());

    // Validate the redirect chain.
    bool cachePolicyIsHistoryBuffer = cachePolicy == CachePolicyHistoryBuffer;
    if (!existingResource->redirectChainAllowsReuse(cachePolicyIsHistoryBuffer ? ReuseExpiredRedirection : DoNotReuseExpiredRedirection)) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to not cached or expired redirections.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonRedirectChainKey());
        return Reload;
    }

    // CachePolicyHistoryBuffer uses the cache except if this is a main resource with "cache-control: no-store".
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

    // CachePolicyReload always reloads
    if (cachePolicy == CachePolicyReload) {
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
        message = makeString("Unsafe attempt to load URL ", url.stringCenterEllipsizedToLength(), '.');
    else
        message = makeString("Unsafe attempt to load URL ", url.stringCenterEllipsizedToLength(), " from origin ", m_document->origin(), ". Domains, protocols and ports must match.\n");

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

CachePolicy CachedResourceLoader::cachePolicy(CachedResource::Type type, const URL& url) const
{
    Frame* frame = this->frame();
    if (!frame)
        return CachePolicyVerify;

    if (type != CachedResource::Type::MainResource)
        return frame->loader().subresourceCachePolicy(url);

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

void CachedResourceLoader::loadDone(LoadCompletionType type, bool shouldPerformPostLoadActions)
{
    RefPtr<DocumentLoader> protectDocumentLoader(m_documentLoader);
    RefPtr<Document> protectDocument(m_document);

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
    LOG(ResourceLoading, "CachedResourceLoader %p garbageCollectDocumentResources", this);

    typedef Vector<String, 10> StringVector;
    StringVector resourcesToDelete;

    for (auto& resource : m_documentResources) {
        LOG(ResourceLoading, "  cached resource %p - hasOneHandle %d", resource.value.get(), resource.value->hasOneHandle());

        if (resource.value->hasOneHandle())
            resourcesToDelete.append(resource.key);
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

ResourceErrorOr<CachedResourceHandle<CachedResource>> CachedResourceLoader::preload(CachedResource::Type type, CachedResourceRequest&& request)
{
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
            m_preloads = std::make_unique<ListHashSet<CachedResource*>>();
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
                remainingLinkPreloads = std::make_unique<ListHashSet<CachedResource*>>();
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

bool CachedResourceLoader::isAlwaysOnLoggingAllowed() const
{
    return m_documentLoader ? m_documentLoader->isAlwaysOnLoggingAllowed() : true;
}

}
