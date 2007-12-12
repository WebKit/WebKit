/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SubresourceLoader.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

Loader::Loader()
{
    m_requestsPending.setAutoDelete(true);
}

Loader::~Loader()
{
    deleteAllValues(m_requestsLoading);
}

void Loader::load(DocLoader* dl, CachedResource* object, bool incremental, bool skipCanLoadCheck, bool sendResourceLoadCallbacks)
{
    ASSERT(dl);
    Request* req = new Request(dl, object, incremental, skipCanLoadCheck, sendResourceLoadCallbacks);
    m_requestsPending.append(req);
    dl->incrementRequestCount();
    servePendingRequests();
}

void Loader::servePendingRequests()
{
    while (!m_requestsPending.isEmpty()) {
        // get the first pending request
        Request* req = m_requestsPending.take(0);
        DocLoader* dl = req->docLoader();
        dl->decrementRequestCount();

        ResourceRequest request(req->cachedResource()->url());

        if (!req->cachedResource()->accept().isEmpty())
            request.setHTTPAccept(req->cachedResource()->accept());

        KURL r = dl->doc()->url();
        if (r.protocol().startsWith("http") && r.path().isEmpty())
            r.setPath("/");
        request.setHTTPReferrer(r.string());
        DeprecatedString domain = r.host();
        if (dl->doc()->isHTMLDocument())
            domain = static_cast<HTMLDocument*>(dl->doc())->domain().deprecatedString();
        
        RefPtr<SubresourceLoader> loader = SubresourceLoader::create(dl->doc()->frame(),
            this, request, req->shouldSkipCanLoadCheck(), req->sendResourceLoadCallbacks());

        if (loader) {
            m_requestsLoading.add(loader.release(), req);
            dl->incrementRequestCount();
            break;
        }

        dl->setLoadInProgress(true);
        req->cachedResource()->error();
        dl->setLoadInProgress(false);

        delete req;
    }
}

void Loader::didFinishLoading(SubresourceLoader* loader)
{
    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;

    Request* req = i->second;
    m_requestsLoading.remove(i);
    DocLoader* docLoader = req->docLoader();
    if (!req->isMultipart())
        docLoader->decrementRequestCount();

    CachedResource* object = req->cachedResource();

    docLoader->setLoadInProgress(true);
    object->data(loader->resourceData(), true);
    docLoader->setLoadInProgress(false);
    object->finish();

    delete req;

    servePendingRequests();
}

void Loader::didFail(SubresourceLoader* loader, const ResourceError&)
{
    didFail(loader);
}

void Loader::didFail(SubresourceLoader* loader, bool cancelled)
{
    RequestMap::iterator i = m_requestsLoading.find(loader);
    if (i == m_requestsLoading.end())
        return;

    Request* req = i->second;
    m_requestsLoading.remove(i);
    DocLoader* docLoader = req->docLoader();
    if (!req->isMultipart())
        docLoader->decrementRequestCount();

    CachedResource* object = req->cachedResource();

    if (!cancelled) {
        docLoader->setLoadInProgress(true);
        object->error();
    }
    
    docLoader->setLoadInProgress(false);
    cache()->remove(object);

    delete req;

    servePendingRequests();
}

void Loader::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    Request* req = m_requestsLoading.get(loader);
    
    // FIXME: This is a workaround for <rdar://problem/5236843>
    // If a load starts while the frame is still in the provisional state 
    // (this can be the case when loading the user style sheet), committing the load then causes all
    // requests to be removed from the m_requestsLoading map. This means that req might be null here.
    // In that case we just return early. 
    // ASSERT(req);
    if (!req)
        return;
    req->cachedResource()->setResponse(response);
    
    String encoding = response.textEncodingName();
    if (!encoding.isNull())
        req->cachedResource()->setEncoding(encoding);
    
    if (req->isMultipart()) {
        ASSERT(req->cachedResource()->isImage());
        static_cast<CachedImage*>(req->cachedResource())->clear();
        if (req->docLoader()->frame())
            req->docLoader()->frame()->loader()->checkCompleted();
    } else if (response.isMultipart()) {
        req->setIsMultipart(true);
        
        // We don't count multiParts in a DocLoader's request count
        req->docLoader()->decrementRequestCount();
            
        // If we get a multipart response, we must have a handle
        ASSERT(loader->handle());
        if (!req->cachedResource()->isImage())
            loader->handle()->cancel();
    }
}

void Loader::didReceiveData(SubresourceLoader* loader, const char* data, int size)
{
    Request* request = m_requestsLoading.get(loader);
    if (!request)
        return;

    CachedResource* object = request->cachedResource();    

    // Set the data.
    if (request->isMultipart()) {
        // The loader delivers the data in a multipart section all at once, send eof.
        // The resource data will change as the next part is loaded, so we need to make a copy.
        SharedBuffer* copiedData = new SharedBuffer(data, size);
        object->data(copiedData, true);
    } else if (request->isIncremental())
        object->data(loader->resourceData(), false);
}

void Loader::cancelRequests(DocLoader* dl)
{
    DeprecatedPtrListIterator<Request> pIt(m_requestsPending);
    while (pIt.current()) {
        if (pIt.current()->docLoader() == dl) {
            cache()->remove(pIt.current()->cachedResource());
            m_requestsPending.remove(pIt);
            dl->decrementRequestCount();
        } else
            ++pIt;
    }

    Vector<SubresourceLoader*, 256> loadersToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->docLoader() == dl)
            loadersToCancel.append(i->first.get());
    }

    for (unsigned i = 0; i < loadersToCancel.size(); ++i) {
        SubresourceLoader* loader = loadersToCancel[i];
        didFail(loader, true);
    }
    
    if (dl->loadInProgress())
        ASSERT(dl->requestCount() == 1);
    else
        ASSERT(dl->requestCount() == 0);
}

} //namespace WebCore
