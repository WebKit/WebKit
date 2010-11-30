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

#include "MemoryCache.h"
#include "CachedImage.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

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

static ResourceLoadScheduler::Priority determinePriority(const CachedResource* resource)
{
    switch (resource->type()) {
        case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
            return ResourceLoadScheduler::High;
        case CachedResource::Script:
        case CachedResource::FontResource:
            return ResourceLoadScheduler::Medium;
        case CachedResource::ImageResource:
            return ResourceLoadScheduler::Low;
#if ENABLE(LINK_PREFETCH)
        case CachedResource::LinkPrefetch:
            return ResourceLoadScheduler::VeryLow;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadScheduler::Low;
}

void Loader::load(CachedResourceLoader* cachedResourceLoader, CachedResource* resource, bool incremental, SecurityCheckPolicy securityCheck, bool sendResourceLoadCallbacks)
{

    ASSERT(cachedResourceLoader);
    Request* request = new Request(cachedResourceLoader, resource, incremental, securityCheck, sendResourceLoadCallbacks);

    cachedResourceLoader->incrementRequestCount(request->cachedResource());

    ResourceRequest resourceRequest(request->cachedResource()->url());
    resourceRequest.setTargetType(cachedResourceTypeToTargetType(request->cachedResource()->type()));

    if (!request->cachedResource()->accept().isEmpty())
        resourceRequest.setHTTPAccept(request->cachedResource()->accept());

    if (request->cachedResource()->isCacheValidator()) {
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
    
#if ENABLE(LINK_PREFETCH)
    if (request->cachedResource()->type() == CachedResource::LinkPrefetch)
        resourceRequest.setHTTPHeaderField("X-Purpose", "prefetch");
#endif

    RefPtr<SubresourceLoader> loader = resourceLoadScheduler()->scheduleSubresourceLoad(cachedResourceLoader->document()->frame(),
        this, resourceRequest, determinePriority(resource), request->shouldDoSecurityCheck(), request->sendResourceLoadCallbacks());
    if (loader && !loader->reachedTerminalState())
        m_requestsLoading.add(loader.release(), request);
    else {
        // FIXME: What if resources in other frames were waiting for this revalidation?
        LOG(ResourceLoading, "Cannot start loading '%s'", request->cachedResource()->url().latin1().data());             
        cachedResourceLoader->decrementRequestCount(request->cachedResource());
        cachedResourceLoader->setLoadInProgress(true);
        if (resource->resourceToRevalidate()) 
            cache()->revalidationFailed(resource); 
        resource->error(CachedResource::LoadError);
        cachedResourceLoader->setLoadInProgress(false);
        delete request;
    }
}

void Loader::cancelRequests(CachedResourceLoader* cachedResourceLoader)
{
    cachedResourceLoader->clearPendingPreloads();

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

void Loader::willSendRequest(SubresourceLoader* loader, ResourceRequest&, const ResourceResponse&)
{
    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    Request* request = i->second;
    request->cachedResource()->setRequestedFromNetworkingLayer();
}

void Loader::didFinishLoading(SubresourceLoader* loader)
{
    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    Request* request = i->second;
    m_requestsLoading.remove(i);
    CachedResourceLoader* cachedResourceLoader = request->cachedResourceLoader();
    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(cachedResourceLoader->document());
    if (!request->isMultipart())
        cachedResourceLoader->decrementRequestCount(request->cachedResource());

    CachedResource* resource = request->cachedResource();
    ASSERT(!resource->resourceToRevalidate());

    LOG(ResourceLoading, "Received '%s'.", resource->url().latin1().data());

    // If we got a 4xx response, we're pretending to have received a network
    // error, so we can't send the successful data() and finish() callbacks.
    if (!resource->errorOccurred()) {
        cachedResourceLoader->setLoadInProgress(true);
        resource->data(loader->resourceData(), true);
        if (!resource->errorOccurred())
            resource->finish();
    }

    delete request;
    cachedResourceLoader->setLoadInProgress(false);    
    cachedResourceLoader->checkForPendingPreloads();
}

void Loader::didFail(SubresourceLoader* loader, const ResourceError&)
{
    didFail(loader);
}

void Loader::didFail(SubresourceLoader* loader, bool cancelled)
{
    loader->clearClient();

    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;
    
    Request* request = i->second;
    m_requestsLoading.remove(i);
    CachedResourceLoader* cachedResourceLoader = request->cachedResourceLoader();
    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(cachedResourceLoader->document());
    if (!request->isMultipart())
        cachedResourceLoader->decrementRequestCount(request->cachedResource());

    CachedResource* resource = request->cachedResource();

    LOG(ResourceLoading, "Failed to load '%s' (cancelled=%d).\n", resource->url().latin1().data(), cancelled);

    if (resource->resourceToRevalidate())
        cache()->revalidationFailed(resource);

    if (!cancelled) {
        cachedResourceLoader->setLoadInProgress(true);
        resource->error(CachedResource::LoadError);
    }
    
    cachedResourceLoader->setLoadInProgress(false);
    if (cancelled || !resource->isPreloaded())
        cache()->remove(resource);
    
    delete request;
    
    cachedResourceLoader->checkForPendingPreloads();
}

void Loader::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
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
            request->cachedResourceLoader()->decrementRequestCount(request->cachedResource());

            // Existing resource is ok, just use it updating the expiration time.
            cache()->revalidationSucceeded(resource, response);
            
            if (request->cachedResourceLoader()->frame())
                request->cachedResourceLoader()->frame()->loader()->checkCompleted();

            delete request;

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

void Loader::didReceiveData(SubresourceLoader* loader, const char* data, int size)
{
    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* resource = request->cachedResource();    
    ASSERT(!resource->isCacheValidator());
    
    if (resource->errorOccurred())
        return;

    if (resource->response().httpStatusCode() >= 400) {
        if (!resource->shouldIgnoreHTTPStatusCodeErrors())
            resource->error(CachedResource::LoadError);
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

void Loader::didReceiveCachedMetadata(SubresourceLoader* loader, const char* data, int size)
{
    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* resource = request->cachedResource();    
    ASSERT(!resource->isCacheValidator());

    resource->setSerializedCachedMetadata(data, size);
}

} //namespace WebCore
