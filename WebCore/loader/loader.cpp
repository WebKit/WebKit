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
#include "CString.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

#define REQUEST_MANAGEMENT_ENABLED 1
#define REQUEST_DEBUG 0

namespace WebCore {

#if REQUEST_MANAGEMENT_ENABLED
// Match the parallel connection count used by the networking layer
// FIXME should not hardcode something like this
static const unsigned maxRequestsInFlightPerHost = 4;
// Having a limit might still help getting more important resources first
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 20;
#else
static const unsigned maxRequestsInFlightPerHost = 10000;
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 10000;
#endif
    
    
Loader::Loader()
    : m_nonHTTPProtocolHost(AtomicString(), maxRequestsInFlightForNonHTTPProtocols)
    , m_requestTimer(this, &Loader::requestTimerFired)
{
}

Loader::~Loader()
{    
    ASSERT_NOT_REACHED();
}
    
Loader::Priority Loader::determinePriority(const CachedResource* resource) const
{
#if REQUEST_MANAGEMENT_ENABLED
    switch (resource->type()) {
        case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
#if ENABLE(XBL)
        case CachedResource::XBL:
#endif
            return High;
        case CachedResource::Script: 
        case CachedResource::FontResource:
            return Medium;
        case CachedResource::ImageResource:
            return Low;
    }
    ASSERT_NOT_REACHED();
    return Low;
#else
    return High;
#endif
}

void Loader::load(DocLoader* docLoader, CachedResource* resource, bool incremental, bool skipCanLoadCheck, bool sendResourceLoadCallbacks)
{
    ASSERT(docLoader);
    Request* request = new Request(docLoader, resource, incremental, skipCanLoadCheck, sendResourceLoadCallbacks);

    Host* host;
    KURL url(resource->url());
    bool isHTTP = url.protocolIs("http") || url.protocolIs("https");
    if (isHTTP) {
        AtomicString hostName = url.host();
        host = m_hosts.get(hostName.impl());
        if (!host) {
            host = new Host(hostName, maxRequestsInFlightPerHost);
            m_hosts.add(hostName.impl(), host);
        }
    } else 
        host = &m_nonHTTPProtocolHost;
    
    bool hadRequests = host->hasRequests();
    Priority priority = determinePriority(resource);
    host->addRequest(request, priority);
    docLoader->incrementRequestCount();

    if (priority > Low || !isHTTP || !hadRequests) {
        // Try to request important resources immediately
        host->servePendingRequests(priority);
    } else {
        // Handle asynchronously so early low priority requests don't get scheduled before later high priority ones
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
    m_requestTimer.stop();
    
    m_nonHTTPProtocolHost.servePendingRequests(minimumPriority);

    Vector<Host*> hostsToServe;
    copyValuesToVector(m_hosts, hostsToServe);
    for (unsigned n = 0; n < hostsToServe.size(); ++n) {
        Host* host = hostsToServe[n];
        if (host->hasRequests())
            host->servePendingRequests(minimumPriority);
        else if (!host->processingResource()){
            AtomicString name = host->name();
            delete host;
            m_hosts.remove(name.impl());
        }
    }
}
    
void Loader::cancelRequests(DocLoader* docLoader)
{
    if (m_nonHTTPProtocolHost.hasRequests())
        m_nonHTTPProtocolHost.cancelRequests(docLoader);
    
    Vector<Host*> hostsToCancel;
    copyValuesToVector(m_hosts, hostsToCancel);
    for (unsigned n = 0; n < hostsToCancel.size(); ++n) {
        Host* host = hostsToCancel[n];
        if (host->hasRequests())
            host->cancelRequests(docLoader);
    }

    scheduleServePendingRequests();
    
    if (docLoader->loadInProgress())
        ASSERT(docLoader->requestCount() == 1);
    else
        ASSERT(docLoader->requestCount() == 0);
}
    
Loader::Host::Host(const AtomicString& name, unsigned maxRequestsInFlight)
    : m_name(name)
    , m_maxRequestsInFlight(maxRequestsInFlight)
    , m_numResourcesProcessing(0)
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
    bool serveMore = true;
    for (int priority = High; priority >= minimumPriority && serveMore; --priority)
        servePendingRequests(m_requestsPending[priority], serveMore);
}

void Loader::Host::servePendingRequests(RequestQueue& requestsPending, bool& serveLowerPriority)
{
    while (!requestsPending.isEmpty()) {        
        Request* request = requestsPending.first();
        DocLoader* docLoader = request->docLoader();
        bool resourceIsCacheValidator = request->cachedResource()->isCacheValidator();
        // If the document is fully parsed and there are no pending stylesheets there won't be any more 
        // resources that we would want to push to the front of the queue. Just hand off the remaining resources
        // to the networking layer.
        bool parsedAndStylesheetsKnown = !docLoader->doc()->parsing() && docLoader->doc()->haveStylesheetsLoaded();
        if (!parsedAndStylesheetsKnown && !resourceIsCacheValidator && m_requestsLoading.size() >= m_maxRequestsInFlight) {
            serveLowerPriority = false;
            return;
        }
        requestsPending.removeFirst();
        
        ResourceRequest resourceRequest(request->cachedResource()->url());
        
        if (!request->cachedResource()->accept().isEmpty())
            resourceRequest.setHTTPAccept(request->cachedResource()->accept());
        
        KURL referrer = docLoader->doc()->url();
        if ((referrer.protocolIs("http") || referrer.protocolIs("https")) && referrer.path().isEmpty())
            referrer.setPath("/");
        resourceRequest.setHTTPReferrer(referrer.string());
        FrameLoader::addHTTPOriginIfNeeded(resourceRequest, docLoader->doc()->securityOrigin()->toString());
        
        if (resourceIsCacheValidator) {
            CachedResource* resourceToRevalidate = request->cachedResource()->resourceToRevalidate();
            ASSERT(resourceToRevalidate->canUseCacheValidator());
            ASSERT(resourceToRevalidate->isLoaded());
            const String& lastModified = resourceToRevalidate->response().httpHeaderField("Last-Modified");
            const String& eTag = resourceToRevalidate->response().httpHeaderField("ETag");
            if (!lastModified.isEmpty() || !eTag.isEmpty()) {
                ASSERT(docLoader->cachePolicy() != CachePolicyReload);
                if (docLoader->cachePolicy() == CachePolicyRevalidate)
                    resourceRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
                if (!lastModified.isEmpty())
                    resourceRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
                if (!eTag.isEmpty())
                    resourceRequest.setHTTPHeaderField("If-None-Match", eTag);
            }
        }

        RefPtr<SubresourceLoader> loader = SubresourceLoader::create(docLoader->doc()->frame(),
                                                                     this, resourceRequest, request->shouldSkipCanLoadCheck(), request->sendResourceLoadCallbacks());
        if (loader) {
            m_requestsLoading.add(loader.release(), request);
            request->cachedResource()->setRequestedFromNetworkingLayer();
#if REQUEST_DEBUG
            printf("HOST %s COUNT %d LOADING %s\n", resourceRequest.url().host().latin1().data(), m_requestsLoading.size(), request->cachedResource()->url().latin1().data());
#endif
        } else {            
            docLoader->decrementRequestCount();
            docLoader->setLoadInProgress(true);
            request->cachedResource()->error();
            docLoader->setLoadInProgress(false);
            delete request;
        }
    }
}

void Loader::Host::didFinishLoading(SubresourceLoader* loader)
{
    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    ProcessingResource processingResource(this);

    Request* request = i->second;
    m_requestsLoading.remove(i);
    DocLoader* docLoader = request->docLoader();
    // Prevent the document from being destroyed before we are done with
    // the docLoader that it will delete when the document gets deleted.
    DocPtr<Document> protector(docLoader->doc());
    if (!request->isMultipart())
        docLoader->decrementRequestCount();

    CachedResource* resource = request->cachedResource();
    ASSERT(!resource->resourceToRevalidate());

    // If we got a 4xx response, we're pretending to have received a network
    // error, so we can't send the successful data() and finish() callbacks.
    if (!resource->errorOccurred()) {
        docLoader->setLoadInProgress(true);
        resource->data(loader->resourceData(), true);
        resource->finish();
    }

    delete request;

    docLoader->setLoadInProgress(false);
    
    docLoader->checkForPendingPreloads();

#if REQUEST_DEBUG
    KURL u(resource->url());
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
    loader->clearClient();

    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;

    ProcessingResource processingResource(this);
    
    Request* request = i->second;
    m_requestsLoading.remove(i);
    DocLoader* docLoader = request->docLoader();
    // Prevent the document from being destroyed before we are done with
    // the docLoader that it will delete when the document gets deleted.
    DocPtr<Document> protector(docLoader->doc());
    if (!request->isMultipart())
        docLoader->decrementRequestCount();

    CachedResource* resource = request->cachedResource();
    
    if (resource->resourceToRevalidate())
        cache()->revalidationFailed(resource);

    if (!cancelled) {
        docLoader->setLoadInProgress(true);
        resource->error();
    }
    
    docLoader->setLoadInProgress(false);
    if (cancelled || !resource->isPreloaded())
        cache()->remove(resource);
    
    delete request;
    
    docLoader->checkForPendingPreloads();

    servePendingRequests();
}

void Loader::Host::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
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
            request->docLoader()->decrementRequestCount();

            // Existing resource is ok, just use it updating the expiration time.
            cache()->revalidationSucceeded(resource, response);
            
            if (request->docLoader()->frame())
                request->docLoader()->frame()->loader()->checkCompleted();

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
        if (request->docLoader()->frame())
            request->docLoader()->frame()->loader()->checkCompleted();
    } else if (response.isMultipart()) {
        request->setIsMultipart(true);
        
        // We don't count multiParts in a DocLoader's request count
        request->docLoader()->decrementRequestCount();

        // If we get a multipart response, we must have a handle
        ASSERT(loader->handle());
        if (!resource->isImage())
            loader->handle()->cancel();
    }
}

void Loader::Host::didReceiveData(SubresourceLoader* loader, const char* data, int size)
{
    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* resource = request->cachedResource();    
    ASSERT(!resource->isCacheValidator());
    
    if (resource->errorOccurred())
        return;
    
    ProcessingResource processingResource(this);
    
    if (resource->response().httpStatusCode() / 100 == 4) {
        // Treat a 4xx response like a network error.
        resource->error();
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
    
void Loader::Host::cancelPendingRequests(RequestQueue& requestsPending, DocLoader* docLoader)
{
    RequestQueue remaining;
    RequestQueue::iterator end = requestsPending.end();
    for (RequestQueue::iterator it = requestsPending.begin(); it != end; ++it) {
        Request* request = *it;
        if (request->docLoader() == docLoader) {
            cache()->remove(request->cachedResource());
            delete request;
            docLoader->decrementRequestCount();
        } else
            remaining.append(request);
    }
    requestsPending.swap(remaining);
}

void Loader::Host::cancelRequests(DocLoader* docLoader)
{
    for (unsigned p = 0; p <= High; p++)
        cancelPendingRequests(m_requestsPending[p], docLoader);

    Vector<SubresourceLoader*, 256> loadersToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->docLoader() == docLoader)
            loadersToCancel.append(i->first.get());
    }

    for (unsigned i = 0; i < loadersToCancel.size(); ++i) {
        SubresourceLoader* loader = loadersToCancel[i];
        didFail(loader, true);
    }
}

} //namespace WebCore
