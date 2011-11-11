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
#include "CachedResourceRequest.h"

#include "CachedImage.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/Assertions.h>
#include <wtf/UnusedParam.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

#if PLATFORM(CHROMIUM)
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
    case CachedResource::RawResource:
        return ResourceRequest::TargetIsSubresource;    
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        return ResourceRequest::TargetIsPrefetch;
    case CachedResource::LinkPrerender:
        return ResourceRequest::TargetIsPrerender;
    case CachedResource::LinkSubresource:
        return ResourceRequest::TargetIsSubresource;
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
        return ResourceRequest::TargetIsTextTrack;
#endif
    }
    ASSERT_NOT_REACHED();
    return ResourceRequest::TargetIsSubresource;
}
#endif

CachedResourceRequest::CachedResourceRequest(CachedResourceLoader* cachedResourceLoader, CachedResource* resource)
    : m_cachedResourceLoader(cachedResourceLoader)
    , m_resource(resource)
    , m_multipart(false)
    , m_finishing(false)
{
}

CachedResourceRequest::~CachedResourceRequest()
{
    if (m_loader)
        m_loader->clearClient();
}

PassOwnPtr<CachedResourceRequest> CachedResourceRequest::load(CachedResourceLoader* cachedResourceLoader, CachedResource* resource, const ResourceLoaderOptions& options)
{
    OwnPtr<CachedResourceRequest> request = adoptPtr(new CachedResourceRequest(cachedResourceLoader, resource));

    ResourceRequest resourceRequest = resource->resourceRequest();
#if PLATFORM(CHROMIUM)
    if (resourceRequest.targetType() == ResourceRequest::TargetIsUnspecified)
        resourceRequest.setTargetType(cachedResourceTypeToTargetType(resource->type()));
#endif

    if (!resource->accept().isEmpty())
        resourceRequest.setHTTPAccept(resource->accept());

    if (resource->isCacheValidator()) {
        CachedResource* resourceToRevalidate = resource->resourceToRevalidate();
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
    if (resource->type() == CachedResource::LinkPrefetch || resource->type() == CachedResource::LinkPrerender || resource->type() == CachedResource::LinkSubresource)
        resourceRequest.setHTTPHeaderField("Purpose", "prefetch");
#endif

    ResourceLoadPriority priority = resource->loadPriority();
    resourceRequest.setPriority(priority);

    RefPtr<SubresourceLoader> loader = resourceLoadScheduler()->scheduleSubresourceLoad(cachedResourceLoader->document()->frame(), request.get(), resourceRequest, priority, options);
    if (!loader || loader->reachedTerminalState()) {
        // FIXME: What if resources in other frames were waiting for this revalidation?
        LOG(ResourceLoading, "Cannot start loading '%s'", resource->url().string().latin1().data());
        if (resource->resourceToRevalidate()) 
            memoryCache()->revalidationFailed(resource); 
        resource->error(CachedResource::LoadError);
        return PassOwnPtr<CachedResourceRequest>(nullptr);
    }
    request->m_loader = loader;
    return request.release();
}

void CachedResourceRequest::willSendRequest(SubresourceLoader* loader, ResourceRequest& req, const ResourceResponse& response)
{
    if (!m_cachedResourceLoader->canRequest(m_resource->type(), req.url())) {
        loader->cancel();
        return;
    }
    m_resource->willSendRequest(req, response);
}

void CachedResourceRequest::cancel()
{
    if (m_finishing)
        return;
    m_loader->cancel();
}

unsigned long CachedResourceRequest::identifier() const
{
    return m_loader->identifier();
}

void CachedResourceRequest::setDefersLoading(bool defers)
{
    if (m_loader)
        m_loader->setDefersLoading(defers);
}

void CachedResourceRequest::didFinishLoading(SubresourceLoader* loader, double finishTime)
{
    if (m_finishing)
        return;

    ASSERT(loader == m_loader.get());
    ASSERT(!m_resource->resourceToRevalidate());
    LOG(ResourceLoading, "Received '%s'.", m_resource->url().string().latin1().data());

    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(m_cachedResourceLoader->document());
    if (!m_multipart)
        m_cachedResourceLoader->decrementRequestCount(m_resource);
    m_finishing = true;

    // If we got a 4xx response, we're pretending to have received a network
    // error, so we can't send the successful data() and finish() callbacks.
    if (!m_resource->errorOccurred()) {
        m_cachedResourceLoader->loadFinishing();
        m_resource->setLoadFinishTime(finishTime);
        m_resource->data(loader->resourceData(), true);
        if (!m_resource->errorOccurred())
            m_resource->finish();
    }
    end();
}

void CachedResourceRequest::didFail(SubresourceLoader*, const ResourceError& error)
{
    if (m_finishing || !m_loader)
        return;

    bool cancelled = error.isCancellation();
    LOG(ResourceLoading, "Failed to load '%s' (cancelled=%d).\n", m_resource->url().string().latin1().data(), cancelled);

    // Prevent the document from being destroyed before we are done with
    // the cachedResourceLoader that it will delete when the document gets deleted.
    RefPtr<Document> protector(m_cachedResourceLoader->document());
    if (!m_multipart)
        m_cachedResourceLoader->decrementRequestCount(m_resource);
    m_finishing = true;
    m_loader->clearClient();

    if (m_resource->resourceToRevalidate())
        memoryCache()->revalidationFailed(m_resource);

    if (!cancelled) {
        m_cachedResourceLoader->loadFinishing();
        m_resource->error(CachedResource::LoadError);
    }

    if (cancelled || !m_resource->isPreloaded())
        memoryCache()->remove(m_resource);
    
    end();
}

void CachedResourceRequest::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    ASSERT(loader == m_loader.get());
    if (m_resource->isCacheValidator()) {
        if (response.httpStatusCode() == 304) {
            // 304 Not modified / Use local copy
            loader->clearClient();
            RefPtr<Document> protector(m_cachedResourceLoader->document());
            m_cachedResourceLoader->decrementRequestCount(m_resource);
            m_finishing = true;

            // Existing resource is ok, just use it updating the expiration time.
            memoryCache()->revalidationSucceeded(m_resource, response);
            
            if (m_cachedResourceLoader->frame())
                m_cachedResourceLoader->frame()->loader()->checkCompleted();

            end();
            return;
        } 
        // Did not get 304 response, continue as a regular resource load.
        memoryCache()->revalidationFailed(m_resource);
    }

    // setResponse() might cancel the request, so we need to keep ourselves alive. Unfortunately,
    // we can't protect a CachedResourceRequest, but we can protect m_resource, which has the
    // power to kill the CachedResourceRequest (and will switch isLoading() to false if it does so).
    CachedResourceHandle<CachedResource> protect(m_resource);
    m_resource->setResponse(response);
    if (!protect->isLoading())
        return;

    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        m_resource->setEncoding(encoding);
    
    if (m_multipart) {
        ASSERT(m_resource->isImage());
        static_cast<CachedImage*>(m_resource)->clear();
        if (m_cachedResourceLoader->frame())
            m_cachedResourceLoader->frame()->loader()->checkCompleted();
    } else if (response.isMultipart()) {
        m_multipart = true;
        
        // We don't count multiParts in a CachedResourceLoader's request count
        m_cachedResourceLoader->decrementRequestCount(m_resource);

        // If we get a multipart response, we must have a handle
        ASSERT(loader->handle());
        if (!m_resource->isImage())
            loader->handle()->cancel();
    }
}

void CachedResourceRequest::didReceiveData(SubresourceLoader* loader, const char* data, int size)
{
    ASSERT(loader == m_loader.get());
    ASSERT(!m_resource->isCacheValidator());
    
    if (m_resource->errorOccurred())
        return;

    if (m_resource->response().httpStatusCode() >= 400 && !m_resource->shouldIgnoreHTTPStatusCodeErrors()) {
        // Prevent the document from being destroyed before we are done with
        // the cachedResourceLoader that it will delete when the document gets deleted.
        RefPtr<Document> protector(m_cachedResourceLoader->document());
        if (!m_multipart)
            m_cachedResourceLoader->decrementRequestCount(m_resource);
        m_finishing = true;
        m_loader->clearClient();
        m_resource->error(CachedResource::LoadError);
        end();
        return;
    }

    // There are two cases where we might need to create our own SharedBuffer instead of copying the one in ResourceLoader.
    // (1) Multipart content: The loader delivers the data in a multipart section all at once, then sends eof.
    //     The resource data will change as the next part is loaded, so we need to make a copy.
    // (2) Our client requested that the data not be buffered at the ResourceLoader level via ResourceLoaderOptions. In this case,
    //     ResourceLoader::resourceData() will be null. However, unlike the multipart case, we don't want to tell the CachedResource
    //     that all data has been received yet.
    if (m_multipart || !loader->resourceData()) {
        RefPtr<SharedBuffer> copiedData = SharedBuffer::create(data, size);
        m_resource->data(copiedData.release(), m_multipart);
    } else
        m_resource->data(loader->resourceData(), false);
}

void CachedResourceRequest::didReceiveCachedMetadata(SubresourceLoader*, const char* data, int size)
{
    ASSERT(!m_resource->isCacheValidator());
    m_resource->setSerializedCachedMetadata(data, size);
}

void CachedResourceRequest::didSendData(SubresourceLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

void CachedResourceRequest::end()
{
    m_cachedResourceLoader->loadDone();
    m_resource->stopLoading();
}

} //namespace WebCore
