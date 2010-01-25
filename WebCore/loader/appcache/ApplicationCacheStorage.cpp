/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
#include "ApplicationCacheHost.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheResource.h"
#include "CString.h"
#include "FileSystem.h"
#include "KURL.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>

using namespace std;

namespace WebCore {

template <class T>
class StorageIDJournal {
public:  
    ~StorageIDJournal()
    {
        size_t size = m_records.size();
        for (size_t i = 0; i < size; ++i)
            m_records[i].restore();
    }

    void add(T* resource, unsigned storageID)
    {
        m_records.append(Record(resource, storageID));
    }

    void commit()
    {
        m_records.clear();
    }

private:
    class Record {
    public:
        Record() : m_resource(0), m_storageID(0) { }
        Record(T* resource, unsigned storageID) : m_resource(resource), m_storageID(storageID) { }

        void restore()
        {
            m_resource->setStorageID(m_storageID);
        }

    private:
        T* m_resource;
        unsigned m_storageID;
    };

    Vector<Record> m_records;
};

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
    
    unsigned newestCacheStorageID = static_cast<unsigned>(statement.getColumnInt64(2));

    RefPtr<ApplicationCache> cache = loadCache(newestCacheStorageID);
    if (!cache)
        return 0;
        
    ApplicationCacheGroup* group = new ApplicationCacheGroup(manifestURL);
      
    group->setStorageID(static_cast<unsigned>(statement.getColumnInt64(0)));
    group->setNewestCache(cache.release());

    return group;
}    

ApplicationCacheGroup* ApplicationCacheStorage::findOrCreateCacheGroup(const KURL& manifestURL)
{
    ASSERT(!manifestURL.hasFragmentIdentifier());

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
        m_cacheHostSet.add(static_cast<unsigned>(statement.getColumnInt64(0)));
}    

ApplicationCacheGroup* ApplicationCacheStorage::cacheGroupForURL(const KURL& url)
{
    ASSERT(!url.hasFragmentIdentifier());
    
    loadManifestHostHashes();
    
    // Hash the host name and see if there's a manifest with the same host.
    if (!m_cacheHostSet.contains(urlHostHash(url)))
        return 0;

    // Check if a cache already exists in memory.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it) {
        ApplicationCacheGroup* group = it->second;

        ASSERT(!group->isObsolete());

        if (!protocolHostAndPortAreEqual(url, group->manifestURL()))
            continue;
        
        if (ApplicationCache* cache = group->newestCache()) {
            ApplicationCacheResource* resource = cache->resourceForURL(url);
            if (!resource)
                continue;
            if (resource->type() & ApplicationCacheResource::Foreign)
                continue;
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
        KURL manifestURL = KURL(ParsedURLString, statement.getColumnText(1));

        if (m_cachesInMemory.contains(manifestURL))
            continue;

        if (!protocolHostAndPortAreEqual(url, manifestURL))
            continue;

        // We found a cache group that matches. Now check if the newest cache has a resource with
        // a matching URL.
        unsigned newestCacheID = static_cast<unsigned>(statement.getColumnInt64(2));
        RefPtr<ApplicationCache> cache = loadCache(newestCacheID);
        if (!cache)
            continue;

        ApplicationCacheResource* resource = cache->resourceForURL(url);
        if (!resource)
            continue;
        if (resource->type() & ApplicationCacheResource::Foreign)
            continue;

        ApplicationCacheGroup* group = new ApplicationCacheGroup(manifestURL);
        
        group->setStorageID(static_cast<unsigned>(statement.getColumnInt64(0)));
        group->setNewestCache(cache.release());
        
        m_cachesInMemory.set(group->manifestURL(), group);
        
        return group;
    }

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
    
    return 0;
}

ApplicationCacheGroup* ApplicationCacheStorage::fallbackCacheGroupForURL(const KURL& url)
{
    ASSERT(!url.hasFragmentIdentifier());

    // Check if an appropriate cache already exists in memory.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it) {
        ApplicationCacheGroup* group = it->second;
        
        ASSERT(!group->isObsolete());

        if (ApplicationCache* cache = group->newestCache()) {
            KURL fallbackURL;
            if (!cache->urlMatchesFallbackNamespace(url, &fallbackURL))
                continue;
            if (cache->resourceForURL(fallbackURL)->type() & ApplicationCacheResource::Foreign)
                continue;
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
        KURL manifestURL = KURL(ParsedURLString, statement.getColumnText(1));

        if (m_cachesInMemory.contains(manifestURL))
            continue;

        // Fallback namespaces always have the same origin as manifest URL, so we can avoid loading caches that cannot match.
        if (!protocolHostAndPortAreEqual(url, manifestURL))
            continue;

        // We found a cache group that matches. Now check if the newest cache has a resource with
        // a matching fallback namespace.
        unsigned newestCacheID = static_cast<unsigned>(statement.getColumnInt64(2));
        RefPtr<ApplicationCache> cache = loadCache(newestCacheID);

        KURL fallbackURL;
        if (!cache->urlMatchesFallbackNamespace(url, &fallbackURL))
            continue;
        if (cache->resourceForURL(fallbackURL)->type() & ApplicationCacheResource::Foreign)
            continue;

        ApplicationCacheGroup* group = new ApplicationCacheGroup(manifestURL);
        
        group->setStorageID(static_cast<unsigned>(statement.getColumnInt64(0)));
        group->setNewestCache(cache.release());
        
        m_cachesInMemory.set(group->manifestURL(), group);
        
        return group;
    }

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
    
    return 0;
}

void ApplicationCacheStorage::cacheGroupDestroyed(ApplicationCacheGroup* group)
{
    if (group->isObsolete()) {
        ASSERT(!group->storageID());
        ASSERT(m_cachesInMemory.get(group->manifestURL()) != group);
        return;
    }

    ASSERT(m_cachesInMemory.get(group->manifestURL()) == group);

    m_cachesInMemory.remove(group->manifestURL());
    
    // If the cache group is half-created, we don't want it in the saved set (as it is not stored in database).
    if (!group->storageID())
        m_cacheHostSet.remove(urlHostHash(group->manifestURL()));
}

void ApplicationCacheStorage::cacheGroupMadeObsolete(ApplicationCacheGroup* group)
{
    ASSERT(m_cachesInMemory.get(group->manifestURL()) == group);
    ASSERT(m_cacheHostSet.contains(urlHostHash(group->manifestURL())));

    if (ApplicationCache* newestCache = group->newestCache())
        remove(newestCache);

    m_cachesInMemory.remove(group->manifestURL());
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

void ApplicationCacheStorage::setMaximumSize(int64_t size)
{
    m_maximumSize = size;
}

int64_t ApplicationCacheStorage::maximumSize() const
{
    return m_maximumSize;
}

bool ApplicationCacheStorage::isMaximumSizeReached() const
{
    return m_isMaximumSizeReached;
}

int64_t ApplicationCacheStorage::spaceNeeded(int64_t cacheToSave)
{
    int64_t spaceNeeded = 0;
    long long fileSize = 0;
    if (!getFileSize(m_cacheFile, fileSize))
        return 0;

    int64_t currentSize = fileSize;

    // Determine the amount of free space we have available.
    int64_t totalAvailableSize = 0;
    if (m_maximumSize < currentSize) {
        // The max size is smaller than the actual size of the app cache file.
        // This can happen if the client previously imposed a larger max size
        // value and the app cache file has already grown beyond the current
        // max size value.
        // The amount of free space is just the amount of free space inside
        // the database file. Note that this is always 0 if SQLite is compiled
        // with AUTO_VACUUM = 1.
        totalAvailableSize = m_database.freeSpaceSize();
    } else {
        // The max size is the same or larger than the current size.
        // The amount of free space available is the amount of free space
        // inside the database file plus the amount we can grow until we hit
        // the max size.
        totalAvailableSize = (m_maximumSize - currentSize) + m_database.freeSpaceSize();
    }

    // The space needed to be freed in order to accommodate the failed cache is
    // the size of the failed cache minus any already available free space.
    spaceNeeded = cacheToSave - totalAvailableSize;
    // The space needed value must be positive (or else the total already
    // available free space would be larger than the size of the failed cache and
    // saving of the cache should have never failed).
    ASSERT(spaceNeeded);
    return spaceNeeded;
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

static const int schemaVersion = 5;
    
void ApplicationCacheStorage::verifySchemaVersion()
{
    int version = SQLiteStatement(m_database, "PRAGMA user_version").getColumnInt(0);
    if (version == schemaVersion)
        return;

    m_database.clearAllTables();

    // Update user version.
    SQLiteTransaction setDatabaseVersion(m_database);
    setDatabaseVersion.begin();

    char userVersionSQL[32];
    int unusedNumBytes = snprintf(userVersionSQL, sizeof(userVersionSQL), "PRAGMA user_version=%d", schemaVersion);
    ASSERT_UNUSED(unusedNumBytes, static_cast<int>(sizeof(userVersionSQL)) >= unusedNumBytes);

    SQLiteStatement statement(m_database, userVersionSQL);
    if (statement.prepare() != SQLResultOk)
        return;
    
    executeStatement(statement);
    setDatabaseVersion.commit();
}
    
void ApplicationCacheStorage::openDatabase(bool createIfDoesNotExist)
{
    if (m_database.isOpen())
        return;

    // The cache directory should never be null, but if it for some weird reason is we bail out.
    if (m_cacheDirectory.isNull())
        return;

    m_cacheFile = pathByAppendingComponent(m_cacheDirectory, "ApplicationCache.db");
    if (!createIfDoesNotExist && !fileExists(m_cacheFile))
        return;

    makeAllDirectories(m_cacheDirectory);
    m_database.open(m_cacheFile);
    
    if (!m_database.isOpen())
        return;
    
    verifySchemaVersion();
    
    // Create tables
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheGroups (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "manifestHostHash INTEGER NOT NULL ON CONFLICT FAIL, manifestURL TEXT UNIQUE ON CONFLICT FAIL, newestCache INTEGER)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS Caches (id INTEGER PRIMARY KEY AUTOINCREMENT, cacheGroup INTEGER, size INTEGER)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheWhitelistURLs (url TEXT NOT NULL ON CONFLICT FAIL, cache INTEGER NOT NULL ON CONFLICT FAIL)");
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheAllowsAllNetworkRequests (wildcard INTEGER NOT NULL ON CONFLICT FAIL, cache INTEGER NOT NULL ON CONFLICT FAIL)");
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
                      "  DELETE FROM CacheAllowsAllNetworkRequests WHERE cache = OLD.id;"
                      "  DELETE FROM FallbackURLs WHERE cache = OLD.id;"
                      " END");

    // When a cache entry is deleted, its resource should also be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheEntryDeleted AFTER DELETE ON CacheEntries"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheResources WHERE id = OLD.resource;"
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

bool ApplicationCacheStorage::store(ApplicationCacheGroup* group, GroupStorageIDJournal* journal)
{
    ASSERT(group->storageID() == 0);
    ASSERT(journal);

    SQLiteStatement statement(m_database, "INSERT INTO CacheGroups (manifestHostHash, manifestURL) VALUES (?, ?)");
    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindInt64(1, urlHostHash(group->manifestURL()));
    statement.bindText(2, group->manifestURL());

    if (!executeStatement(statement))
        return false;

    group->setStorageID(static_cast<unsigned>(m_database.lastInsertRowID()));
    journal->add(group, 0);
    return true;
}    

bool ApplicationCacheStorage::store(ApplicationCache* cache, ResourceStorageIDJournal* storageIDJournal)
{
    ASSERT(cache->storageID() == 0);
    ASSERT(cache->group()->storageID() != 0);
    ASSERT(storageIDJournal);
    
    SQLiteStatement statement(m_database, "INSERT INTO Caches (cacheGroup, size) VALUES (?, ?)");
    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindInt64(1, cache->group()->storageID());
    statement.bindInt64(2, cache->estimatedSizeInStorage());

    if (!executeStatement(statement))
        return false;
    
    unsigned cacheStorageID = static_cast<unsigned>(m_database.lastInsertRowID());

    // Store all resources
    {
        ApplicationCache::ResourceMap::const_iterator end = cache->end();
        for (ApplicationCache::ResourceMap::const_iterator it = cache->begin(); it != end; ++it) {
            unsigned oldStorageID = it->second->storageID();
            if (!store(it->second.get(), cacheStorageID))
                return false;

            // Storing the resource succeeded. Log its old storageID in case
            // it needs to be restored later.
            storageIDJournal->add(it->second.get(), oldStorageID);
        }
    }
    
    // Store the online whitelist
    const Vector<KURL>& onlineWhitelist = cache->onlineWhitelist();
    {
        size_t whitelistSize = onlineWhitelist.size();
        for (size_t i = 0; i < whitelistSize; ++i) {
            SQLiteStatement statement(m_database, "INSERT INTO CacheWhitelistURLs (url, cache) VALUES (?, ?)");
            statement.prepare();

            statement.bindText(1, onlineWhitelist[i]);
            statement.bindInt64(2, cacheStorageID);

            if (!executeStatement(statement))
                return false;
        }
    }

    // Store online whitelist wildcard flag.
    {
        SQLiteStatement statement(m_database, "INSERT INTO CacheAllowsAllNetworkRequests (wildcard, cache) VALUES (?, ?)");
        statement.prepare();

        statement.bindInt64(1, cache->allowsAllNetworkRequests());
        statement.bindInt64(2, cacheStorageID);

        if (!executeStatement(statement))
            return false;
    }
    
    // Store fallback URLs.
    const FallbackURLVector& fallbackURLs = cache->fallbackURLs();
    {
        size_t fallbackCount = fallbackURLs.size();
        for (size_t i = 0; i < fallbackCount; ++i) {
            SQLiteStatement statement(m_database, "INSERT INTO FallbackURLs (namespace, fallbackURL, cache) VALUES (?, ?, ?)");
            statement.prepare();

            statement.bindText(1, fallbackURLs[i].first);
            statement.bindText(2, fallbackURLs[i].second);
            statement.bindInt64(3, cacheStorageID);

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

    unsigned dataId = static_cast<unsigned>(m_database.lastInsertRowID());

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
    
    // The same ApplicationCacheResource are used in ApplicationCacheResource::size()
    // to calculate the approximate size of an ApplicationCacheResource object. If
    // you change the code below, please also change ApplicationCacheResource::size().
    resourceStatement.bindText(1, resource->url());
    resourceStatement.bindInt64(2, resource->response().httpStatusCode());
    resourceStatement.bindText(3, resource->response().url());
    resourceStatement.bindText(4, headers);
    resourceStatement.bindInt64(5, dataId);
    resourceStatement.bindText(6, resource->response().mimeType());
    resourceStatement.bindText(7, resource->response().textEncodingName());

    if (!executeStatement(resourceStatement))
        return false;

    unsigned resourceId = static_cast<unsigned>(m_database.lastInsertRowID());
    
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

bool ApplicationCacheStorage::storeUpdatedType(ApplicationCacheResource* resource, ApplicationCache* cache)
{
    ASSERT_UNUSED(cache, cache->storageID());
    ASSERT(resource->storageID());

    // First, insert the data
    SQLiteStatement entryStatement(m_database, "UPDATE CacheEntries SET type=? WHERE resource=?");
    if (entryStatement.prepare() != SQLResultOk)
        return false;

    entryStatement.bindInt64(1, resource->type());
    entryStatement.bindInt64(2, resource->storageID());

    return executeStatement(entryStatement);
}

bool ApplicationCacheStorage::store(ApplicationCacheResource* resource, ApplicationCache* cache)
{
    ASSERT(cache->storageID());
    
    openDatabase(true);
 
    m_isMaximumSizeReached = false;
    m_database.setMaximumSize(m_maximumSize);

    SQLiteTransaction storeResourceTransaction(m_database);
    storeResourceTransaction.begin();
    
    if (!store(resource, cache->storageID())) {
        checkForMaxSizeReached();
        return false;
    }

    // A resource was added to the cache. Update the total data size for the cache.
    SQLiteStatement sizeUpdateStatement(m_database, "UPDATE Caches SET size=size+? WHERE id=?");
    if (sizeUpdateStatement.prepare() != SQLResultOk)
        return false;

    sizeUpdateStatement.bindInt64(1, resource->estimatedSizeInStorage());
    sizeUpdateStatement.bindInt64(2, cache->storageID());

    if (!executeStatement(sizeUpdateStatement))
        return false;
    
    storeResourceTransaction.commit();
    return true;
}

bool ApplicationCacheStorage::storeNewestCache(ApplicationCacheGroup* group)
{
    openDatabase(true);

    m_isMaximumSizeReached = false;
    m_database.setMaximumSize(m_maximumSize);

    SQLiteTransaction storeCacheTransaction(m_database);
    
    storeCacheTransaction.begin();

    GroupStorageIDJournal groupStorageIDJournal;
    if (!group->storageID()) {
        // Store the group
        if (!store(group, &groupStorageIDJournal)) {
            checkForMaxSizeReached();
            return false;
        }
    }
    
    ASSERT(group->newestCache());
    ASSERT(!group->isObsolete());
    ASSERT(!group->newestCache()->storageID());
    
    // Log the storageID changes to the in-memory resource objects. The journal
    // object will roll them back automatically in case a database operation
    // fails and this method returns early.
    ResourceStorageIDJournal resourceStorageIDJournal;

    // Store the newest cache
    if (!store(group->newestCache(), &resourceStorageIDJournal)) {
        checkForMaxSizeReached();
        return false;
    }
    
    // Update the newest cache in the group.
    
    SQLiteStatement statement(m_database, "UPDATE CacheGroups SET newestCache=? WHERE id=?");
    if (statement.prepare() != SQLResultOk)
        return false;
    
    statement.bindInt64(1, group->newestCache()->storageID());
    statement.bindInt64(2, group->storageID());
    
    if (!executeStatement(statement))
        return false;
    
    groupStorageIDJournal.commit();
    resourceStorageIDJournal.commit();
    storeCacheTransaction.commit();
    return true;
}

static inline void parseHeader(const UChar* header, size_t headerLength, ResourceResponse& response)
{
    int pos = find(header, headerLength, ':');
    ASSERT(pos != -1);
    
    AtomicString headerName = AtomicString(header, pos);
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
        KURL url(ParsedURLString, cacheStatement.getColumnText(0));
        
        unsigned type = static_cast<unsigned>(cacheStatement.getColumnInt64(1));

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
    
    Vector<KURL> whitelist;
    while ((result = whitelistStatement.step()) == SQLResultRow) 
        whitelist.append(KURL(ParsedURLString, whitelistStatement.getColumnText(0)));

    if (result != SQLResultDone)
        LOG_ERROR("Could not load cache online whitelist, error \"%s\"", m_database.lastErrorMsg());

    cache->setOnlineWhitelist(whitelist);

    // Load online whitelist wildcard flag.
    SQLiteStatement whitelistWildcardStatement(m_database, "SELECT wildcard FROM CacheAllowsAllNetworkRequests WHERE cache=?");
    if (whitelistWildcardStatement.prepare() != SQLResultOk)
        return 0;
    whitelistWildcardStatement.bindInt64(1, storageID);
    
    result = whitelistWildcardStatement.step();
    if (result != SQLResultRow)
        LOG_ERROR("Could not load cache online whitelist wildcard flag, error \"%s\"", m_database.lastErrorMsg());

    cache->setAllowsAllNetworkRequests(whitelistWildcardStatement.getColumnInt64(0));

    if (whitelistWildcardStatement.step() != SQLResultDone)
        LOG_ERROR("Too many rows for online whitelist wildcard flag");

    // Load fallback URLs.
    SQLiteStatement fallbackStatement(m_database, "SELECT namespace, fallbackURL FROM FallbackURLs WHERE cache=?");
    if (fallbackStatement.prepare() != SQLResultOk)
        return 0;
    fallbackStatement.bindInt64(1, storageID);
    
    FallbackURLVector fallbackURLs;
    while ((result = fallbackStatement.step()) == SQLResultRow) 
        fallbackURLs.append(make_pair(KURL(ParsedURLString, fallbackStatement.getColumnText(0)), KURL(ParsedURLString, fallbackStatement.getColumnText(1))));

    if (result != SQLResultDone)
        LOG_ERROR("Could not load fallback URLs, error \"%s\"", m_database.lastErrorMsg());

    cache->setFallbackURLs(fallbackURLs);
    
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

    ASSERT(cache->group());
    ASSERT(cache->group()->storageID());

    // All associated data will be deleted by database triggers.
    SQLiteStatement statement(m_database, "DELETE FROM Caches WHERE id=?");
    if (statement.prepare() != SQLResultOk)
        return;
    
    statement.bindInt64(1, cache->storageID());
    executeStatement(statement);

    cache->clearStorageID();

    if (cache->group()->newestCache() == cache) {
        // Currently, there are no triggers on the cache group, which is why the cache had to be removed separately above.
        SQLiteStatement groupStatement(m_database, "DELETE FROM CacheGroups WHERE id=?");
        if (groupStatement.prepare() != SQLResultOk)
            return;
        
        groupStatement.bindInt64(1, cache->group()->storageID());
        executeStatement(groupStatement);

        cache->group()->clearStorageID();
    }
}    

void ApplicationCacheStorage::empty()
{
    openDatabase(false);
    
    if (!m_database.isOpen())
        return;
    
    // Clear cache groups, caches and cache resources.
    executeSQLCommand("DELETE FROM CacheGroups");
    executeSQLCommand("DELETE FROM Caches");
    
    // Clear the storage IDs for the caches in memory.
    // The caches will still work, but cached resources will not be saved to disk 
    // until a cache update process has been initiated.
    CacheGroupMap::const_iterator end = m_cachesInMemory.end();
    for (CacheGroupMap::const_iterator it = m_cachesInMemory.begin(); it != end; ++it)
        it->second->clearStorageID();
}    

bool ApplicationCacheStorage::storeCopyOfCache(const String& cacheDirectory, ApplicationCacheHost* cacheHost)
{
    ApplicationCache* cache = cacheHost->applicationCache();
    if (!cache)
        return true;

    // Create a new cache.
    RefPtr<ApplicationCache> cacheCopy = ApplicationCache::create();

    cacheCopy->setOnlineWhitelist(cache->onlineWhitelist());
    cacheCopy->setFallbackURLs(cache->fallbackURLs());

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

bool ApplicationCacheStorage::manifestURLs(Vector<KURL>* urls)
{
    ASSERT(urls);
    openDatabase(false);
    if (!m_database.isOpen())
        return false;

    SQLiteStatement selectURLs(m_database, "SELECT manifestURL FROM CacheGroups");

    if (selectURLs.prepare() != SQLResultOk)
        return false;

    while (selectURLs.step() == SQLResultRow)
        urls->append(KURL(ParsedURLString, selectURLs.getColumnText(0)));

    return true;
}

bool ApplicationCacheStorage::cacheGroupSize(const String& manifestURL, int64_t* size)
{
    ASSERT(size);
    openDatabase(false);
    if (!m_database.isOpen())
        return false;

    SQLiteStatement statement(m_database, "SELECT sum(Caches.size) FROM Caches INNER JOIN CacheGroups ON Caches.cacheGroup=CacheGroups.id WHERE CacheGroups.manifestURL=?");
    if (statement.prepare() != SQLResultOk)
        return false;

    statement.bindText(1, manifestURL);

    int result = statement.step();
    if (result == SQLResultDone)
        return false;

    if (result != SQLResultRow) {
        LOG_ERROR("Could not get the size of the cache group, error \"%s\"", m_database.lastErrorMsg());
        return false;
    }

    *size = statement.getColumnInt64(0);
    return true;
}

bool ApplicationCacheStorage::deleteCacheGroup(const String& manifestURL)
{
    SQLiteTransaction deleteTransaction(m_database);
    // Check to see if the group is in memory.
    ApplicationCacheGroup* group = m_cachesInMemory.get(manifestURL);
    if (group)
        cacheGroupMadeObsolete(group);
    else {
        // The cache group is not in memory, so remove it from the disk.
        openDatabase(false);
        if (!m_database.isOpen())
            return false;

        SQLiteStatement idStatement(m_database, "SELECT id FROM CacheGroups WHERE manifestURL=?");
        if (idStatement.prepare() != SQLResultOk)
            return false;

        idStatement.bindText(1, manifestURL);

        int result = idStatement.step();
        if (result == SQLResultDone)
            return false;

        if (result != SQLResultRow) {
            LOG_ERROR("Could not load cache group id, error \"%s\"", m_database.lastErrorMsg());
            return false;
        }

        int64_t groupId = idStatement.getColumnInt64(0);

        SQLiteStatement cacheStatement(m_database, "DELETE FROM Caches WHERE cacheGroup=?");
        if (cacheStatement.prepare() != SQLResultOk)
            return false;

        SQLiteStatement groupStatement(m_database, "DELETE FROM CacheGroups WHERE id=?");
        if (groupStatement.prepare() != SQLResultOk)
            return false;

        cacheStatement.bindInt64(1, groupId);
        executeStatement(cacheStatement);
        groupStatement.bindInt64(1, groupId);
        executeStatement(groupStatement);
    }

    deleteTransaction.commit();
    return true;
}

void ApplicationCacheStorage::vacuumDatabaseFile()
{
    openDatabase(false);
    if (!m_database.isOpen())
        return;

    m_database.runVacuumCommand();
}

void ApplicationCacheStorage::checkForMaxSizeReached()
{
    if (m_database.lastError() == SQLResultFull)
        m_isMaximumSizeReached = true;
}

ApplicationCacheStorage::ApplicationCacheStorage() 
    : m_maximumSize(INT_MAX)
    , m_isMaximumSizeReached(false)
{
}

ApplicationCacheStorage& cacheStorage()
{
    DEFINE_STATIC_LOCAL(ApplicationCacheStorage, storage, ());
    
    return storage;
}

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
