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

#include "CachePolicy.h"
#include "CachedResource.h"
#include "loader.h"
#include "PlatformString.h"
#include "StringHash.h"
#include <wtf/Vector.h>
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>

namespace WebCore  {

class CachedCSSStyleSheet;
class CachedImage;
class CachedResource;
class CachedScript;
class CachedXSLStyleSheet;
class DocLoader;
class Image;
class KURL;

struct LRUList {
    CachedResource* m_head;
    CachedResource* m_tail;
    LRUList() : m_head(0), m_tail(0) { }
};

typedef HashMap<String, CachedResource*> CachedResourceMap;

// This cache hold subresources used by Web pages.  These resources consist of images, scripts and stylesheets.
class Cache {
public:
    Cache();
       
    // The loader that fetches resources.
    Loader* loader() { return &m_loader; }

    // Request resources from the cache.  A load will be initiated and a cache object created if the object is not
    // found in the cache.
    CachedResource* requestResource(DocLoader*, CachedResource::Type, const KURL& url, time_t expireDate = 0, const String* charset = 0);

    // Set/retreive the size of the cache. This will only hold approximately, since the size some 
    // cached objects (like stylesheets) take up in memory is not exactly known.
    void setMaximumSize(unsigned bytes);
    unsigned maximumSize() const { return m_maximumSize; };

    // Turn the cache on and off.  Disabling the cache will remove all resources from the cache.  They may
    // still live on if they are referenced by some Web page though.
    void setDisabled(bool);
    bool disabled() const { return m_disabled; }
    
    // Remove an existing cache entry from both the resource map and from the LRU list.
    void remove(CachedResource*);

    // Flush the cache.  Any resources still referenced by Web pages will not be removed by this call.
    void prune();

    void addDocLoader(DocLoader*);
    void removeDocLoader(DocLoader*);

    CachedResource* resourceForURL(const String&);

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(CachedResource*);
    void removeFromLRUList(CachedResource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, unsigned oldResourceSize, unsigned newResourceSize);

    // Track the size of all resources that are in the cache and still referenced by a Web page. 
    void addToLiveObjectSize(unsigned s) { m_liveResourcesSize += s; }
    void removeFromLiveObjectSize(unsigned s) { m_liveResourcesSize -= s; }

    // Functions to collect cache statistics for the caches window in the Safari Debug menu.
    struct TypeStatistic {
        int count;
        int size;
        TypeStatistic() : count(0), size(0) { }
    };
    
    struct Statistics {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
#if ENABLE(XSLT)
        TypeStatistic xslStyleSheets;
#endif
#if ENABLE(XBL)
        TypeStatistic xblDocs;
#endif
    };

    Statistics getStatistics();

private:
    LRUList* lruListFor(CachedResource*);
    
    void resourceAccessed(CachedResource*);

private:
    // Member variables.
    HashSet<DocLoader*> m_docLoaders;
    Loader m_loader;

    bool m_disabled;  // Whether or not the cache is enabled.

    unsigned m_maximumSize;  // The maximum size in bytes that the global cache can consume.
    unsigned m_currentSize;  // The current size of the global cache in bytes.
    unsigned m_liveResourcesSize; // The current size of "live" resources that cannot be flushed.

    // Size-adjusted and popularity-aware LRU list collection for cache objects.  This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" muiltiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_lruLists;
    
    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being 
    // referenced by a Web page).
    HashMap<String, CachedResource*> m_resources;
};

// Function to obtain the global cache.
Cache* cache();

}

#endif
