/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
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
#include "LoaderFunctions.h"
#include "Request.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
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

void Loader::load(DocLoader* dl, CachedResource* object, bool incremental)
{
    Request* req = new Request(dl, object, incremental);
    m_requestsPending.append(req);
    servePendingRequests();
}

void Loader::servePendingRequests()
{
    if (m_requestsPending.count() == 0)
        return;

    // get the first pending request
    Request* req = m_requestsPending.take(0);

    ResourceRequest request(req->cachedResource()->url());

    if (!req->cachedResource()->accept().isEmpty())
        request.setHTTPAccept(req->cachedResource()->accept());
    if (req->docLoader())  {
        KURL r = req->docLoader()->doc()->URL();
        if (r.protocol().startsWith("http") && r.path().isEmpty())
            r.setPath("/");
        request.setHTTPReferrer(r.url());
        DeprecatedString domain = r.host();
        if (req->docLoader()->doc()->isHTMLDocument())
            domain = static_cast<HTMLDocument*>(req->docLoader()->doc())->domain().deprecatedString();
    }
    
    RefPtr<ResourceHandle> loader = ResourceHandle::create(request, this, req->docLoader());

    if (loader)
        m_requestsLoading.add(loader.get(), req);
}

void Loader::receivedAllData(ResourceHandle* job, PlatformData allData)
{
    RequestMap::iterator i = m_requestsLoading.find(job);
    if (i == m_requestsLoading.end())
        return;

    Request* req = i->second;
    m_requestsLoading.remove(i);

    CachedResource* object = req->cachedResource();
    DocLoader* docLoader = req->docLoader();

    docLoader->setLoadInProgress(true);
    object->data(req->buffer(), true);
    object->setAllData(allData);
    docLoader->setLoadInProgress(false);
    object->finish();

    delete req;

    servePendingRequests();
}

void Loader::didFailWithError(ResourceHandle* handle, const ResourceError& error)
{
    RequestMap::iterator i = m_requestsLoading.find(handle);
    if (i == m_requestsLoading.end())
        return;

    Request* req = i->second;
    m_requestsLoading.remove(i);

    CachedResource* object = req->cachedResource();
    DocLoader* docLoader = req->docLoader();

    docLoader->setLoadInProgress(true);
    object->error();
    docLoader->setLoadInProgress(false);
    cache()->remove(object);

    delete req;

    servePendingRequests();
}

void Loader::didReceiveResponse(ResourceHandle* handle, const ResourceResponse& response)
{
    Request* req = m_requestsLoading.get(handle);
    ASSERT(req);
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
        if (!req->cachedResource()->isImage())
            handle->cancel();
    }
}

void Loader::didReceiveData(ResourceHandle* job, const char* data, int size)
{
    Request* request = m_requestsLoading.get(job);
    if (!request)
        return;

    CachedResource* object = request->cachedResource();    
    Vector<char>& buffer = object->bufferData(data, size, request);

    // Set the data.
    if (request->isMultipart())
        // The loader delivers the data in a multipart section all at once, send eof.
        object->data(buffer, true);
    else if (request->isIncremental())
        object->data(buffer, false);
}

int Loader::numRequests(DocLoader* dl) const
{
    // FIXME: Maybe we should keep a collection of requests by DocLoader, so we can do this instantly.

    int res = 0;

    DeprecatedPtrListIterator<Request> pIt(m_requestsPending);
    for (; pIt.current(); ++pIt) {
        if (pIt.current()->docLoader() == dl)
            res++;
    }

    RequestMap::const_iterator end = m_requestsLoading.end();
    for (RequestMap::const_iterator i = m_requestsLoading.begin(); !(i == end); ++i) {
        Request* r = i->second;
        res += (r->docLoader() == dl && !r->isMultipart());
    }

    DeprecatedPtrListIterator<Request> bdIt(m_requestsBackgroundDecoding);
    for (; bdIt.current(); ++bdIt)
        if (bdIt.current()->docLoader() == dl)
            res++;

    if (dl->loadInProgress())
        res++;

    return res;
}

void Loader::cancelRequests(DocLoader* dl)
{
    DeprecatedPtrListIterator<Request> pIt(m_requestsPending);
    while (pIt.current()) {
        if (pIt.current()->docLoader() == dl) {
            cache()->remove(pIt.current()->cachedResource());
            m_requestsPending.remove(pIt);
        } else
            ++pIt;
    }

    Vector<ResourceHandle*, 256> jobsToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->docLoader() == dl)
            jobsToCancel.append(i->first);
    }

    for (unsigned i = 0; i < jobsToCancel.size(); ++i) {
        ResourceHandle* job = jobsToCancel[i];
        Request* r = m_requestsLoading.get(job);
        m_requestsLoading.remove(job);
        cache()->remove(r->cachedResource());
        job->kill();
    }

    DeprecatedPtrListIterator<Request> bdIt(m_requestsBackgroundDecoding);
    while (bdIt.current()) {
        if (bdIt.current()->docLoader() == dl) {
            cache()->remove(bdIt.current()->cachedResource());
            m_requestsBackgroundDecoding.remove(bdIt);
        } else
            ++bdIt;
    }
}

void Loader::removeBackgroundDecodingRequest(Request* r)
{
    if (m_requestsBackgroundDecoding.containsRef(r))
        m_requestsBackgroundDecoding.remove(r);
}

ResourceHandle* Loader::jobForRequest(const String& URL) const
{
    RequestMap::const_iterator end = m_requestsLoading.end();
    for (RequestMap::const_iterator i = m_requestsLoading.begin(); i != end; ++i) {
        CachedResource* obj = i->second->cachedResource();
        if (obj && obj->url() == URL)
            return i->first;
    }
    return 0;
}

} //namespace WebCore
