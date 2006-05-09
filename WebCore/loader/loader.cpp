/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
#include "KWQLoader.h"
#include "Request.h"
#include "HTMLDocument.h"
#include "TransferJob.h"
#include "TransferJob.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

static bool crossDomain(const DeprecatedString &a, const DeprecatedString &b)
{
    if (a == b) return false;

    DeprecatedStringList l1 = DeprecatedStringList::split('.', a);
    DeprecatedStringList l2 = DeprecatedStringList::split('.', b);

    while(l1.count() > l2.count())
        l1.pop_front();

    while(l2.count() > l1.count())
        l2.pop_front();

    while(l2.count() >= 2)
    {
        if (l1 == l2)
           return false;

        l1.pop_front();
        l2.pop_front();
    }
    return true;
}

Loader::Loader()
{
    m_requestsPending.setAutoDelete( true );
}

Loader::~Loader()
{
    deleteAllValues(m_requestsLoading);
}

void Loader::load(DocLoader* dl, CachedObject *object, bool incremental)
{
    Request* req = new Request(dl, object, incremental);
    m_requestsPending.append(req);
    servePendingRequests();
}

void Loader::servePendingRequests()
{
  if ( m_requestsPending.count() == 0 )
      return;

  // get the first pending request
  Request *req = m_requestsPending.take(0);

  KURL u(req->object->url().deprecatedString());
  TransferJob* job = new TransferJob(this, "GET", u);
  
  if (!req->object->accept().isEmpty())
      job->addMetaData("accept", req->object->accept());
  if (req->m_docLoader)  {
      KURL r = req->m_docLoader->doc()->URL();
      if (r.protocol().startsWith("http") && r.path().isEmpty())
          r.setPath("/");
      job->addMetaData("referrer", r.url());
      DeprecatedString domain = r.host();
      if (req->m_docLoader->doc()->isHTMLDocument())
         domain = static_cast<HTMLDocument*>(req->m_docLoader->doc())->domain().deprecatedString();
      if (crossDomain(u.host(), domain))
         job->addMetaData("cross-domain", "true");
  }

  if (job->start(req->m_docLoader))
    m_requestsLoading.add(job, req);
}

void Loader::receivedAllData(TransferJob* job, PlatformData allData)
{
    RequestMap::iterator i = m_requestsLoading.find(job);
    if (i == m_requestsLoading.end())
        return;
    Request* r = i->second;
    m_requestsLoading.remove(i);

    CachedObject *object = r->object;
    DocLoader *docLoader = r->m_docLoader;

    if (job->error() || job->isErrorPage()) {
        docLoader->setLoadInProgress(true);
        r->object->error();
        docLoader->setLoadInProgress(false);
        Cache::remove(object);
    } else {
        docLoader->setLoadInProgress(true);
        object->data(r->m_buffer, true);
#ifdef __APPLE__
        r->object->setAllData(allData);
#endif
        docLoader->setLoadInProgress(false);
        object->finish();
    }

    delete r;

    servePendingRequests();
}

void Loader::receivedResponse(TransferJob* job, PlatformResponse response)
{
#ifdef __APPLE__
    Request *r = m_requestsLoading.get(job);
    ASSERT(r);
    ASSERT(response);
    r->object->setResponse(response);
    r->object->setExpireDate(KWQCacheObjectExpiresTime(r->m_docLoader, response), false);
    
    DeprecatedString chs = job->queryMetaData("charset").deprecatedString();
    if (!chs.isNull())
        r->object->setCharset(chs);
    
    if (r->multipart) {
        ASSERT(r->object->isImage());
        static_cast<CachedImage *>(r->object)->clear();
        r->m_buffer.resize(0);
        if (r->m_docLoader->frame())
            r->m_docLoader->frame()->checkCompleted();
    } else if (KWQResponseIsMultipart(response)) {
        r->multipart = true;
        if (!r->object->isImage())
            job->cancel();
    }
#endif
}

void Loader::receivedData(TransferJob* job, const char* data, int size)
{
    Request *r = m_requestsLoading.get(job);
    if (!r)
        return;

    unsigned oldSize = r->m_buffer.size();
    r->m_buffer.resize(oldSize + size);
    memcpy(r->m_buffer.data() + oldSize, data, size);

    if (r->multipart)
        r->object->data(r->m_buffer, true); // the loader delivers the data in a multipart section all at once, send eof
    else if (r->incremental)
        r->object->data(r->m_buffer, false);
}

int Loader::numRequests( DocLoader* dl ) const
{
    // FIXME: Maybe we should keep a collection of requests by DocLoader, so we can do this instantly.

    int res = 0;

    DeprecatedPtrListIterator<Request> pIt( m_requestsPending );
    for (; pIt.current(); ++pIt )
        if ( pIt.current()->m_docLoader == dl )
            res++;

    RequestMap::const_iterator end = m_requestsLoading.end();
    for (RequestMap::const_iterator i = m_requestsLoading.begin(); !(i == end); ++i) {
        Request* r = i->second;
        res += (r->m_docLoader == dl && !r->multipart);
    }

    DeprecatedPtrListIterator<Request> bdIt( m_requestsBackgroundDecoding );
    for (; bdIt.current(); ++bdIt )
        if ( bdIt.current()->m_docLoader == dl )
            res++;

    if (dl->loadInProgress())
        res++;

    return res;
}

void Loader::cancelRequests(DocLoader* dl)
{
    DeprecatedPtrListIterator<Request> pIt(m_requestsPending);
    while (pIt.current())
    {
        if (pIt.current()->m_docLoader == dl) {
            Cache::remove( pIt.current()->object );
            m_requestsPending.remove( pIt );
        }
        else
            ++pIt;
    }

    Vector<TransferJob*, 256> jobsToCancel;

    RequestMap::iterator end = m_requestsLoading.end();
    for (RequestMap::iterator i = m_requestsLoading.begin(); i != end; ++i) {
        Request* r = i->second;
        if (r->m_docLoader == dl)
            jobsToCancel.append(i->first);
    }

    for (unsigned i = 0; i < jobsToCancel.size(); ++i) {
        TransferJob* job = jobsToCancel[i];
        Request* r = m_requestsLoading.get(job);
        m_requestsLoading.remove(job);
        Cache::remove(r->object);
        job->kill();
    }

    DeprecatedPtrListIterator<Request> bdIt( m_requestsBackgroundDecoding );
    while (bdIt.current()) {
        if (bdIt.current()->m_docLoader == dl) {
            Cache::remove( bdIt.current()->object );
            m_requestsBackgroundDecoding.remove( bdIt );
        }
        else
            ++bdIt;
    }
}

void Loader::removeBackgroundDecodingRequest (Request *r)
{
    bool present = m_requestsBackgroundDecoding.containsRef(r);
    if (present)
        m_requestsBackgroundDecoding.remove(r);
}

TransferJob* Loader::jobForRequest(const String& URL) const
{
    RequestMap::const_iterator end = m_requestsLoading.end();
    for (RequestMap::const_iterator i = m_requestsLoading.begin(); i != end; ++i) {
        CachedObject* obj = i->second->object;
        if (obj && obj->url() == URL)
            return i->first;
    }
    return 0;
}

}
