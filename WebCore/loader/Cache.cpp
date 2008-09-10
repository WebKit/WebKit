/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
*/

#include "config.h"
#include "Cache.h"

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Image.h"
#include "ResourceHandle.h"
#include "SystemTime.h"
#include <stdio.h>

#ifndef NDEBUG
#include "CString.h"
#include "FileSystem.h"
#include "PlatformString.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#endif

using namespace std;

namespace WebCore {

#ifndef NDEBUG
class SimulatedCachedResource : public CachedResource {
public:
    SimulatedCachedResource(const String& url, Type type)
        : CachedResource(url, type)
        , m_peakEncodedSize(0)
        , m_peakDecodedSize(0)
        , m_suppressSizeChanges(false)
    {
    }

    virtual void load(DocLoader*) { }
    virtual void data(PassRefPtr<SharedBuffer>, bool) { }
    virtual void error() { }

    virtual void destroyDecodedData() { setDecodedSize(0); }

    unsigned peakEncodedSize() const { return m_peakEncodedSize; }
    void setPeakEncodedSize(unsigned size) { m_peakEncodedSize = size; }

    unsigned peakDecodedSize() const { return m_peakDecodedSize; }
    void setPeakDecodedSize(unsigned size) { m_peakDecodedSize = size; }

    double finalDecodedDataAccessTime() const { return m_finalDecodedAccessTime; }
    void setFinalDecodedDataAccessTime(double time) { m_finalDecodedAccessTime = time; }

    bool suppressSizeChanges() const { return m_suppressSizeChanges; }
    void setSuppressSizeChanges(bool suppress = true) { m_suppressSizeChanges = suppress; }

private:
    unsigned m_peakEncodedSize;
    unsigned m_peakDecodedSize;
    bool m_suppressSizeChanges;
    double m_finalDecodedAccessTime;
};

class CacheEventLogger : public Noncopyable {
public:
    CacheEventLogger();

    void log(double time, Cache::EventType, CachedResource*, unsigned size);

    unsigned getIdentifierForURL(const String&);
    void removeIdentifierForURL(const String&);

    void setTypeForResourceIdentifier(unsigned, CachedResource::Type);
    void updatePeakDecodedSizeForResourceIdentifier(unsigned identifier, unsigned decodedSize);

private:
    SQLiteDatabase m_database;
    unsigned m_lastIdentifier;
};

CacheEventLogger::CacheEventLogger()
    : m_lastIdentifier(0)
{
    String logDirectory = pathByAppendingComponent(homeDirectoryPath(), "WebCore Cache Logs");
    makeAllDirectories(logDirectory);
    m_database.open(pathByAppendingComponent(logDirectory, String::number(static_cast<unsigned>(WebCore::currentTime())) + ".db"));
    m_database.setSynchronous(SQLiteDatabase::SyncOff);
    m_database.executeCommand("CREATE TABLE IF NOT EXISTS log (time REAL, eventType INTEGER, identifier INTEGER, encodedSize INTEGER)");
    m_database.executeCommand("CREATE TEMPORARY TABLE resources (url UNIQUE PRIMARY KEY, identifier INTEGER)");
    m_database.executeCommand("CREATE TABLE IF NOT EXISTS typesAndSizes (identifier UNIQUE PRIMARY KEY, resourceType INTEGER, peakDecodedSize INTEGER DEFAULT 0)");
}

void CacheEventLogger::log(double time, Cache::EventType type, CachedResource* resource, unsigned size)
{
    SQLiteStatement statement(m_database, "INSERT INTO log (time, eventType, identifier, encodedSize) VALUES (?, ?, ?, ?)");
    statement.prepare();
    statement.bindDouble(1, time);
    statement.bindInt64(2, type);
    ASSERT(resource->m_identifier);
    statement.bindInt64(3, resource->m_identifier);
    statement.bindInt64(4, size);
    statement.step();
}

unsigned CacheEventLogger::getIdentifierForURL(const String& url)
{
    SQLiteStatement selectStatement(m_database, "SELECT identifier FROM resources WHERE url = ?");
    selectStatement.prepare();
    selectStatement.bindText(1, url);
    selectStatement.step();
    unsigned identifier = selectStatement.getColumnInt(0);
    if (identifier)
        return identifier;

    identifier = ++m_lastIdentifier;
    SQLiteStatement insertStatement(m_database, "INSERT INTO resources (url, identifier) VALUES (?, ?)");
    insertStatement.prepare();
    insertStatement.bindText(1, url);
    insertStatement.bindInt64(2, identifier);
    insertStatement.step();
    return identifier;
}

void CacheEventLogger::removeIdentifierForURL(const String& url)
{
    SQLiteStatement statement(m_database, "DELETE FROM resources WHERE url = ?");
    statement.prepare();
    statement.bindText(1, url);
    statement.step();
}

void CacheEventLogger::setTypeForResourceIdentifier(unsigned identifier, CachedResource::Type resourceType)
{
    SQLiteStatement statement(m_database, "INSERT OR REPLACE INTO typesAndSizes (identifier, resourceType) VALUES (?, ?)");
    statement.prepare();
    statement.bindInt64(1, identifier);
    statement.bindInt64(2, resourceType);
    statement.step();
}

void CacheEventLogger::updatePeakDecodedSizeForResourceIdentifier(unsigned identifier, unsigned decodedSize)
{
    SQLiteStatement statement(m_database, "UPDATE typesAndSizes SET peakDecodedSize = ? WHERE identifier = ? AND peakDecodedSize < ?");
    statement.prepare();
    statement.bindInt64(1, decodedSize);
    statement.bindInt64(2, identifier);
    statement.bindInt64(3, decodedSize);
    statement.step();
}

#endif

static const int cDefaultCacheCapacity = 8192 * 1024;
static const double cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.

Cache* cache()
{
    static Cache* staticCache = new Cache;
    return staticCache;
}

Cache::Cache()
: m_disabled(false)
, m_pruneEnabled(true)
, m_capacity(cDefaultCacheCapacity)
, m_minDeadCapacity(0)
, m_maxDeadCapacity(cDefaultCacheCapacity)
, m_liveSize(0)
, m_deadSize(0)
{
#ifndef NDEBUG
    m_eventLogger = new CacheEventLogger();
    m_inSimulation = false;
    m_simulatedTime = 0;
#endif
}

static CachedResource* createResource(CachedResource::Type type, const KURL& url, const String& charset)
{
    switch (type) {
    case CachedResource::ImageResource:
        return new CachedImage(url.string());
    case CachedResource::CSSStyleSheet:
        return new CachedCSSStyleSheet(url.string(), charset);
    case CachedResource::Script:
        return new CachedScript(url.string(), charset);
    case CachedResource::FontResource:
        return new CachedFont(url.string());
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return new CachedXSLStyleSheet(url.string());
#endif
#if ENABLE(XBL)
    case CachedResource::XBLStyleSheet:
        return new CachedXBLDocument(url.string());
#endif
    default:
        break;
    }

    return 0;
}

CachedResource* Cache::requestResource(DocLoader* docLoader, CachedResource::Type type, const KURL& url, const String& charset, bool isPreload)
{
    // FIXME: Do we really need to special-case an empty URL?
    // Would it be better to just go on with the cache code and let it fail later?
    if (url.isEmpty())
        return 0;
    
    // Look up the resource in our map.
    CachedResource* resource = m_resources.get(url.string());

#ifndef NDEBUG
    bool created = false;
#endif
    if (resource) {
        if (isPreload && !resource->isPreloaded())
            return 0;
        if (
#ifndef NDEBUG
            !m_inSimulation &&
#endif
                FrameLoader::restrictAccessToLocal() && !FrameLoader::canLoad(url, String(), docLoader->doc())) {
            Document* doc = docLoader->doc();
            if(doc && !isPreload)
                FrameLoader::reportLocalLoadFailed(doc->frame(), resource->url());
            return 0;
        }
    } else {
        if (
#ifndef NDEBUG
            !m_inSimulation &&
#endif
                FrameLoader::restrictAccessToLocal() && !FrameLoader::canLoad(url, String(), docLoader->doc())) {
            Document* doc = docLoader->doc();
            if(doc && !isPreload)
                FrameLoader::reportLocalLoadFailed(doc->frame(), url.string());
            return 0;
        }

        // The resource does not exist. Create it.
#ifndef NDEBUG
        created = true;
        if (m_inSimulation)
            resource = new SimulatedCachedResource(url.string(), type);
        else
#endif
            resource = createResource(type, url, charset);
        ASSERT(resource);

#ifndef NDEBUG
        if (!m_inSimulation) {
            resource->m_identifier = m_eventLogger->getIdentifierForURL(url);
            m_eventLogger->setTypeForResourceIdentifier(resource->m_identifier, type);
        }
#endif

        // Pretend the resource is in the cache, to prevent it from being deleted during the load() call.
        // FIXME: CachedResource should just use normal refcounting instead.
        resource->setInCache(true);
        
        resource->load(docLoader);
        
        if (!disabled())
            m_resources.set(url.string(), resource);  // The size will be added in later once the resource is loaded and calls back to us with the new size.
        else {
            // Kick the resource out of the cache, because the cache is disabled.
            resource->setInCache(false);
            resource->setDocLoader(docLoader);
            if (resource->errorOccurred()) {
                // We don't support immediate loads, but we do support immediate failure.
                // In that case we should to delete the resource now and return 0 because otherwise
                // it would leak if no ref/deref was ever done on it.
                delete resource;
                return 0;
            }
        }
    }

    if (resource->type() != type)
        return 0;

#if USE(LOW_BANDWIDTH_DISPLAY)
    // addLowBandwidthDisplayRequest() returns true if requesting CSS or JS during low bandwidth display.
    // Here, return 0 to not block parsing or layout.
    if (docLoader->frame() && docLoader->frame()->loader()->addLowBandwidthDisplayRequest(resource))
        return 0;
#endif

#ifndef NDEBUG
    log(WebCore::currentTime(), created ? RequestResourceCreatedEvent : RequestResourceExistedEvent, resource);
#endif
    if (!disabled()) {
        // This will move the resource to the front of its LRU list and increase its access count.
        resourceAccessed(resource);
    }

    return resource;
}
    
CachedCSSStyleSheet* Cache::requestUserCSSStyleSheet(DocLoader* docLoader, const String& url, const String& charset)
{
    CachedCSSStyleSheet* userSheet;
    if (CachedResource* existing = m_resources.get(url)) {
        if (existing->type() != CachedResource::CSSStyleSheet)
            return 0;
        userSheet = static_cast<CachedCSSStyleSheet*>(existing);
    } else {
        userSheet = new CachedCSSStyleSheet(url, charset);

        // Pretend the resource is in the cache, to prevent it from being deleted during the load() call.
        // FIXME: CachedResource should just use normal refcounting instead.
        userSheet->setInCache(true);
        // Don't load incrementally, skip load checks, don't send resource load callbacks.
        userSheet->load(docLoader, false, true, false);
        if (!disabled())
            m_resources.set(url, userSheet);
        else
            userSheet->setInCache(false);
    }

    if (!disabled()) {
        // This will move the resource to the front of its LRU list and increase its access count.
        resourceAccessed(userSheet);
    }

    return userSheet;
}
    
void Cache::revalidateResource(CachedResource* resource, DocLoader* docLoader)
{
    ASSERT(resource);
    ASSERT(!disabled());
    if (resource->resourceToRevalidate())
        return;
    if (!resource->canUseCacheValidator()) {
        remove(resource);
        return;
    }
    const String& url = resource->url();
    CachedResource* newResource = createResource(resource->type(), KURL(url), resource->encoding());
    newResource->setResourceToRevalidate(resource);
    remove(resource);
    m_resources.set(url, newResource);
    newResource->setInCache(true);
    resourceAccessed(newResource);
    newResource->load(docLoader);
}
    
void Cache::revalidationSucceeded(CachedResource* revalidatingResource, const ResourceResponse& response)
{
    CachedResource* resource = revalidatingResource->resourceToRevalidate();
    ASSERT(resource);
    ASSERT(!resource->inCache());
    ASSERT(resource->isLoaded());
    
    remove(revalidatingResource);

    ASSERT(!m_resources.get(resource->url()));
    m_resources.set(resource->url(), resource);
    resource->setInCache(true);
    resource->setExpirationDate(response.expirationDate());
    insertInLRUList(resource);
    int delta = resource->size();
    if (resource->decodedSize() && resource->hasClients())
        insertInLiveDecodedResourcesList(resource);
    if (delta)
        adjustSize(resource->hasClients(), delta);
    
    revalidatingResource->switchClientsToRevalidatedResource();
    // this deletes the revalidating resource
    revalidatingResource->clearResourceToRevalidate();
}

void Cache::revalidationFailed(CachedResource* revalidatingResource)
{
    ASSERT(revalidatingResource->resourceToRevalidate());
    revalidatingResource->clearResourceToRevalidate();
}

CachedResource* Cache::resourceForURL(const String& url)
{
    return m_resources.get(url);
}

unsigned Cache::deadCapacity() const 
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    unsigned capacity = m_capacity - min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

unsigned Cache::liveCapacity() const 
{ 
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

void Cache::pruneLiveResources()
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = liveCapacity();
    if (m_liveSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    double currentTime = 
#ifndef NDEBUG
        m_inSimulation ? m_simulatedTime : 
#endif
        Frame::currentPaintTimeStamp();
    if (!currentTime) // In case prune is called directly, outside of a Frame paint.
        currentTime = WebCore::currentTime();
    
    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the least recently accessed of the objects.
    CachedResource* current = m_liveDecodedResources.m_tail;
    while (current) {
        CachedResource* prev = current->m_prevInLiveResourcesList;
        ASSERT(current->hasClients());
        if (current->isLoaded() && current->decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            double elapsedTime = currentTime - current->m_lastDecodedAccessTime;
            if (elapsedTime < cMinDelayBeforeLiveDecodedPrune)
                return;

            // Destroy our decoded data. This will remove us from 
            // m_liveDecodedResources, and possibly move us to a differnt LRU 
            // list in m_allResources.
            current->destroyDecodedData();

            if (m_liveSize <= targetSize)
                return;
        }
        current = prev;
    }
}

void Cache::pruneDeadResources()
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = deadCapacity();
    if (m_deadSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    int size = m_allResources.size();
    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        CachedResource* current = m_allResources[i].m_tail;
        
        // First flush all the decoded data in this queue.
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (!current->hasClients() && !current->isPreloaded() && current->isLoaded() && current->decodedSize()) {
                // Destroy our decoded data. This will remove us from 
                // m_liveDecodedResources, and possibly move us to a differnt 
                // LRU list in m_allResources.
                current->destroyDecodedData();
                
                if (m_deadSize <= targetSize)
                    return;
            }
            current = prev;
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (!current->hasClients() && !current->isPreloaded()) {
                evict(current);

                if (m_deadSize <= targetSize)
                    return;
            }
            current = prev;
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

void Cache::setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

void Cache::remove(CachedResource* resource)
{
#ifndef NDEBUG
    log(WebCore::currentTime(), RemoveEvent, resource);
    if (m_eventLogger->getIdentifierForURL(resource->url()) == resource->m_identifier)
        m_eventLogger->removeIdentifierForURL(resource->url());
#endif
    evict(resource);
}

void Cache::evict(CachedResource* resource)
{
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    if (resource->inCache()) {
        // Remove from the resource map.
        m_resources.remove(resource->url());
        resource->setInCache(false);

        // Remove from the appropriate LRU list.
        removeFromLRUList(resource);
        removeFromLiveDecodedResourcesList(resource);
        
        // Notify all doc loaders that might be observing this object still that it has been
        // extracted from the set of resources.
        HashSet<DocLoader*>::iterator end = m_docLoaders.end();
        for (HashSet<DocLoader*>::iterator itr = m_docLoaders.begin(); itr != end; ++itr)
            (*itr)->removeCachedResource(resource);

        // Subtract from our size totals.
        int delta = -static_cast<int>(resource->size());
        if (delta)
            adjustSize(resource->hasClients(), delta);
    } else
        ASSERT(m_resources.get(resource->url()) != resource);

    if (resource->canDelete())
        delete resource;
}

void Cache::addDocLoader(DocLoader* docLoader)
{
    m_docLoaders.add(docLoader);
}

void Cache::removeDocLoader(DocLoader* docLoader)
{
    m_docLoaders.remove(docLoader);
}

static inline unsigned fastLog2(unsigned i)
{
    unsigned log2 = 0;
    if (i & (i - 1))
        log2 += 1;
    if (i >> 16)
        log2 += 16, i >>= 16;
    if (i >> 8)
        log2 += 8, i >>= 8;
    if (i >> 4)
        log2 += 4, i >>= 4;
    if (i >> 2)
        log2 += 2, i >>= 2;
    if (i >> 1)
        log2 += 1;
    return log2;
}

Cache::LRUList* Cache::lruListFor(CachedResource* resource)
{
    unsigned accessCount = max(resource->accessCount(), 1U);
    unsigned queueIndex = fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void Cache::removeFromLRUList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (resource->accessCount() == 0)
        return;

#ifndef NDEBUG
    unsigned oldListIndex = resource->m_lruIndex;
#endif

    LRUList* list = lruListFor(resource);

#ifndef NDEBUG
    // Verify that the list we got is the list we want.
    ASSERT(resource->m_lruIndex == oldListIndex);

    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInAllResourcesList;
    CachedResource* prev = resource->m_prevInAllResourcesList;
    
    if (next == 0 && prev == 0 && list->m_head != resource)
        return;
    
    resource->m_nextInAllResourcesList = 0;
    resource->m_prevInAllResourcesList = 0;
    
    if (next)
        next->m_prevInAllResourcesList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInAllResourcesList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void Cache::insertInLRUList(CachedResource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInAllResourcesList && !resource->m_prevInAllResourcesList);
    ASSERT(resource->inCache());
    ASSERT(resource->accessCount() > 0);
    
    LRUList* list = lruListFor(resource);

    resource->m_nextInAllResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInAllResourcesList = resource;
    list->m_head = resource;
    
    if (!resource->m_nextInAllResourcesList)
        list->m_tail = resource;
        
#ifndef NDEBUG
    // Verify that we are in now in the list like we should be.
    list = lruListFor(resource);
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void Cache::resourceAccessed(CachedResource* resource)
{
    ASSERT(resource->inCache());
    
    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    removeFromLRUList(resource);
    
    // Add to our access count.
    resource->increaseAccessCount();
    
    // Now insert into the new queue.
    insertInLRUList(resource);
}

void Cache::removeFromLiveDecodedResourcesList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->m_inLiveDecodedResourcesList)
        return;
    resource->m_inLiveDecodedResourcesList = false;

#ifndef NDEBUG
    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInLiveResourcesList;
    CachedResource* prev = resource->m_prevInLiveResourcesList;
    
    if (next == 0 && prev == 0 && m_liveDecodedResources.m_head != resource)
        return;
    
    resource->m_nextInLiveResourcesList = 0;
    resource->m_prevInLiveResourcesList = 0;
    
    if (next)
        next->m_prevInLiveResourcesList = prev;
    else if (m_liveDecodedResources.m_tail == resource)
        m_liveDecodedResources.m_tail = prev;

    if (prev)
        prev->m_nextInLiveResourcesList = next;
    else if (m_liveDecodedResources.m_head == resource)
        m_liveDecodedResources.m_head = next;
}

void Cache::insertInLiveDecodedResourcesList(CachedResource* resource)
{
    // Make sure we aren't in the list already.
    ASSERT(!resource->m_nextInLiveResourcesList && !resource->m_prevInLiveResourcesList && !resource->m_inLiveDecodedResourcesList);
    resource->m_inLiveDecodedResourcesList = true;

    resource->m_nextInLiveResourcesList = m_liveDecodedResources.m_head;
    if (m_liveDecodedResources.m_head)
        m_liveDecodedResources.m_head->m_prevInLiveResourcesList = resource;
    m_liveDecodedResources.m_head = resource;
    
    if (!resource->m_nextInLiveResourcesList)
        m_liveDecodedResources.m_tail = resource;
        
#ifndef NDEBUG
    // Verify that we are in now in the list like we should be.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void Cache::addToLiveResourcesSize(CachedResource* resource)
{
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void Cache::removeFromLiveResourcesSize(CachedResource* resource)
{
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
}

void Cache::adjustSize(bool live, int delta)
{
    if (live) {
        ASSERT(delta >= 0 || ((int)m_liveSize + delta >= 0));
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || ((int)m_deadSize + delta >= 0));
        m_deadSize += delta;
    }
}

Cache::Statistics Cache::getStatistics()
{
    Statistics stats;
    CachedResourceMap::iterator e = m_resources.end();
    for (CachedResourceMap::iterator i = m_resources.begin(); i != e; ++i) {
        CachedResource *o = i->second;
        switch (o->type()) {
            case CachedResource::ImageResource:
                stats.images.count++;
                stats.images.size += o->size();
                stats.images.liveSize += o->hasClients() ? o->size() : 0;
                stats.images.decodedSize += o->decodedSize();
                break;

            case CachedResource::CSSStyleSheet:
                stats.cssStyleSheets.count++;
                stats.cssStyleSheets.size += o->size();
                stats.cssStyleSheets.liveSize += o->hasClients() ? o->size() : 0;
                stats.cssStyleSheets.decodedSize += o->decodedSize();
                break;

            case CachedResource::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                stats.scripts.liveSize += o->hasClients() ? o->size() : 0;
                stats.scripts.decodedSize += o->decodedSize();
                break;
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
                stats.xslStyleSheets.count++;
                stats.xslStyleSheets.size += o->size();
                stats.xslStyleSheets.liveSize += o->hasClients() ? o->size() : 0;
                stats.xslStyleSheets.decodedSize += o->decodedSize();
                break;
#endif
            case CachedResource::FontResource:
                stats.fonts.count++;
                stats.fonts.size += o->size();
                stats.fonts.liveSize += o->hasClients() ? o->size() : 0;
                stats.fonts.decodedSize += o->decodedSize();
                break;
#if ENABLE(XBL)
            case CachedResource::XBL:
                stats.xblDocs.count++;
                stats.xblDocs.size += o->size();
                stats.xblDocs.liveSize += o->hasClients() ? o->size() : 0;
                stats.xblDocs.decodedSize += o->decodedSize();
                break;
#endif
            default:
                break;
        }
    }
    
    return stats;
}

void Cache::setDisabled(bool disabled)
{
    m_disabled = disabled;
    if (!m_disabled)
        return;

    for (;;) {
        CachedResourceMap::iterator i = m_resources.begin();
        if (i == m_resources.end())
            break;
        remove(i->second);
    }
}

#ifndef NDEBUG
void Cache::dumpLRULists(bool includeLive) const
{
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (includeLive || !current->hasClients())
                printf("(%.1fK, %.1fK, %uA, %dR); ", current->decodedSize() / 1024.0f, current->encodedSize() / 1024.0f, current->accessCount(), current->hasClients());
            current = prev;
        }
    }
}

void Cache::log(double time, EventType type, CachedResource* resource, unsigned size)
{
    if (m_inSimulation)
        return;
    m_eventLogger->log(time, type, resource, size);
}

void Cache::updatePeakDecodedSizeForResource(CachedResource* resource)
{
    if (m_inSimulation)
        return;
    m_eventLogger->updatePeakDecodedSizeForResourceIdentifier(resource->m_identifier, resource->decodedSize());
}

#endif

String Cache::runSimulation(const String& databasePath)
{
    String result;
#ifndef NDEBUG
    m_inSimulation = true;

    String utf8Charset("UTF-8");
    String prefix("sim:");
    HashMap<unsigned, SimulatedCachedResource*> resourceMap;
    Vector<double> accessIntervals;
    Vector<double> decodedDataAges;
    Vector<std::pair<double, unsigned> > uselessDecodedDataSizeLog;
    CachedResourceClient client;

    SQLiteDatabase database;
    database.open(databasePath);
    SQLiteStatement readLogStatement(database, "SELECT * FROM log ORDER BY time ASC");
    readLogStatement.prepare();

    unsigned totalEncodedSizeReloaded = 0;
    unsigned totalDecodingDone = 0;

    while (readLogStatement.step() == SQLResultRow) {
        double time = readLogStatement.getColumnDouble(0);
        Cache::EventType type = static_cast<Cache::EventType>(readLogStatement.getColumnInt(1));
        unsigned identifier = readLogStatement.getColumnInt(2);
        unsigned size = readLogStatement.getColumnInt(3);

        m_simulatedTime = time;
        SimulatedCachedResource* existingResource = resourceMap.get(identifier);

        if (!existingResource && type != RequestResourceCreatedEvent && type != RequestResourceExistedEvent) {
            LOG_ERROR("No existing resource for %d event for resource identifier %d at time %f", type, identifier, time);
            continue;
        }

        switch (type) {
            case RequestResourceCreatedEvent:
            case RequestResourceExistedEvent:
                {
                    SQLiteStatement getTypeStatement(database, "SELECT resourceType, peakDecodedSize FROM typesAndSizes WHERE identifier = ?");
                    getTypeStatement.prepare();
                    getTypeStatement.bindInt64(1, identifier);
                    getTypeStatement.step();
                    SimulatedCachedResource* resource = static_cast<SimulatedCachedResource*>(cache()->requestResource(0, static_cast<CachedResource::Type>(getTypeStatement.getColumnInt(0)), KURL(prefix + String::number(identifier)), utf8Charset));
                    if (resource == existingResource) {
                        if (type == RequestResourceCreatedEvent)
                            resource->setSuppressSizeChanges();
                        break;
                    }

                    if (existingResource) {
                        ASSERT(!existingResource->hasClients());
                        ASSERT(!existingResource->inCache());
                        unsigned existingPeakEncodedSize = existingResource->peakEncodedSize();
                        totalEncodedSizeReloaded += existingPeakEncodedSize;
                        // Should cause the resource to be deleted.
                        existingResource->setRequest(0);
                        if (type == RequestResourceExistedEvent)
                            resource->setPeakEncodedSize(existingPeakEncodedSize);
                    }
                    resource->setPeakDecodedSize(getTypeStatement.getColumnInt(1));

                    SQLiteStatement getFinalDecodedDataAccessTimeStatement(database, "SELECT MAX(time) FROM log WHERE identifier = ? AND eventType = ?");
                    getFinalDecodedDataAccessTimeStatement.prepare();
                    getFinalDecodedDataAccessTimeStatement.bindInt64(1, identifier);
                    getFinalDecodedDataAccessTimeStatement.bindInt64(2, DecodedDataAccessEvent);
                    getFinalDecodedDataAccessTimeStatement.step();
                    resource->setFinalDecodedDataAccessTime(getFinalDecodedDataAccessTimeStatement.getColumnDouble(0));

                    // Ensures that canDelete() is always false, so we control deletion.
                    resource->m_request = reinterpret_cast<Request*>(-1);
                    resourceMap.set(identifier, resource);
                }
                break;
            case RemoveEvent:
                cache()->remove(existingResource);
                break;
            case FirstRefEvent:
                existingResource->addClient(&client);
                if (!existingResource->encodedSize()) {
                    if (unsigned peakEncodedSize = existingResource->peakEncodedSize())
                        existingResource->setEncodedSize(peakEncodedSize);  // Simulate instantaneous loading
                }
                break;
            case LastDerefEvent:
                if (!existingResource->hasClients()) {
                    LOG_ERROR("Extra last deref event for resource identifier %d at time %f", identifier, time);
                    continue;
                }
                existingResource->removeClient(&client);
                break;
            case EncodedSizeEvent:
                if (size > existingResource->peakEncodedSize())
                    existingResource->setPeakEncodedSize(size);
                if (!existingResource->suppressSizeChanges()) {
                    existingResource->destroyDecodedData();
                    existingResource->setEncodedSize(size);
                }
                break;
            case DecodedDataAccessEvent:
                if (existingResource->m_lastDecodedAccessTime)
                    accessIntervals.append(time - existingResource->m_lastDecodedAccessTime);
                existingResource->didAccessDecodedData(time);
                if (!existingResource->decodedSize()) {
                    if (unsigned peakDecodedSize = existingResource->peakDecodedSize()) {
                        existingResource->setDecodedSize(peakDecodedSize);
                        totalDecodingDone += peakDecodedSize;
                    }
                }
                break;
            default:
                break;
        }

        CachedResourceMap::iterator e = m_resources.end();
        unsigned totalUselessDecodedDataSize = 0;
        for (CachedResourceMap::iterator i = m_resources.begin(); i != e; ++i) {
            SimulatedCachedResource* o = static_cast<SimulatedCachedResource*>(i->second);
            if (o->type() != CachedResource::ImageResource)
                continue;
            if (m_simulatedTime > o->finalDecodedDataAccessTime())
                totalUselessDecodedDataSize += o->decodedSize();
        }
        if (uselessDecodedDataSizeLog.isEmpty() || totalUselessDecodedDataSize != uselessDecodedDataSizeLog.last().second)
            uselessDecodedDataSizeLog.append(std::pair<double, unsigned>(m_simulatedTime, totalUselessDecodedDataSize));
    }

    FILE* logFile = fopen("/Volumes/Data/Users/dan/Desktop/uselessDecodedDataSizeLog.txt", "w");
    for (size_t i = 0; i < uselessDecodedDataSizeLog.size(); ++i)
        fprintf(logFile, "%lf\t%d\n", uselessDecodedDataSizeLog[i].first, uselessDecodedDataSizeLog[i].second);
    fclose(logFile);

    unsigned totalEncodedSize = 0;
    unsigned totalEncodedSizeInCache = 0;
    unsigned totalDecodedSize = 0;
    unsigned totalDecodedSizeInCache = 0;
    HashMap<unsigned, SimulatedCachedResource*>::iterator end = resourceMap.end();
    for (HashMap<unsigned, SimulatedCachedResource*>::iterator it = resourceMap.begin(); it != end; ++it) {
        totalEncodedSize += it->second->peakEncodedSize();
        totalDecodedSize += it->second->peakDecodedSize();
        if (it->second->inCache()) {
            totalEncodedSizeInCache += it->second->peakEncodedSize();
            totalDecodedSizeInCache += it->second->peakDecodedSize();
            if (it->second->decodedSize())
                decodedDataAges.append(m_simulatedTime - it->second->m_lastDecodedAccessTime);
        }
    }

    std::sort(accessIntervals.begin(), accessIntervals.end());
    std::sort(decodedDataAges.begin(), decodedDataAges.end());

    result = String::format("Total encoded data size: %d\nCached encoded data size: %d\nTotal cache miss encoded data size: %d\nTotal decoded data size: %d\nCached decoded data size: %d\nTotal decoded bytes produced: %d\n",
                            totalEncodedSize, totalEncodedSizeInCache, totalEncodedSizeReloaded, totalDecodedSize, totalDecodedSizeInCache, totalDecodingDone);

    result += "Decoded data repeated access interval histogram:\n";
    int binTop = 0;
    size_t samplesInBin = 0;
    for (size_t i = 0; i < accessIntervals.size(); ++i) {
        if (accessIntervals[i] > binTop) {
            result += String::number(binTop) + " : " + String::number(samplesInBin) + ", ";
            samplesInBin = 0;
            binTop = static_cast<int>(ceil(accessIntervals[i] / 10) * 10);
        }
        samplesInBin++;
    }
    result += String::number(binTop) + " : " + String::number(samplesInBin) + ".\n";

    result += "Cached decoded data age histogram:\n";
    binTop = 0;
    samplesInBin = 0;
    for (size_t i = 0; i < decodedDataAges.size(); ++i) {
        if (decodedDataAges[i] > binTop) {
            result += String::number(binTop) + " : " + String::number(samplesInBin) + ", ";
            samplesInBin = 0;
            binTop = static_cast<int>(ceil(decodedDataAges[i] / 10) * 10);
        }
        samplesInBin++;
    }
    result += String::number(binTop) + " : " + String::number(samplesInBin) + ".\n";

    m_inSimulation = false;
#endif // NDEBUG
    return result;
}

} // namespace WebCore
