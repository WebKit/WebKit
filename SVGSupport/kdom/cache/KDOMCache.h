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

#ifndef KDOM_Cache_H
#define KDOM_Cache_H

#include <qdict.h>
#include <qptrlist.h>
#include <qpixmap.h>

#include "KDOMCachedObject.h"
#include "KDOMLoader.h"

namespace KDOM
{
    class Loader;
    class CachedObject;
    class DocumentLoader;

    class Cache
    {
    public:
        static void init();

        static int size();
        static void setSize(int bytes);

        static void statistics();
        static void flush(bool force = false);

        static bool hasPending(CachedObject::Type type);

        /**
         * Clears the cache.
         *
         * @note Call this only at the end of your program, to clean
         * up memory. This can be useful for finding memory holes.
         */
        static void clear();

        static void removeCacheEntry(CachedObject *object);

        static Loader *loader() { return s_loader; }

    protected:
        friend class CachedObject;

        static void insertInLRUList(CachedObject *object);
        static void removeFromLRUList(CachedObject *object);

    private:
        friend class CachedImage;
        
        static QPixmap *nullPixmap;
        static QPixmap *brokenPixmap;
        static QPixmap *blockedPixmap;

    private:
        friend class DocumentLoader;

        static int s_maxSize;
        static int s_cacheSize;
        static int s_totalSizeOfLRU;

        static Loader *s_loader;

        static QDict<CachedObject> *s_objectDict; // cache
        static QPtrList<CachedObject> *s_freeList; // freelist
        static QPtrList<DocumentLoader> *s_docLoaderList; // docloader;

    private: // Used by 'DocumentLoader' exclusively
        template<typename CachedObjectType, enum CachedObject::Type CachedType>
        static CachedObjectType *Cache::requestObject(DocumentLoader *docLoader, const KURL &url, const char *accept)
        {
            KIO::CacheControl cachePolicy = docLoader ? docLoader->cachePolicy() : KIO::CC_Verify;

            CachedObject *obj = s_objectDict->find(url.url());
            if(obj && obj->type() != CachedType)
            {
                removeCacheEntry(obj);
                obj = 0;
            }

            if(obj && docLoader->needReload(url))
            {
                obj = 0;
                Q_ASSERT(s_objectDict->find(url.url()) == 0);
            }

            if(!obj)
            {
#ifdef CACHE_DEBUG
                kdDebug() << "[KDOM::Cache] New entry: " << url.url() << endl;
#endif

                CachedObjectType *cot = new CachedObjectType(docLoader, url.url(), cachePolicy, accept);
                s_objectDict->insert(url.url(), cot);
                if(cot->allowInLRUList())
                    insertInLRUList(cot);

                obj = cot;
            }
#ifdef CACHE_DEBUG
            else
                kdDebug() << "[KDOM::Cache] Using pending/cached: " << url.url() << endl;
#endif

            docLoader->insertCachedObject(obj);
            return static_cast<CachedObjectType *>(obj);
        }
    };
};

#endif

// vim:ts=4:noet
