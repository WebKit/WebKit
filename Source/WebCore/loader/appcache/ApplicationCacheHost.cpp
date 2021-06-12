/*
 * Copyright (C) 2008-2016 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "ApplicationCacheHost.h"

#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheResource.h"
#include "ContentSecurityPolicy.h"
#include "DocumentLoader.h"
#include "DOMApplicationCache.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ProgressEvent.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include <wtf/FileSystem.h>
#include <wtf/UUID.h>

namespace WebCore {

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader& documentLoader)
    : m_documentLoader(documentLoader)
{
}

ApplicationCacheHost::~ApplicationCacheHost()
{
    ASSERT(!m_applicationCache || !m_candidateApplicationCacheGroup || m_applicationCache->group() == m_candidateApplicationCacheGroup);
    if (m_applicationCache)
        m_applicationCache->group()->disassociateDocumentLoader(m_documentLoader);
    else if (m_candidateApplicationCacheGroup)
        m_candidateApplicationCacheGroup->disassociateDocumentLoader(m_documentLoader);
}

ApplicationCacheGroup* ApplicationCacheHost::candidateApplicationCacheGroup() const
{
    return m_candidateApplicationCacheGroup.get();
}

void ApplicationCacheHost::selectCacheWithoutManifest()
{
    ASSERT(m_documentLoader.frame());
    ApplicationCacheGroup::selectCacheWithoutManifestURL(*m_documentLoader.frame());
}

void ApplicationCacheHost::selectCacheWithManifest(const URL& manifestURL)
{
    ASSERT(m_documentLoader.frame());
    ApplicationCacheGroup::selectCache(*m_documentLoader.frame(), manifestURL);
}

bool ApplicationCacheHost::canLoadMainResource(const ResourceRequest& request)
{
    if (!isApplicationCacheEnabled() || isApplicationCacheBlockedForRequest(request))
        return false;
    return !!ApplicationCacheGroup::cacheForMainRequest(request, &m_documentLoader);
}

void ApplicationCacheHost::maybeLoadMainResource(const ResourceRequest& request, SubstituteData& substituteData)
{
    // Check if this request should be loaded from the application cache
    if (!substituteData.isValid() && isApplicationCacheEnabled() && !isApplicationCacheBlockedForRequest(request)) {
        ASSERT(!m_mainResourceApplicationCache);

        m_mainResourceApplicationCache = ApplicationCacheGroup::cacheForMainRequest(request, &m_documentLoader);

        if (m_mainResourceApplicationCache) {
            // Get the resource from the application cache. By definition, cacheForMainRequest() returns a cache that contains the resource.
            ApplicationCacheResource* resource = m_mainResourceApplicationCache->resourceForRequest(request);

            // ApplicationCache resources have fragment identifiers stripped off of their URLs,
            // but we'll need to restore that for the SubstituteData.
            ResourceResponse responseToUse = resource->response();
            if (request.url().hasFragmentIdentifier()) {
                URL url = responseToUse.url();
                url.setFragmentIdentifier(request.url().fragmentIdentifier());
                responseToUse.setURL(url);
            }

            substituteData = SubstituteData(&resource->data(),
                                            URL(),
                                            responseToUse,
                                            SubstituteData::SessionHistoryVisibility::Visible);
        }
    }
}

void ApplicationCacheHost::maybeLoadMainResourceForRedirect(const ResourceRequest& request, SubstituteData& substituteData)
{
    ASSERT(status() == UNCACHED);
    maybeLoadMainResource(request, substituteData);
}

bool ApplicationCacheHost::maybeLoadFallbackForMainResponse(const ResourceRequest& request, const ResourceResponse& r)
{
    if (r.httpStatusCode() / 100 == 4 || r.httpStatusCode() / 100 == 5) {
        ASSERT(!m_mainResourceApplicationCache);
        if (isApplicationCacheEnabled() && !isApplicationCacheBlockedForRequest(request)) {
            m_mainResourceApplicationCache = ApplicationCacheGroup::fallbackCacheForMainRequest(request, &m_documentLoader);

            if (scheduleLoadFallbackResourceFromApplicationCache(m_documentLoader.mainResourceLoader(), m_mainResourceApplicationCache.get()))
                return true;
        }
    }
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForMainError(const ResourceRequest& request, const ResourceError& error)
{
    if (!error.isCancellation()) {
        ASSERT(!m_mainResourceApplicationCache);
        if (isApplicationCacheEnabled() && !isApplicationCacheBlockedForRequest(request)) {
            m_mainResourceApplicationCache = ApplicationCacheGroup::fallbackCacheForMainRequest(request, &m_documentLoader);

            if (scheduleLoadFallbackResourceFromApplicationCache(m_documentLoader.mainResourceLoader(), m_mainResourceApplicationCache.get()))
                return true;
        }
    }
    return false;
}

void ApplicationCacheHost::mainResourceDataReceived(const uint8_t*, int, long long, bool)
{
}

void ApplicationCacheHost::failedLoadingMainResource()
{
    auto* group = m_candidateApplicationCacheGroup.get();
    if (!group && m_applicationCache) {
        if (mainResourceApplicationCache()) {
            // Even when the main resource is being loaded from an application cache, loading can fail if aborted.
            return;
        }
        group = m_applicationCache->group();
    }
    
    if (group)
        group->failedLoadingMainResource(m_documentLoader);
}

void ApplicationCacheHost::finishedLoadingMainResource()
{
    auto* group = candidateApplicationCacheGroup();
    if (!group && applicationCache() && !mainResourceApplicationCache())
        group = applicationCache()->group();
    
    if (group)
        group->finishedLoadingMainResource(m_documentLoader);
}

bool ApplicationCacheHost::maybeLoadResource(ResourceLoader& loader, const ResourceRequest& request, const URL& originalURL)
{
    if (loader.options().applicationCacheMode != ApplicationCacheMode::Use)
        return false;

    if (!isApplicationCacheEnabled() && !isApplicationCacheBlockedForRequest(request))
        return false;
    
    if (request.url() != originalURL)
        return false;

#if ENABLE(SERVICE_WORKER)
    if (loader.options().serviceWorkerRegistrationIdentifier)
        return false;
#endif

    ApplicationCacheResource* resource;
    if (!shouldLoadResourceFromApplicationCache(request, resource))
        return false;

    if (resource)
        m_documentLoader.scheduleSubstituteResourceLoad(loader, *resource);
    else
        m_documentLoader.scheduleCannotShowURLError(loader);
    return true;
}

bool ApplicationCacheHost::maybeLoadFallbackForRedirect(ResourceLoader* resourceLoader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (resourceLoader && resourceLoader->options().applicationCacheMode != ApplicationCacheMode::Use)
        return false;

    if (!redirectResponse.isNull() && !protocolHostAndPortAreEqual(request.url(), redirectResponse.url())) {
        if (scheduleLoadFallbackResourceFromApplicationCache(resourceLoader))
            return true;
    }
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForResponse(ResourceLoader* resourceLoader, const ResourceResponse& response)
{
    if (resourceLoader && resourceLoader->options().applicationCacheMode != ApplicationCacheMode::Use)
        return false;

    if (response.httpStatusCode() / 100 == 4 || response.httpStatusCode() / 100 == 5) {
        if (scheduleLoadFallbackResourceFromApplicationCache(resourceLoader))
            return true;
    }
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForError(ResourceLoader* resourceLoader, const ResourceError& error)
{
    if (resourceLoader && resourceLoader->options().applicationCacheMode != ApplicationCacheMode::Use)
        return false;

    if (!error.isCancellation()) {
        if (resourceLoader == m_documentLoader.mainResourceLoader())
            return maybeLoadFallbackForMainError(resourceLoader->request(), error);
        if (scheduleLoadFallbackResourceFromApplicationCache(resourceLoader))
            return true;
    }
    return false;
}

URL ApplicationCacheHost::createFileURL(const String& path)
{
#if USE(CF) && PLATFORM(WIN)
    // FIXME: Is this correct? Seems improbable that the passed-in paths would be in the Windows path style.
    return adoptCF(CFURLCreateWithFileSystemPath(0, path.createCFString().get(), kCFURLWindowsPathStyle, false)).get();
#else
    return URL::fileURLWithFileSystemPath(path);
#endif
}

static inline RefPtr<SharedBuffer> bufferFromResource(ApplicationCacheResource& resource)
{
    // FIXME: Clients probably do not need a copy of the SharedBuffer.
    // Remove the call to copy() once we ensure SharedBuffer will not be modified.
    if (resource.path().isEmpty())
        return resource.data().copy();
    return SharedBuffer::createWithContentsOfFile(resource.path());
}

bool ApplicationCacheHost::maybeLoadSynchronously(ResourceRequest& request, ResourceError& error, ResourceResponse& response, RefPtr<SharedBuffer>& data)
{
    ApplicationCacheResource* resource;
    if (!shouldLoadResourceFromApplicationCache(request, resource))
        return false;

    auto responseData = resource ? bufferFromResource(*resource) : nullptr;
    if (!responseData) {
        error = m_documentLoader.frameLoader()->client().cannotShowURLError(request);
        return true;
    }

    response = resource->response();
    data = WTFMove(responseData);
    return true;
}

void ApplicationCacheHost::maybeLoadFallbackSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, RefPtr<SharedBuffer>& data)
{
    // If normal loading results in a redirect to a resource with another origin (indicative of a captive portal), or a 4xx or 5xx status code or equivalent,
    // or if there were network errors (but not if the user canceled the download), then instead get, from the cache, the resource of the fallback entry
    // corresponding to the matched namespace.
    if ((!error.isNull() && !error.isCancellation())
         || response.httpStatusCode() / 100 == 4 || response.httpStatusCode() / 100 == 5
         || !protocolHostAndPortAreEqual(request.url(), response.url())) {
        ApplicationCacheResource* resource;
        if (getApplicationCacheFallbackResource(request, resource)) {
            response = resource->response();
            // FIXME: Clients proably do not need a copy of the SharedBuffer.
            // Remove the call to copy() once we ensure SharedBuffer will not be modified.
            data = resource->data().copy();
        }
    }
}

bool ApplicationCacheHost::canCacheInBackForwardCache()
{
    return !applicationCache() && !candidateApplicationCacheGroup();
}

void ApplicationCacheHost::setDOMApplicationCache(DOMApplicationCache* domApplicationCache)
{
    ASSERT(!m_domApplicationCache || !domApplicationCache);
    m_domApplicationCache = makeWeakPtr(domApplicationCache);
}

void ApplicationCacheHost::notifyDOMApplicationCache(const AtomString& eventType, int total, int done)
{
    if (eventType != eventNames().progressEvent)
        InspectorInstrumentation::updateApplicationCacheStatus(m_documentLoader.frame());

    if (m_defersEvents) {
        // Event dispatching is deferred until document.onload has fired.
        m_deferredEvents.append({ eventType, total, done });
        return;
    }

    dispatchDOMEvent(eventType, total, done);
}

void ApplicationCacheHost::stopLoadingInFrame(Frame& frame)
{
    ASSERT(!m_applicationCache || !m_candidateApplicationCacheGroup || m_applicationCache->group() == m_candidateApplicationCacheGroup);

    if (m_candidateApplicationCacheGroup)
        m_candidateApplicationCacheGroup->stopLoadingInFrame(frame);
    else if (m_applicationCache)
        m_applicationCache->group()->stopLoadingInFrame(frame);
}

void ApplicationCacheHost::stopDeferringEvents()
{
    Ref<DocumentLoader> protect(m_documentLoader);

    // Note, do not cache the size in a local variable.
    // This code needs to properly handle the case where more events are added to
    // m_deferredEvents while iterating it. This is why we don't use a modern for loop.
    for (size_t i = 0; i < m_deferredEvents.size(); ++i) {
        auto& event = m_deferredEvents[i];
        dispatchDOMEvent(event.eventType, event.progressTotal, event.progressDone);
    }

    m_deferredEvents.clear();
    m_defersEvents = false;
}

Vector<ApplicationCacheHost::ResourceInfo> ApplicationCacheHost::resourceList()
{
    auto* cache = applicationCache();
    if (!cache || !cache->isComplete())
        return { };

    return WTF::map(cache->resources(), [] (auto& urlAndResource) -> ApplicationCacheHost::ResourceInfo {
        ASSERT(urlAndResource.value);
        auto& resource = *urlAndResource.value;

        unsigned type = resource.type();
        bool isMaster = type & ApplicationCacheResource::Master;
        bool isManifest = type & ApplicationCacheResource::Manifest;
        bool isExplicit = type & ApplicationCacheResource::Explicit;
        bool isForeign = type & ApplicationCacheResource::Foreign;
        bool isFallback = type & ApplicationCacheResource::Fallback;

        return { resource.url(), isMaster, isManifest, isFallback, isForeign, isExplicit, resource.estimatedSizeInStorage() };
    });
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::applicationCacheInfo()
{
    auto* cache = applicationCache();
    if (!cache || !cache->isComplete())
        return { { }, 0, 0, 0 };

    // FIXME: Add "Creation Time" and "Update Time" to Application Caches.
    return { cache->manifestResource()->url(), 0, 0, cache->estimatedSizeInStorage() };
}

static Ref<Event> createApplicationCacheEvent(const AtomString& eventType, int total, int done)
{
    if (eventType == eventNames().progressEvent)
        return ProgressEvent::create(eventType, true, done, total);
    return Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No);
}

void ApplicationCacheHost::dispatchDOMEvent(const AtomString& eventType, int total, int done)
{
    if (!m_domApplicationCache || !m_domApplicationCache->frame())
        return;

    m_domApplicationCache->dispatchEvent(createApplicationCacheEvent(eventType, total, done));
}

void ApplicationCacheHost::setCandidateApplicationCacheGroup(ApplicationCacheGroup* group)
{
    ASSERT(!m_applicationCache);
    m_candidateApplicationCacheGroup = makeWeakPtr(group);
}
    
void ApplicationCacheHost::setApplicationCache(RefPtr<ApplicationCache>&& applicationCache)
{
    if (m_candidateApplicationCacheGroup) {
        ASSERT(!m_applicationCache);
        m_candidateApplicationCacheGroup = nullptr;
    }
    m_applicationCache = WTFMove(applicationCache);
}

bool ApplicationCacheHost::shouldLoadResourceFromApplicationCache(const ResourceRequest& originalRequest, ApplicationCacheResource*& resource)
{
    auto* cache = applicationCache();
    if (!cache || !cache->isComplete())
        return false;

    ResourceRequest request(originalRequest);
    if (auto* loaderFrame = m_documentLoader.frame()) {
        if (auto* document = loaderFrame->document())
            document->contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(request, ContentSecurityPolicy::InsecureRequestType::Load);
    }
    
    // If the resource is not to be fetched using the HTTP GET mechanism or equivalent, or if its URL has a different
    // <scheme> component than the application cache's manifest, then fetch the resource normally.
    if (!ApplicationCache::requestIsHTTPOrHTTPSGet(request) || !equalIgnoringASCIICase(request.url().protocol(), cache->manifestResource()->url().protocol()))
        return false;

    // If the resource's URL is an master entry, the manifest, an explicit entry, or a fallback entry
    // in the application cache, then get the resource from the cache (instead of fetching it).
    resource = cache->resourceForURL(request.url().string());

    // Resources that match fallback namespaces or online allowlist entries are fetched from the network,
    // unless they are also cached.
    if (!resource && (cache->allowsAllNetworkRequests() || cache->urlMatchesFallbackNamespace(request.url()) || cache->isURLInOnlineAllowlist(request.url())))
        return false;

    // Resources that are not present in the manifest will always fail to load (at least, after the
    // cache has been primed the first time), making the testing of offline applications simpler.
    return true;
}

bool ApplicationCacheHost::getApplicationCacheFallbackResource(const ResourceRequest& request, ApplicationCacheResource*& resource, ApplicationCache* cache)
{
    if (!cache) {
        cache = applicationCache();
        if (!cache)
            return false;
    }
    if (!cache->isComplete())
        return false;
    
    // If the resource is not a HTTP/HTTPS GET, then abort
    if (!ApplicationCache::requestIsHTTPOrHTTPSGet(request))
        return false;

    URL fallbackURL;
    if (cache->isURLInOnlineAllowlist(request.url()))
        return false;
    if (!cache->urlMatchesFallbackNamespace(request.url(), &fallbackURL))
        return false;

    resource = cache->resourceForURL(fallbackURL.string());
    ASSERT(resource);
    return true;
}

bool ApplicationCacheHost::scheduleLoadFallbackResourceFromApplicationCache(ResourceLoader* loader, ApplicationCache* cache)
{
    if (!loader)
        return false;

    if (!isApplicationCacheEnabled() && !isApplicationCacheBlockedForRequest(loader->request()))
        return false;

#if ENABLE(SERVICE_WORKER)
    if (loader->options().serviceWorkerRegistrationIdentifier)
        return false;
#endif

    ApplicationCacheResource* resource;
    if (!getApplicationCacheFallbackResource(loader->request(), resource, cache))
        return false;

    loader->willSwitchToSubstituteResource();
    m_documentLoader.scheduleSubstituteResourceLoad(*loader, *resource);
    return true;
}

ApplicationCacheHost::Status ApplicationCacheHost::status() const
{
    auto* cache = applicationCache();
    if (!cache)
        return UNCACHED;

    switch (cache->group()->updateStatus()) {
    case ApplicationCacheGroup::Checking:
        return CHECKING;
    case ApplicationCacheGroup::Downloading:
        return DOWNLOADING;
    case ApplicationCacheGroup::Idle:
        if (cache->group()->isObsolete())
            return OBSOLETE;
        if (cache != cache->group()->newestCache())
            return UPDATEREADY;
        return IDLE;
    }

    ASSERT_NOT_REACHED();
    return UNCACHED;
}

bool ApplicationCacheHost::update()
{
    auto* cache = applicationCache();
    if (!cache)
        return false;
    auto* frame = m_documentLoader.frame();
    if (!frame)
        return false;
    cache->group()->update(*frame, ApplicationCacheUpdateWithoutBrowsingContext);
    return true;
}

bool ApplicationCacheHost::swapCache()
{
    auto* cache = applicationCache();
    if (!cache)
        return false;
    
    auto* group = cache->group();
    if (!group)
        return false;

    // If the group of application caches to which cache belongs has the lifecycle status obsolete, unassociate document from cache.
    if (group->isObsolete()) {
        group->disassociateDocumentLoader(m_documentLoader);
        return true;
    }

    // If there is no newer cache, raise an InvalidStateError exception.
    auto* newestCache = group->newestCache();
    if (!newestCache || cache == newestCache)
        return false;
    
    ASSERT(group == newestCache->group());
    setApplicationCache(newestCache);
    InspectorInstrumentation::updateApplicationCacheStatus(m_documentLoader.frame());
    return true;
}

void ApplicationCacheHost::abort()
{
    auto* frame = m_documentLoader.frame();
    if (!frame)
        return;
    if (auto* cacheGroup = candidateApplicationCacheGroup())
        cacheGroup->abort(*frame);
    else if (auto* cache = applicationCache())
        cache->group()->abort(*frame);
}

bool ApplicationCacheHost::isApplicationCacheEnabled()
{
    return m_documentLoader.frame() && m_documentLoader.frame()->settings().offlineWebApplicationCacheEnabled() && !m_documentLoader.frame()->page()->usesEphemeralSession();
}

bool ApplicationCacheHost::isApplicationCacheBlockedForRequest(const ResourceRequest& request)
{
    auto* frame = m_documentLoader.frame();
    if (!frame)
        return false;
    if (frame->isMainFrame())
        return false;
    return !SecurityOrigin::create(request.url())->canAccessApplicationCache(frame->document()->topOrigin());
}

}  // namespace WebCore
