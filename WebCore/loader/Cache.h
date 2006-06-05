/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef Cache_h
#define Cache_h

#include "PlatformString.h"
#include "KWQKIOGlobal.h"
#include "DeprecatedPtrList.h"
#include <wtf/HashSet.h>

class KURL;

namespace WebCore  {

    class CachedCSSStyleSheet;
    class CachedImage;
    class CachedObject;
    class CachedScript;
    class CachedXSLStyleSheet;
    class DocLoader;
    class Image;
    class Loader;
    struct LRUList;
    
    /**
     * Provides a cache/loader for objects needed for displaying the html page.
     */
    class Cache {
        friend class DocLoader;
    public:
        /**
         * Initialize the cache in case it's not already.
         * This needs to get called once before using it.
         */
        static void init();
        
        /**
         * Ask the cache for some URL.
         * If the DocLoader is zero, the URL must be fully-qualified. Otherwise, it is automatically base-URL expanded
         */
        static CachedImage* requestImage(DocLoader*, const String& URL, bool reload = false, time_t expireDate = 0);
        static CachedImage* requestImage(DocLoader*, const KURL& URL, bool reload, time_t expireDate);
        static CachedCSSStyleSheet* requestStyleSheet(DocLoader*, const String& URL, bool reload, time_t expireDate, const DeprecatedString& charset);
        static CachedScript* requestScript(DocLoader*, const String& URL, bool reload, time_t expireDate, const DeprecatedString& charset);

#ifdef KHTML_XSLT
        static CachedXSLStyleSheet* requestXSLStyleSheet(DocLoader*, const String& URL, bool reload, time_t expireDate);
#endif

#ifndef KHTML_NO_XBL
        static CachedXBLDocument* requestXBLDocument(DocLoader*, const String& URL, bool reload, time_t expireDate);
#endif

        /**
         * Sets the size of the cache. This will only hold approximately, since the size some
         * cached objects (like stylesheets) take up in memory is not exactly known.
         */
        static void setSize(int bytes);

        /**
         * returns the size of the cache
         */
        static int size() { return maxSize; };

        static int maxCacheableObjectSize() { return maxCacheable; }

        // Get an existing cache entry by URL.
        static CachedObject* get(const String& URL);

        // Remove an existing cache entry.
        static void remove(CachedObject*);

        /**
         * clean up cache
         */
        static void flush(bool force = false);

        /**
         * clears the cache
         * Warning: call this only at the end of your program, to clean up memory
         * (useful for finding memory leaks).
         */
        static void clear();

        static Loader* loader() { return m_loader; }

        static Image* nullImage;
        static Image* brokenImage;

        struct TypeStatistic {
            int count;
            int size;
            TypeStatistic() : count(0), size(0) { }
        };
        
        struct Statistics {
            TypeStatistic images;
            TypeStatistic movies;
            TypeStatistic styleSheets;
            TypeStatistic scripts;
#ifdef KHTML_XSLT
            TypeStatistic xslStyleSheets;
#endif
#ifndef KHTML_NO_XBL
            TypeStatistic xblDocs;
#endif
            TypeStatistic other;
        };

        static Statistics getStatistics();
        static void flushAll();
        static void setCacheDisabled(bool);

        static void insertInLRUList(CachedObject*);
        static void removeFromLRUList(CachedObject*);
        static bool adjustSize(CachedObject*, int sizeDelta);
        
        static LRUList* getLRUListFor(CachedObject*);
        
        static void checkLRUAndUncacheableListIntegrity();

    private:
        static HashSet<DocLoader*>* docloaders;
    
        static int maxSize;
        static int maxCacheable;
        static int flushCount;
    
        static Loader* m_loader;
    
        static void moveToHeadOfLRUList(CachedObject*);
        static void updateCacheStatus(DocLoader*, CachedObject*);
    
        static LRUList* m_LRULists;
        static int m_totalSizeOfLRULists;
            
        static CachedObject* m_headOfUncacheableList;
            
        static int m_countOfLRUAndUncacheableLists;
    };

}

#endif
