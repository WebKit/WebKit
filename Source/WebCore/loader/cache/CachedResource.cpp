/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.

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

#include "MemoryCache.h"
#include "CachedMetadata.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceHandle.h"
#include "CachedResourceLoader.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "KURL.h"
#include "Logging.h"
#include "MemoryInstrumentation.h"
#include "PurgeableBuffer.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {
    
static ResourceLoadPriority defaultPriorityForResourceType(CachedResource::Type type)
{
    switch (type) {
        case CachedResource::CSSStyleSheet:
            return ResourceLoadPriorityHigh;
        case CachedResource::Script:
        case CachedResource::FontResource:
        case CachedResource::RawResource:
            return ResourceLoadPriorityMedium;
        case CachedResource::ImageResource:
            return ResourceLoadPriorityLow;
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
            return ResourceLoadPriorityHigh;
#endif
#if ENABLE(SVG)
        case CachedResource::SVGDocumentResource:
            return ResourceLoadPriorityLow;
#endif
#if ENABLE(LINK_PREFETCH)
        case CachedResource::LinkPrefetch:
            return ResourceLoadPriorityVeryLow;
        case CachedResource::LinkSubresource:
            return ResourceLoadPriorityVeryLow;
#endif
#if ENABLE(VIDEO_TRACK)
        case CachedResource::TextTrackResource:
            return ResourceLoadPriorityLow;
#endif
#if ENABLE(CSS_SHADERS)
        case CachedResource::ShaderResource:
            return ResourceLoadPriorityMedium;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadPriorityLow;
}

#if PLATFORM(CHROMIUM) || PLATFORM(BLACKBERRY)
static ResourceRequest::TargetType cachedResourceTypeToTargetType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return ResourceRequest::TargetIsStyleSheet;
    case CachedResource::Script: 
        return ResourceRequest::TargetIsScript;
    case CachedResource::FontResource:
        return ResourceRequest::TargetIsFontResource;
    case CachedResource::ImageResource:
        return ResourceRequest::TargetIsImage;
#if ENABLE(CSS_SHADERS)
    case CachedResource::ShaderResource:
#endif
    case CachedResource::RawResource:
        return ResourceRequest::TargetIsSubresource;    
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return ResourceRequest::TargetIsPrefetch;
    case CachedResource::LinkSubresource:
        return ResourceRequest::TargetIsSubresource;
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
        return ResourceRequest::TargetIsTextTrack;
#endif
#if ENABLE(SVG)
    case CachedResource::SVGDocumentResource:
        return ResourceRequest::TargetIsImage;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceRequest::TargetIsSubresource;
}
#endif

DEFINE_DEBUG_ONLY_GLOBAL(RefCountedLeakCounter, cachedResourceLeakCounter, ("CachedResource"));

CachedResource::CachedResource(const ResourceRequest& request, Type type)
    : m_resourceRequest(request)
    , m_loadPriority(defaultPriorityForResourceType(type))
    , m_responseTimestamp(currentTime())
    , m_decodedDataDeletionTimer(this, &CachedResource::decodedDataDeletionTimerFired)
    , m_lastDecodedAccessTime(0)
    , m_loadFinishTime(0)
    , m_encodedSize(0)
    , m_decodedSize(0)
    , m_accessCount(0)
    , m_handleCount(0)
    , m_preloadCount(0)
    , m_preloadResult(PreloadNotReferenced)
    , m_inLiveDecodedResourcesList(false)
    , m_requestedFromNetworkingLayer(false)
    , m_inCache(false)
    , m_loading(false)
    , m_switchingClientsToRevalidatedResource(false)
    , m_type(type)
    , m_status(Pending)
#ifndef NDEBUG
    , m_deleted(false)
    , m_lruIndex(0)
#endif
    , m_nextInAllResourcesList(0)
    , m_prevInAllResourcesList(0)
    , m_nextInLiveResourcesList(0)
    , m_prevInLiveResourcesList(0)
    , m_owningCachedResourceLoader(0)
    , m_resourceToRevalidate(0)
    , m_proxyResource(0)
{
    ASSERT(m_type == unsigned(type)); // m_type is a bitfield, so this tests careless updates of the enum.
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
    ASSERT(url().isNull() || memoryCache()->resourceForURL(KURL(ParsedURLString, url())) != this);

#ifndef NDEBUG
    m_deleted = true;
    cachedResourceLeakCounter.decrement();
#endif

    if (m_owningCachedResourceLoader)
        m_owningCachedResourceLoader->removeCachedResource(this);
}

void CachedResource::load(CachedResourceLoader* cachedResourceLoader, const ResourceLoaderOptions& options)
{
    m_options = options;
    m_loading = true;

#if PLATFORM(CHROMIUM) || PLATFORM(BLACKBERRY)
    if (m_resourceRequest.targetType() == ResourceRequest::TargetIsUnspecified)
        m_resourceRequest.setTargetType(cachedResourceTypeToTargetType(type()));
#endif

    if (!accept().isEmpty())
        m_resourceRequest.setHTTPAccept(accept());

    if (isCacheValidator()) {
        CachedResource* resourceToRevalidate = m_resourceToRevalidate;
        ASSERT(resourceToRevalidate->canUseCacheValidator());
        ASSERT(resourceToRevalidate->isLoaded());
        const String& lastModified = resourceToRevalidate->response().httpHeaderField("Last-Modified");
        const String& eTag = resourceToRevalidate->response().httpHeaderField("ETag");
        if (!lastModified.isEmpty() || !eTag.isEmpty()) {
            ASSERT(cachedResourceLoader->cachePolicy() != CachePolicyReload);
            if (cachedResourceLoader->cachePolicy() == CachePolicyRevalidate)
                m_resourceRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
            if (!lastModified.isEmpty())
                m_resourceRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
            if (!eTag.isEmpty())
                m_resourceRequest.setHTTPHeaderField("If-None-Match", eTag);
        }
    }

#if ENABLE(LINK_PREFETCH)
    if (type() == CachedResource::LinkPrefetch || type() == CachedResource::LinkSubresource)
        m_resourceRequest.setHTTPHeaderField("Purpose", "prefetch");
#endif
    m_resourceRequest.setPriority(loadPriority());
    
    m_loader = resourceLoadScheduler()->scheduleSubresourceLoad(cachedResourceLoader->document()->frame(), this, m_resourceRequest, m_resourceRequest.priority(), options);
    if (!m_loader) {
        // FIXME: What if resources in other frames were waiting for this revalidation?
        LOG(ResourceLoading, "Cannot start loading '%s'", url().string().latin1().data());
        if (m_resourceToRevalidate) 
            memoryCache()->revalidationFailed(this); 
        error(CachedResource::LoadError);
        return;
    }

    m_status = Pending;
}

void CachedResource::checkNotify()
{
    if (isLoading())
        return;

    CachedResourceClientWalker<CachedResourceClient> w(m_clients);
    while (CachedResourceClient* c = w.next())
        c->notifyFinished(this);
}

void CachedResource::data(PassRefPtr<SharedBuffer>, bool allDataReceived)
{
    if (!allDataReceived)
        return;
    
    setLoading(false);
    checkNotify();
}

void CachedResource::error(CachedResource::Status status)
{
    setStatus(status);
    ASSERT(errorOccurred());
    m_data.clear();

    setLoading(false);
    checkNotify();
}

void CachedResource::finish()
{
    if (!errorOccurred())
        m_status = Cached;
}

bool CachedResource::passesAccessControlCheck(SecurityOrigin* securityOrigin)
{
    String errorDescription;
    return WebCore::passesAccessControlCheck(m_response, resourceRequest().allowCookies() ? AllowStoredCredentials : DoNotAllowStoredCredentials, securityOrigin, errorDescription);
}

bool CachedResource::isExpired() const
{
    if (m_response.isNull())
        return false;

    return currentAge() > freshnessLifetime();
}
    
double CachedResource::currentAge() const
{
    // RFC2616 13.2.3
    // No compensation for latency as that is not terribly important in practice
    double dateValue = m_response.date();
    double apparentAge = isfinite(dateValue) ? max(0., m_responseTimestamp - dateValue) : 0;
    double ageValue = m_response.age();
    double correctedReceivedAge = isfinite(ageValue) ? max(apparentAge, ageValue) : apparentAge;
    double residentTime = currentTime() - m_responseTimestamp;
    return correctedReceivedAge + residentTime;
}
    
double CachedResource::freshnessLifetime() const
{
    // Cache non-http resources liberally
    if (!m_response.url().protocolIsInHTTPFamily())
        return std::numeric_limits<double>::max();

    // RFC2616 13.2.4
    double maxAgeValue = m_response.cacheControlMaxAge();
    if (isfinite(maxAgeValue))
        return maxAgeValue;
    double expiresValue = m_response.expires();
    double dateValue = m_response.date();
    double creationTime = isfinite(dateValue) ? dateValue : m_responseTimestamp;
    if (isfinite(expiresValue))
        return expiresValue - creationTime;
    double lastModifiedValue = m_response.lastModified();
    if (isfinite(lastModifiedValue))
        return (creationTime - lastModifiedValue) * 0.1;
    // If no cache headers are present, the specification leaves the decision to the UA. Other browsers seem to opt for 0.
    return 0;
}

void CachedResource::setResponse(const ResourceResponse& response)
{
    m_response = response;
    m_responseTimestamp = currentTime();
    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        setEncoding(encoding);
}

void CachedResource::setSerializedCachedMetadata(const char* data, size_t size)
{
    // We only expect to receive cached metadata from the platform once.
    // If this triggers, it indicates an efficiency problem which is most
    // likely unexpected in code designed to improve performance.
    ASSERT(!m_cachedMetadata);

    m_cachedMetadata = CachedMetadata::deserialize(data, size);
}

void CachedResource::setCachedMetadata(unsigned dataTypeID, const char* data, size_t size)
{
    // Currently, only one type of cached metadata per resource is supported.
    // If the need arises for multiple types of metadata per resource this could
    // be enhanced to store types of metadata in a map.
    ASSERT(!m_cachedMetadata);

    m_cachedMetadata = CachedMetadata::create(dataTypeID, data, size);
    ResourceHandle::cacheMetadata(m_response, m_cachedMetadata->serialize());
}

CachedMetadata* CachedResource::cachedMetadata(unsigned dataTypeID) const
{
    if (!m_cachedMetadata || m_cachedMetadata->dataTypeID() != dataTypeID)
        return 0;
    return m_cachedMetadata.get();
}

void CachedResource::stopLoading()
{
    ASSERT(m_loader);            
    m_loader = 0;

    CachedResourceHandle<CachedResource> protect(this);

    // All loads finish with data(allDataReceived = true) or error(), except for
    // canceled loads, which silently set our request to 0. Be sure to notify our
    // client in that case, so we don't seem to continue loading forever.
    if (isLoading()) {
        setLoading(false);
        setStatus(Canceled);
        checkNotify();
    }
}

void CachedResource::addClient(CachedResourceClient* client)
{
    if (addClientToSet(client))
        didAddClient(client);
}

void CachedResource::didAddClient(CachedResourceClient* c)
{
    if (m_decodedDataDeletionTimer.isActive())
        m_decodedDataDeletionTimer.stop();

    if (m_clientsAwaitingCallback.contains(c)) {
        m_clients.add(c);
        m_clientsAwaitingCallback.remove(c);
    }
    if (!isLoading())
        c->notifyFinished(this);
}

bool CachedResource::addClientToSet(CachedResourceClient* client)
{
    ASSERT(!isPurgeable());

    if (m_preloadResult == PreloadNotReferenced) {
        if (isLoaded())
            m_preloadResult = PreloadReferencedWhileComplete;
        else if (m_requestedFromNetworkingLayer)
            m_preloadResult = PreloadReferencedWhileLoading;
        else
            m_preloadResult = PreloadReferenced;
    }
    if (!hasClients() && inCache())
        memoryCache()->addToLiveResourcesSize(this);

    if (m_type == RawResource && !m_response.isNull() && !m_proxyResource) {
        // Certain resources (especially XHRs) do crazy things if an asynchronous load returns
        // synchronously (e.g., scripts may not have set all the state they need to handle the load).
        // Therefore, rather than immediately sending callbacks on a cache hit like other CachedResources,
        // we schedule the callbacks and ensure we never finish synchronously.
        ASSERT(!m_clientsAwaitingCallback.contains(client));
        m_clientsAwaitingCallback.add(client, CachedResourceCallback::schedule(this, client));
        return false;
    }

    m_clients.add(client);
    return true;
}

void CachedResource::removeClient(CachedResourceClient* client)
{
    OwnPtr<CachedResourceCallback> callback = m_clientsAwaitingCallback.take(client);
    if (callback) {
        ASSERT(!m_clients.contains(client));
        callback->cancel();
        callback.clear();
    } else {
        ASSERT(m_clients.contains(client));
        m_clients.remove(client);
        didRemoveClient(client);
    }

    bool deleted = deleteIfPossible();
    if (!deleted && !hasClients() && inCache()) {
        memoryCache()->removeFromLiveResourcesSize(this);
        memoryCache()->removeFromLiveDecodedResourcesList(this);
        allClientsRemoved();
        destroyDecodedDataIfNeeded();
        if (response().cacheControlContainsNoStore()) {
            // RFC2616 14.9.2:
            // "no-store: ... MUST make a best-effort attempt to remove the information from volatile storage as promptly as possible"
            // "... History buffers MAY store such responses as part of their normal operation."
            // We allow non-secure content to be reused in history, but we do not allow secure content to be reused.
            if (protocolIs(url(), "https"))
                memoryCache()->remove(this);
        } else
            memoryCache()->prune();
    }
    // This object may be dead here.
}

void CachedResource::destroyDecodedDataIfNeeded()
{
    if (!m_decodedSize)
        return;

    if (double interval = memoryCache()->deadDecodedDataDeletionInterval())
        m_decodedDataDeletionTimer.startOneShot(interval);
}

void CachedResource::decodedDataDeletionTimerFired(Timer<CachedResource>*)
{
    destroyDecodedData();
}

bool CachedResource::deleteIfPossible()
{
    if (canDelete() && !inCache()) {
        InspectorInstrumentation::willDestroyCachedResource(this);
        delete this;
        return true;
    }
    return false;
}

void CachedResource::setDecodedSize(unsigned size)
{
    if (size == m_decodedSize)
        return;

    int delta = size - m_decodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_decodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        memoryCache()->removeFromLRUList(this);
    
    m_decodedSize = size;
   
    if (inCache()) { 
        // Now insert into the new LRU list.
        memoryCache()->insertInLRUList(this);
        
        // Insert into or remove from the live decoded list if necessary.
        // When inserting into the LiveDecodedResourcesList it is possible
        // that the m_lastDecodedAccessTime is still zero or smaller than
        // the m_lastDecodedAccessTime of the current list head. This is a
        // violation of the invariant that the list is to be kept sorted
        // by access time. The weakening of the invariant does not pose
        // a problem. For more details please see: https://bugs.webkit.org/show_bug.cgi?id=30209
        if (m_decodedSize && !m_inLiveDecodedResourcesList && hasClients())
            memoryCache()->insertInLiveDecodedResourcesList(this);
        else if (!m_decodedSize && m_inLiveDecodedResourcesList)
            memoryCache()->removeFromLiveDecodedResourcesList(this);

        // Update the cache's size totals.
        memoryCache()->adjustSize(hasClients(), delta);
    }
}

void CachedResource::setEncodedSize(unsigned size)
{
    if (size == m_encodedSize)
        return;

    // The size cannot ever shrink (unless it is being nulled out because of an error).  If it ever does, assert.
    ASSERT(size == 0 || size >= m_encodedSize);
    
    int delta = size - m_encodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_encodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        memoryCache()->removeFromLRUList(this);
    
    m_encodedSize = size;
   
    if (inCache()) { 
        // Now insert into the new LRU list.
        memoryCache()->insertInLRUList(this);
        
        // Update the cache's size totals.
        memoryCache()->adjustSize(hasClients(), delta);
    }
}

void CachedResource::didAccessDecodedData(double timeStamp)
{
    m_lastDecodedAccessTime = timeStamp;
    
    if (inCache()) {
        if (m_inLiveDecodedResourcesList) {
            memoryCache()->removeFromLiveDecodedResourcesList(this);
            memoryCache()->insertInLiveDecodedResourcesList(this);
        }
        memoryCache()->prune();
    }
}
    
void CachedResource::setResourceToRevalidate(CachedResource* resource) 
{ 
    ASSERT(resource);
    ASSERT(!m_resourceToRevalidate);
    ASSERT(resource != this);
    ASSERT(m_handlesToRevalidate.isEmpty());
    ASSERT(resource->type() == type());

    LOG(ResourceLoading, "CachedResource %p setResourceToRevalidate %p", this, resource);

    // The following assert should be investigated whenever it occurs. Although it should never fire, it currently does in rare circumstances.
    // https://bugs.webkit.org/show_bug.cgi?id=28604.
    // So the code needs to be robust to this assert failing thus the "if (m_resourceToRevalidate->m_proxyResource == this)" in CachedResource::clearResourceToRevalidate.
    ASSERT(!resource->m_proxyResource);

    resource->m_proxyResource = this;
    m_resourceToRevalidate = resource;
}

void CachedResource::clearResourceToRevalidate() 
{ 
    ASSERT(m_resourceToRevalidate);
    if (m_switchingClientsToRevalidatedResource)
        return;

    // A resource may start revalidation before this method has been called, so check that this resource is still the proxy resource before clearing it out.
    if (m_resourceToRevalidate->m_proxyResource == this) {
        m_resourceToRevalidate->m_proxyResource = 0;
        m_resourceToRevalidate->deleteIfPossible();
    }
    m_handlesToRevalidate.clear();
    m_resourceToRevalidate = 0;
    deleteIfPossible();
}
    
void CachedResource::switchClientsToRevalidatedResource()
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(m_resourceToRevalidate->inCache());
    ASSERT(!inCache());

    LOG(ResourceLoading, "CachedResource %p switchClientsToRevalidatedResource %p", this, m_resourceToRevalidate);

    m_switchingClientsToRevalidatedResource = true;
    HashSet<CachedResourceHandleBase*>::iterator end = m_handlesToRevalidate.end();
    for (HashSet<CachedResourceHandleBase*>::iterator it = m_handlesToRevalidate.begin(); it != end; ++it) {
        CachedResourceHandleBase* handle = *it;
        handle->m_resource = m_resourceToRevalidate;
        m_resourceToRevalidate->registerHandle(handle);
        --m_handleCount;
    }
    ASSERT(!m_handleCount);
    m_handlesToRevalidate.clear();

    Vector<CachedResourceClient*> clientsToMove;
    HashCountedSet<CachedResourceClient*>::iterator end2 = m_clients.end();
    for (HashCountedSet<CachedResourceClient*>::iterator it = m_clients.begin(); it != end2; ++it) {
        CachedResourceClient* client = it->first;
        unsigned count = it->second;
        while (count) {
            clientsToMove.append(client);
            --count;
        }
    }
    // Equivalent of calling removeClient() for all clients
    m_clients.clear();

    unsigned moveCount = clientsToMove.size();
    for (unsigned n = 0; n < moveCount; ++n)
        m_resourceToRevalidate->addClientToSet(clientsToMove[n]);
    for (unsigned n = 0; n < moveCount; ++n) {
        // Calling didAddClient may do anything, including trying to cancel revalidation.
        // Assert that it didn't succeed.
        ASSERT(m_resourceToRevalidate);
        // Calling didAddClient for a client may end up removing another client. In that case it won't be in the set anymore.
        if (m_resourceToRevalidate->m_clients.contains(clientsToMove[n]))
            m_resourceToRevalidate->didAddClient(clientsToMove[n]);
    }
    m_switchingClientsToRevalidatedResource = false;
}
    
void CachedResource::updateResponseAfterRevalidation(const ResourceResponse& validatingResponse)
{
    m_responseTimestamp = currentTime();

    DEFINE_STATIC_LOCAL(const AtomicString, contentHeaderPrefix, ("content-"));
    // RFC2616 10.3.5
    // Update cached headers from the 304 response
    const HTTPHeaderMap& newHeaders = validatingResponse.httpHeaderFields();
    HTTPHeaderMap::const_iterator end = newHeaders.end();
    for (HTTPHeaderMap::const_iterator it = newHeaders.begin(); it != end; ++it) {
        // Don't allow 304 response to update content headers, these can't change but some servers send wrong values.
        if (it->first.startsWith(contentHeaderPrefix, false))
            continue;
        m_response.setHTTPHeaderField(it->first, it->second);
    }
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
    return m_response.hasCacheValidatorFields();
}

bool CachedResource::mustRevalidateDueToCacheHeaders(CachePolicy cachePolicy) const
{    
    ASSERT(cachePolicy == CachePolicyRevalidate || cachePolicy == CachePolicyCache || cachePolicy == CachePolicyVerify);

    if (cachePolicy == CachePolicyRevalidate)
        return true;

    if (m_response.cacheControlContainsNoCache() || m_response.cacheControlContainsNoStore()) {
        LOG(ResourceLoading, "CachedResource %p mustRevalidate because of m_response.cacheControlContainsNoCache() || m_response.cacheControlContainsNoStore()\n", this);
        return true;
    }

    if (cachePolicy == CachePolicyCache) {
        if (m_response.cacheControlContainsMustRevalidate() && isExpired()) {
            LOG(ResourceLoading, "CachedResource %p mustRevalidate because of cachePolicy == CachePolicyCache and m_response.cacheControlContainsMustRevalidate() && isExpired()\n", this);
            return true;
        }
        return false;
    }

    // CachePolicyVerify
    if (isExpired()) {
        LOG(ResourceLoading, "CachedResource %p mustRevalidate because of isExpired()\n", this);
        return true;
    }

    return false;
}

bool CachedResource::isSafeToMakePurgeable() const
{ 
    return !hasClients() && !m_proxyResource && !m_resourceToRevalidate;
}

bool CachedResource::makePurgeable(bool purgeable) 
{ 
    if (purgeable) {
        ASSERT(isSafeToMakePurgeable());

        if (m_purgeableData) {
            ASSERT(!m_data);
            return true;
        }
        if (!m_data)
            return false;
        
        // Should not make buffer purgeable if it has refs other than this since we don't want two copies.
        if (!m_data->hasOneRef())
            return false;
        
        if (m_data->hasPurgeableBuffer()) {
            m_purgeableData = m_data->releasePurgeableBuffer();
        } else {
            m_purgeableData = PurgeableBuffer::create(m_data->data(), m_data->size());
            if (!m_purgeableData)
                return false;
            m_purgeableData->setPurgePriority(purgePriority());
        }
        
        m_purgeableData->makePurgeable(true);
        m_data.clear();
        return true;
    }

    if (!m_purgeableData)
        return true;
    ASSERT(!m_data);
    ASSERT(!hasClients());

    if (!m_purgeableData->makePurgeable(false))
        return false; 

    m_data = SharedBuffer::adoptPurgeableBuffer(m_purgeableData.release());
    return true;
}

bool CachedResource::isPurgeable() const
{
    return m_purgeableData && m_purgeableData->isPurgeable();
}

bool CachedResource::wasPurged() const
{
    return m_purgeableData && m_purgeableData->wasPurged();
}

unsigned CachedResource::overheadSize() const
{
    static const int kAverageClientsHashMapSize = 384;
    return sizeof(CachedResource) + m_response.memoryUsage() + kAverageClientsHashMapSize + m_resourceRequest.url().string().length() * 2;
}
    
void CachedResource::setLoadPriority(ResourceLoadPriority loadPriority) 
{ 
    if (loadPriority == ResourceLoadPriorityUnresolved)
        return;
    m_loadPriority = loadPriority;
}


CachedResource::CachedResourceCallback::CachedResourceCallback(CachedResource* resource, CachedResourceClient* client)
    : m_resource(resource)
    , m_client(client)
    , m_callbackTimer(this, &CachedResourceCallback::timerFired)
{
    m_callbackTimer.startOneShot(0);
}

void CachedResource::CachedResourceCallback::cancel()
{
    if (m_callbackTimer.isActive())
        m_callbackTimer.stop();
}

void CachedResource::CachedResourceCallback::timerFired(Timer<CachedResourceCallback>*)
{
    m_resource->didAddClient(m_client);
}

void CachedResource::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CachedResource);
    info.addMember(m_resourceRequest);
    info.addHashSet(m_clients);
    info.addMember(m_accept);
    info.addInstrumentedMember(m_loader);
    info.addMember(m_response);
    info.addInstrumentedMember(m_data);
    info.addMember(m_cachedMetadata.get());
    info.addInstrumentedMember(m_nextInAllResourcesList);
    info.addInstrumentedMember(m_prevInAllResourcesList);
    info.addInstrumentedMember(m_nextInLiveResourcesList);
    info.addInstrumentedMember(m_prevInLiveResourcesList);
    info.addInstrumentedMember(m_owningCachedResourceLoader);
    info.addInstrumentedMember(m_resourceToRevalidate);
    info.addInstrumentedMember(m_proxyResource);
    info.addInstrumentedHashSet(m_handlesToRevalidate);

    if (m_purgeableData && !m_purgeableData->wasPurged())
        info.addRawBuffer(m_purgeableData.get(), m_purgeableData->size());
}

}
