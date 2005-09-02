/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2002 Waldo Bastian <bastian@kde.org>
    Copyright (C) 2003 Apple Computer, Inc.
    
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
    
    // regarding the LRU:
    // http://www.is.kyusan-u.ac.jp/~chengk/pub/papers/compsac00_A07-07.pdf
*/

#include <kdebug.h>
#include <kglobal.h>
#include <kiconloader.h>

#include <qpixmap.h>

#include "KDOMCache.h"
#include "KDOMLoader.h"
#include "KDOMCachedImage.h"
#include "KDOMCacheHelper.h"

using namespace KDOM;

#define KDOM_CACHE_DICT_SIZE 401
#define KDOM_CACHE_DEFAULT_SIZE 2146304 // 2096 * 1024

Loader *Cache::s_loader = 0;
Q3Dict<CachedObject> *Cache::s_objectDict = 0;
Q3PtrList<CachedObject> *Cache::s_freeList = 0;
Q3PtrList<DocumentLoader> *Cache::s_docLoaderList = 0;

QPixmap *Cache::nullPixmap;
QPixmap *Cache::brokenPixmap;
QPixmap *Cache::blockedPixmap;

int Cache::s_maxSize = KDOM_CACHE_DEFAULT_SIZE;
int Cache::s_totalSizeOfLRU = 0;

void Cache::init()
{
    if(!s_loader)
        s_loader = new Loader();

    if(!s_objectDict)
        s_objectDict = new Q3Dict<CachedObject>(KDOM_CACHE_DICT_SIZE, true);

    if(!s_docLoaderList)
        s_docLoaderList = new Q3PtrList<DocumentLoader>;

    if(!s_freeList)
    {
        s_freeList = new Q3PtrList<CachedObject>;
        s_freeList->setAutoDelete(true);
    }
    
    if(!nullPixmap)
        nullPixmap = new QPixmap();

    if(!brokenPixmap)
#ifdef APPLE_CHANGES
        brokenPixmap = KWQLoadPixmap("missing_image");
#else
        brokenPixmap = new QPixmap(KGlobal::iconLoader()->loadIcon(QString::fromLatin1("file_broken"),
                                                                   KIcon::Desktop, 16, KIcon::DisabledState));
#endif
}

void Cache::clear()
{
    if(!s_objectDict)
        return;

#ifdef CACHE_DEBUG
    kdDebug() << "[KDOM::Cache] clear()" << endl;
    statistics();
#endif

    s_objectDict->setAutoDelete(true);

    delete s_loader; s_loader = 0;
    delete s_freeList; s_freeList = 0;
    delete s_objectDict; s_objectDict = 0;
    delete s_docLoaderList; s_docLoaderList = 0;
    
    delete nullPixmap; nullPixmap = 0;
    delete brokenPixmap; brokenPixmap = 0;
    delete blockedPixmap; blockedPixmap = 0;
}

void Cache::flush(bool force)
{
    init();

    if(force || s_totalSizeOfLRU > (s_maxSize + s_maxSize / 4))
    {
        for(int i = KDOM_CACHE_MAX_LRU_LISTS - 1; i >= 0 && s_totalSizeOfLRU > s_maxSize; --i)
        {
            while(s_totalSizeOfLRU > s_maxSize && s_lruLists[i].tail)
                removeCacheEntry(s_lruLists[i].tail);
        }

#ifdef CACHE_DEBUG
        statistics();
#endif
    }

    for(CachedObject *p = s_freeList->first(); p != 0; p = s_freeList->next())
    {
        if(p->canDelete())
            s_freeList->remove();
    }
}

void Cache::setSize(int bytes)
{
    s_maxSize = bytes;
    flush(true /* force */);
}

bool Cache::hasPending(CachedObject::Type type)
{
    Q3DictIterator<CachedObject> it(*s_objectDict);
    for(it.toFirst(); it.current(); ++it)
    {
        if(it.current()->type() == type &&
           it.current()->status() != CachedObject::Cached)
            return true;
    }

    return false;
}

void Cache::statistics()
{
    CachedObject *o;

    // this function is for debugging purposes only
    init();

    int size = 0;
    int msize = 0;
    int movie = 0;
    int images = 0;
    int scripts = 0;
    int stylesheets = 0;
    int documents = 0;

    Q3DictIterator<CachedObject> it(*s_objectDict);
    for(it.toFirst(); it.current(); ++it)
    {
        o = it.current();
        switch(o->type())
        {
            case CachedObject::Image:
            {
                CachedImage *im = static_cast<CachedImage *>(o);
                images++;
                if(im->m_movie != 0)
                {
                    movie++;
                    msize += im->size();
                }
                
                break;
            }
            case CachedObject::StyleSheet:
            {
                stylesheets++;
                break;
            }
            case CachedObject::Script:
            {
                scripts++;
                break;
            }
            case CachedObject::TextDocument:
            {
                documents++;
                break;
            }
        }

        size += o->size();
    }

    size /= 1024;

    kdDebug() << "------------------------- image cache statistics -------------------" << endl;
    kdDebug() << "Number of items in cache: " << s_objectDict->count() << endl;
    kdDebug() << "Number of cached images: " << images << endl;
    kdDebug() << "Number of cached movies: " << movie << endl;
    kdDebug() << "Number of cached scripts: " << scripts << endl;
    kdDebug() << "Number of cached stylesheets: " << stylesheets << endl;
    kdDebug() << "pixmaps:   allocated space approx. " << size << " kB" << endl;
    kdDebug() << "movies :   allocated space approx. " << msize/1024 << " kB" << endl;
    kdDebug() << "Number of cached XML documents: " << documents << endl;
    kdDebug() << "--------------------------------------------------------------------" << endl;
}

void Cache::removeCacheEntry(CachedObject *object)
{
    QString key = object->url().string();

    s_objectDict->remove(key);
    removeFromLRUList(object);

    for(const DocumentLoader *docLoader = s_docLoaderList->first(); docLoader != 0; docLoader = s_docLoaderList->next())
        docLoader->removeCachedObject(object);

    if(!object->free())
    {
        s_freeList->append(object);
        object->m_free = true;
    }
}

void Cache::removeFromLRUList(CachedObject *object)
{
    CachedObject *next = object->m_next;
    CachedObject *prev = object->m_prev;

    LRUList *list = lruListFor(object);
    CachedObject *&head = lruListFor(object)->head;

    if(next == 0 && prev == 0 && head != object)
        return;

    object->m_next = 0;
    object->m_prev = 0;

    if(next)
        next->m_prev = prev;
    else if(list->tail == object)
        list->tail = prev;

    if(prev)
        prev->m_next = next;
    else if(head == object)
        head = next;

    s_totalSizeOfLRU -= object->size();
}

void Cache::insertInLRUList(CachedObject *object)
{
    removeFromLRUList(object);

    Q_ASSERT(object != 0);
    Q_ASSERT(!object->free());
    Q_ASSERT(object->canDelete());
    Q_ASSERT(object->allowInLRUList());

    LRUList *list = lruListFor(object);

    CachedObject *&head = list->head;
    object->m_next = head;

    if(head)
        head->m_prev = object;

    head = object;

    if(object->m_next == 0)
        list->tail = object;

    s_totalSizeOfLRU += object->size();
}

// vim:ts=4:noet
