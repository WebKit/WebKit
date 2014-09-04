/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef Cache_h
#define Cache_h

#include "NativeImagePtr.h"
#include "SecurityOriginHash.h"
#include "SessionIDHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore  {

class CachedCSSStyleSheet;
class CachedResource;
class CachedResourceLoader;
class URL;
class ResourceRequest;
class ResourceResponse;
class ScriptExecutionContext;
class SecurityOrigin;
struct CrossThreadResourceRequestData;
struct SecurityOriginHash;

// This cache holds subresources used by Web pages: images, scripts, stylesheets, etc.

// The cache keeps a flexible but bounded window of dead resources that grows/shrinks 
// depending on the live resource load. Here's an example of cache growth over time,
// with a min dead resource capacity of 25% and a max dead resource capacity of 50%:

//        |-----|                              Dead: -
//        |----------|                         Live: +
//      --|----------|                         Cache boundary: | (objects outside this mark have been evicted)
//      --|----------++++++++++|
// -------|-----+++++++++++++++|
// -------|-----+++++++++++++++|+++++

class MemoryCache {
    WTF_MAKE_NONCOPYABLE(MemoryCache); WTF_MAKE_FAST_ALLOCATED;
public:
    friend MemoryCache* memoryCache();

#if ENABLE(CACHE_PARTITIONING)
    typedef HashMap<String, CachedResource*> CachedResourceItem;
    typedef HashMap<String, OwnPtr<CachedResourceItem>> CachedResourceMap;
#else
    typedef HashMap<String, CachedResource*> CachedResourceMap;
#endif
    typedef HashMap<SessionID, std::unique_ptr<CachedResourceMap>> SessionCachedResourceMap;

    struct LRUList {
        CachedResource* m_head;
        CachedResource* m_tail;
        LRUList() : m_head(0), m_tail(0) { }
    };

    struct TypeStatistic {
        int count;
        int size;
        int liveSize;
        int decodedSize;

        TypeStatistic()
            : count(0)
            , size(0)
            , liveSize(0)
            , decodedSize(0)
        { 
        }

        void addResource(CachedResource*);
    };
    
    struct Statistics {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
        TypeStatistic xslStyleSheets;
        TypeStatistic fonts;
    };

    WEBCORE_EXPORT CachedResource* resourceForURL(const URL&);
    WEBCORE_EXPORT CachedResource* resourceForURL(const URL&, SessionID);
    WEBCORE_EXPORT CachedResource* resourceForRequest(const ResourceRequest&, SessionID);

    bool add(CachedResource*);
    void remove(CachedResource* resource) { evict(resource); }

    static URL removeFragmentIdentifierIfNeeded(const URL& originalURL);
    
    void revalidationSucceeded(CachedResource* revalidatingResource, const ResourceResponse&);
    void revalidationFailed(CachedResource* revalidatingResource);
    
    // Sets the cache's memory capacities, in bytes. These will hold only approximately, 
    // since the decoded cost of resources like scripts and stylesheets is not known.
    //  - minDeadBytes: The maximum number of bytes that dead resources should consume when the cache is under pressure.
    //  - maxDeadBytes: The maximum number of bytes that dead resources should consume when the cache is not under pressure.
    //  - totalBytes: The maximum number of bytes that the cache should consume overall.
    WEBCORE_EXPORT void setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes);

    // Turn the cache on and off.  Disabling the cache will remove all resources from the cache.  They may
    // still live on if they are referenced by some Web page though.
    WEBCORE_EXPORT void setDisabled(bool);
    bool disabled() const { return m_disabled; }

    WEBCORE_EXPORT void evictResources();
    
    void setPruneEnabled(bool enabled) { m_pruneEnabled = enabled; }
    void prune();
    void pruneToPercentage(float targetPercentLive);

    void setDeadDecodedDataDeletionInterval(std::chrono::milliseconds interval) { m_deadDecodedDataDeletionInterval = interval; }
    std::chrono::milliseconds deadDecodedDataDeletionInterval() const { return m_deadDecodedDataDeletionInterval; }

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(CachedResource*);
    void removeFromLRUList(CachedResource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, int delta);

    // Track decoded resources that are in the cache and referenced by a Web page.
    void insertInLiveDecodedResourcesList(CachedResource*);
    void removeFromLiveDecodedResourcesList(CachedResource*);

    void addToLiveResourcesSize(CachedResource*);
    void removeFromLiveResourcesSize(CachedResource*);

    static void removeUrlFromCache(ScriptExecutionContext*, const String& urlString, SessionID);
    static void removeRequestFromCache(ScriptExecutionContext*, const ResourceRequest&, SessionID);
    static void removeRequestFromSessionCaches(ScriptExecutionContext*, const ResourceRequest&);

    // Function to collect cache statistics for the caches window in the Safari Debug menu.
    WEBCORE_EXPORT Statistics getStatistics();
    
    void resourceAccessed(CachedResource*);

    typedef HashSet<RefPtr<SecurityOrigin>> SecurityOriginSet;
    WEBCORE_EXPORT void removeResourcesWithOrigin(SecurityOrigin*);
    WEBCORE_EXPORT void getOriginsWithCache(SecurityOriginSet& origins);

    unsigned minDeadCapacity() const { return m_minDeadCapacity; }
    unsigned maxDeadCapacity() const { return m_maxDeadCapacity; }
    unsigned capacity() const { return m_capacity; }
    unsigned liveSize() const { return m_liveSize; }
    unsigned deadSize() const { return m_deadSize; }

#if USE(CG)
    // FIXME: Remove the USE(CG) once we either make NativeImagePtr a smart pointer on all platforms or
    // remove the usage of CFRetain() in MemoryCache::addImageToCache() so as to make the code platform-independent.
    WEBCORE_EXPORT bool addImageToCache(NativeImagePtr, const URL&, const String& cachePartition);
    WEBCORE_EXPORT void removeImageFromCache(const URL&, const String& cachePartition);
#endif

    // pruneDead*() - Flush decoded and encoded data from resources not referenced by Web pages.
    // pruneLive*() - Flush decoded data from resources still referenced by Web pages.
    WEBCORE_EXPORT void pruneDeadResources(); // Automatically decide how much to prune.
    WEBCORE_EXPORT void pruneLiveResources(bool shouldDestroyDecodedDataForAllLiveResources = false);

private:
    void pruneDeadResourcesToPercentage(float prunePercentage); // Prune to % current size
    void pruneLiveResourcesToPercentage(float prunePercentage);
    void pruneDeadResourcesToSize(unsigned targetSize);
    void pruneLiveResourcesToSize(unsigned targetSize, bool shouldDestroyDecodedDataForAllLiveResources = false);

    MemoryCache();
    ~MemoryCache(); // Not implemented to make sure nobody accidentally calls delete -- WebCore does not delete singletons.

    LRUList* lruListFor(CachedResource*);
#ifndef NDEBUG
    void dumpStats();
    void dumpLRULists(bool includeLive) const;
#endif

    unsigned liveCapacity() const;
    unsigned deadCapacity() const;

    void evict(CachedResource*);

    WEBCORE_EXPORT CachedResource* resourceForRequestImpl(const ResourceRequest&, CachedResourceMap&);
    static void removeRequestFromCacheImpl(ScriptExecutionContext*, const ResourceRequest&, SessionID);
    static void removeRequestFromSessionCachesImpl(ScriptExecutionContext*, const ResourceRequest&);
    static void crossThreadRemoveRequestFromCache(ScriptExecutionContext&, PassOwnPtr<CrossThreadResourceRequestData>, SessionID);
    static void crossThreadRemoveRequestFromSessionCaches(ScriptExecutionContext&, PassOwnPtr<CrossThreadResourceRequestData>);

    CachedResourceMap& getSessionMap(SessionID);

    bool m_disabled;  // Whether or not the cache is enabled.
    bool m_pruneEnabled;
    bool m_inPruneResources;

    unsigned m_capacity;
    unsigned m_minDeadCapacity;
    unsigned m_maxDeadCapacity;
    std::chrono::milliseconds m_deadDecodedDataDeletionInterval;

    unsigned m_liveSize; // The number of bytes currently consumed by "live" resources in the cache.
    unsigned m_deadSize; // The number of bytes currently consumed by "dead" resources in the cache.

    // Size-adjusted and popularity-aware LRU list collection for cache objects.  This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" multiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_allResources;
    
    // List just for live resources with decoded data.  Access to this list is based off of painting the resource.
    LRUList m_liveDecodedResources;
    
    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being 
    // referenced by a Web page).
    SessionCachedResourceMap m_sessionResources;
};

// Function to obtain the global cache.
WEBCORE_EXPORT MemoryCache* memoryCache();

}

#endif
