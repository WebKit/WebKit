/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplicationCacheStorage.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheResource.h"
#include "FileSystem.h"
#include "CString.h"
#include "KURL.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

static unsigned urlHostHash(const KURL& url)
{
    unsigned hostStart = url.hostStart();
    unsigned hostEnd = url.hostEnd();
    
    return AlreadyHashed::avoidDeletedValue(StringImpl::computeHash(url.string().characters() + hostStart, hostEnd - hostStart));
}

ApplicationCacheGroup* ApplicationCacheStorage::loadCacheGroup(const KURL& manifestURL)
{
    openDatabase(false);
    if (!m_database.isOpen())
        return 0;

    SQLiteStatement statement(m_database, "SELECT id, manifestURL, newestCache FROM CacheGroups WHERE newestCache IS NOT NULL AND manifestURL=?");
    if (statement.prepare() != SQLResultOk)
        return 0;
    
    statement.bindText(1, manifestURL);
   
    int result = statement.step();
    if (result == SQLResultDone)
        return 0;
    
    if (result != SQLResultRow) {
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
        return 0;
    }
    
    unsigned newestCacheStorageID = (unsigned)statement.getColumnInt64(2);

    RefPtr<ApplicationCache> cache = loadCache(newestCacheStorageID);
    if (!cache)
        return 0;
        
    ApplicationCacheGroup* group = new ApplicationCacheGroup(manifestURL);
      
    group->setStorageID((unsigned)statement.getColumnInt64(0));
    group->setNewestCache(cache.release());

    return group;
}    

ApplicationCacheGroup* ApplicationCacheStorage::findOrCreateCacheGroup(const KURL& manifestURL)
{
    std::pair<CacheGroupMap::iterator, bool> result = m_cachesInMemory.add(manifestURL, 0);
    
    if (!result.second) {
        ASSERT(result.first->second);
        
        return result.first->second;
    }

    // Look up the group in the database
    ApplicationCacheGroup* group = loadCacheGroup(manifestURL);
    
    // If the group was not found we need to create it
    if (!group) {
        group = new ApplicationCacheGroup(manifestURL);
        m_cacheHostSet.add(urlHostHash(manifestURL));
    }
    
    result.first->second = group;
    
    return group;
}

void ApplicationCacheStorage::loadManifestHostHashes()
{
    static bool hasLoadedHashes = false;
    
    if (hasLoadedHashes)
        return;
    
    // We set this flag to true before the database has been opened
    // to avoid trying to open the database over and over if it doesn't exist.
    hasLoadedHashes = true;
    
    openDatabase(false);
    if (!m_database.isOpen())
        return;

    // Fetch the host hashes.
    SQLiteStatement statement(m_database, "SELECT manifestHostHash FROM CacheGroups");    
    if (statement.prepare() != SQLResultOk)
        return;
    
    int result;
    while ((result = statement.step()) == SQLResultRow)
        m_cacheHostSet.add((unsigned)statement.getColumnInt64(0));
}    

ApplicationCacheGroup* ApplicationCacheStorage::cacheGroupForURL(const KURL& url)
{
    loadManifestHostHashes();
    
    // Hash the host name and see if there's a manifest with the same host.
    if (!m_cacheHostSet.contains(urlHostHash(url)))
        return 0;

    // Check if a cache already exists in memory.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it) {
        ApplicationCacheGroup* group = it->second;
        
        if (!protocolHostAndPortAreEqual(url, group->manifestURL()))
            continue;
        
        if (ApplicationCache* cache = group->newestCache()) {
            if (cache->resourceForURL(url))
                return group;
        }
    }
    
    if (!m_database.isOpen())
        return 0;
        
    // Check the database. Look for all cache groups with a newest cache.
    SQLiteStatement statement(m_database, "SELECT id, manifestURL, newestCache FROM CacheGroups WHERE newestCache IS NOT NULL");
    if (statement.prepare() != SQLResultOk)
        return 0;
    
    int result;
    while ((result = statement.step()) == SQLResultRow) {
        KURL manifestURL = KURL(statement.getColumnText(1));

        if (!protocolHostAndPortAreEqual(url, manifestURL))
            continue;

        // We found a cache group that matches. Now check if the newest cache has a resource with
        // a matching URL.
        unsigned newestCacheID = (unsigned)statement.getColumnInt64(2);
        RefPtr<ApplicationCache> cache = loadCache(newestCacheID);

        if (!cache->resourceForURL(url))
            continue;

        ApplicationCacheGroup* group = new ApplicationCacheGroup(manifestURL);
        
        group->setStorageID((unsigned)statement.getColumnInt64(0));
        group->setNewestCache(cache.release());
        
        ASSERT(!m_cachesInMemory.contains(manifestURL));
        m_cachesInMemory.set(group->manifestURL(), group);
        
        return group;
    }

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
    
    return 0;
}

void ApplicationCacheStorage::cacheGroupDestroyed(ApplicationCacheGroup* group)
{
    ASSERT(m_cachesInMemory.get(group->manifestURL()) == group);

    m_cachesInMemory.remove(group->manifestURL());
    
    // If the cache is half-created, we don't want it in the saved set.
    if (!group->savedNewestCachePointer())
        m_cacheHostSet.remove(urlHostHash(group->manifestURL()));
}

void ApplicationCacheStorage::setCacheDirectory(const String& cacheDirectory)
{
    ASSERT(m_cacheDirectory.isNull());
    ASSERT(!cacheDirectory.isNull());
    
    m_cacheDirectory = cacheDirectory;
}

const String& ApplicationCacheStorage::cacheDirectory() const
{
    return m_cacheDirectory;
}


bool ApplicationCacheStorage::executeSQLCommand(const String& sql)
{
    ASSERT(m_database.isOpen());
    
    bool result = m_database.executeCommand(sql);
    if (!result)
        LOG_ERROR("Application Cache Storage: failed to execute statement \"%s\" error \"%s\"", 
                  sql.utf8().data(), m_database.lastErrorMsg());

    return result;
}

static const int SchemaVersion = 2;
    
void ApplicationCacheStorage::verifySchemaVersion()
{
    if (m_database.tableExists("SchemaVersion")) {
        int version = SQLiteStatement(m_database, "SELECT version from SchemaVersion").getColumnInt(0);
        
        if (version == SchemaVersion)
            return;
    }
    
    m_database.clearAllTables();

    SQLiteTransaction createSchemaVersionTable(m_database);
    createSchemaVersionTable.begin();

    executeSQLCommand("CREATE TABLE SchemaVersion (version INTEGER NOT NULL)");
    SQLiteStatement statement(m_database, "INSERT INTO SchemaVersion (version) VALUES (?)");
    if (statement.prepare() != SQLResultOk)
        return;
    
    statement.bindInt64(1, SchemaVersion);
    executeStatement(statement);
    createSchemaVersionTable.commit();
}
    
void ApplicationCacheStorage::openDatabase(bool createIfDoesNotExist)
{
    if (m_database.isOpen())
        return;

    // The cache directory should never be null, but if it for some weird reason is we bail out.
    if (m_cacheDirectory.isNull())
        return;
    
    String applicationCachePath = pathByAppendingComponent(m_cacheDirectory, "ApplicationCache.db");
    if (!createIfDoesNotExist && !fileExists(applicationCachePath))
        return;

    makeAllDirectories(m_cacheDirectory);
    m_database.open(applicationCachePath);
    
    if (!m_database.isOpen())
        return;
    
    verifySchemaVersion();
    
    // Create tables
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheGroups (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "manifestHostHash INTEGER NOT NULL ON CONFLICT FAIL, manifestURL TEXT UNIQUE ON CONFLICT FAIL, newestCache INTEGER)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS Caches (id INTEGER PRIMARY KEY AUTOINCREMENT, cacheGroup INTEGER)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheWhitelistURLs (url TEXT NOT NULL ON CONFLICT FAIL, cache INTEGER NOT NULL ON CONFLICT FAIL)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS FallbackURLs (namespace TEXT NOT NULL ON CONFLICT FAIL, fallbackURL TEXT NOT NULL ON CONFLICT FAIL, "
                      "cache INTEGER NOT NULL ON CONFLICT FAIL)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheEntries (cache INTEGER NOT NULL ON CONFLICT FAIL, type INTEGER, resource INTEGER NOT NULL)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheResources (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL ON CONFLICT FAIL, "
                      "statusCode INTEGER NOT NULL, responseURL TEXT NOT NULL, mimeType TEXT, textEncodingName TEXT, headers TEXT, data INTEGER NOT NULL ON CONFLICT FAIL)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheResourceData (id INTEGER PRIMARY KEY AUTOINCREMENT, data BLOB)");

    // When a cache is deleted, all its entries and its whitelist should be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheDeleted AFTER DELETE ON Caches"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheEntries WHERE cache = OLD.id;"
                      "  DELETE FROM CacheWhitelistURLs WHERE cache = OLD.id;"
                      "  DELETE FROM FallbackURLs WHERE cache = OLD.id;"
                      " END");
    
    // When a cache resource is deleted, its data blob should also be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheResourceDeleted AFTER DELETE ON CacheResources"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheResourceData WHERE id = OLD.data;"
                      " END");
}

bool ApplicationCacheStorage::executeStatement(SQLiteStatement& statement)
{
    bool result = statement.executeCommand();
    if (!result)
        LOG_ERROR("Application Cache Storage: failed to execute statement \"%s\" error \"%s\"", 
                  statement.query().utf8().data(), m_database.lastErrorMsg());
    
    return result;
}    

bool ApplicationCacheStorage::store(ApplicationCacheGroup* group)
{
    ASSERT(group->storageID() == 0);

    SQLiteStatement statement(m_database, "INSERT INTO CacheGroups (manifestHostHash, manifestURL) VALUES (?, ?)");
    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindInt64(1, urlHostHash(group->manifestURL()));
    statement.bindText(2, group->manifestURL());

    if (!executeStatement(statement))
        return false;

    group->setStorageID((unsigned)m_database.lastInsertRowID());
    return true;
}    

bool ApplicationCacheStorage::store(ApplicationCache* cache)
{
    ASSERT(cache->storageID() == 0);
    ASSERT(cache->group()->storageID() != 0);
    
    SQLiteStatement statement(m_database, "INSERT INTO Caches (cacheGroup) VALUES (?)");
    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindInt64(1, cache->group()->storageID());

    if (!executeStatement(statement))
        return false;
    
    unsigned cacheStorageID = (unsigned)m_database.lastInsertRowID();

    // Store all resources
    {
        ApplicationCache::ResourceMap::const_iterator end = cache->end();
        for (ApplicationCache::ResourceMap::const_iterator it = cache->begin(); it != end; ++it) {
            if (!store(it->second.get(), cacheStorageID))
                return false;
        }
    }
    
    // Store the online whitelist
    const HashSet<String>& onlineWhitelist = cache->onlineWhitelist();
    {
        HashSet<String>::const_iterator end = onlineWhitelist.end();
        for (HashSet<String>::const_iterator it = onlineWhitelist.begin(); it != end; ++it) {
            SQLiteStatement statement(m_database, "INSERT INTO CacheWhitelistURLs (url, cache) VALUES (?, ?)");
            statement.prepare();

            statement.bindText(1, *it);
            statement.bindInt64(2, cacheStorageID);

            if (!executeStatement(statement))
                return false;
        }
    }
    
    cache->setStorageID(cacheStorageID);
    return true;
}

bool ApplicationCacheStorage::store(ApplicationCacheResource* resource, unsigned cacheStorageID)
{
    ASSERT(cacheStorageID);
    ASSERT(!resource->storageID());
    
    openDatabase(true);
    
    // First, insert the data
    SQLiteStatement dataStatement(m_database, "INSERT INTO CacheResourceData (data) VALUES (?)");
    if (dataStatement.prepare() != SQLResultOk)
        return false;
    
    if (resource->data()->size())
        dataStatement.bindBlob(1, resource->data()->data(), resource->data()->size());
    
    if (!dataStatement.executeCommand())
        return false;

    unsigned dataId = (unsigned)m_database.lastInsertRowID();

    // Then, insert the resource
    
    // Serialize the headers
    Vector<UChar> stringBuilder;
    
    HTTPHeaderMap::const_iterator end = resource->response().httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = resource->response().httpHeaderFields().begin(); it!= end; ++it) {
        stringBuilder.append(it->first.characters(), it->first.length());
        stringBuilder.append((UChar)':');
        stringBuilder.append(it->second.characters(), it->second.length());
        stringBuilder.append((UChar)'\n');
    }
    
    String headers = String::adopt(stringBuilder);
    
    SQLiteStatement resourceStatement(m_database, "INSERT INTO CacheResources (url, statusCode, responseURL, headers, data, mimeType, textEncodingName) VALUES (?, ?, ?, ?, ?, ?, ?)");
    if (resourceStatement.prepare() != SQLResultOk)
        return false;
    
    resourceStatement.bindText(1, resource->url());
    resourceStatement.bindInt64(2, resource->response().httpStatusCode());
    resourceStatement.bindText(3, resource->response().url());
    resourceStatement.bindText(4, headers);
    resourceStatement.bindInt64(5, dataId);
    resourceStatement.bindText(6, resource->response().mimeType());
    resourceStatement.bindText(7, resource->response().textEncodingName());

    if (!executeStatement(resourceStatement))
        return false;

    unsigned resourceId = (unsigned)m_database.lastInsertRowID();
    
    // Finally, insert the cache entry
    SQLiteStatement entryStatement(m_database, "INSERT INTO CacheEntries (cache, type, resource) VALUES (?, ?, ?)");
    if (entryStatement.prepare() != SQLResultOk)
        return false;
    
    entryStatement.bindInt64(1, cacheStorageID);
    entryStatement.bindInt64(2, resource->type());
    entryStatement.bindInt64(3, resourceId);
    
    if (!executeStatement(entryStatement))
        return false;
    
    resource->setStorageID(resourceId);
    return true;
}

void ApplicationCacheStorage::store(ApplicationCacheResource* resource, ApplicationCache* cache)
{
    ASSERT(cache->storageID());
    
    openDatabase(true);
 
    SQLiteTransaction storeResourceTransaction(m_database);
    storeResourceTransaction.begin();
    
    if (!store(resource, cache->storageID()))
        return;
    
    storeResourceTransaction.commit();
}

bool ApplicationCacheStorage::storeNewestCache(ApplicationCacheGroup* group)
{
    openDatabase(true);
    
    SQLiteTransaction storeCacheTransaction(m_database);
    
    storeCacheTransaction.begin();
    
    if (!group->storageID()) {
        // Store the group
        if (!store(group))
            return false;
    }
    
    ASSERT(group->newestCache());
    ASSERT(!group->newestCache()->storageID());
    
    // Store the newest cache
    if (!store(group->newestCache()))
        return false;
    
    // Update the newest cache in the group.
    
    SQLiteStatement statement(m_database, "UPDATE CacheGroups SET newestCache=? WHERE id=?");
    if (statement.prepare() != SQLResultOk)
        return false;
    
    statement.bindInt64(1, group->newestCache()->storageID());
    statement.bindInt64(2, group->storageID());
    
    if (!executeStatement(statement))
        return false;
    
    storeCacheTransaction.commit();
    return true;
}

static inline void parseHeader(const UChar* header, size_t headerLength, ResourceResponse& response)
{
    int pos = find(header, headerLength, ':');
    ASSERT(pos != -1);
    
    String headerName = String(header, pos);
    String headerValue = String(header + pos + 1, headerLength - pos - 1);
    
    response.setHTTPHeaderField(headerName, headerValue);
}

static inline void parseHeaders(const String& headers, ResourceResponse& response)
{
    int startPos = 0;
    int endPos;
    while ((endPos = headers.find('\n', startPos)) != -1) {
        ASSERT(startPos != endPos);

        parseHeader(headers.characters() + startPos, endPos - startPos, response);
        
        startPos = endPos + 1;
    }
    
    if (startPos != static_cast<int>(headers.length()))
        parseHeader(headers.characters(), headers.length(), response);
}
    
PassRefPtr<ApplicationCache> ApplicationCacheStorage::loadCache(unsigned storageID)
{
    SQLiteStatement cacheStatement(m_database, 
                                   "SELECT url, type, mimeType, textEncodingName, headers, CacheResourceData.data FROM CacheEntries INNER JOIN CacheResources ON CacheEntries.resource=CacheResources.id "
                                   "INNER JOIN CacheResourceData ON CacheResourceData.id=CacheResources.data WHERE CacheEntries.cache=?");
    if (cacheStatement.prepare() != SQLResultOk) {
        LOG_ERROR("Could not prepare cache statement, error \"%s\"", m_database.lastErrorMsg());
        return 0;
    }
    
    cacheStatement.bindInt64(1, storageID);

    RefPtr<ApplicationCache> cache = ApplicationCache::create();
    
    int result;
    while ((result = cacheStatement.step()) == SQLResultRow) {
        KURL url(cacheStatement.getColumnText(0));
        
        unsigned type = (unsigned)cacheStatement.getColumnInt64(1);

        Vector<char> blob;
        cacheStatement.getColumnBlobAsVector(5, blob);
        
        RefPtr<SharedBuffer> data = SharedBuffer::adoptVector(blob);
        
        String mimeType = cacheStatement.getColumnText(2);
        String textEncodingName = cacheStatement.getColumnText(3);
        
        ResourceResponse response(url, mimeType, data->size(), textEncodingName, "");

        String headers = cacheStatement.getColumnText(4);
        parseHeaders(headers, response);
        
        RefPtr<ApplicationCacheResource> resource = ApplicationCacheResource::create(url, response, type, data.release());

        if (type & ApplicationCacheResource::Manifest)
            cache->setManifestResource(resource.release());
        else
            cache->addResource(resource.release());
    }

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache resources, error \"%s\"", m_database.lastErrorMsg());
    
    // Load the online whitelist
    SQLiteStatement whitelistStatement(m_database, "SELECT url FROM CacheWhitelistURLs WHERE cache=?");
    if (whitelistStatement.prepare() != SQLResultOk)
        return 0;
    whitelistStatement.bindInt64(1, storageID);
    
    HashSet<String> whitelist;
    while ((result = whitelistStatement.step()) == SQLResultRow) 
        whitelist.add(whitelistStatement.getColumnText(0));

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache online whitelist, error \"%s\"", m_database.lastErrorMsg());

    cache->setOnlineWhitelist(whitelist);
    
    cache->setStorageID(storageID);

    return cache.release();
}    
    
void ApplicationCacheStorage::remove(ApplicationCache* cache)
{
    if (!cache->storageID())
        return;
    
    openDatabase(false);
    if (!m_database.isOpen())
        return;

    SQLiteStatement statement(m_database, "DELETE FROM Caches WHERE id=?");
    if (statement.prepare() != SQLResultOk)
        return;
    
    statement.bindInt64(1, cache->storageID());
    executeStatement(statement);
}    

void ApplicationCacheStorage::empty()
{
    openDatabase(false);
    
    if (!m_database.isOpen())
        return;
    
    // Clear cache groups, caches and cache resources.
    executeSQLCommand("DELETE FROM CacheGroups");
    executeSQLCommand("DELETE FROM Caches");
    executeSQLCommand("DELETE FROM CacheResources");
    
    // Clear the storage IDs for the caches in memory.
    // The caches will still work, but cached resources will not be saved to disk 
    // until a cache update process has been initiated.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it)
        it->second->clearStorageID();
}    

bool ApplicationCacheStorage::storeCopyOfCache(const String& cacheDirectory, ApplicationCache* cache)
{
    // Create a new cache.
    RefPtr<ApplicationCache> cacheCopy = ApplicationCache::create();
    
    // Set the online whitelist
    cacheCopy->setOnlineWhitelist(cache->onlineWhitelist());
    
    // Traverse the cache and add copies of all resources.
    ApplicationCache::ResourceMap::const_iterator end = cache->end();
    for (ApplicationCache::ResourceMap::const_iterator it = cache->begin(); it != end; ++it) {
        ApplicationCacheResource* resource = it->second.get();
        
        RefPtr<ApplicationCacheResource> resourceCopy = ApplicationCacheResource::create(resource->url(), resource->response(), resource->type(), resource->data());
        
        cacheCopy->addResource(resourceCopy.release());
    }
    
    // Now create a new cache group.
    OwnPtr<ApplicationCacheGroup> groupCopy(new ApplicationCacheGroup(cache->group()->manifestURL(), true));
    
    groupCopy->setNewestCache(cacheCopy);
    
    ApplicationCacheStorage copyStorage;
    copyStorage.setCacheDirectory(cacheDirectory);
    
    // Empty the cache in case something was there before.
    copyStorage.empty();
    
    return copyStorage.storeNewestCache(groupCopy.get());
}
    
ApplicationCacheStorage& cacheStorage()
{
    DEFINE_STATIC_LOCAL(ApplicationCacheStorage, storage, ());
    
    return storage;
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
