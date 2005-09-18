/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc

    This file is part of the KDE project

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
*/

#include <kdebug.h>
#ifndef APPLE_CHANGES
#include <kio/scheduler.h>
#endif

#include <kdom/core/DocumentImpl.h>

#include "KDOMCache.h"
#include "KDOMLoader.moc"
#include "KDOMCacheHelper.h"
#include "KDOMCachedImage.h"
#include "KDOMCachedScript.h"
#include "KDOMCachedDocument.h"
#include "KDOMCachedStyleSheet.h"

#define KDOM_CACHE_MAX_JOB_COUNT 32

using namespace KDOM;
    
DocumentLoader::DocumentLoader(KDOMPart *part, DocumentImpl *docImpl)
{
    m_part = part;
    m_doc = docImpl;
    m_expireDate = 0;
    m_creationDate = time(0);
    m_autoloadImages = true;
    m_cachePolicy = KIO::CC_Verify;
    
    Cache::s_docLoaderList->append(this);
}

DocumentLoader::~DocumentLoader()
{
    Cache::loader()->cancelRequests(this);
    Cache::s_docLoaderList->remove(this);
}

CachedImage *DocumentLoader::requestImage(const KURL &url)
{
#ifndef APPLE_COMPILE_HACK
    // TODO: Add security checks (see khtml DocLoader!)
    CachedImage *img = Cache::requestObject<CachedImage, CachedObject::Image>(this, url, 0);

    if(img && img->status() == CachedObject::Unknown && autoloadImages())
        Cache::loader()->load(this, img, true);

    return img;
#else
    // FIXME: this is just a hack to make compile...
    return NULL;
#endif
}

CachedStyleSheet *DocumentLoader::requestStyleSheet(const KURL &url, const QString &charset,
                                                    const char *accept, bool /* userSheet */)
{
    // TODO: Add security checks (see khtml DocLoader!)
    CachedStyleSheet *s = Cache::requestObject<CachedStyleSheet, CachedObject::StyleSheet>(this, url, accept);
    if(s && !charset.isEmpty())
        s->setCharset(charset);

    return s;
}

CachedDocument *DocumentLoader::requestDocument(const KURL &url, const QString& charset,
                                                const char* accept)
{
    // TODO: Add security checks (see khtml DocLoader!)
    CachedDocument *doc = Cache::requestObject<CachedDocument, CachedObject::TextDocument>(this, url, accept);
    if(doc && !charset.isEmpty())
        doc->setCharset(charset);

    return doc;
}

CachedScript *DocumentLoader::requestScript(const KURL &url, const QString &charset)
{
    // TODO: Add security checks (see khtml DocLoader!)
    CachedScript *script = Cache::requestObject<CachedScript, CachedObject::Script>(this, url, 0);
    if(script && !charset.isEmpty())
        script->setCharset(charset);

    return script;
}

void DocumentLoader::setCacheCreationDate(time_t date)
{
    if(date)
        m_creationDate = date;
    else
        m_creationDate = time(0); // now
}

void DocumentLoader::setExpireDate(time_t date, bool relative)
{
    if(relative)
        m_expireDate = date + m_creationDate; // Relative date
    else
        m_expireDate = date; // Absolute date

#ifdef CACHE_DEBUG
    kdDebug() << "[DocumentLoader] " << m_expireDate - time(0) << " seconds left until reload required.\n";
#endif
}

void DocumentLoader::setAutoloadImages(bool load)
{
    if(load == m_autoloadImages)
        return;

    m_autoloadImages = load;
    if(!m_autoloadImages)
        return;

#if 0
    for(Q3PtrDictIterator<CachedObject> it(m_docObjects); it.current(); ++it)
    {
        if(it.current()->type() == CachedObject::Image)
        {
            CachedImage *img = const_cast<CachedImage*>( static_cast<const CachedImage *>(it.current()));

            CachedObject::Status status = img->status();
            if(status != CachedObject::Unknown)
                continue;
            Cache::loader()->load(this, img, true);
        }
    }
#endif
}

void DocumentLoader::setShowAnimations(KDOMSettings::KAnimationAdvice showAnimations)
{
    if(showAnimations == m_showAnimations)
        return;

    m_showAnimations = showAnimations;

#ifndef APPLE_COMPILE_HACK
    for(Q3PtrDictIterator<CachedObject> it(m_docObjects); it.current(); ++it)
    {
        if(it.current()->type() == CachedObject::Image)
        {
            CachedImage *img = const_cast<CachedImage *>(static_cast<const CachedImage *>(it.current()));
            img->setShowAnimations( m_showAnimations );
        }
    }
#endif
}

void DocumentLoader::insertCachedObject(CachedObject *object) const
{
    if(m_docObjects.find(object))
        return;

    m_docObjects.insert(object, object);
    
#ifndef APPLE_CHANGES
    if(m_docObjects.count() > 3 * m_docObjects.size())
        m_docObjects.resize(cacheNextSeed(m_docObjects.size()));
#endif
}

void DocumentLoader::removeCachedObject(CachedObject *object) const
{
    m_docObjects.remove(object);
}

bool DocumentLoader::needReload(const KURL &fullUrl)
{
    bool reload = false;
    if(m_cachePolicy == KIO::CC_Verify)
    {
        if(!m_reloadedURLs.contains(fullUrl.url()))
        {
            CachedObject *existing = Cache::s_objectDict->find(fullUrl.url());
            if(existing && existing->isExpired())
            {
                Cache::removeCacheEntry(existing);
                m_reloadedURLs.append(fullUrl.url());
                reload = true;
            }
        }
    }
    else if((m_cachePolicy == KIO::CC_Reload) || (m_cachePolicy == KIO::CC_Refresh))
    {
        if(!m_reloadedURLs.contains(fullUrl.url()))
        {
            CachedObject *existing = Cache::s_objectDict->find(fullUrl.url());
            if(existing)
                Cache::removeCacheEntry(existing);
            
            m_reloadedURLs.append(fullUrl.url());
            reload = true;
        }
    }

    return reload;
}

bool DocumentLoader::hasPending(CachedObject::Type type) const
{
    return Cache::hasPending(type);
}

KURL DocumentLoader::referrer() const
{
    if(m_doc)
        return m_doc->documentKURI();
    else
        return uri;

}

Loader::Loader() : QObject()
#ifdef APPLE_CHANGES
    , _requestStarted(this, SIGNAL(requestStarted(KDOM::DocumentLoader *, KDOM::CachedObject *)))
    , _requestDone(this, SIGNAL(requestDone(KDOM::DocumentLoader *, KDOM::CachedObject *)))
    , _requestFailed(this, SIGNAL(requestFailed(KDOM::DocumentLoader *, KDOM::CachedObject *)))
#endif
{
    m_requestsPending.setAutoDelete(true);
    m_requestsLoading.setAutoDelete(true);
    
#ifndef APPLE_CHANGES
    connect(&m_timer, SIGNAL(timeout()), this, SLOT( servePendingRequests()));
#endif
}

void Loader::load(DocumentLoader *docLoader, CachedObject *object, bool incremental)
{
    Request *req = new Request(docLoader, object, incremental);
    m_requestsPending.append(req);

    emit requestStarted(req->docLoader, req->object);

    m_timer.start(0, true);
}

int Loader::numRequests(DocumentLoader *docLoader) const
{
    int res = 0;

    Q3PtrListIterator<Request> pIt(m_requestsPending);
    for(; pIt.current(); ++pIt)
    {
        if(pIt.current()->docLoader == docLoader)
            res++;
    }

    Q3PtrDictIterator<Request> lIt(m_requestsLoading);
    for (; lIt.current(); ++lIt)
    {
        if(lIt.current()->docLoader == docLoader)
            res++;
    }

    return res;
}

void Loader::cancelRequests(DocumentLoader *docLoader)
{
    Q3PtrListIterator<Request> pIt(m_requestsPending);
    while(pIt.current())
    {
        if(pIt.current()->docLoader == docLoader)
        {
            kdDebug() << "canceling pending request for " << pIt.current()->object->url().string() << endl;
            
            Cache::removeCacheEntry(pIt.current()->object);
            m_requestsPending.remove(pIt);
        }
        else
            ++pIt;
    }

    Q3PtrDictIterator<Request> lIt( m_requestsLoading );
    while(lIt.current())
    {
        if(lIt.current()->docLoader == docLoader)
        {
            // kdDebug( 6060 ) << "canceling loading request for " << lIt.current()->object->url().string() << endl;
            
            KIO::Job *job = static_cast<KIO::Job *>(lIt.currentKey());
            Cache::removeCacheEntry(lIt.current()->object);
            m_requestsLoading.remove(lIt.currentKey());
            job->kill();
            
            // emit requestFailed(docLoader, pIt.current()->object);
        }
        else
            ++lIt;
    }
}

KIO::Job *Loader::jobForRequest(const DOMString &url) const
{
    Q3PtrDictIterator<Request> it(m_requestsLoading);
    for (; it.current(); ++it)
    {
        CachedObject *obj = it.current()->object;

        if(obj && obj->url() == url)
            return static_cast<KIO::Job *>(it.currentKey());
    }

    return 0;
}

void Loader::slotFinished(KIO::Job *job)
{
    Request *r = m_requestsLoading.take(job);
    KIO::TransferJob *j = static_cast<KIO::TransferJob *>(job);

    if(!r)
        return;

    if(j->error() || j->isErrorPage())
    {
#ifdef LOADER_DEBUG
        kdDebug() << "Loader::slotFinished, with error. job->error() = " << j->error() << " job->isErrorPage() = " << j->isErrorPage() << endl;
#endif

        r->object->error(job->error(), job->errorText().ascii());
        emit requestFailed(r->docLoader, r->object);
    }
    else
    {
        r->object->data(r->buffer, true);
        emit requestDone(r->docLoader, r->object);

        time_t expireDate = j->queryMetaData(QString::fromLatin1("expire-date")).toInt();

#ifdef LOADER_DEBUG
        kdDebug() << "Loader::slotFinished, url = " << j->url().url() << endl;
#endif

        r->object->setExpireDate( expireDate );
    }

    r->object->finish();

#ifdef LOADER_DEBUG
    kdDebug() << "Loader:: JOB FINISHED " << r->object << ": " << r->object->url().string() << endl;
#endif

    delete r;

    if((m_requestsPending.count() != 0) && (m_requestsLoading.count() < KDOM_CACHE_MAX_JOB_COUNT / 2))
        m_timer.start(0, true);
}

void Loader::slotData(KIO::Job *job, const QByteArray &buffer)
{
    Request *r = m_requestsLoading[job];
    if(!r)
    {
        kdDebug() << "[Loader::slotData] got data for unknown request!" << endl;
        return;
    }

    if(!r->buffer.isOpen())
        r->buffer.open(IO_WriteOnly);

    r->buffer.writeBlock(buffer.data(), buffer.size());

    if(r->incremental)
        r->object->data(r->buffer, false);
}

void Loader::servePendingRequests()
{
    while((m_requestsPending.count() != 0) && (m_requestsLoading.count() < KDOM_CACHE_MAX_JOB_COUNT))
    {
        // get the first pending request
        Request *req = m_requestsPending.take(0);

#ifdef LOADER_DEBUG
        kdDebug() << "[Loader::servePendingRequests] url = " << req->object->url().string() << endl;
#endif

        KURL u(req->object->url().string());
        KIO::TransferJob* job = KIO::get(u, false, false /*no GUI*/);

        job->addMetaData(QString::fromLatin1("cache"), KIO::getCacheControlString(req->object->cachePolicy()));

        if(!req->object->accept().isEmpty())
            job->addMetaData(QString::fromLatin1("accept"), req->object->accept());

        if(!req->object->acceptLanguage().isEmpty())
            job->addMetaData(QString::fromLatin1("Languages"), req->object->acceptLanguage());

        if(req->docLoader)
            job->addMetaData(QString::fromLatin1("referrer"), req->docLoader->referrer().url());

        connect(job, SIGNAL(result(KIO::Job *)), this, SLOT(slotFinished(KIO::Job *)));
        connect(job, SIGNAL(data(KIO::Job *, const QByteArray &)), SLOT(slotData(KIO::Job *, const QByteArray &)));

#ifndef APPLE_COMPILE_HACK
        if(req->object->schedule())
            KIO::Scheduler::scheduleJob(job);
#endif

        m_requestsLoading.insert(job, req);
    }
}


#ifdef APPLE_CHANGES
void Loader::requestStarted(DocumentLoader *l, CachedObject *o) {
    //_requestStarted.call(l, o);
    fprintf(stderr, "requestStarted()");
}

void Loader::requestDone(DocumentLoader *l, CachedObject *o) {
    //_requestDone.call(l, o);
    fprintf(stderr, "requestDone()");
}

void Loader::requestFailed(DocumentLoader *l, CachedObject *o) {
    //_requestFailed.call(l, o);
    fprintf(stderr, "requestFailed()");
}
#endif


void DocumentLoader::setReferrer(const KURL& u)
{
    uri = u;
}

// vim:ts=4:noet
