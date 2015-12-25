/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "CachedSVGDocument.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedRawResource.h"
#include "CachedResourceRequest.h"
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
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SessionID.h"
#include "Settings.h"
#include "StyleSheetContents.h"
#include "UserContentController.h"
#include "UserStyleSheet.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(VIDEO_TRACK)
#include "CachedTextTrack.h"
#endif

#if ENABLE(RESOURCE_TIMING)
#include "Performance.h"
#endif

#define PRELOAD_DEBUG 0

namespace WebCore {

static CachedResource* createResource(CachedResource::Type type, ResourceRequest& request, const String& charset, SessionID sessionID)
{
    switch (type) {
    case CachedResource::ImageResource:
        return new CachedImage(request, sessionID);
    case CachedResource::CSSStyleSheet:
        return new CachedCSSStyleSheet(request, charset, sessionID);
    case CachedResource::Script:
        return new CachedScript(request, charset, sessionID);
    case CachedResource::SVGDocumentResource:
        return new CachedSVGDocument(request, sessionID);
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
        return new CachedSVGFont(request, sessionID);
#endif
    case CachedResource::FontResource:
        return new CachedFont(request, sessionID);
    case CachedResource::RawResource:
    case CachedResource::MainResource:
        return new CachedRawResource(request, type, sessionID);
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return new CachedXSLStyleSheet(request, sessionID);
#endif
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return new CachedResource(request, CachedResource::LinkPrefetch, sessionID);
    case CachedResource::LinkSubresource:
        return new CachedResource(request, CachedResource::LinkSubresource, sessionID);
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
        return new CachedTextTrack(request, sessionID);
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

    clearPreloads();
    for (auto& resource : m_documentResources.values())
        resource->setOwningCachedResourceLoader(nullptr);

    // Make sure no requests still point to this CachedResourceLoader
    ASSERT(m_requestCount == 0);
}

CachedResource* CachedResourceLoader::cachedResource(const String& resourceURL) const 
{
    ASSERT(!resourceURL.isNull());
    URL url = m_document->completeURL(resourceURL);
    return cachedResource(url); 
}

CachedResource* CachedResourceLoader::cachedResource(const URL& resourceURL) const
{
    URL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
    return m_documentResources.get(url).get(); 
}

Frame* CachedResourceLoader::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : 0;
}

SessionID CachedResourceLoader::sessionID() const
{
    SessionID sessionID = SessionID::defaultSessionID();

    if (Frame* f = frame())
        sessionID = f->page()->sessionID();

    return sessionID;
}

CachedResourceHandle<CachedImage> CachedResourceLoader::requestImage(CachedResourceRequest& request)
{
    if (Frame* frame = this->frame()) {
        if (frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None) {
            URL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(CachedResource::ImageResource, requestURL, request.options(), request.forPreload()))
                PingLoader::loadImage(*frame, requestURL);
            return nullptr;
        }
    }
    
    request.setDefer(clientDefersImage(request.resourceRequest().url()) ? CachedResourceRequest::DeferredByClient : CachedResourceRequest::NoDefer);
    return downcast<CachedImage>(requestResource(CachedResource::ImageResource, request).get());
}

CachedResourceHandle<CachedFont> CachedResourceLoader::requestFont(CachedResourceRequest& request, bool isSVG)
{
#if ENABLE(SVG_FONTS)
    if (isSVG)
        return downcast<CachedSVGFont>(requestResource(CachedResource::SVGFontResource, request).get());
#else
    UNUSED_PARAM(isSVG);
#endif
    return downcast<CachedFont>(requestResource(CachedResource::FontResource, request).get());
}

#if ENABLE(VIDEO_TRACK)
CachedResourceHandle<CachedTextTrack> CachedResourceLoader::requestTextTrack(CachedResourceRequest& request)
{
    return downcast<CachedTextTrack>(requestResource(CachedResource::TextTrackResource, request).get());
}
#endif

CachedResourceHandle<CachedCSSStyleSheet> CachedResourceLoader::requestCSSStyleSheet(CachedResourceRequest& request)
{
    return downcast<CachedCSSStyleSheet>(requestResource(CachedResource::CSSStyleSheet, request).get());
}

CachedResourceHandle<CachedCSSStyleSheet> CachedResourceLoader::requestUserCSSStyleSheet(CachedResourceRequest& request)
{
    URL url = MemoryCache::removeFragmentIdentifierIfNeeded(request.resourceRequest().url());

#if ENABLE(CACHE_PARTITIONING)
    request.mutableResourceRequest().setDomainForCachePartition(document()->topOrigin()->domainForCachePartition());
#endif

    auto& memoryCache = MemoryCache::singleton();
    if (CachedResource* existing = memoryCache.resourceForRequest(request.resourceRequest(), sessionID())) {
        if (is<CachedCSSStyleSheet>(*existing))
            return downcast<CachedCSSStyleSheet>(existing);
        memoryCache.remove(*existing);
    }
    if (url.string() != request.resourceRequest().url())
        request.mutableResourceRequest().setURL(url);

    CachedResourceHandle<CachedCSSStyleSheet> userSheet = new CachedCSSStyleSheet(request.resourceRequest(), request.charset(), sessionID());

    memoryCache.add(*userSheet);
    // FIXME: loadResource calls setOwningCachedResourceLoader() if the resource couldn't be added to cache. Does this function need to call it, too?

    userSheet->load(*this, ResourceLoaderOptions(DoNotSendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForAllCredentials, SkipSecurityCheck, UseDefaultOriginRestrictionsForType, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::SkipPolicyCheck, DefersLoadingPolicy::AllowDefersLoading));
    
    return userSheet;
}

CachedResourceHandle<CachedScript> CachedResourceLoader::requestScript(CachedResourceRequest& request)
{
    return downcast<CachedScript>(requestResource(CachedResource::Script, request).get());
}

#if ENABLE(XSLT)
CachedResourceHandle<CachedXSLStyleSheet> CachedResourceLoader::requestXSLStyleSheet(CachedResourceRequest& request)
{
    return downcast<CachedXSLStyleSheet>(requestResource(CachedResource::XSLStyleSheet, request).get());
}
#endif

CachedResourceHandle<CachedSVGDocument> CachedResourceLoader::requestSVGDocument(CachedResourceRequest& request)
{
    return downcast<CachedSVGDocument>(requestResource(CachedResource::SVGDocumentResource, request).get());
}

#if ENABLE(LINK_PREFETCH)
CachedResourceHandle<CachedResource> CachedResourceLoader::requestLinkResource(CachedResource::Type type, CachedResourceRequest& request)
{
    ASSERT(frame());
    ASSERT(type == CachedResource::LinkPrefetch || type == CachedResource::LinkSubresource);
    return requestResource(type, request);
}
#endif

CachedResourceHandle<CachedRawResource> CachedResourceLoader::requestRawResource(CachedResourceRequest& request)
{
    return downcast<CachedRawResource>(requestResource(CachedResource::RawResource, request).get());
}

CachedResourceHandle<CachedRawResource> CachedResourceLoader::requestMainResource(CachedResourceRequest& request)
{
    return downcast<CachedRawResource>(requestResource(CachedResource::MainResource, request).get());
}

static MixedContentChecker::ContentType contentTypeFromResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::ImageResource:
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
    switch (type) {
    case CachedResource::Script:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
    case CachedResource::SVGDocumentResource:
    case CachedResource::CSSStyleSheet:
        // These resource can inject script into the current document (Script,
        // XSL) or exfiltrate the content of the current document (CSS).
        if (Frame* f = frame())
            if (!f->loader().mixedContentChecker().canRunInsecureContent(m_document->securityOrigin(), url))
                return false;
        break;
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
    case CachedResource::RawResource:
    case CachedResource::ImageResource:
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource: {
        // These resources can corrupt only the frame's pixels.
        if (Frame* f = frame()) {
            Frame& topFrame = f->tree().top();
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

bool CachedResourceLoader::canRequest(CachedResource::Type type, const URL& url, const ResourceLoaderOptions& options, bool forPreload)
{
    if (document() && !document()->securityOrigin()->canDisplay(url)) {
        if (!forPreload)
            FrameLoader::reportLocalLoadFailed(frame(), url.stringCenterEllipsizedToLength());
        LOG(ResourceLoading, "CachedResourceLoader::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return false;
    }

    bool skipContentSecurityPolicyCheck = options.contentSecurityPolicyImposition() == ContentSecurityPolicyImposition::SkipPolicyCheck;

    // Some types of resources can be loaded only from the same origin.  Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case CachedResource::MainResource:
    case CachedResource::ImageResource:
    case CachedResource::CSSStyleSheet:
    case CachedResource::Script:
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource:
    case CachedResource::RawResource:
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
        if (options.requestOriginPolicy() == RestrictToSameOrigin && !m_document->securityOrigin()->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    case CachedResource::SVGDocumentResource:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        if (!m_document->securityOrigin()->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
#endif
        break;
    }

    switch (type) {
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        if (!m_document->contentSecurityPolicy()->allowScriptFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        break;
#endif
    case CachedResource::Script:
        if (!m_document->contentSecurityPolicy()->allowScriptFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        if (frame() && !frame()->settings().isScriptEnabled())
            return false;
        break;
    case CachedResource::CSSStyleSheet:
        if (!m_document->contentSecurityPolicy()->allowStyleFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        break;
    case CachedResource::SVGDocumentResource:
    case CachedResource::ImageResource:
        if (!m_document->contentSecurityPolicy()->allowImageFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        break;
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource: {
        if (!m_document->contentSecurityPolicy()->allowFontFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        break;
    }
    case CachedResource::MainResource:
    case CachedResource::RawResource:
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
#endif
        break;
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
        if (!m_document->contentSecurityPolicy()->allowMediaFromSource(url, skipContentSecurityPolicyCheck))
            return false;
        break;
#endif
    }

    // SVG Images have unique security rules that prevent all subresource requests except for data urls.
    if (type != CachedResource::MainResource && frame() && frame()->page()) {
        if (frame()->page()->chrome().client().isSVGImageChromeClient() && !url.protocolIsData())
            return false;
    }

    if (!canRequestInContentDispositionAttachmentSandbox(type, url))
        return false;

    // Last of all, check for insecure content. We do this last so that when
    // folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.

    // FIXME: Should we consider forPreload here?
    if (!checkInsecureContent(type, url))
        return false;

    return true;
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

    if (!document->shouldEnforceContentDispositionAttachmentSandbox() || document->securityOrigin()->canRequest(url))
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
    if (request.resourceRequest().hiddenFromInspector())
        newRequest.setHiddenFromInspector(true);
    frame()->loader().loadedResourceFromMemoryCache(resource, newRequest);

    // FIXME <http://webkit.org/b/113251>: If the delegate modifies the request's
    // URL, it is no longer appropriate to use this CachedResource.
    return !newRequest.isNull();
}

static inline void logMemoryCacheResourceRequest(Frame* frame, const String& description, const String& value = String())
{
    if (!frame)
        return;
    if (value.isNull())
        frame->mainFrame().diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::resourceRequestKey(), description, ShouldSample::Yes);
    else
        frame->mainFrame().diagnosticLoggingClient().logDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceRequestKey(), description, value, ShouldSample::Yes);
}

CachedResourceHandle<CachedResource> CachedResourceLoader::requestResource(CachedResource::Type type, CachedResourceRequest& request)
{
    URL url = request.resourceRequest().url();
    
    LOG(ResourceLoading, "CachedResourceLoader::requestResource '%s', charset '%s', priority=%d, forPreload=%u", url.stringCenterEllipsizedToLength().latin1().data(), request.charset().latin1().data(), request.priority() ? static_cast<int>(request.priority().value()) : -1, request.forPreload());
    
    // If only the fragment identifiers differ, it is the same resource.
    url = MemoryCache::removeFragmentIdentifierIfNeeded(url);

    if (!url.isValid())
        return nullptr;

    if (!canRequest(type, url, request.options(), request.forPreload()))
        return nullptr;

#if ENABLE(CONTENT_EXTENSIONS)
    if (frame() && frame()->mainFrame().page() && frame()->mainFrame().page()->userContentController() && m_documentLoader) {
        if (frame()->mainFrame().page()->userContentController()->processContentExtensionRulesForLoad(request.mutableResourceRequest(), toResourceType(type), *m_documentLoader) == ContentExtensions::BlockedStatus::Blocked) {
            if (type == CachedResource::Type::MainResource) {
                auto resource = createResource(type, request.mutableResourceRequest(), request.charset(), sessionID());
                ASSERT(resource);
                resource->error(CachedResource::Status::LoadError);
                resource->setResourceError(ResourceError(ContentExtensions::WebKitContentBlockerDomain, 0, request.resourceRequest().url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description")));
                return resource;
            }
            return nullptr;
        }
        url = request.resourceRequest().url(); // The content extension could have changed it from http to https.
        url = MemoryCache::removeFragmentIdentifierIfNeeded(url); // Might need to remove fragment identifier again.
    }
#endif

    auto& memoryCache = MemoryCache::singleton();
    if (memoryCache.disabled()) {
        DocumentResourceMap::iterator it = m_documentResources.find(url.string());
        if (it != m_documentResources.end()) {
            it->value->setOwningCachedResourceLoader(nullptr);
            m_documentResources.remove(it);
        }
    }

    // See if we can use an existing resource from the cache.
    CachedResourceHandle<CachedResource> resource;
#if ENABLE(CACHE_PARTITIONING)
    if (document())
        request.mutableResourceRequest().setDomainForCachePartition(document()->topOrigin()->domainForCachePartition());
#endif

    resource = memoryCache.resourceForRequest(request.resourceRequest(), sessionID());

    logMemoryCacheResourceRequest(frame(), resource ? DiagnosticLoggingKeys::inMemoryCacheKey() : DiagnosticLoggingKeys::notInMemoryCacheKey());

    const RevalidationPolicy policy = determineRevalidationPolicy(type, request.mutableResourceRequest(), request.forPreload(), resource.get(), request.defer());
    switch (policy) {
    case Reload:
        memoryCache.remove(*resource);
        FALLTHROUGH;
    case Load:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedKey());
        resource = loadResource(type, request);
        break;
    case Revalidate:
        if (resource)
            logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::revalidatingKey());
        resource = revalidateResource(request, resource.get());
        break;
    case Use:
        if (!shouldContinueAfterNotifyingLoadedFromMemoryCache(request, resource.get()))
            return nullptr;
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::usedKey());
        memoryCache.resourceAccessed(*resource);
        break;
    }

    if (!resource)
        return nullptr;

    if (!request.forPreload() || policy != Use)
        resource->setLoadPriority(request.priority());

    if ((policy != Use || resource->stillNeedsLoad()) && CachedResourceRequest::NoDefer == request.defer()) {
        resource->load(*this, request.options());

        // We don't support immediate loads, but we do support immediate failure.
        if (resource->errorOccurred()) {
            if (resource->inCache())
                memoryCache.remove(*resource);
            return nullptr;
        }
    }

    if (document() && !document()->loadEventFinished() && !request.resourceRequest().url().protocolIsData())
        m_validatedURLs.add(request.resourceRequest().url());

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url(), resource);
    return resource;
}

void CachedResourceLoader::documentDidFinishLoadEvent()
{
    m_validatedURLs.clear();
}

CachedResourceHandle<CachedResource> CachedResourceLoader::revalidateResource(const CachedResourceRequest& request, CachedResource* resource)
{
    ASSERT(resource);
    ASSERT(resource->inCache());
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!memoryCache.disabled());
    ASSERT(resource->canUseCacheValidator());
    ASSERT(!resource->resourceToRevalidate());
    ASSERT(resource->sessionID() == sessionID());

    CachedResourceHandle<CachedResource> newResource = createResource(resource->type(), resource->resourceRequest(), resource->encoding(), resource->sessionID());

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), resource);
    newResource->setResourceToRevalidate(resource);
    
    memoryCache.remove(*resource);
    memoryCache.add(*newResource);
#if ENABLE(RESOURCE_TIMING)
    storeResourceTimingInitiatorInformation(resource, request);
#else
    UNUSED_PARAM(request);
#endif
    return newResource;
}

CachedResourceHandle<CachedResource> CachedResourceLoader::loadResource(CachedResource::Type type, CachedResourceRequest& request)
{
    auto& memoryCache = MemoryCache::singleton();
    ASSERT(!memoryCache.resourceForRequest(request.resourceRequest(), sessionID()));

    LOG(ResourceLoading, "Loading CachedResource for '%s'.", request.resourceRequest().url().stringCenterEllipsizedToLength().latin1().data());

    CachedResourceHandle<CachedResource> resource = createResource(type, request.mutableResourceRequest(), request.charset(), sessionID());

    if (!memoryCache.add(*resource))
        resource->setOwningCachedResourceLoader(this);
#if ENABLE(RESOURCE_TIMING)
    storeResourceTimingInitiatorInformation(resource, request);
#endif
    return resource;
}

#if ENABLE(RESOURCE_TIMING)
void CachedResourceLoader::storeResourceTimingInitiatorInformation(const CachedResourceHandle<CachedResource>& resource, const CachedResourceRequest& request)
{
    if (resource->type() == CachedResource::MainResource) {
        // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
        if (frame()->ownerElement() && m_documentLoader->frameLoader()->stateMachine().committingFirstRealLoad()) {
            InitiatorInfo info = { frame()->ownerElement()->localName(), monotonicallyIncreasingTime() };
            m_initiatorMap.add(resource.get(), info);
        }
    } else {
        InitiatorInfo info = { request.initiatorName(), monotonicallyIncreasingTime() };
        m_initiatorMap.add(resource.get(), info);
    }
}
#endif // ENABLE(RESOURCE_TIMING)

static void logRevalidation(const String& reason, DiagnosticLoggingClient& logClient)
{
    logClient.logDiagnosticMessageWithValue(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), DiagnosticLoggingKeys::reasonKey(), reason, ShouldSample::Yes);
}

static void logResourceRevalidationDecision(CachedResource::RevalidationDecision reason, const Frame* frame)
{
    if (!frame)
        return;
    auto& logClient = frame->mainFrame().diagnosticLoggingClient();
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

CachedResourceLoader::RevalidationPolicy CachedResourceLoader::determineRevalidationPolicy(CachedResource::Type type, ResourceRequest& request, bool forPreload, CachedResource* existingResource, CachedResourceRequest::DeferOption defer) const
{
    if (!existingResource)
        return Load;

    // We already have a preload going for this URL.
    if (forPreload && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        LOG(ResourceLoading, "CachedResourceLoader::determineRevalidationPolicy reloading due to type mismatch.");
        logMemoryCacheResourceRequest(frame(), DiagnosticLoggingKeys::inMemoryCacheKey(), DiagnosticLoggingKeys::unusedReasonTypeMismatchKey());
        return Reload;
    }

    // FIXME: We should use the same cache policy for all resource types. The raw resource policy is overly strict
    //        while the normal subresource policy is too loose.
    if (existingResource->isMainOrRawResource()) {
        bool strictPolicyDisabled = frame()->loader().isStrictRawResourceValidationPolicyDisabledForTesting();
        bool canReuseRawResource = strictPolicyDisabled || downcast<CachedRawResource>(*existingResource).canReuse(request);
        if (!canReuseRawResource)
            return Reload;
    }

    // Conditional requests should have failed canReuse check.
    ASSERT(!request.isConditional());

    // Do not load from cache if images are not enabled. The load for this image will be blocked
    // in CachedImage::load.
    if (CachedResourceRequest::DeferredByClient == defer)
        return Reload;
    
    // Don't reload resources while pasting.
    if (m_allowStaleResources)
        return Use;
    
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
    
    // For resources that are not yet loaded we ignore the cache policy.
    if (existingResource->isLoading())
        return Use;

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
            downcast<CachedImage>(*resource).load(*this, defaultCachedResourceOptions());
    }
}

CachePolicy CachedResourceLoader::cachePolicy(CachedResource::Type type) const
{
    if (!frame())
        return CachePolicyVerify;

    if (type != CachedResource::MainResource)
        return frame()->loader().subresourceCachePolicy();
    
    switch (frame()->loader().loadType()) {
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

void CachedResourceLoader::loadDone(CachedResource* resource, bool shouldPerformPostLoadActions)
{
    RefPtr<DocumentLoader> protectDocumentLoader(m_documentLoader);
    RefPtr<Document> protectDocument(m_document);

#if ENABLE(RESOURCE_TIMING)
    if (resource && resource->response().isHTTP() && ((!resource->errorOccurred() && !resource->wasCanceled()) || resource->response().httpStatusCode() == 304)) {
        HashMap<CachedResource*, InitiatorInfo>::iterator initiatorIt = m_initiatorMap.find(resource);
        if (initiatorIt != m_initiatorMap.end()) {
            ASSERT(document());
            Document* initiatorDocument = document();
            if (resource->type() == CachedResource::MainResource)
                initiatorDocument = document()->parentDocument();
            ASSERT(initiatorDocument);
            const InitiatorInfo& info = initiatorIt->value;
            initiatorDocument->domWindow()->performance()->addResourceTiming(info.name, initiatorDocument, resource->resourceRequest(), resource->response(), info.startTime, resource->loadFinishTime());
            m_initiatorMap.remove(initiatorIt);
        }
    }
#else
    UNUSED_PARAM(resource);
#endif // ENABLE(RESOURCE_TIMING)

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
    checkForPendingPreloads();

    platformStrategies()->loaderStrategy()->servePendingRequests();
}

void CachedResourceLoader::incrementRequestCount(const CachedResource* res)
{
    if (res->ignoreForRequestCount())
        return;

    ++m_requestCount;
}

void CachedResourceLoader::decrementRequestCount(const CachedResource* res)
{
    if (res->ignoreForRequestCount())
        return;

    --m_requestCount;
    ASSERT(m_requestCount > -1);
}

void CachedResourceLoader::preload(CachedResource::Type type, CachedResourceRequest& request, const String& charset)
{
    // We always preload resources on iOS. See <https://bugs.webkit.org/show_bug.cgi?id=91276>.
    // FIXME: We should consider adding a setting to toggle aggressive preloading behavior as opposed
    // to making this behavior specific to iOS.
#if !PLATFORM(IOS)
    bool hasRendering = m_document->bodyOrFrameset() && m_document->renderView();
    bool canBlockParser = type == CachedResource::Script || type == CachedResource::CSSStyleSheet;
    if (!hasRendering && !canBlockParser) {
        // Don't preload subresources that can't block the parser before we have something to draw.
        // This helps prevent preloads from delaying first display when bandwidth is limited.
        PendingPreload pendingPreload = { type, request, charset };
        m_pendingPreloads.append(pendingPreload);
        return;
    }
#endif
    requestPreload(type, request, charset);
}

void CachedResourceLoader::checkForPendingPreloads() 
{
    if (m_pendingPreloads.isEmpty())
        return;
    auto* body = m_document->bodyOrFrameset();
    if (!body || !body->renderer())
        return;
#if PLATFORM(IOS)
    // We always preload resources on iOS. See <https://bugs.webkit.org/show_bug.cgi?id=91276>.
    // So, we should never have any pending preloads.
    // FIXME: We should look to avoid compiling this code entirely when building for iOS.
    ASSERT_NOT_REACHED();
#endif
    while (!m_pendingPreloads.isEmpty()) {
        PendingPreload preload = m_pendingPreloads.takeFirst();
        // Don't request preload if the resource already loaded normally (this will result in double load if the page is being reloaded with cached results ignored).
        if (!cachedResource(preload.m_request.resourceRequest().url()))
            requestPreload(preload.m_type, preload.m_request, preload.m_charset);
    }
    m_pendingPreloads.clear();
}

void CachedResourceLoader::requestPreload(CachedResource::Type type, CachedResourceRequest& request, const String& charset)
{
    String encoding;
    if (type == CachedResource::Script || type == CachedResource::CSSStyleSheet)
        encoding = charset.isEmpty() ? m_document->charset() : charset;

    request.setCharset(encoding);
    request.setForPreload(true);

    CachedResourceHandle<CachedResource> resource = requestResource(type, request);
    if (!resource || (m_preloads && m_preloads->contains(resource.get())))
        return;
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = std::make_unique<ListHashSet<CachedResource*>>();
    m_preloads->add(resource.get());

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().latin1().data());
#endif
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

    for (auto& pendingPreload : m_pendingPreloads) {
        if (pendingPreload.m_request.resourceRequest().url() == url)
            return true;
    }
    return false;
}

void CachedResourceLoader::clearPreloads()
{
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    if (!m_preloads)
        return;

    for (auto* resource : *m_preloads) {
        resource->decreasePreloadCount();
        bool deleted = resource->deleteIfPossible();
        if (!deleted && resource->preloadResult() == CachedResource::PreloadNotReferenced)
            MemoryCache::singleton().remove(*resource);
    }
    m_preloads = nullptr;
}

void CachedResourceLoader::clearPendingPreloads()
{
    m_pendingPreloads.clear();
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
        
        if (resource->errorOccurred())
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
    static ResourceLoaderOptions options(SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForAllCredentials, DoSecurityCheck, UseDefaultOriginRestrictionsForType, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading);
    return options;
}

}
