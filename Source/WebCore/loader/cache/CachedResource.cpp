/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2011, 2014, 2018 Apple Inc. All rights reserved.

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
*/

#include "config.h"
#include "CachedResource.h"

#include "CachePolicy.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceHandle.h"
#include "CachedResourceLoader.h"
#include "CookieJar.h"
#include "CrossOriginAccessControl.h"
#include "DefaultResourceLoadPriority.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTTPHeaderNames.h"
#include "HTTPHeaderValues.h"
#include "InspectorInstrumentation.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MathExtras.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

#undef CACHEDRESOURCE_RELEASE_LOG
#define PAGE_ID(frame) (valueOrDefault(frame.pageID()).toUInt64())
#define FRAME_ID(frame) (frame.frameID().object().toUInt64())
#define CACHEDRESOURCE_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - CachedResource::" fmt, this, ##__VA_ARGS__)
#define CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME(fmt, frame, ...) RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 "] CachedResource::" fmt, this, PAGE_ID(frame), FRAME_ID(frame), ##__VA_ARGS__)

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CachedResource);

static Seconds deadDecodedDataDeletionIntervalForResourceType(CachedResource::Type type)
{
    if (type == CachedResource::Type::Script)
        return 5_s;

    return MemoryCache::singleton().deadDecodedDataDeletionInterval();
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, cachedResourceLeakCounter, ("CachedResource"));

CachedResource::CachedResource(CachedResourceRequest&& request, Type type, PAL::SessionID sessionID, const CookieJar* cookieJar)
    : m_options(request.options())
    , m_resourceRequest(request.releaseResourceRequest())
    , m_decodedDataDeletionTimer(*this, &CachedResource::destroyDecodedData, deadDecodedDataDeletionIntervalForResourceType(type))
    , m_sessionID(sessionID)
    , m_cookieJar(cookieJar)
    , m_responseTimestamp(WallTime::now())
    , m_fragmentIdentifierForRequest(request.releaseFragmentIdentifier())
    , m_origin(request.releaseOrigin())
    , m_initiatorType(request.initiatorType())
    , m_type(type)
    , m_preloadResult(PreloadResult::PreloadNotReferenced)
    , m_responseTainting(ResourceResponse::Tainting::Basic)
    , m_loadPriority(DefaultResourceLoadPriority::forResourceType(type))
    , m_status(Pending)
    , m_requestedFromNetworkingLayer(false)
    , m_inCache(false)
    , m_loading(false)
    , m_isLinkPreload(request.isLinkPreload())
    , m_hasUnknownEncoding(request.isLinkPreload())
    , m_switchingClientsToRevalidatedResource(false)
    , m_ignoreForRequestCount(request.ignoreForRequestCount())
{
    ASSERT(m_sessionID.isValid());

    setLoadPriority(request.priority());
#ifndef NDEBUG
    cachedResourceLeakCounter.increment();
#endif

    // FIXME: We should have a better way of checking for Navigation loads, maybe FetchMode::Options::Navigate.
    ASSERT(m_origin || m_type == Type::MainResource);

    if (isRequestCrossOrigin(m_origin.get(), m_resourceRequest.url(), m_options))
        setCrossOrigin();
}

// FIXME: For this constructor, we should probably mandate that the URL has no fragment identifier.
CachedResource::CachedResource(const URL& url, Type type, PAL::SessionID sessionID, const CookieJar* cookieJar)
    : m_resourceRequest(url)
    , m_decodedDataDeletionTimer(*this, &CachedResource::destroyDecodedData, deadDecodedDataDeletionIntervalForResourceType(type))
    , m_sessionID(sessionID)
    , m_cookieJar(cookieJar)
    , m_responseTimestamp(WallTime::now())
    , m_fragmentIdentifierForRequest(CachedResourceRequest::splitFragmentIdentifierFromRequestURL(m_resourceRequest))
    , m_type(type)
    , m_preloadResult(PreloadResult::PreloadNotReferenced)
    , m_responseTainting(ResourceResponse::Tainting::Basic)
    , m_status(Cached)
    , m_requestedFromNetworkingLayer(false)
    , m_inCache(false)
    , m_loading(false)
    , m_isLinkPreload(false)
    , m_hasUnknownEncoding(false)
    , m_switchingClientsToRevalidatedResource(false)
    , m_ignoreForRequestCount(false)
{
    ASSERT(m_sessionID.isValid());
#ifndef NDEBUG
    cachedResourceLeakCounter.increment();
#endif
}

CachedResource::~CachedResource()
{
    ASSERT(!m_resourceToRevalidate); // Should be true because canDelete() checks this.
    ASSERT(canDelete());
    ASSERT(!inCache());
    ASSERT(!m_deleted);
    ASSERT(url().isNull() || !allowsCaching() || MemoryCache::singleton().resourceForRequest(resourceRequest(), sessionID()) != this);

#ifndef NDEBUG
    m_deleted = true;
    cachedResourceLeakCounter.decrement();
#endif
}

void CachedResource::failBeforeStarting()
{
    // FIXME: What if resources in other frames were waiting for this revalidation?
    LOG(ResourceLoading, "Cannot start loading '%s'", url().string().latin1().data());
    if (allowsCaching() && m_resourceToRevalidate)
        MemoryCache::singleton().revalidationFailed(*this);
    error(CachedResource::LoadError);
}

void CachedResource::load(CachedResourceLoader& cachedResourceLoader)
{
    if (!cachedResourceLoader.frame()) {
        CACHEDRESOURCE_RELEASE_LOG("load: No associated frame");
        failBeforeStarting();
        return;
    }
    Frame& frame = *cachedResourceLoader.frame();

    // Prevent new loads if we are in the BackForwardCache or being added to the BackForwardCache.
    // We query the top document because new frames may be created in pagehide event handlers
    // and their backForwardCacheState will not reflect the fact that they are about to enter page
    // cache.
    if (auto* topDocument = frame.mainFrame().document()) {
        switch (topDocument->backForwardCacheState()) {
        case Document::NotInBackForwardCache:
            break;
        case Document::AboutToEnterBackForwardCache:
            // Beacons are allowed to go through in 'pagehide' event handlers.
            if (m_options.keepAlive || shouldUsePingLoad(type()))
                break;
            CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME("load: About to enter back/forward cache", frame);
            failBeforeStarting();
            return;
        case Document::InBackForwardCache:
            CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME("load: Already in back/forward cache", frame);
            failBeforeStarting();
            return;
        }
    }

    FrameLoader& frameLoader = frame.loader();
    if (m_options.securityCheck == SecurityCheckPolicy::DoSecurityCheck && !m_options.keepAlive && !shouldUsePingLoad(type())) {
        while (true) {
            if (frameLoader.state() == FrameState::Provisional)
                CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME("load: Failed security check -- state is provisional", frame);
            else if (!frameLoader.activeDocumentLoader())
                CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME("load: Failed security check -- not active document", frame);
            else if (frameLoader.activeDocumentLoader()->isStopping())
                CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME("load: Failed security check -- active loader is stopping", frame);
            else
                break;
            failBeforeStarting();
            return;
        }
    }

    m_loading = true;

    if (isCacheValidator()) {
        CachedResource* resourceToRevalidate = m_resourceToRevalidate;
        ASSERT(resourceToRevalidate->canUseCacheValidator());
        ASSERT(resourceToRevalidate->isLoaded());
        const String& lastModified = resourceToRevalidate->response().httpHeaderField(HTTPHeaderName::LastModified);
        const String& eTag = resourceToRevalidate->response().httpHeaderField(HTTPHeaderName::ETag);
        if (!lastModified.isEmpty() || !eTag.isEmpty()) {
            ASSERT(cachedResourceLoader.cachePolicy(type(), url()) != CachePolicy::Reload);
            if (cachedResourceLoader.cachePolicy(type(), url()) == CachePolicy::Revalidate)
                m_resourceRequest.setHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());
            if (!lastModified.isEmpty())
                m_resourceRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
            if (!eTag.isEmpty())
                m_resourceRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);
        }
    }

    if (type() == Type::LinkPrefetch)
        m_resourceRequest.setHTTPHeaderField(HTTPHeaderName::Purpose, "prefetch"_s);
    m_resourceRequest.setPriority(loadPriority());

    // Navigation algorithm is setting up the request before sending it to CachedResourceLoader?CachedResource.
    // So no need for extra fields for MainResource.
    if (type() != Type::MainResource) {
        bool isServiceWorkerNavigationLoad = type() != Type::SVGDocumentResource && m_options.serviceWorkersMode == ServiceWorkersMode::None && (m_options.destination == FetchOptions::Destination::Document || m_options.destination == FetchOptions::Destination::Iframe);
        frameLoader.updateRequestAndAddExtraFields(m_resourceRequest, IsMainResource::No, FrameLoadType::Standard, ShouldUpdateAppInitiatedValue::Yes, isServiceWorkerNavigationLoad ? FrameLoader::IsServiceWorkerNavigationLoad::Yes : FrameLoader::IsServiceWorkerNavigationLoad::No);
    }

    // FIXME: It's unfortunate that the cache layer and below get to know anything about fragment identifiers.
    // We should look into removing the expectation of that knowledge from the platform network stacks.
    ResourceRequest request(m_resourceRequest);
    if (!m_fragmentIdentifierForRequest.isNull()) {
        URL url = request.url();
        url.setFragmentIdentifier(m_fragmentIdentifierForRequest);
        request.setURL(url);
        m_fragmentIdentifierForRequest = String();
    }

    if (m_options.keepAlive && type() != Type::Ping && !cachedResourceLoader.keepaliveRequestTracker().tryRegisterRequest(*this)) {
        setResourceError({ errorDomainWebKitInternal, 0, request.url(), "Reached maximum amount of queued data of 64Kb for keepalive requests"_s, ResourceError::Type::AccessControl });
        failBeforeStarting();
        return;
    }

    // FIXME: Deprecate that code path.
    if (m_options.keepAlive && shouldUsePingLoad(type()) && platformStrategies()->loaderStrategy()->usePingLoad()) {
        ASSERT(m_originalRequest);
        CachedResourceHandle<CachedResource> protectedThis(this);

        auto identifier = ResourceLoaderIdentifier::generate();
        InspectorInstrumentation::willSendRequestOfType(&frame, identifier, frameLoader.activeDocumentLoader(), request, InspectorInstrumentation::LoadType::Beacon);

        platformStrategies()->loaderStrategy()->startPingLoad(frame, request, m_originalRequest->httpHeaderFields(), m_options, m_options.contentSecurityPolicyImposition, [this, protectedThis = WTFMove(protectedThis), protectedFrame = Ref { frame }, identifier] (const ResourceError& error, const ResourceResponse& response) {
            if (!response.isNull())
                InspectorInstrumentation::didReceiveResourceResponse(protectedFrame, identifier, protectedFrame->loader().activeDocumentLoader(), response, nullptr);
            if (!error.isNull()) {
                setResourceError(error);
                this->error(LoadError);
                InspectorInstrumentation::didFailLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, error);
                return;
            }
            finishLoading(nullptr, { });
            NetworkLoadMetrics emptyMetrics;
            InspectorInstrumentation::didFinishLoading(protectedFrame.ptr(), protectedFrame->loader().activeDocumentLoader(), identifier, emptyMetrics, nullptr);
        });
        return;
    }

    platformStrategies()->loaderStrategy()->loadResource(frame, *this, WTFMove(request), m_options, [this, protectedThis = CachedResourceHandle<CachedResource>(this), frameRef = Ref { frame }] (RefPtr<SubresourceLoader>&& loader) {
        m_loader = WTFMove(loader);
        if (!m_loader) {
            RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 "] CachedResource::load: Unable to create SubresourceLoader", this, PAGE_ID(frameRef.get()), FRAME_ID(frameRef.get()));
            failBeforeStarting();
            return;
        }
        setStatus(Pending);
    });
}

void CachedResource::loadFrom(const CachedResource& resource)
{
    ASSERT(url() == resource.url());
    ASSERT(type() == resource.type());
    ASSERT(resource.status() == Status::Cached);

    if (isCrossOrigin() && m_options.mode == FetchOptions::Mode::Cors) {
        ASSERT(m_origin);
        auto accessControlCheckResult = WebCore::passesAccessControlCheck(resource.response(), m_options.storedCredentialsPolicy, *m_origin, &CrossOriginAccessControlCheckDisabler::singleton());
        if (!accessControlCheckResult) {
            setResourceError(ResourceError(String(), 0, url(), accessControlCheckResult.error(), ResourceError::Type::AccessControl));
            return;
        }
    }

    setBodyDataFrom(resource);
    setStatus(Status::Cached);
    setLoading(false);
}

void CachedResource::setBodyDataFrom(const CachedResource& resource)
{
    m_data = resource.m_data;
    m_response = resource.m_response;
    m_response.setTainting(m_responseTainting);
    setDecodedSize(resource.decodedSize());
    setEncodedSize(resource.encodedSize());
}

void CachedResource::checkNotify(const NetworkLoadMetrics& metrics)
{
    if (isLoading() || stillNeedsLoad())
        return;

    CachedResourceClientWalker<CachedResourceClient> walker(*this);
    while (CachedResourceClient* client = walker.next())
        client->notifyFinished(*this, metrics);
}

void CachedResource::updateBuffer(const FragmentedSharedBuffer&)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::BufferData);
}

void CachedResource::updateData(const SharedBuffer&)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData);
}

void CachedResource::finishLoading(const FragmentedSharedBuffer*, const NetworkLoadMetrics& metrics)
{
    setLoading(false);
    checkNotify(metrics);
}

void CachedResource::error(CachedResource::Status status)
{
    setStatus(status);
    ASSERT(errorOccurred());
    m_data = nullptr;

    setLoading(false);
    checkNotify({ });
}
    
void CachedResource::cancelLoad()
{
    if (!isLoading() && !stillNeedsLoad())
        return;

    auto* documentLoader = (m_loader && m_loader->frame()) ? m_loader->frame()->loader().activeDocumentLoader() : nullptr;
    if (m_options.keepAlive && (!documentLoader || documentLoader->isStopping()))
        m_error = { };
    else
        setStatus(LoadError);

    setLoading(false);
    checkNotify({ });
}

void CachedResource::finish()
{
    if (!errorOccurred())
        setStatus(Cached);
}

void CachedResource::setCrossOrigin()
{
    ASSERT(m_options.mode != FetchOptions::Mode::SameOrigin);
    m_responseTainting = (m_options.mode == FetchOptions::Mode::Cors) ? ResourceResponse::Tainting::Cors : ResourceResponse::Tainting::Opaque;
}

bool CachedResource::isCrossOrigin() const
{
    return m_responseTainting != ResourceResponse::Tainting::Basic;
}

bool CachedResource::isCORSCrossOrigin() const
{
    return m_responseTainting == ResourceResponse::Tainting::Opaque || m_responseTainting == ResourceResponse::Tainting::Opaqueredirect;
}

bool CachedResource::isCORSSameOrigin() const
{
    // Following resource types do not use CORS
    ASSERT(type() != Type::FontResource);
    ASSERT(type() != Type::SVGFontResource);
#if ENABLE(XSLT)
    ASSERT(type() != Type::XSLStyleSheet);
#endif

    // https://html.spec.whatwg.org/multipage/infrastructure.html#cors-same-origin
    return !loadFailedOrCanceled() && m_responseTainting != ResourceResponse::Tainting::Opaque;
}

bool CachedResource::isExpired() const
{
    if (m_response.isNull())
        return false;

    return computeCurrentAge(m_response, m_responseTimestamp) > freshnessLifetime(m_response);
}

static inline bool shouldCacheSchemeIndefinitely(StringView scheme)
{
#if PLATFORM(COCOA)
    if (equalLettersIgnoringASCIICase(scheme, "applewebdata"_s))
        return true;
#endif
#if USE(SOUP)
    if (equalLettersIgnoringASCIICase(scheme, "resource"_s))
        return true;
#endif
    return equalLettersIgnoringASCIICase(scheme, "data"_s);
}

Seconds CachedResource::freshnessLifetime(const ResourceResponse& response) const
{
    if (!response.url().protocolIsInHTTPFamily()) {
        StringView protocol = response.url().protocol();
        if (!shouldCacheSchemeIndefinitely(protocol)) {
            // Don't cache non-HTTP main resources since we can't check for freshness.
            // FIXME: We should not cache subresources either, but when we tried this
            // it caused performance and flakiness issues in our test infrastructure.
            if (m_type == Type::MainResource || LegacySchemeRegistry::shouldAlwaysRevalidateURLScheme(protocol))
                return 0_us;
        }

        return Seconds::infinity();
    }

    return computeFreshnessLifetimeForHTTPFamily(response, m_responseTimestamp);
}

void CachedResource::redirectReceived(ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    CACHEDRESOURCE_RELEASE_LOG("redirectReceived:");

    m_requestedFromNetworkingLayer = true;
    if (!response.isNull())
        updateRedirectChainStatus(m_redirectChainCacheStatus, response);

    completionHandler(WTFMove(request));
}

#if ASSERT_ENABLED
static bool isOpaqueRedirectResponseWithoutLocationHeader(const ResourceResponse& response)
{
    return response.type() == ResourceResponse::Type::Opaqueredirect && response.isRedirection() && response.httpHeaderField(HTTPHeaderName::Location).isNull();
}
#endif

void CachedResource::setResponse(const ResourceResponse& response)
{
    ASSERT(m_response.type() == ResourceResponse::Type::Default || isOpaqueRedirectResponseWithoutLocationHeader(m_response));
    m_response = response;
    m_varyingHeaderValues = collectVaryingRequestHeaders(cookieJar(), m_resourceRequest, m_response);

#if ENABLE(SERVICE_WORKER)
    if (m_response.source() == ResourceResponse::Source::ServiceWorker) {
        m_responseTainting = m_response.tainting();
        return;
    }
#endif
    m_response.setRedirected(m_redirectChainCacheStatus.status != RedirectChainCacheStatus::Status::NoRedirection);
    if ((m_response.tainting() == ResourceResponse::Tainting::Basic || m_response.tainting() == ResourceResponse::Tainting::Cors) && !m_response.url().protocolIsData())
        m_response.setTainting(m_responseTainting);
}

void CachedResource::responseReceived(const ResourceResponse& response)
{
    setResponse(response);
    m_responseTimestamp = WallTime::now();
    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        setEncoding(encoding);
}

void CachedResource::clearLoader()
{
    if (m_loader)
        m_identifierForLoadWithoutResourceLoader = m_loader->identifier();
    else
        ASSERT_NOT_REACHED();
    m_loader = nullptr;
    deleteIfPossible();
}

void CachedResource::addClient(CachedResourceClient& client)
{
    if (addClientToSet(client))
        didAddClient(client);
}

void CachedResource::didAddClient(CachedResourceClient& client)
{
    if (m_decodedDataDeletionTimer.isActive())
        m_decodedDataDeletionTimer.stop();

    if (m_clientsAwaitingCallback.remove(client)) {
        m_clients.add(client);
#if ASSERT_ENABLED
        client.addAssociatedResource(*this);
#endif
    }

    // FIXME: Make calls to notifyFinished async
    if (!isLoading() && !stillNeedsLoad())
        client.notifyFinished(*this, { });
}

bool CachedResource::addClientToSet(CachedResourceClient& client)
{
    if (m_preloadResult == PreloadResult::PreloadNotReferenced && client.shouldMarkAsReferenced()) {
        if (isLoaded())
            m_preloadResult = PreloadResult::PreloadReferencedWhileComplete;
        else if (m_requestedFromNetworkingLayer)
            m_preloadResult = PreloadResult::PreloadReferencedWhileLoading;
        else
            m_preloadResult = PreloadResult::PreloadReferenced;
    }
    if (allowsCaching() && !hasClients() && inCache())
        MemoryCache::singleton().addToLiveResourcesSize(*this);

    if ((m_type == Type::RawResource || m_type == Type::MainResource) && !m_response.isNull() && !m_proxyResource) {
        // Certain resources (especially XHRs and main resources) do crazy things if an asynchronous load returns
        // synchronously (e.g., scripts may not have set all the state they need to handle the load).
        // Therefore, rather than immediately sending callbacks on a cache hit like other CachedResources,
        // we schedule the callbacks and ensure we never finish synchronously.
        ASSERT(!m_clientsAwaitingCallback.contains(client));
        m_clientsAwaitingCallback.add(client, makeUnique<Callback>(*this, client));
        return false;
    }

    m_clients.add(client);
#if ASSERT_ENABLED
    client.addAssociatedResource(*this);
#endif
    return true;
}

void CachedResource::removeClient(CachedResourceClient& client)
{
    auto callback = m_clientsAwaitingCallback.take(client);
    if (callback) {
        ASSERT(!m_clients.contains(client));
        callback->cancel();
        callback = nullptr;
    } else {
        ASSERT(m_clients.contains(client));
        m_clients.remove(client);
#if ASSERT_ENABLED
        if (!m_clients.contains(client))
            client.removeAssociatedResource(*this);
#endif
        didRemoveClient(client);
    }

    if (hasClients())
        return;

    auto& memoryCache = MemoryCache::singleton();
    if (allowsCaching() && inCache()) {
        memoryCache.removeFromLiveResourcesSize(*this);
        memoryCache.removeFromLiveDecodedResourcesList(*this);
    }

    if (deleteIfPossible()) {
        // `this` object is dead here.
        return;
    }

    if (!m_switchingClientsToRevalidatedResource)
        allClientsRemoved();
    destroyDecodedDataIfNeeded();

    if (!allowsCaching())
        return;

    memoryCache.pruneSoon();
}

void CachedResource::allClientsRemoved()
{
    if (isLinkPreload() && m_loader)
        m_loader->cancelIfNotFinishing();
}

void CachedResource::destroyDecodedDataIfNeeded()
{
    if (!m_decodedSize)
        return;
    if (!MemoryCache::singleton().deadDecodedDataDeletionInterval())
        return;
    m_decodedDataDeletionTimer.restart();
}

void CachedResource::decodedDataDeletionTimerFired()
{
    destroyDecodedData();
}

bool CachedResource::deleteIfPossible()
{
    if (!canDelete()) {
        LOG(ResourceLoading, "CachedResource %p deleteIfPossible - can't delete (hasClients %d loader %p preloadCount %u handleCount %u resourceToRevalidate %p proxyResource %p)", this, hasClients(), m_loader.get(), m_preloadCount, m_handleCount, m_resourceToRevalidate, m_proxyResource);
        return false;
    }

    LOG(ResourceLoading, "CachedResource %p deleteIfPossible - can delete, in cache %d", this, inCache());

    if (!inCache()) {
        deleteThis();
        return true;
    }

    auto shouldRemoveFromCache = [&] {
        // We may still keeps some of these cases in disk cache for history navigation.
        if (response().cacheControlContainsNoStore())
            return true;
        if (isExpired() && !canUseCacheValidator())
            return true;
        return false;
    }();

    if (shouldRemoveFromCache) {
        // Deletes this.
        MemoryCache::singleton().remove(*this);
        return true;
    }

    if (m_data)
        m_data->hintMemoryNotNeededSoon();

    return false;
}

void CachedResource::deleteThis()
{
    RELEASE_ASSERT(canDelete());
    RELEASE_ASSERT(!inCache());

    InspectorInstrumentation::willDestroyCachedResource(*this);

    delete this;
}

void CachedResource::setDecodedSize(unsigned size)
{
    if (size == m_decodedSize)
        return;

    long long delta = static_cast<long long>(size) - m_decodedSize;

    // The object must be moved to a different queue, since its size has been changed.
    // Remove before updating m_decodedSize, so we find the resource in the correct LRU list.
    if (allowsCaching() && inCache())
        MemoryCache::singleton().removeFromLRUList(*this);

    m_decodedSize = size;
   
    if (allowsCaching() && inCache()) {
        auto& memoryCache = MemoryCache::singleton();
        // Now insert into the new LRU list.
        memoryCache.insertInLRUList(*this);
        
        // Insert into or remove from the live decoded list if necessary.
        // When inserting into the LiveDecodedResourcesList it is possible
        // that the m_lastDecodedAccessTime is still zero or smaller than
        // the m_lastDecodedAccessTime of the current list head. This is a
        // violation of the invariant that the list is to be kept sorted
        // by access time. The weakening of the invariant does not pose
        // a problem. For more details please see: https://bugs.webkit.org/show_bug.cgi?id=30209
        bool inLiveDecodedResourcesList = memoryCache.inLiveDecodedResourcesList(*this);
        if (m_decodedSize && !inLiveDecodedResourcesList && hasClients())
            memoryCache.insertInLiveDecodedResourcesList(*this);
        else if (!m_decodedSize && inLiveDecodedResourcesList)
            memoryCache.removeFromLiveDecodedResourcesList(*this);

        // Update the cache's size totals.
        memoryCache.adjustSize(hasClients(), delta);
    }
}

void CachedResource::setEncodedSize(unsigned size)
{
    if (size == m_encodedSize)
        return;

    long long delta = static_cast<long long>(size) - m_encodedSize;

    // The object must be moved to a different queue, since its size has been changed.
    // Remove before updating m_encodedSize, so we find the resource in the correct LRU list.
    if (allowsCaching() && inCache())
        MemoryCache::singleton().removeFromLRUList(*this);

    m_encodedSize = size;

    if (allowsCaching() && inCache()) {
        auto& memoryCache = MemoryCache::singleton();
        memoryCache.insertInLRUList(*this);
        memoryCache.adjustSize(hasClients(), delta);
    }
}

void CachedResource::didAccessDecodedData(MonotonicTime timeStamp)
{
    m_lastDecodedAccessTime = timeStamp;
    
    if (allowsCaching() && inCache()) {
        auto& memoryCache = MemoryCache::singleton();
        if (memoryCache.inLiveDecodedResourcesList(*this)) {
            memoryCache.removeFromLiveDecodedResourcesList(*this);
            memoryCache.insertInLiveDecodedResourcesList(*this);
        }
        memoryCache.pruneSoon();
    }
}
    
void CachedResource::setResourceToRevalidate(CachedResource* resource) 
{ 
    ASSERT(resource);
    ASSERT(!m_resourceToRevalidate);
    ASSERT(resource != this);
    ASSERT(m_handlesToRevalidate.isEmpty());
    ASSERT(resource->type() == type());
    ASSERT(!resource->m_proxyResource);

    LOG(ResourceLoading, "CachedResource %p setResourceToRevalidate %p", this, resource);

    resource->m_proxyResource = this;
    m_resourceToRevalidate = resource;
}

void CachedResource::clearResourceToRevalidate() 
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(m_resourceToRevalidate->m_proxyResource == this);

    if (m_switchingClientsToRevalidatedResource)
        return;

    m_resourceToRevalidate->m_proxyResource = nullptr;
    m_resourceToRevalidate->deleteIfPossible();

    m_handlesToRevalidate.clear();
    m_resourceToRevalidate = nullptr;
    deleteIfPossible();
}
    
void CachedResource::switchClientsToRevalidatedResource()
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(m_resourceToRevalidate->inCache());
    ASSERT(!inCache());

    LOG(ResourceLoading, "CachedResource %p switchClientsToRevalidatedResource %p", this, m_resourceToRevalidate);

    m_switchingClientsToRevalidatedResource = true;
    for (auto& handle : m_handlesToRevalidate) {
        handle->m_resource = m_resourceToRevalidate;
        m_resourceToRevalidate->registerHandle(handle);
        --m_handleCount;
    }
    ASSERT(!m_handleCount);
    m_handlesToRevalidate.clear();

    Vector<WeakPtr<CachedResourceClient>> clientsToMove;
    for (auto entry : m_clients) {
        auto& client = entry.key;
        unsigned count = entry.value;
        while (count) {
            clientsToMove.append(client);
            --count;
        }
    }

    for (auto& client : clientsToMove) {
        if (client)
            removeClient(*client);
    }
    ASSERT(!m_clients.computeSize());

    for (auto& client : clientsToMove) {
        if (client)
            m_resourceToRevalidate->addClientToSet(*client);
    }
    for (auto& client : clientsToMove) {
        // Calling didAddClient may do anything, including trying to cancel revalidation.
        // Assert that it didn't succeed.
        ASSERT(m_resourceToRevalidate);
        // Calling didAddClient for a client may end up removing another client. In that case it won't be in the set anymore.
        if (client && m_resourceToRevalidate->m_clients.contains(*client))
            m_resourceToRevalidate->didAddClient(*client);
    }
    m_switchingClientsToRevalidatedResource = false;
}

void CachedResource::updateResponseAfterRevalidation(const ResourceResponse& validatingResponse)
{
    m_responseTimestamp = WallTime::now();

    updateResponseHeadersAfterRevalidation(m_response, validatingResponse);
}

void CachedResource::registerHandle(CachedResourceHandleBase* h)
{
    ++m_handleCount;
    if (m_resourceToRevalidate)
        m_handlesToRevalidate.add(h);
}

void CachedResource::unregisterHandle(CachedResourceHandleBase* h)
{
    ASSERT(m_handleCount > 0);
    --m_handleCount;

    if (m_resourceToRevalidate)
         m_handlesToRevalidate.remove(h);

    if (!m_handleCount)
        deleteIfPossible();
}

bool CachedResource::canUseCacheValidator() const
{
    if (m_loading || errorOccurred())
        return false;

    if (m_response.cacheControlContainsNoStore())
        return false;
    // Network process will handle revalidation for s-w-r.
    if (m_response.cacheControlStaleWhileRevalidate())
        return false;
    return m_response.hasCacheValidatorFields();
}

CachedResource::RevalidationDecision CachedResource::makeRevalidationDecision(CachePolicy cachePolicy) const
{    
    switch (cachePolicy) {
    case CachePolicy::HistoryBuffer:
        return RevalidationDecision::No;

    case CachePolicy::Reload:
        return RevalidationDecision::YesDueToCachePolicy;

    case CachePolicy::Revalidate:
        if (m_response.cacheControlContainsImmutable() && m_response.url().protocolIs("https"_s)) {
            if (isExpired())
                return RevalidationDecision::YesDueToExpired;
            return RevalidationDecision::No;
        }
        return RevalidationDecision::YesDueToCachePolicy;

    case CachePolicy::Verify:
        if (m_response.cacheControlContainsNoCache())
            return RevalidationDecision::YesDueToNoCache;
        // FIXME: Cache-Control:no-store should prevent storing, not reuse.
        if (m_response.cacheControlContainsNoStore())
            return RevalidationDecision::YesDueToNoStore;

        if (isExpired())
            return RevalidationDecision::YesDueToExpired;

        return RevalidationDecision::No;
    };
    ASSERT_NOT_REACHED();
    return RevalidationDecision::No;
}

bool CachedResource::redirectChainAllowsReuse(ReuseExpiredRedirectionOrNot reuseExpiredRedirection) const
{
    return WebCore::redirectChainAllowsReuse(m_redirectChainCacheStatus, reuseExpiredRedirection);
}

bool CachedResource::varyHeaderValuesMatch(const ResourceRequest& request)
{
    if (m_varyingHeaderValues.isEmpty())
        return true;

    return verifyVaryingRequestHeaders(cookieJar(), m_varyingHeaderValues, request);
}

unsigned CachedResource::overheadSize() const
{
    static const int kAverageClientsHashMapSize = 384;
    return sizeof(CachedResource) + m_response.memoryUsage() + kAverageClientsHashMapSize + m_resourceRequest.url().string().length() * 2;
}

void CachedResource::setLoadPriority(const std::optional<ResourceLoadPriority>& loadPriority)
{
    if (loadPriority)
        m_loadPriority = loadPriority.value();
    else
        m_loadPriority = DefaultResourceLoadPriority::forResourceType(type());
}

inline CachedResource::Callback::Callback(CachedResource& resource, CachedResourceClient& client)
    : m_resource(resource)
    , m_client(client)
    , m_timer(*this, &Callback::timerFired)
{
    m_timer.startOneShot(0_s);
}

inline void CachedResource::Callback::cancel()
{
    if (m_timer.isActive())
        m_timer.stop();
}

void CachedResource::Callback::timerFired()
{
    m_resource.didAddClient(m_client);
}

#if ENABLE(SHAREABLE_RESOURCE)

void CachedResource::tryReplaceEncodedData(SharedBuffer& newBuffer)
{
    if (!m_data)
        return;
    
    if (!mayTryReplaceEncodedData())
        return;

    // We have to compare the buffers because we can't tell if the replacement file backed data is for the
    // same resource or if we made a second request with the same URL which gave us a different resource.
    // We have seen this happen for cached POST resources.
    if (*m_data != newBuffer)
        return;

    m_data = Ref { newBuffer };
    didReplaceSharedBufferContents();
}

#endif

#if USE(QUICK_LOOK)

void CachedResource::previewResponseReceived(const ResourceResponse& response)
{
    ASSERT(response.url().protocolIs(QLPreviewProtocol));
    CachedResource::responseReceived(response);
}

#endif

}

#undef PAGE_ID
#undef FRAME_ID
#undef CACHEDRESOURCE_RELEASE_LOG
#undef CACHEDRESOURCE_RELEASE_LOG_WITH_FRAME
