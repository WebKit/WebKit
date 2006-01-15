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
#include "Cache.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "DocLoader.h"
#include "loader.h"
#include "DocumentImpl.h"

#include <qpixmap.h>

// up to which size is a picture for sure cacheable
#define MAXCACHEABLE 40*1024
// default cache size
#define DEFCACHESIZE 4096*1024

#include <kio/job.h>
#include <kio/jobclasses.h>

#include <kxmlcore/Assertions.h>

using namespace khtml;
using namespace DOM;

static bool cacheDisabled;

LRUList::LRUList() :m_head(0), m_tail(0) 
{
}

LRUList::~LRUList()
{
}

QDict<CachedObject> *Cache::cache = 0;
QPtrList<DocLoader>* Cache::docloader = 0;
Loader *Cache::m_loader = 0;

int Cache::maxSize = DEFCACHESIZE;
int Cache::maxCacheable = MAXCACHEABLE;
int Cache::flushCount = 0;

QPixmap *Cache::nullPixmap = 0;
QPixmap *Cache::brokenPixmap = 0;

CachedObject *Cache::m_headOfUncacheableList = 0;
int Cache::m_totalSizeOfLRULists = 0;
int Cache::m_countOfLRUAndUncacheableLists;
LRUList *Cache::m_LRULists = 0;

void Cache::init()
{
    if ( !cache )
        cache = new QDict<CachedObject>(401, true);

    if ( !docloader )
        docloader = new QPtrList<DocLoader>;

    if ( !nullPixmap )
        nullPixmap = new QPixmap;

    if ( !brokenPixmap )
        brokenPixmap = KWQLoadPixmap("missing_image");

    if ( !m_loader )
        m_loader = new Loader();
}

void Cache::clear()
{
    if (!cache)
        return;

    cache->setAutoDelete( true );
    delete cache; cache = 0;
    delete nullPixmap; nullPixmap = 0;
    delete brokenPixmap; brokenPixmap = 0;
    delete m_loader;   m_loader = 0;
    delete docloader; docloader = 0;
}

CachedImage *Cache::requestImage( DocLoader* dl, const DOMString & url, bool reload, time_t _expireDate )
{
    // this brings the _url to a standard form...
    KURL kurl;
    if (dl)
        kurl = dl->m_doc->completeURL( url.qstring() );
    else
        kurl = url.qstring();
    return requestImage(dl, kurl, reload, _expireDate);
}

CachedImage *Cache::requestImage( DocLoader* dl, const KURL & url, bool reload, time_t _expireDate )
{
    KIO::CacheControl cachePolicy;
    if (dl)
        cachePolicy = dl->cachePolicy();
    else
        cachePolicy = KIO::CC_Verify;

    // Checking if the URL is malformed is lots of extra work for little benefit.

    if (!dl->doc()->shouldCreateRenderers()){
        return 0;
    }

    CachedObject *o = 0;
    if (!reload)
        o = cache->find(url.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << url.url() << endl;
#endif
        CachedImage *im = new CachedImage(dl, url.url(), cachePolicy, _expireDate);
        if ( dl && dl->autoloadImages() ) Cache::loader()->load(dl, im, true);
        if (cacheDisabled)
            im->setFree(true);
        else {
        cache->insert( url.url(), im );
        moveToHeadOfLRUList(im);
        }
        o = im;
    }

    
    if(o->type() != CachedObject::Image)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestImage url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << ", status " << o->status() << endl;
#endif

    moveToHeadOfLRUList(o);
    if ( dl ) {
        dl->m_docObjects.remove( o );
        if (!cacheDisabled)
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedImage *>(o);
}

CachedCSSStyleSheet *Cache::requestStyleSheet( DocLoader* dl, const DOMString & url, bool reload, time_t _expireDate, const QString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    KIO::CacheControl cachePolicy;
    if ( dl )
    {
        kurl = dl->m_doc->completeURL( url.qstring() );
        cachePolicy = dl->cachePolicy();
    }
    else
    {
        kurl = url.qstring();
        cachePolicy = KIO::CC_Verify;
    }

    // Checking if the URL is malformed is lots of extra work for little benefit.

    CachedObject *o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedCSSStyleSheet *sheet = new CachedCSSStyleSheet(dl, kurl.url(), cachePolicy, _expireDate, charset);
        if (cacheDisabled)
            sheet->setFree(true);
        else {
        cache->insert( kurl.url(), sheet );
        moveToHeadOfLRUList(sheet);
        }
        o = sheet;
    }

    
    if(o->type() != CachedObject::CSSStyleSheet)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestStyleSheet url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif

    moveToHeadOfLRUList(o);
    if ( dl ) {
        dl->m_docObjects.remove( o );
        if (!cacheDisabled)
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedCSSStyleSheet *>(o);
}

void Cache::preloadStyleSheet( const QString &url, const QString &stylesheet_data)
{
    CachedObject *o = cache->find(url);
    if(o)
        removeCacheEntry(o);

    CachedCSSStyleSheet *stylesheet = new CachedCSSStyleSheet(url, stylesheet_data);
    cache->insert( url, stylesheet );
}

CachedScript *Cache::requestScript( DocLoader* dl, const DOM::DOMString &url, bool reload, time_t _expireDate, const QString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    KIO::CacheControl cachePolicy;
    if ( dl )
    {
        kurl = dl->m_doc->completeURL( url.qstring() );
        cachePolicy = dl->cachePolicy();
    }
    else
    {
        kurl = url.qstring();
        cachePolicy = KIO::CC_Verify;
    }

    // Checking if the URL is malformed is lots of extra work for little benefit.

    CachedObject *o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedScript *script = new CachedScript(dl, kurl.url(), cachePolicy, _expireDate, charset);
        if (cacheDisabled)
            script->setFree(true);
        else {
        cache->insert( kurl.url(), script );
        moveToHeadOfLRUList(script);
        }
        o = script;
    }

    
    if(!(o->type() == CachedObject::Script))
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestScript url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
    
#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif

    moveToHeadOfLRUList(o);
    if ( dl ) {
        dl->m_docObjects.remove( o );
        if (!cacheDisabled)
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedScript *>(o);
}

void Cache::preloadScript( const QString &url, const QString &script_data)
{
    CachedObject *o = cache->find(url);
    if(o)
        removeCacheEntry(o);

    CachedScript *script = new CachedScript(url, script_data);
    cache->insert( url, script );
}

#ifdef KHTML_XSLT
CachedXSLStyleSheet* Cache::requestXSLStyleSheet(DocLoader* dl, const DOMString & url, bool reload, 
                                                 time_t _expireDate)
{
    // this brings the _url to a standard form...
    KURL kurl;
    KIO::CacheControl cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.qstring());
        cachePolicy = dl->cachePolicy();
    }
    else {
        kurl = url.qstring();
        cachePolicy = KIO::CC_Verify;
    }
    
    // Checking if the URL is malformed is lots of extra work for little benefit.
    
    CachedObject *o = cache->find(kurl.url());
    if (!o) {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedXSLStyleSheet* doc = new CachedXSLStyleSheet(dl, kurl.url(), cachePolicy, _expireDate);
        if (cacheDisabled)
            doc->setFree(true);
        else {
            cache->insert(kurl.url(), doc);
            moveToHeadOfLRUList(doc);
        }
        o = doc;
    }
    
    
    if (o->type() != CachedObject::XSLStyleSheet) {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestXSLStyleSheet url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
#ifdef CACHE_DEBUG
    if (o->status() == CachedObject::Pending)
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif
    
    moveToHeadOfLRUList(o);
    if (dl) {
        dl->m_docObjects.remove( o );
        if (!cacheDisabled)
            dl->m_docObjects.append( o );
    }
    return static_cast<CachedXSLStyleSheet*>(o);
}
#endif

#ifndef KHTML_NO_XBL
CachedXBLDocument* Cache::requestXBLDocument(DocLoader* dl, const DOMString & url, bool reload, 
                                             time_t _expireDate)
{
    // this brings the _url to a standard form...
    KURL kurl;
    KIO::CacheControl cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.qstring());
        cachePolicy = dl->cachePolicy();
    }
    else {
        kurl = url.qstring();
        cachePolicy = KIO::CC_Verify;
    }
    
    // Checking if the URL is malformed is lots of extra work for little benefit.
    
    CachedObject *o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedXBLDocument* doc = new CachedXBLDocument(dl, kurl.url(), cachePolicy, _expireDate);
        if (cacheDisabled)
            doc->setFree(true);
        else {
            cache->insert(kurl.url(), doc);
            moveToHeadOfLRUList(doc);
        }
        o = doc;
    }
    
    
    if(o->type() != CachedObject::XBL)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestXBLDocument url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif
    
    moveToHeadOfLRUList(o);
    if ( dl ) {
        dl->m_docObjects.remove( o );
        if (!cacheDisabled)
            dl->m_docObjects.append( o );
    }
    return static_cast<CachedXBLDocument*>(o);
}
#endif

void Cache::flush(bool force)
{
    if (force)
       flushCount = 0;
    // Don't flush for every image.
    if (m_countOfLRUAndUncacheableLists < flushCount)
       return;

    init();

    while (m_headOfUncacheableList)
        removeCacheEntry(m_headOfUncacheableList);

    for (int i = MAX_LRU_LISTS-1; i>=0; i--) {
        if (m_totalSizeOfLRULists <= maxSize)
            break;
            
        while (m_totalSizeOfLRULists > maxSize && m_LRULists[i].m_tail)
            removeCacheEntry(m_LRULists[i].m_tail);
    }

    flushCount = m_countOfLRUAndUncacheableLists+10; // Flush again when the cache has grown.
}


void Cache::setSize( int bytes )
{
    maxSize = bytes;
    maxCacheable = kMax(maxSize / 128, MAXCACHEABLE);

    // may be we need to clear parts of the cache
    flushCount = 0;
    flush(true);
}

void Cache::removeCacheEntry( CachedObject *object )
{
  QString key = object->url().qstring();

  // this indicates the deref() method of CachedObject to delete itself when the reference counter
  // drops down to zero
  object->setFree( true );

  cache->remove( key );
  removeFromLRUList(object);

  const DocLoader* dl;
  for ( dl=docloader->first(); dl; dl=docloader->next() )
      dl->removeCachedObject( object );

  if ( object->canDelete() )
     delete object;
}

#define FAST_LOG2(_log2,_n)   \
      unsigned int j_ = (unsigned int)(_n);   \
      (_log2) = 0;                    \
      if ((j_) & ((j_)-1))            \
      (_log2) += 1;               \
      if ((j_) >> 16)                 \
      (_log2) += 16, (j_) >>= 16; \
      if ((j_) >> 8)                  \
      (_log2) += 8, (j_) >>= 8;   \
      if ((j_) >> 4)                  \
      (_log2) += 4, (j_) >>= 4;   \
      if ((j_) >> 2)                  \
      (_log2) += 2, (j_) >>= 2;   \
      if ((j_) >> 1)                  \
      (_log2) += 1;

static int FastLog2(unsigned int i) {
    int log2;
    FAST_LOG2(log2,i);
    return log2;
}

LRUList* Cache::getLRUListFor(CachedObject* o)
{
    int accessCount = o->accessCount();
    int queueIndex;
    if (accessCount == 0) {
        queueIndex = 0;
    } else {
        int sizeLog = FastLog2(o->size());
        queueIndex = sizeLog/o->accessCount() - 1;
        if (queueIndex < 0)
            queueIndex = 0;
        if (queueIndex >= MAX_LRU_LISTS)
            queueIndex = MAX_LRU_LISTS-1;
    }
    if (m_LRULists == 0) {
        m_LRULists = new LRUList [MAX_LRU_LISTS];
    }
    return &m_LRULists[queueIndex];
}

void Cache::removeFromLRUList(CachedObject *object)
{
    CachedObject *next = object->m_nextInLRUList;
    CachedObject *prev = object->m_prevInLRUList;
    bool uncacheable = object->status() == CachedObject::Uncacheable;
    
    LRUList* list = uncacheable ? 0 : getLRUListFor(object);
    CachedObject *&head = uncacheable ? m_headOfUncacheableList : list->m_head;
    
    if (next == 0 && prev == 0 && head != object) {
        return;
    }
    
    object->m_nextInLRUList = 0;
    object->m_prevInLRUList = 0;
    
    if (next)
        next->m_prevInLRUList = prev;
    else if (!uncacheable && list->m_tail == object)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInLRUList = next;
    else if (head == object)
        head = next;
    
    --m_countOfLRUAndUncacheableLists;
    
    if (!uncacheable)
        m_totalSizeOfLRULists -= object->size();
}

void Cache::moveToHeadOfLRUList(CachedObject *object)
{
    insertInLRUList(object);
}

void Cache::insertInLRUList(CachedObject *object)
{
    removeFromLRUList(object);
    
    if (!object->allowInLRUList())
        return;
    
    LRUList* list = getLRUListFor(object);
    
    bool uncacheable = object->status() == CachedObject::Uncacheable;
    CachedObject *&head = uncacheable ? m_headOfUncacheableList : list->m_head;

    object->m_nextInLRUList = head;
    if (head)
        head->m_prevInLRUList = object;
    head = object;
    
    if (object->m_nextInLRUList == 0 && !uncacheable)
        list->m_tail = object;
    
    ++m_countOfLRUAndUncacheableLists;
    
    if (!uncacheable)
        m_totalSizeOfLRULists += object->size();
}

bool Cache::adjustSize(CachedObject *object, int delta)
{
    if (object->status() == CachedObject::Uncacheable)
        return false;

    if (object->m_nextInLRUList == 0 && object->m_prevInLRUList == 0 &&
        getLRUListFor(object)->m_head != object)
        return false;
    
    m_totalSizeOfLRULists += delta;
    return delta != 0;
}

Cache::Statistics Cache::getStatistics()
{
    Statistics stats;

    if (!cache)
        return stats;

    QDictIterator<CachedObject> i(*cache);
    for (i.toFirst(); i.current(); ++i) {
        CachedObject *o = i.current();
        switch (o->type()) {
            case CachedObject::Image:
                stats.images.count++;
                stats.images.size += o->size();
                break;

            case CachedObject::CSSStyleSheet:
                stats.styleSheets.count++;
                stats.styleSheets.size += o->size();
                break;

            case CachedObject::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                break;
#ifdef KHTML_XSLT
            case CachedObject::XSLStyleSheet:
                stats.xslStyleSheets.count++;
                stats.xslStyleSheets.size += o->size();
                break;
#endif
#ifndef KHTML_NO_XBL
            case CachedObject::XBL:
                stats.xblDocs.count++;
                stats.xblDocs.size += o->size();
                break;
#endif
            default:
                stats.other.count++;
                stats.other.size += o->size();
        }
    }
    
    return stats;
}

void Cache::flushAll()
{
    if (!cache)
        return;

    for (;;) {
        QDictIterator<CachedObject> i(*cache);
        CachedObject *o = i.toFirst();
        if (!o)
            break;
        removeCacheEntry(o);
    }
}

void Cache::setCacheDisabled(bool disabled)
{
    cacheDisabled = disabled;
    if (disabled)
        flushAll();
}
