/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
#include "loader.h"

#include "Cache.h"
#include "CachedImage.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "InspectorTimelineAgent.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

#define REQUEST_MANAGEMENT_ENABLED 1
#define REQUEST_DEBUG 0

namespace WebCore {

#if REQUEST_MANAGEMENT_ENABLED
// Match the parallel connection count used by the networking layer
static unsigned maxRequestsInFlightPerHost;
// Having a limit might still help getting more important resources first
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 20;
#else
static const unsigned maxRequestsInFlightPerHost = 10000;
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 10000;
#endif

Loader::Loader()
    : m_requestTimer(this, &Loader::requestTimerFired)
    , m_isSuspendingPendingRequests(false)
{
    m_nonHTTPProtocolHost = Host::create(AtomicString(), maxRequestsInFlightForNonHTTPProtocols);
#if REQUEST_MANAGEMENT_ENABLED
    maxRequestsInFlightPerHost = initializeMaximumHTTPConnectionCountPerHost();
#endif
}

Loader::~Loader()
{    
    ASSERT_NOT_REACHED();
}
    
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
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return ResourceRequest::TargetIsPrefetch;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceRequest::TargetIsSubresource;
}

Loader::Priority Loader::determinePriority(const CachedResource* resource) const
{
#if REQUEST_MANAGEMENT_ENABLED
    switch (resource->type()) {
        case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
            return High;
        case CachedResource::Script: 
        case CachedResource::FontResource:
            return Medium;
        case CachedResource::ImageResource:
            return Low;
#if ENABLE(LINK_PREFETCH)
        case CachedResource::LinkPrefetch:
            return VeryLow;
#endif
    }
    ASSERT_NOT_REACHED();
    return Low;
#else
    return High;
#endif
}

void Loader::load(CachedResourceLoader* cachedResourceLoader, CachedResource* resource, bool incremental, SecurityCheckPolicy securityCheck, bool sendResourceLoadCallbacks)
{
    ASSERT(cachedResourceLoader);
    Request* request = new Request(cachedResourceLoader, resource, incremental, securityCheck, sendResourceLoadCallbacks);

    RefPtr<Host> host;
    KURL url(ParsedURLString, resource->url());
    if (url.protocolInHTTPFamily()) {
        m_hosts.checkConsistency();
        AtomicString hostName = url.host();
        host = m_hosts.get(hostName.impl());
        if (!host) {
            host = Host::create(hostName, maxRequestsInFlightPerHost);
            m_hosts.add(hostName.impl(), host);
        }
    } else 
        host = m_nonHTTPProtocolHost;
    
    bool hadRequests = host->hasRequests();
    Priority priority = determinePriority(resource);
    host->addRequest(request, priority);
    cachedResourceLoader->incrementRequestCount(request->cachedResource());

    if (priority > Low || !url.protocolInHTTPFamily() || (priority == Low && !hadRequests)) {
        // Try to request important resources immediately
        host->servePendingRequests(priority);
    } else {
        // Handle asynchronously so early low priority requests don't get scheduled before later high priority ones
#if ENABLE(INSPECTOR)
        if (InspectorTimelineAgent::instanceCount()) {
            InspectorTimelineAgent* agent = cachedResourceLoader->doc()->inspectorTimelineAgent();
            if (agent)
                agent->didScheduleResourceRequest(resource->url());
        }
#endif // ENABLE(INSPECTOR)
        scheduleServePendingRequests();
    }
}
    
void Loader::scheduleServePendingRequests()
{
    if (!m_requestTimer.isActive())
        m_requestTimer.startOneShot(0);
}

void Loader::requestTimerFired(Timer<Loader>*) 
{
    servePendingRequests();
}

void Loader::servePendingRequests(Priority minimumPriority)
{
    if (m_isSuspendingPendingRequests)
        return;

    m_requestTimer.stop();
    
    m_nonHTTPProtocolHost->servePendingRequests(minimumPriority);

    Vector<Host*> hostsToServe;
    m_hosts.checkConsistency();
    HostMap::iterator i = m_hosts.begin();
    HostMap::iterator end = m_hosts.end();
    for (;i != end; ++i)
        hostsToServe.append(i->second.get());
        
    for (unsigned n = 0; n < hostsToServe.size(); ++n) {
        Host* host = hostsToServe[n];
        if (host->hasRequests())
            host->servePendingRequests(minimumPriority);
        else if (!host->processingResource()){
            AtomicString name = host->name();
            m_hosts.remove(name.impl());
        }
    }
}

void Loader::suspendPendingRequests()
{
    ASSERT(!m_isSuspendingPendingRequests);
    m_isSuspendingPendingRequests = true;
}

void Loader::resumePendingRequests()
{
    ASSERT(m_isSuspendingPendingRequests);
    m_isSuspendingPendingRequests = false;
    if (!m_hosts.isEmpty() || m_nonHTTPProtocolHost->hasRequests())
        scheduleServePendingRequests();
}

void Loader::nonCacheRequestInFlight(const KURL& url)
{
    if (!url.protocolInHTTPFamily())
        return;
    
    AtomicString hostName = url.host();
    m_hosts.checkConsistency();
    RefPtr<Host> host = m_hosts.get(hostName.impl());
    if (!host) {
        host = Host::create(hostName, maxRequestsInFlightPerHost);
        m_hosts.add(hostName.impl(), host);
    }

    host->nonCacheRequestInFlight();
}

void Loader::nonCacheRequestComplete(const KURL& url)
{
    if (!url.protocolInHTTPFamily())
        return;
    
    AtomicString hostName = url.host();
    m_hosts.checkConsistency();
    RefPtr<Host> host = m_hosts.get(hostName.impl());
    ASSERT(host);
    if (!host)
        return;

    host->nonCacheRequestComplete();
}

void Loader::cancelRequests(CachedResourceLoader* cachedResourceLoader)
{
    cachedResourceLoader->clearPendingPreloads();

    if (m_nonHTTPProtocolHost->hasRequests())
        m_nonHTTPProtocolHost->cancelRequests(cachedResourceLoader);
    
    Vector<Host*> hostsToCancel;
    m_hosts.checkConsistency();
    HostMap::iterator i = m_hosts.begin();
    HostMap::iterator end = m_hosts.end();
    for (;i != end; ++i)
        hostsToCancel.append(i->second.get());

    for (unsigned n = 0; n < hostsToCancel.size(); ++n) {
        Host* host = hostsToCancel[n];
        if (host->hasRequests())
            host->cancelRequests(cachedResourceLoader);
    }

    scheduleServePendingRequests();
    
    ASSERT(cachedResourceLoader->requestCount() == (cachedResourceLoader->loadInProgress() ? 1 : 0));
}

Loader::Host::Host(const AtomicString& name, unsigned maxRequestsInFlight)
    : m_name(name)
    , m_maxRequestsInFlight(maxRequestsInFlight)
    , m_numResourcesProcessing(0)
    , m_nonCachedRequestsInFlight(0)
{
}

Loader::Host::~Host()
{
    ASSERT(m_requestsLoading.isEmpty());
    for (unsigned p = 0; p <= High; p++)
        ASSERT(m_requestsPending[p].isEmpty());
}
    
void Loader::Host::addRequest(Request* request, Priority priority)
{
    m_requestsPending[priority].append(request);
}
    
void Loader::Host::nonCacheRequestInFlight()
{
    ++m_nonCachedRequestsInFlight;
}

void Loader::Host::nonCacheRequestComplete()
{
    --m_nonCachedRequestsInFlight;
    ASSERT(m_nonCachedRequestsInFlight >= 0);

    cache()->loader()->scheduleServePendingRequests();
}

bool Loader::Host::hasRequests() const
{
    if (!m_requestsLoading.isEmpty())
        return true;
    for (unsigned p = 0; p <= High; p++) {
        if (!m_requestsPending[p].isEmpty())
            return true;
    }
    return false;
}

void Loader::Host::servePendingRequests(Loader::Priority minimumPriority)
{
    if (cache()->loader()->isSuspendingPendingRequests())
        return;

    bool serveMore = true;
    for (int priority = High; priority >= minimumPriority && serveMore; --priority)
        servePendingRequests(m_requestsPending[priority], serveMore);
}

void Loader::Host::servePendingRequests(RequestQueue& requestsPending, bool& serveLowerPriority)
{
    while (!requestsPending.isEmpty()) {        
        Request* request = requestsPending.first();
        CachedResourceLoader* cachedResourceLoader = request->cachedResourceLoader();
        bool resourceIsCacheValidator = request->cachedResource()->isCacheValidator();

        // For named hosts - which are only http(s) hosts - we should always enforce the connection limit.
        // For non-named hosts - everything but http(s) - we should only enforce the limit if the document isn't done parsing 
        // and we don't know all stylesheets yet.
        bool shouldLimitRequests = !m_name.isNull() || cachedResourceLoader->doc()->parsing() || !cachedResourceLoader->doc()->haveStylesheetsLoaded();
        if (shouldLimitRequests && m_requestsLoading.size() + m_nonCachedRequestsInFlight >= m_maxRequestsInFlight) {
            serveLowerPriority = false;
            return;
        }
        requestsPending.removeFirst();
        
        ResourceRequest resourceRequest(request->cachedResource()->url());
        resourceRequest.setTargetType(cachedResourceTypeToTargetType(request->cachedResource()->type()));
        
        if (!request->cachedResource()->accept().isEmpty())
            resourceRequest.setHTTPAccept(request->cachedResource()->accept());
        
         // Do not set the referrer or HTTP origin here. That's handled by SubresourceLoader::create.
        
        if (resourceIsCacheValidator) {
            CachedResource* resourceToRevalidate = request->cachedResource()->resourceToRevalidate();
            ASSERT(resourceToRevalidate->canUseCacheValidator());
            ASSERT(resourceToRevalidate->isLoaded());
            const String& lastModified = resourceToRevalidate->response().httpHeaderField("Last-Modified");
            const String& eTag = resourceToRevalidate->response().httpHeaderField("ETag");
            if (!lastModified.isEmpty() || !eTag.isEmpty()) {
                ASSERT(cachedResourceLoader->cachePolicy() != CachePolicyReload);
                if (cachedResourceLoader->cachePolicy() == CachePolicyRevalidate)
                    resourceRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
                if (!lastModified.isEmpty())
                    resourceRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
                if (!eTag.isEmpty())
                    resourceRequest.setHTTPHeaderField("If-None-Match", eTag);
            }
        }

        RefPtr<SubresourceLoader> loader = SubresourceLoader::create(cachedResourceLoader->doc()->frame(),
            this, resourceRequest, request->shouldDoSecurityCheck(), request->sendResourceLoadCallbacks());
        if (loader) {
            m_requestsLoading.add(loader.release(), request);
            request->cachedResource()->setRequestedFromNetworkingLayer();
#if REQUEST_DEBUG
            printf("HOST %s COUNT %d LOADING %s\n", resourceRequest.url().host().latin1().data(), m_requestsLoading.size(), request->cachedResource()->url().latin1().data());
#endif
        } else {            
            cachedResourceLoader->decrementRequestCount(request->cachedResource());
            cachedResourceLoader->setLoadInProgress(true);
            request->cachedResource()->error();
            cachedResourceLoader->setLoadInProgress(false);
            delete request;
        }
    }
}

void Loader::Host::didFinishLoading(SubresourceLoader* loader)
{
    RefPtr<Host> myProtector(this);

    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    Request* request = i->second;
    m_requestsLoading.remove(i);
    CachedResourceLoader* cachedResourceLoader = request->cachedResourceLoader();
    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(cachedResourceLoader->doc());
    if (!request->isMultipart())
        cachedResourceLoader->decrementRequestCount(request->cachedResource());

    CachedResource* resource = request->cachedResource();
    ASSERT(!resource->resourceToRevalidate());

    // If we got a 4xx response, we're pretending to have received a network
    // error, so we can't send the successful data() and finish() callbacks.
    if (!resource->errorOccurred()) {
        cachedResourceLoader->setLoadInProgress(true);
        resource->data(loader->resourceData(), true);
        resource->finish();
    }

    delete request;

    cachedResourceLoader->setLoadInProgress(false);
    
    cachedResourceLoader->checkForPendingPreloads();

#if REQUEST_DEBUG
    KURL u(ParsedURLString, resource->url());
    printf("HOST %s COUNT %d RECEIVED %s\n", u.host().latin1().data(), m_requestsLoading.size(), resource->url().latin1().data());
#endif
    servePendingRequests();
}

void Loader::Host::didFail(SubresourceLoader* loader, const ResourceError&)
{
    didFail(loader);
}

void Loader::Host::didFail(SubresourceLoader* loader, bool cancelled)
{
    RefPtr<Host> myProtector(this);

    loader->clearClient();

    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    Request* request = i->second;
    m_requestsLoading.remove(i);
    CachedResourceLoader* cachedResourceLoader = request->cachedResourceLoader();
    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(cachedResourceLoader->doc());
    if (!request->isMultipart())
        cachedResourceLoader->decrementRequestCount(request->cachedResource());

    CachedResource* resource = request->cachedResource();
    
    if (resource->resourceToRevalidate())
        cache()->revalidationFailed(resource);

    if (!cancelled) {
        cachedResourceLoader->setLoadInProgress(true);
        resource->error();
    }
    
    cachedResourceLoader->setLoadInProgress(false);
    if (cancelled || !resource->isPreloaded())
        cache()->remove(resource);
    
    delete request;
    
    cachedResourceLoader->checkForPendingPreloads();

    servePendingRequests();
}

void Loader::Host::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    RefPtr<Host> protector(this);

    Request* request = m_requestsLoading.get(loader);
    
    // FIXME: This is a workaround for <rdar://problem/5236843>
    // If a load starts while the frame is still in the provisional state 
    // (this can be the case when loading the user style sheet), committing the load then causes all
    // requests to be removed from the m_requestsLoading map. This means that request might be null here.
    // In that case we just return early. 
    // ASSERT(request);
    if (!request)
        return;
    
    CachedResource* resource = request->cachedResource();
    
    if (resource->isCacheValidator()) {
        if (response.httpStatusCode() == 304) {
            // 304 Not modified / Use local copy
            m_requestsLoading.remove(loader);
            loader->clearClient();
            request->cachedResourceLoader()->decrementRequestCount(request->cachedResource());

            // Existing resource is ok, just use it updating the expiration time.
            cache()->revalidationSucceeded(resource, response);
            
            if (request->cachedResourceLoader()->frame())
                request->cachedResourceLoader()->frame()->loader()->checkCompleted();

            delete request;

            servePendingRequests();
            return;
        } 
        // Did not get 304 response, continue as a regular resource load.
        cache()->revalidationFailed(resource);
    }

    resource->setResponse(response);

    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        resource->setEncoding(encoding);
    
    if (request->isMultipart()) {
        ASSERT(resource->isImage());
        static_cast<CachedImage*>(resource)->clear();
        if (request->cachedResourceLoader()->frame())
            request->cachedResourceLoader()->frame()->loader()->checkCompleted();
    } else if (response.isMultipart()) {
        request->setIsMultipart(true);
        
        // We don't count multiParts in a CachedResourceLoader's request count
        request->cachedResourceLoader()->decrementRequestCount(request->cachedResource());

        // If we get a multipart response, we must have a handle
        ASSERT(loader->handle());
        if (!resource->isImage())
            loader->handle()->cancel();
    }
}

void Loader::Host::didReceiveData(SubresourceLoader* loader, const char* data, int size)
{
    RefPtr<Host> protector(this);

    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* resource = request->cachedResource();    
    ASSERT(!resource->isCacheValidator());
    
    if (resource->errorOccurred())
        return;
        
    if (resource->response().httpStatusCode() >= 400) {
        resource->httpStatusCodeError();
        return;
    }

    // Set the data.
    if (request->isMultipart()) {
        // The loader delivers the data in a multipart section all at once, send eof.
        // The resource data will change as the next part is loaded, so we need to make a copy.
        RefPtr<SharedBuffer> copiedData = SharedBuffer::create(data, size);
        resource->data(copiedData.release(), true);
    } else if (request->isIncremental())
        resource->data(loader->resourceData(), false);
}
    
void Loader::Host::didReceiveCachedMetadata(SubresourceLoader* loader, const char* data, int size)
{
    RefPtr<Host> protector(this);

    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* resource = request->cachedResource();    
    ASSERT(!resource->isCacheValidator());

    resource->setSerializedCachedMetadata(data, size);
}
    
void Loader::Host::cancelPendingRequests(RequestQueue& requestsPending, CachedResourceLoader* cachedResourceLoader)
{
    RequestQueue remaining;
    RequestQueue::iterator end = requestsPending.end();
    for (RequestQueue::iterator it = requestsPending.begin(); it != end; ++it) {
        Request* request = *it;
        if (request->cachedResourceLoader() == cachedResourceLoader) {
            cache()->remove(request->cachedResource());
            cachedResourceLoader->decrementRequestCount(request->cachedResource());
            delete request;
        } else
            remaining.append(request);
    }
    requestsPending.swap(remaining);
}

void Loader::Host::cancelRequests(CachedResourceLoader* cachedResourceLoader)
{
    for (unsigned p = 0; p <= High; p++)
        cancelPendingRequests(m_requestsPending[p], cachedResourceLoader);

    Vector<SubresourceLoader*, 256> loadersToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->cachedResourceLoader() == cachedResourceLoader)
            loadersToCancel.append(i->first.get());
    }

    for (unsigned i = 0; i < loadersToCancel.size(); ++i) {
        SubresourceLoader* loader = loadersToCancel[i];
        didFail(loader, true);
    }
}

} //namespace WebCore
