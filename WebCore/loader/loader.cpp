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
#include "CachedObject.h"
#include "DocLoader.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "KWQLoader.h"
#include "Request.h"
#include "TransferJob.h"
#include "TransferJob.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

static bool crossDomain(const DeprecatedString& a, const DeprecatedString& b)
{
    if (a == b)
        return false;

    DeprecatedStringList l1 = DeprecatedStringList::split('.', a);
    DeprecatedStringList l2 = DeprecatedStringList::split('.', b);

    while (l1.count() > l2.count())
        l1.pop_front();

    while (l2.count() > l1.count())
        l2.pop_front();

    while (l2.count() >= 2) {
        if (l1 == l2)
            return false;

        l1.pop_front();
        l2.pop_front();
    }

    return true;
}

Loader::Loader()
{
    m_requestsPending.setAutoDelete(true);
}

Loader::~Loader()
{
    deleteAllValues(m_requestsLoading);
}

void Loader::load(DocLoader* dl, CachedObject* object, bool incremental)
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

    KURL u(req->cachedObject()->url().deprecatedString());
    TransferJob* job = new TransferJob(this, "GET", u);

    if (!req->cachedObject()->accept().isEmpty())
        job->addMetaData("accept", req->cachedObject()->accept());
    if (req->docLoader())  {
        KURL r = req->docLoader()->doc()->URL();
        if (r.protocol().startsWith("http") && r.path().isEmpty())
            r.setPath("/");
        job->addMetaData("referrer", r.url());
        DeprecatedString domain = r.host();
        if (req->docLoader()->doc()->isHTMLDocument())
            domain = static_cast<HTMLDocument*>(req->docLoader()->doc())->domain().deprecatedString();
        if (crossDomain(u.host(), domain))
            job->addMetaData("cross-domain", "true");
    }

    if (job->start(req->docLoader()))
        m_requestsLoading.add(job, req);
}

void Loader::receivedAllData(TransferJob* job, PlatformData allData)
{
    RequestMap::iterator i = m_requestsLoading.find(job);
    if (i == m_requestsLoading.end())
        return;

    Request* req = i->second;
    m_requestsLoading.remove(i);

    CachedObject* object = req->cachedObject();
    DocLoader* docLoader = req->docLoader();

    if (job->error() || job->isErrorPage()) {
        docLoader->setLoadInProgress(true);
        object->error();
        docLoader->setLoadInProgress(false);
        Cache::remove(object);
    } else {
        docLoader->setLoadInProgress(true);
        object->data(req->buffer(), true);
#ifdef __APPLE__
        object->setAllData(allData);
#endif
        docLoader->setLoadInProgress(false);
        object->finish();
    }

    delete req;

    servePendingRequests();
}

void Loader::receivedResponse(TransferJob* job, PlatformResponse response)
{
#ifdef __APPLE__
    Request* req = m_requestsLoading.get(job);
    ASSERT(req);
    ASSERT(response);
    req->cachedObject()->setResponse(response);
    req->cachedObject()->setExpireDate(KWQCacheObjectExpiresTime(req->docLoader(), response), false);
    
    DeprecatedString chs = job->queryMetaData("charset").deprecatedString();
    if (!chs.isNull())
        req->cachedObject()->setCharset(chs);
    
    if (req->isMultipart()) {
        ASSERT(req->cachedObject()->isImage());
        static_cast<CachedImage*>(req->cachedObject())->clear();
        if (req->docLoader()->frame())
            req->docLoader()->frame()->checkCompleted();
    } else if (KWQResponseIsMultipart(response)) {
        req->setIsMultipart(true);
        if (!req->cachedObject()->isImage())
            job->cancel();
    }
#endif
}

void Loader::receivedData(TransferJob* job, const char* data, int size)
{
    Request* request = m_requestsLoading.get(job);
    if (!request)
        return;

    CachedObject* object = request->cachedObject();    
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
            Cache::remove(pIt.current()->cachedObject());
            m_requestsPending.remove(pIt);
        } else
            ++pIt;
    }

    Vector<TransferJob*, 256> jobsToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->docLoader() == dl)
            jobsToCancel.append(i->first);
    }

    for (unsigned i = 0; i < jobsToCancel.size(); ++i) {
        TransferJob* job = jobsToCancel[i];
        Request* r = m_requestsLoading.get(job);
        m_requestsLoading.remove(job);
        Cache::remove(r->cachedObject());
        job->kill();
    }

    DeprecatedPtrListIterator<Request> bdIt(m_requestsBackgroundDecoding);
    while (bdIt.current()) {
        if (bdIt.current()->docLoader() == dl) {
            Cache::remove(bdIt.current()->cachedObject());
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

TransferJob* Loader::jobForRequest(const String& URL) const
{
    RequestMap::const_iterator end = m_requestsLoading.end();
    for (RequestMap::const_iterator i = m_requestsLoading.begin(); i != end; ++i) {
        CachedObject* obj = i->second->cachedObject();
        if (obj && obj->url() == URL)
            return i->first;
    }
    return 0;
}

} //namespace WebCore
