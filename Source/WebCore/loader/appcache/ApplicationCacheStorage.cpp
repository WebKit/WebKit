/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
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

#include "ApplicationCache.h"
#include "ApplicationCacheGroup.h"
#include "ApplicationCacheHost.h"
#include "ApplicationCacheResource.h"
#include "SQLiteDatabaseTracker.h"
#include "SQLiteStatement.h"
#include "SQLiteTransaction.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include <wtf/FileSystem.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

template <class T>
class StorageIDJournal {
public:  
    ~StorageIDJournal()
    {
        for (auto& record : m_records)
            record.restore();
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
        Record() : m_resource(nullptr), m_storageID(0) { }
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

static unsigned urlHostHash(const URL& url)
{
    StringView host = url.host();
    if (host.is8Bit())
        return AlreadyHashed::avoidDeletedValue(StringHasher::computeHashAndMaskTop8Bits(host.characters8(), host.length()));
    return AlreadyHashed::avoidDeletedValue(StringHasher::computeHashAndMaskTop8Bits(host.characters16(), host.length()));
}

ApplicationCacheGroup* ApplicationCacheStorage::loadCacheGroup(const URL& manifestURL)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    if (!m_database.isOpen())
        return nullptr;

    auto statement = m_database.prepareStatement("SELECT id, manifestURL, newestCache FROM CacheGroups WHERE newestCache IS NOT NULL AND manifestURL=?"_s);
    if (!statement)
        return nullptr;
    
    statement->bindText(1, manifestURL.string());
   
    int result = statement->step();
    if (result == SQLITE_DONE)
        return nullptr;
    
    if (result != SQLITE_ROW) {
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
        return nullptr;
    }
    
    unsigned newestCacheStorageID = static_cast<unsigned>(statement->columnInt64(2));

    auto cache = loadCache(newestCacheStorageID);
    if (!cache)
        return nullptr;
        
    auto& group = *new ApplicationCacheGroup(*this, manifestURL);
    group.setStorageID(static_cast<unsigned>(statement->columnInt64(0)));
    group.setNewestCache(cache.releaseNonNull());
    return &group;
}    

ApplicationCacheGroup* ApplicationCacheStorage::findOrCreateCacheGroup(const URL& manifestURL)
{
    ASSERT(!manifestURL.hasFragmentIdentifier());

    auto result = m_cachesInMemory.add(manifestURL.string(), nullptr);
    if (!result.isNewEntry) {
        ASSERT(result.iterator->value);
        return result.iterator->value;
    }

    // Look up the group in the database
    auto* group = loadCacheGroup(manifestURL);
    
    // If the group was not found we need to create it
    if (!group) {
        group = new ApplicationCacheGroup(*this, manifestURL);
        m_cacheHostSet.add(urlHostHash(manifestURL));
    }

    result.iterator->value = group;
    return group;
}

ApplicationCacheGroup* ApplicationCacheStorage::findInMemoryCacheGroup(const URL& manifestURL) const
{
    return m_cachesInMemory.get(manifestURL.string());
}

void ApplicationCacheStorage::loadManifestHostHashes()
{
    static bool hasLoadedHashes = false;
    
    if (hasLoadedHashes)
        return;
    
    // We set this flag to true before the database has been opened
    // to avoid trying to open the database over and over if it doesn't exist.
    hasLoadedHashes = true;
    
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    if (!m_database.isOpen())
        return;

    // Fetch the host hashes.
    auto statement = m_database.prepareStatement("SELECT manifestHostHash FROM CacheGroups"_s);
    if (!statement)
        return;
    
    while (statement->step() == SQLITE_ROW)
        m_cacheHostSet.add(static_cast<unsigned>(statement->columnInt64(0)));
}    

ApplicationCacheGroup* ApplicationCacheStorage::cacheGroupForURL(const URL& url)
{
    ASSERT(!url.hasFragmentIdentifier());
    
    loadManifestHostHashes();
    
    // Hash the host name and see if there's a manifest with the same host.
    if (!m_cacheHostSet.contains(urlHostHash(url)))
        return nullptr;

    // Check if a cache already exists in memory.
    for (const auto& group : m_cachesInMemory.values()) {
        ASSERT(!group->isObsolete());

        if (!protocolHostAndPortAreEqual(url, group->manifestURL()))
            continue;
        
        if (ApplicationCache* cache = group->newestCache()) {
            ApplicationCacheResource* resource = cache->resourceForURL(url.string());
            if (!resource)
                continue;
            if (resource->type() & ApplicationCacheResource::Foreign)
                continue;
            return group;
        }
    }
    
    if (!m_database.isOpen())
        return nullptr;
        
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    // Check the database. Look for all cache groups with a newest cache.
    auto statement = m_database.prepareStatement("SELECT id, manifestURL, newestCache FROM CacheGroups WHERE newestCache IS NOT NULL"_s);
    if (!statement)
        return nullptr;
    
    int result;
    while ((result = statement->step()) == SQLITE_ROW) {
        URL manifestURL = URL({ }, statement->columnText(1));

        if (m_cachesInMemory.contains(manifestURL.string()))
            continue;

        if (!protocolHostAndPortAreEqual(url, manifestURL))
            continue;

        // We found a cache group that matches. Now check if the newest cache has a resource with
        // a matching URL.
        unsigned newestCacheID = static_cast<unsigned>(statement->columnInt64(2));
        auto cache = loadCache(newestCacheID);
        if (!cache)
            continue;

        auto* resource = cache->resourceForURL(url.string());
        if (!resource)
            continue;
        if (resource->type() & ApplicationCacheResource::Foreign)
            continue;

        auto& group = *new ApplicationCacheGroup(*this, manifestURL);
        group.setStorageID(static_cast<unsigned>(statement->columnInt64(0)));
        group.setNewestCache(cache.releaseNonNull());
        m_cachesInMemory.set(group.manifestURL().string(), &group);

        return &group;
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
    
    return nullptr;
}

ApplicationCacheGroup* ApplicationCacheStorage::fallbackCacheGroupForURL(const URL& url)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    ASSERT(!url.hasFragmentIdentifier());

    // Check if an appropriate cache already exists in memory.
    for (auto* group : m_cachesInMemory.values()) {
        ASSERT(!group->isObsolete());

        if (ApplicationCache* cache = group->newestCache()) {
            URL fallbackURL;
            if (cache->isURLInOnlineAllowlist(url))
                continue;
            if (!cache->urlMatchesFallbackNamespace(url, &fallbackURL))
                continue;
            if (cache->resourceForURL(fallbackURL.string())->type() & ApplicationCacheResource::Foreign)
                continue;
            return group;
        }
    }
    
    if (!m_database.isOpen())
        return nullptr;
        
    // Check the database. Look for all cache groups with a newest cache.
    auto statement = m_database.prepareStatement("SELECT id, manifestURL, newestCache FROM CacheGroups WHERE newestCache IS NOT NULL"_s);
    if (!statement)
        return nullptr;
    
    int result;
    while ((result = statement->step()) == SQLITE_ROW) {
        URL manifestURL = URL({ }, statement->columnText(1));

        if (m_cachesInMemory.contains(manifestURL.string()))
            continue;

        // Fallback namespaces always have the same origin as manifest URL, so we can avoid loading caches that cannot match.
        if (!protocolHostAndPortAreEqual(url, manifestURL))
            continue;

        // We found a cache group that matches. Now check if the newest cache has a resource with
        // a matching fallback namespace.
        unsigned newestCacheID = static_cast<unsigned>(statement->columnInt64(2));
        auto cache = loadCache(newestCacheID);

        URL fallbackURL;
        if (cache->isURLInOnlineAllowlist(url))
            continue;
        if (!cache->urlMatchesFallbackNamespace(url, &fallbackURL))
            continue;
        if (cache->resourceForURL(fallbackURL.string())->type() & ApplicationCacheResource::Foreign)
            continue;

        auto& group = *new ApplicationCacheGroup(*this, manifestURL);
        group.setStorageID(static_cast<unsigned>(statement->columnInt64(0)));
        group.setNewestCache(cache.releaseNonNull());

        m_cachesInMemory.set(group.manifestURL().string(), &group);

        return &group;
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Could not load cache group, error \"%s\"", m_database.lastErrorMsg());
    
    return nullptr;
}

void ApplicationCacheStorage::cacheGroupDestroyed(ApplicationCacheGroup& group)
{
    if (group.isObsolete()) {
        ASSERT(!group.storageID());
        ASSERT(m_cachesInMemory.get(group.manifestURL().string()) != &group);
        return;
    }

    ASSERT(m_cachesInMemory.get(group.manifestURL().string()) == &group);

    m_cachesInMemory.remove(group.manifestURL().string());
    
    // If the cache group is half-created, we don't want it in the saved set (as it is not stored in database).
    if (!group.storageID())
        m_cacheHostSet.remove(urlHostHash(group.manifestURL()));
}

void ApplicationCacheStorage::cacheGroupMadeObsolete(ApplicationCacheGroup& group)
{
    ASSERT(m_cachesInMemory.get(group.manifestURL().string()) == &group);
    ASSERT(m_cacheHostSet.contains(urlHostHash(group.manifestURL())));

    if (auto* newestCache = group.newestCache())
        remove(newestCache);

    m_cachesInMemory.remove(group.manifestURL().string());
    m_cacheHostSet.remove(urlHostHash(group.manifestURL()));
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
    auto fileSize = FileSystem::fileSize(m_cacheFile);
    if (!fileSize)
        return 0;

    int64_t currentSize = *fileSize + flatFileAreaSize();

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

void ApplicationCacheStorage::setDefaultOriginQuota(int64_t quota)
{
    m_defaultOriginQuota = quota;
}

bool ApplicationCacheStorage::calculateQuotaForOrigin(const SecurityOrigin& origin, int64_t& quota)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    // If an Origin record doesn't exist, then the COUNT will be 0 and quota will be 0.
    // Using the count to determine if a record existed or not is a safe way to determine
    // if a quota of 0 is real, from the record, or from null.
    auto statement = m_database.prepareStatement("SELECT COUNT(quota), quota FROM Origins WHERE origin=?"_s);
    if (!statement)
        return false;

    statement->bindText(1, origin.data().databaseIdentifier());
    int result = statement->step();

    // Return the quota, or if it was null the default.
    if (result == SQLITE_ROW) {
        bool wasNoRecord = !statement->columnInt64(0);
        quota = wasNoRecord ? m_defaultOriginQuota : statement->columnInt64(1);
        return true;
    }

    LOG_ERROR("Could not get the quota of an origin, error \"%s\"", m_database.lastErrorMsg());
    return false;
}

bool ApplicationCacheStorage::calculateUsageForOrigin(const SecurityOriginData& origin, int64_t& usage)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    // If an Origins record doesn't exist, then the SUM will be null,
    // which will become 0, as expected, when converting to a number.
    auto statement = m_database.prepareStatement("SELECT SUM(Caches.size)"
                                                 " FROM CacheGroups"
                                                 " INNER JOIN Origins ON CacheGroups.origin = Origins.origin"
                                                 " INNER JOIN Caches ON CacheGroups.id = Caches.cacheGroup"
                                                 " WHERE Origins.origin=?"_s);
    if (!statement)
        return false;

    statement->bindText(1, origin.databaseIdentifier());
    int result = statement->step();

    if (result == SQLITE_ROW) {
        usage = statement->columnInt64(0);
        return true;
    }

    LOG_ERROR("Could not get the quota of an origin, error \"%s\"", m_database.lastErrorMsg());
    return false;
}

bool ApplicationCacheStorage::calculateRemainingSizeForOriginExcludingCache(const SecurityOrigin& origin, ApplicationCache* cache, int64_t& remainingSize)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    if (!m_database.isOpen())
        return false;

    // Remaining size = total origin quota - size of all caches with origin excluding the provided cache.
    // Keep track of the number of caches so we can tell if the result was a calculation or not.
    int64_t excludingCacheIdentifier = cache ? cache->storageID() : 0;
    auto query = [excludingCacheIdentifier]() {
        if (excludingCacheIdentifier) {
            return "SELECT COUNT(Caches.size), Origins.quota - SUM(Caches.size)"
                    "  FROM CacheGroups"
                    " INNER JOIN Origins ON CacheGroups.origin = Origins.origin"
                    " INNER JOIN Caches ON CacheGroups.id = Caches.cacheGroup"
                    " WHERE Origins.origin=?"
                    "   AND Caches.id!=?"_s;
        }
        return "SELECT COUNT(Caches.size), Origins.quota - SUM(Caches.size)"
               "  FROM CacheGroups"
               " INNER JOIN Origins ON CacheGroups.origin = Origins.origin"
               " INNER JOIN Caches ON CacheGroups.id = Caches.cacheGroup"
               " WHERE Origins.origin=?"_s;
    }();

    auto statement = m_database.prepareStatement(query);
    if (!statement)
        return false;

    statement->bindText(1, origin.data().databaseIdentifier());
    if (excludingCacheIdentifier != 0)
        statement->bindInt64(2, excludingCacheIdentifier);
    int result = statement->step();

    // If the count was 0 that then we have to query the origin table directly
    // for its quota. Otherwise we can use the calculated value.
    if (result == SQLITE_ROW) {
        int64_t numberOfCaches = statement->columnInt64(0);
        if (numberOfCaches == 0)
            calculateQuotaForOrigin(origin, remainingSize);
        else
            remainingSize = statement->columnInt64(1);
        return true;
    }

    LOG_ERROR("Could not get the remaining size of an origin's quota, error \"%s\"", m_database.lastErrorMsg());
    return false;
}

bool ApplicationCacheStorage::storeUpdatedQuotaForOrigin(const SecurityOrigin* origin, int64_t quota)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(true);
    if (!m_database.isOpen())
        return false;

    if (!ensureOriginRecord(origin))
        return false;

    auto updateStatement = m_database.prepareStatement("UPDATE Origins SET quota=? WHERE origin=?"_s);
    if (!updateStatement)
        return false;

    updateStatement->bindInt64(1, quota);
    updateStatement->bindText(2, origin->data().databaseIdentifier());

    return executeStatement(updateStatement.value());
}

bool ApplicationCacheStorage::executeSQLCommand(ASCIILiteral sql)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(m_database.isOpen());
    
    bool result = m_database.executeCommand(sql);
    if (!result)
        LOG_ERROR("Application Cache Storage: failed to execute statement \"%s\" error \"%s\"", sql.characters(), m_database.lastErrorMsg());

    return result;
}

// Update the schemaVersion when the schema of any the Application Cache
// SQLite tables changes. This allows the database to be rebuilt when
// a new, incompatible change has been introduced to the database schema.
static const int schemaVersion = 7;
    
void ApplicationCacheStorage::verifySchemaVersion()
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());

    auto versionStatement = m_database.prepareStatement("PRAGMA user_version"_s);
    int version = versionStatement ? versionStatement->columnInt(0) : 0;
    if (version == schemaVersion)
        return;

    // Version will be 0 if we just created an empty file. Trying to delete tables would cause errors, because they don't exist yet.
    if (version)
        deleteTables();

    // Update user version.
    SQLiteTransaction setDatabaseVersion(m_database);
    setDatabaseVersion.begin();

    auto statement = m_database.prepareStatementSlow(makeString("PRAGMA user_version=", schemaVersion));
    if (!statement)
        return;
    
    executeStatement(statement.value());
    setDatabaseVersion.commit();
}
    
void ApplicationCacheStorage::openDatabase(bool createIfDoesNotExist)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    if (m_database.isOpen())
        return;

    // The cache directory should never be null, but if it for some weird reason is we bail out.
    if (m_cacheDirectory.isNull())
        return;

    m_cacheFile = FileSystem::pathByAppendingComponent(m_cacheDirectory, "ApplicationCache.db"_s);
    if (!createIfDoesNotExist && !FileSystem::fileExists(m_cacheFile))
        return;

    FileSystem::makeAllDirectories(m_cacheDirectory);
    m_database.open(m_cacheFile);
    
    if (!m_database.isOpen())
        return;
    
    verifySchemaVersion();
    
    // Create tables
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheGroups (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "manifestHostHash INTEGER NOT NULL ON CONFLICT FAIL, manifestURL TEXT UNIQUE ON CONFLICT FAIL, newestCache INTEGER, origin TEXT)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS Caches (id INTEGER PRIMARY KEY AUTOINCREMENT, cacheGroup INTEGER, size INTEGER)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheWhitelistURLs (url TEXT NOT NULL ON CONFLICT FAIL, cache INTEGER NOT NULL ON CONFLICT FAIL)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheAllowsAllNetworkRequests (wildcard INTEGER NOT NULL ON CONFLICT FAIL, cache INTEGER NOT NULL ON CONFLICT FAIL)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS FallbackURLs (namespace TEXT NOT NULL ON CONFLICT FAIL, fallbackURL TEXT NOT NULL ON CONFLICT FAIL, "
                      "cache INTEGER NOT NULL ON CONFLICT FAIL)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheEntries (cache INTEGER NOT NULL ON CONFLICT FAIL, type INTEGER, resource INTEGER NOT NULL)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheResources (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL ON CONFLICT FAIL, "
                      "statusCode INTEGER NOT NULL, responseURL TEXT NOT NULL, mimeType TEXT, textEncodingName TEXT, headers TEXT, data INTEGER NOT NULL ON CONFLICT FAIL)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS CacheResourceData (id INTEGER PRIMARY KEY AUTOINCREMENT, data BLOB, path TEXT)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS DeletedCacheResources (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT)"_s);
    executeSQLCommand("CREATE TABLE IF NOT EXISTS Origins (origin TEXT UNIQUE ON CONFLICT IGNORE, quota INTEGER NOT NULL ON CONFLICT FAIL)"_s);

    // When a cache is deleted, all its entries and its allowlist should be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheDeleted AFTER DELETE ON Caches"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheEntries WHERE cache = OLD.id;"
                      "  DELETE FROM CacheWhitelistURLs WHERE cache = OLD.id;"
                      "  DELETE FROM CacheAllowsAllNetworkRequests WHERE cache = OLD.id;"
                      "  DELETE FROM FallbackURLs WHERE cache = OLD.id;"
                      " END"_s);

    // When a cache entry is deleted, its resource should also be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheEntryDeleted AFTER DELETE ON CacheEntries"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheResources WHERE id = OLD.resource;"
                      " END"_s);

    // When a cache resource is deleted, its data blob should also be deleted.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheResourceDeleted AFTER DELETE ON CacheResources"
                      " FOR EACH ROW BEGIN"
                      "  DELETE FROM CacheResourceData WHERE id = OLD.data;"
                      " END"_s);
    
    // When a cache resource is deleted, if it contains a non-empty path, that path should
    // be added to the DeletedCacheResources table so the flat file at that path can
    // be deleted at a later time.
    executeSQLCommand("CREATE TRIGGER IF NOT EXISTS CacheResourceDataDeleted AFTER DELETE ON CacheResourceData"
                      " FOR EACH ROW"
                      " WHEN OLD.path NOT NULL BEGIN"
                      "  INSERT INTO DeletedCacheResources (path) values (OLD.path);"
                      " END"_s);
}

bool ApplicationCacheStorage::executeStatement(SQLiteStatement& statement)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    bool result = statement.executeCommand();
    if (!result)
        LOG_ERROR("Application Cache Storage: failed to execute statement, error \"%s\"", m_database.lastErrorMsg());
    
    return result;
}    

bool ApplicationCacheStorage::store(ApplicationCacheGroup* group, GroupStorageIDJournal* journal)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(group->storageID() == 0);
    ASSERT(journal);

    // For some reason, an app cache may be partially written to disk. In particular, there may be
    // a cache group with an identical manifest URL and associated cache entries. We want to remove
    // this cache group and its associated cache entries so that we can create it again (below) as
    // a way to repair it.
    deleteCacheGroupRecord(group->manifestURL().string());

    auto statement = m_database.prepareStatement("INSERT INTO CacheGroups (manifestHostHash, manifestURL, origin) VALUES (?, ?, ?)"_s);
    if (!statement)
        return false;

    statement->bindInt64(1, urlHostHash(group->manifestURL()));
    statement->bindText(2, group->manifestURL().string());
    statement->bindText(3, group->origin().data().databaseIdentifier());

    if (!executeStatement(statement.value()))
        return false;

    unsigned groupStorageID = static_cast<unsigned>(m_database.lastInsertRowID());

    if (!ensureOriginRecord(&group->origin()))
        return false;

    group->setStorageID(groupStorageID);
    journal->add(group, 0);
    return true;
}    

bool ApplicationCacheStorage::store(ApplicationCache* cache, ResourceStorageIDJournal* storageIDJournal)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(cache->storageID() == 0);
    ASSERT(cache->group()->storageID() != 0);
    ASSERT(storageIDJournal);
    
    auto statement = m_database.prepareStatement("INSERT INTO Caches (cacheGroup, size) VALUES (?, ?)"_s);
    if (!statement)
        return false;

    statement->bindInt64(1, cache->group()->storageID());
    statement->bindInt64(2, cache->estimatedSizeInStorage());

    if (!executeStatement(statement.value()))
        return false;
    
    unsigned cacheStorageID = static_cast<unsigned>(m_database.lastInsertRowID());

    // Store all resources
    for (auto& resource : cache->resources().values()) {
        unsigned oldStorageID = resource->storageID();
        if (!store(resource.get(), cacheStorageID))
            return false;

        // Storing the resource succeeded. Log its old storageID in case
        // it needs to be restored later.
        storageIDJournal->add(resource.get(), oldStorageID);
    }
    
    // Store the online allowlist
    const Vector<URL>& onlineAllowlist = cache->onlineAllowlist();
    {
        for (auto& allowlistURL : onlineAllowlist) {
            auto statement = m_database.prepareStatement("INSERT INTO CacheWhitelistURLs (url, cache) VALUES (?, ?)"_s);
            if (!statement)
                return false;

            statement->bindText(1, allowlistURL.string());
            statement->bindInt64(2, cacheStorageID);

            if (!executeStatement(statement.value()))
                return false;
        }
    }

    // Store online allowlist wildcard flag.
    {
        auto statement = m_database.prepareStatement("INSERT INTO CacheAllowsAllNetworkRequests (wildcard, cache) VALUES (?, ?)"_s);
        if (!statement)
            return false;

        statement->bindInt64(1, cache->allowsAllNetworkRequests());
        statement->bindInt64(2, cacheStorageID);

        if (!executeStatement(statement.value()))
            return false;
    }
    
    // Store fallback URLs.
    const FallbackURLVector& fallbackURLs = cache->fallbackURLs();
    {
        for (auto& fallbackURL : fallbackURLs) {
            auto statement = m_database.prepareStatement("INSERT INTO FallbackURLs (namespace, fallbackURL, cache) VALUES (?, ?, ?)"_s);
            if (!statement)
                return false;

            statement->bindText(1, fallbackURL.first.string());
            statement->bindText(2, fallbackURL.second.string());
            statement->bindInt64(3, cacheStorageID);

            if (!executeStatement(statement.value()))
                return false;
        }
    }

    cache->setStorageID(cacheStorageID);
    return true;
}

bool ApplicationCacheStorage::store(ApplicationCacheResource* resource, unsigned cacheStorageID)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(cacheStorageID);
    ASSERT(!resource->storageID());
    
    openDatabase(true);

    // openDatabase(true) could still fail, for example when cacheStorage is full or no longer available.
    if (!m_database.isOpen())
        return false;

    // First, insert the data
    auto dataStatement = m_database.prepareStatement("INSERT INTO CacheResourceData (data, path) VALUES (?, ?)"_s);
    if (!dataStatement)
        return false;

    String fullPath;
    if (!resource->path().isEmpty())
        dataStatement->bindText(2, FileSystem::pathFileName(resource->path()));
    else if (shouldStoreResourceAsFlatFile(resource)) {
        // First, check to see if creating the flat file would violate the maximum total quota. We don't need
        // to check the per-origin quota here, as it was already checked in storeNewestCache().
        if (m_database.totalSize() + flatFileAreaSize() + static_cast<int64_t>(resource->data().size()) > m_maximumSize) {
            m_isMaximumSizeReached = true;
            return false;
        }
        
        String flatFileDirectory = FileSystem::pathByAppendingComponent(m_cacheDirectory, m_flatFileSubdirectoryName);
        FileSystem::makeAllDirectories(flatFileDirectory);

        StringView extension;
        
        String fileName = resource->response().suggestedFilename();
        size_t dotIndex = fileName.reverseFind('.');
        if (dotIndex != notFound && dotIndex < (fileName.length() - 1))
            extension = StringView(fileName).substring(dotIndex);

        String path;
        if (!writeDataToUniqueFileInDirectory(resource->data(), flatFileDirectory, path, extension))
            return false;
        
        fullPath = FileSystem::pathByAppendingComponent(flatFileDirectory, path);
        resource->setPath(fullPath);
        dataStatement->bindText(2, path);
    } else {
        if (resource->data().size())
            dataStatement->bindBlob(1, resource->data().makeContiguous().get());
    }
    
    if (!dataStatement->executeCommand()) {
        // Clean up the file which we may have written to:
        if (!fullPath.isEmpty())
            FileSystem::deleteFile(fullPath);

        return false;
    }

    unsigned dataId = static_cast<unsigned>(m_database.lastInsertRowID());

    // Then, insert the resource
    
    // Serialize the headers
    StringBuilder stringBuilder;
    
    for (const auto& header : resource->response().httpHeaderFields()) {
        stringBuilder.append(header.key);
        stringBuilder.append(':');
        stringBuilder.append(header.value);
        stringBuilder.append('\n');
    }
    
    String headers = stringBuilder.toString();
    
    auto resourceStatement = m_database.prepareStatement("INSERT INTO CacheResources (url, statusCode, responseURL, headers, data, mimeType, textEncodingName) VALUES (?, ?, ?, ?, ?, ?, ?)"_s);
    if (!resourceStatement)
        return false;
    
    // The same ApplicationCacheResource are used in ApplicationCacheResource::size()
    // to calculate the approximate size of an ApplicationCacheResource object. If
    // you change the code below, please also change ApplicationCacheResource::size().
    resourceStatement->bindText(1, resource->url().string());
    resourceStatement->bindInt64(2, resource->response().httpStatusCode());
    resourceStatement->bindText(3, resource->response().url().string());
    resourceStatement->bindText(4, headers);
    resourceStatement->bindInt64(5, dataId);
    resourceStatement->bindText(6, resource->response().mimeType());
    resourceStatement->bindText(7, resource->response().textEncodingName());

    if (!executeStatement(resourceStatement.value()))
        return false;

    unsigned resourceId = static_cast<unsigned>(m_database.lastInsertRowID());
    
    // Finally, insert the cache entry
    auto entryStatement = m_database.prepareStatement("INSERT INTO CacheEntries (cache, type, resource) VALUES (?, ?, ?)"_s);
    if (!entryStatement)
        return false;
    
    entryStatement->bindInt64(1, cacheStorageID);
    entryStatement->bindInt64(2, resource->type());
    entryStatement->bindInt64(3, resourceId);
    
    if (!executeStatement(entryStatement.value()))
        return false;

    // Did we successfully write the resource data to a file? If so,
    // release the resource's data and free up a potentially large amount
    // of memory:
    if (!fullPath.isEmpty())
        resource->clear();

    resource->setStorageID(resourceId);
    return true;
}

bool ApplicationCacheStorage::storeUpdatedType(ApplicationCacheResource* resource, ApplicationCache* cache)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    ASSERT_UNUSED(cache, cache->storageID());
    ASSERT(resource->storageID());

    // First, insert the data
    auto entryStatement = m_database.prepareStatement("UPDATE CacheEntries SET type=? WHERE resource=?"_s);
    if (!entryStatement)
        return false;

    entryStatement->bindInt64(1, resource->type());
    entryStatement->bindInt64(2, resource->storageID());

    return executeStatement(entryStatement.value());
}

bool ApplicationCacheStorage::store(ApplicationCacheResource* resource, ApplicationCache* cache)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    ASSERT(cache->storageID());
    
    openDatabase(true);

    if (!m_database.isOpen())
        return false;
 
    m_isMaximumSizeReached = false;
    m_database.setMaximumSize(m_maximumSize - flatFileAreaSize());

    SQLiteTransaction storeResourceTransaction(m_database);
    storeResourceTransaction.begin();
    
    if (!store(resource, cache->storageID())) {
        checkForMaxSizeReached();
        return false;
    }

    // A resource was added to the cache. Update the total data size for the cache.
    auto sizeUpdateStatement = m_database.prepareStatement("UPDATE Caches SET size=size+? WHERE id=?"_s);
    if (!sizeUpdateStatement)
        return false;

    sizeUpdateStatement->bindInt64(1, resource->estimatedSizeInStorage());
    sizeUpdateStatement->bindInt64(2, cache->storageID());

    if (!executeStatement(sizeUpdateStatement.value()))
        return false;
    
    storeResourceTransaction.commit();
    return true;
}

bool ApplicationCacheStorage::ensureOriginRecord(const SecurityOrigin* origin)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    auto insertOriginStatement = m_database.prepareStatement("INSERT INTO Origins (origin, quota) VALUES (?, ?)"_s);
    if (!insertOriginStatement)
        return false;

    insertOriginStatement->bindText(1, origin->data().databaseIdentifier());
    insertOriginStatement->bindInt64(2, m_defaultOriginQuota);
    if (!executeStatement(insertOriginStatement.value()))
        return false;

    return true;
}

bool ApplicationCacheStorage::checkOriginQuota(ApplicationCacheGroup* group, ApplicationCache* oldCache, ApplicationCache* newCache, int64_t& totalSpaceNeeded)
{
    // Check if the oldCache with the newCache would reach the per-origin quota.
    int64_t remainingSpaceInOrigin;
    auto& origin = group->origin();
    if (calculateRemainingSizeForOriginExcludingCache(origin, oldCache, remainingSpaceInOrigin)) {
        if (remainingSpaceInOrigin < newCache->estimatedSizeInStorage()) {
            int64_t quota;
            if (calculateQuotaForOrigin(origin, quota)) {
                totalSpaceNeeded = quota - remainingSpaceInOrigin + newCache->estimatedSizeInStorage();
                return false;
            }

            ASSERT_NOT_REACHED();
            totalSpaceNeeded = 0;
            return false;
        }
    }

    return true;
}

bool ApplicationCacheStorage::storeNewestCache(ApplicationCacheGroup& group, ApplicationCache* oldCache, FailureReason& failureReason)
{
    openDatabase(true);

    if (!m_database.isOpen())
        return false;

    m_isMaximumSizeReached = false;
    m_database.setMaximumSize(m_maximumSize - flatFileAreaSize());

    SQLiteTransaction storeCacheTransaction(m_database);
    
    storeCacheTransaction.begin();

    // Check if this would reach the per-origin quota.
    int64_t totalSpaceNeededIgnored;
    if (!checkOriginQuota(&group, oldCache, group.newestCache(), totalSpaceNeededIgnored)) {
        failureReason = OriginQuotaReached;
        return false;
    }

    GroupStorageIDJournal groupStorageIDJournal;
    if (!group.storageID()) {
        // Store the group
        if (!store(&group, &groupStorageIDJournal)) {
            checkForMaxSizeReached();
            failureReason = isMaximumSizeReached() ? TotalQuotaReached : DiskOrOperationFailure;
            return false;
        }
    }
    
    ASSERT(group.newestCache());
    ASSERT(!group.isObsolete());
    ASSERT(!group.newestCache()->storageID());
    
    // Log the storageID changes to the in-memory resource objects. The journal
    // object will roll them back automatically in case a database operation
    // fails and this method returns early.
    ResourceStorageIDJournal resourceStorageIDJournal;

    // Store the newest cache
    if (!store(group.newestCache(), &resourceStorageIDJournal)) {
        checkForMaxSizeReached();
        failureReason = isMaximumSizeReached() ? TotalQuotaReached : DiskOrOperationFailure;
        return false;
    }
    
    // Update the newest cache in the group.
    
    auto statement = m_database.prepareStatement("UPDATE CacheGroups SET newestCache=? WHERE id=?"_s);
    if (!statement) {
        failureReason = DiskOrOperationFailure;
        return false;
    }
    
    statement->bindInt64(1, group.newestCache()->storageID());
    statement->bindInt64(2, group.storageID());
    
    if (!executeStatement(statement.value())) {
        failureReason = DiskOrOperationFailure;
        return false;
    }
    
    groupStorageIDJournal.commit();
    resourceStorageIDJournal.commit();
    storeCacheTransaction.commit();
    return true;
}

bool ApplicationCacheStorage::storeNewestCache(ApplicationCacheGroup& group)
{
    // Ignore the reason for failing, just attempt the store.
    FailureReason ignoredFailureReason;
    return storeNewestCache(group, nullptr, ignoredFailureReason);
}

template<typename CharacterType>
static inline void parseHeader(const CharacterType* header, unsigned headerLength, ResourceResponse& response)
{
    ASSERT(WTF::find(header, headerLength, ':') != notFound);
    unsigned colonPosition = WTF::find(header, headerLength, ':');

    // Save memory by putting the header names into atom strings so each is stored only once,
    // even though the setHTTPHeaderField function does not require an atom string.
    AtomString headerName { header, colonPosition };
    String headerValue { header + colonPosition + 1, headerLength - colonPosition - 1 };

    response.setHTTPHeaderField(headerName, headerValue);
}

static inline void parseHeaders(const String& headers, ResourceResponse& response)
{
    unsigned startPos = 0;
    size_t endPos;
    while ((endPos = headers.find('\n', startPos)) != notFound) {
        ASSERT(startPos != endPos);

        if (headers.is8Bit())
            parseHeader(headers.characters8() + startPos, endPos - startPos, response);
        else
            parseHeader(headers.characters16() + startPos, endPos - startPos, response);
        
        startPos = endPos + 1;
    }
    
    if (startPos != headers.length()) {
        if (headers.is8Bit())
            parseHeader(headers.characters8(), headers.length(), response);
        else
            parseHeader(headers.characters16(), headers.length(), response);
    }
}
    
RefPtr<ApplicationCache> ApplicationCacheStorage::loadCache(unsigned storageID)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    auto cacheStatement = m_database.prepareStatement(
        "SELECT url, statusCode, type, mimeType, textEncodingName, headers, CacheResourceData.data, CacheResourceData.path FROM CacheEntries INNER JOIN CacheResources ON CacheEntries.resource=CacheResources.id "
        "INNER JOIN CacheResourceData ON CacheResourceData.id=CacheResources.data WHERE CacheEntries.cache=?"_s);
    if (!cacheStatement) {
        LOG_ERROR("Could not prepare cache statement, error \"%s\"", m_database.lastErrorMsg());
        return nullptr;
    }
    
    cacheStatement->bindInt64(1, storageID);

    auto cache = ApplicationCache::create();

    String flatFileDirectory = FileSystem::pathByAppendingComponent(m_cacheDirectory, m_flatFileSubdirectoryName);

    int result;
    while ((result = cacheStatement->step()) == SQLITE_ROW) {
        URL url({ }, cacheStatement->columnText(0));
        
        int httpStatusCode = cacheStatement->columnInt(1);

        unsigned type = static_cast<unsigned>(cacheStatement->columnInt64(2));
        auto data = SharedBuffer::create(cacheStatement->columnBlob(6));
        
        String path = cacheStatement->columnText(7);
        long long size = 0;
        if (path.isEmpty())
            size = data->size();
        else {
            path = FileSystem::pathByAppendingComponent(flatFileDirectory, path);
            size = FileSystem::fileSize(path).value_or(0);
        }
        
        String mimeType = cacheStatement->columnText(3);
        String textEncodingName = cacheStatement->columnText(4);
        
        ResourceResponse response(url, mimeType, size, textEncodingName);
        response.setHTTPStatusCode(httpStatusCode);

        String headers = cacheStatement->columnText(5);
        parseHeaders(headers, response);
        
        auto resource = ApplicationCacheResource::create(url, response, type, WTFMove(data), path);

        if (type & ApplicationCacheResource::Manifest)
            cache->setManifestResource(WTFMove(resource));
        else
            cache->addResource(WTFMove(resource));
    }

    if (result != SQLITE_DONE)
        LOG_ERROR("Could not load cache resources, error \"%s\"", m_database.lastErrorMsg());

    if (!cache->manifestResource()) {
        LOG_ERROR("Could not load application cache because there was no manifest resource");
        return nullptr;
    }

    // Load the online allowlist
    auto allowlistStatement = m_database.prepareStatement("SELECT url FROM CacheWhitelistURLs WHERE cache=?"_s);
    if (!allowlistStatement)
        return nullptr;
    allowlistStatement->bindInt64(1, storageID);
    
    Vector<URL> allowlist;
    while ((result = allowlistStatement->step()) == SQLITE_ROW)
        allowlist.append(URL({ }, allowlistStatement->columnText(0)));

    if (result != SQLITE_DONE)
        LOG_ERROR("Could not load cache online allowlist, error \"%s\"", m_database.lastErrorMsg());

    cache->setOnlineAllowlist(allowlist);

    // Load online allowlist wildcard flag.
    auto allowlistWildcardStatement = m_database.prepareStatement("SELECT wildcard FROM CacheAllowsAllNetworkRequests WHERE cache=?"_s);
    if (!allowlistWildcardStatement)
        return nullptr;
    allowlistWildcardStatement->bindInt64(1, storageID);
    
    result = allowlistWildcardStatement->step();
    if (result != SQLITE_ROW)
        LOG_ERROR("Could not load cache online allowlist wildcard flag, error \"%s\"", m_database.lastErrorMsg());

    cache->setAllowsAllNetworkRequests(allowlistWildcardStatement->columnInt64(0));

    if (allowlistWildcardStatement->step() != SQLITE_DONE)
        LOG_ERROR("Too many rows for online allowlist wildcard flag");

    // Load fallback URLs.
    auto fallbackStatement = m_database.prepareStatement("SELECT namespace, fallbackURL FROM FallbackURLs WHERE cache=?"_s);
    if (!fallbackStatement)
        return nullptr;
    fallbackStatement->bindInt64(1, storageID);
    
    FallbackURLVector fallbackURLs;
    while ((result = fallbackStatement->step()) == SQLITE_ROW)
        fallbackURLs.append(std::make_pair(URL({ }, fallbackStatement->columnText(0)), URL({ }, fallbackStatement->columnText(1))));

    if (result != SQLITE_DONE)
        LOG_ERROR("Could not load fallback URLs, error \"%s\"", m_database.lastErrorMsg());

    cache->setFallbackURLs(fallbackURLs);
    
    cache->setStorageID(storageID);

    return cache;
}    
    
void ApplicationCacheStorage::remove(ApplicationCache* cache)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    if (!cache->storageID())
        return;
    
    openDatabase(false);
    if (!m_database.isOpen())
        return;

    ASSERT(cache->group());
    ASSERT(cache->group()->storageID());

    // All associated data will be deleted by database triggers.
    auto statement = m_database.prepareStatement("DELETE FROM Caches WHERE id=?"_s);
    if (!statement)
        return;
    
    statement->bindInt64(1, cache->storageID());
    executeStatement(statement.value());

    cache->clearStorageID();

    if (cache->group()->newestCache() == cache) {
        // Currently, there are no triggers on the cache group, which is why the cache had to be removed separately above.
        auto groupStatement = m_database.prepareStatement("DELETE FROM CacheGroups WHERE id=?"_s);
        if (!groupStatement)
            return;
        
        groupStatement->bindInt64(1, cache->group()->storageID());
        executeStatement(groupStatement.value());

        cache->group()->clearStorageID();
    }
    
    checkForDeletedResources();
}    

void ApplicationCacheStorage::empty()
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    
    if (!m_database.isOpen())
        return;
    
    // Clear cache groups, caches, cache resources, and origins.
    executeSQLCommand("DELETE FROM CacheGroups"_s);
    executeSQLCommand("DELETE FROM Caches"_s);
    executeSQLCommand("DELETE FROM Origins"_s);
    
    // Clear the storage IDs for the caches in memory.
    // The caches will still work, but cached resources will not be saved to disk 
    // until a cache update process has been initiated.
    for (auto* group : m_cachesInMemory.values())
        group->clearStorageID();

    checkForDeletedResources();
}
    
void ApplicationCacheStorage::deleteTables()
{
    empty();
    m_database.clearAllTables();
}
    
bool ApplicationCacheStorage::shouldStoreResourceAsFlatFile(ApplicationCacheResource* resource)
{
    auto& type = resource->response().mimeType();
    return startsWithLettersIgnoringASCIICase(type, "audio/"_s) || startsWithLettersIgnoringASCIICase(type, "video/"_s);
}
    
bool ApplicationCacheStorage::writeDataToUniqueFileInDirectory(FragmentedSharedBuffer& data, const String& directory, String& path, StringView fileExtension)
{
    String fullPath;
    
    do {
        path = makeString(UUID::createVersion4(), fileExtension);
        if (path.isEmpty())
            return false;
        
        fullPath = FileSystem::pathByAppendingComponent(directory, path);
    } while (FileSystem::parentPath(fullPath) != directory || FileSystem::fileExists(fullPath));
    
    FileSystem::PlatformFileHandle handle = FileSystem::openFile(fullPath, FileSystem::FileOpenMode::Truncate);
    if (!handle)
        return false;
    
    int64_t writtenBytes = 0;
    data.forEachSegment([&](auto& segment) {
        writtenBytes += FileSystem::writeToFile(handle, segment.data(), segment.size());
    });
    FileSystem::closeFile(handle);
    
    if (writtenBytes != static_cast<int64_t>(data.size())) {
        FileSystem::deleteFile(fullPath);
        return false;
    }
    
    return true;
}

std::optional<Vector<URL>> ApplicationCacheStorage::manifestURLs()
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    if (!m_database.isOpen())
        return std::nullopt;

    auto selectURLs = m_database.prepareStatement("SELECT manifestURL FROM CacheGroups"_s);
    if (!selectURLs)
        return std::nullopt;

    Vector<URL> urls;
    while (selectURLs->step() == SQLITE_ROW)
        urls.append(URL({ }, selectURLs->columnText(0)));

    return urls;
}

bool ApplicationCacheStorage::deleteCacheGroupRecord(const String& manifestURL)
{
    ASSERT(SQLiteDatabaseTracker::hasTransactionInProgress());
    auto idStatement = m_database.prepareStatement("SELECT id FROM CacheGroups WHERE manifestURL=?"_s);
    if (!idStatement)
        return false;

    idStatement->bindText(1, manifestURL);

    int result = idStatement->step();
    if (result != SQLITE_ROW)
        return false;

    int64_t groupId = idStatement->columnInt64(0);

    auto cacheStatement = m_database.prepareStatement("DELETE FROM Caches WHERE cacheGroup=?"_s);
    if (!cacheStatement)
        return false;

    auto groupStatement = m_database.prepareStatement("DELETE FROM CacheGroups WHERE id=?"_s);
    if (!groupStatement)
        return false;

    cacheStatement->bindInt64(1, groupId);
    executeStatement(cacheStatement.value());
    groupStatement->bindInt64(1, groupId);
    executeStatement(groupStatement.value());
    return true;
}

bool ApplicationCacheStorage::deleteCacheGroup(const String& manifestURL)
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    SQLiteTransaction deleteTransaction(m_database);

    // Check to see if the group is in memory.
    if (auto* group = m_cachesInMemory.get(manifestURL))
        cacheGroupMadeObsolete(*group);
    else {
        // The cache group is not in memory, so remove it from the disk.
        openDatabase(false);
        if (!m_database.isOpen())
            return false;
        if (!deleteCacheGroupRecord(manifestURL)) {
            LOG_ERROR("Could not delete cache group record, error \"%s\"", m_database.lastErrorMsg());
            return false;
        }
    }

    deleteTransaction.commit();

    checkForDeletedResources();

    return true;
}

void ApplicationCacheStorage::vacuumDatabaseFile()
{
    SQLiteTransactionInProgressAutoCounter transactionCounter;

    openDatabase(false);
    if (!m_database.isOpen())
        return;

    m_database.runVacuumCommand();
}

void ApplicationCacheStorage::checkForMaxSizeReached()
{
    if (m_database.lastError() == SQLITE_FULL)
        m_isMaximumSizeReached = true;
}
    
void ApplicationCacheStorage::checkForDeletedResources()
{
    openDatabase(false);
    if (!m_database.isOpen())
        return;

    // Select only the paths in DeletedCacheResources that do not also appear in CacheResourceData:
    auto selectPaths = m_database.prepareStatement("SELECT DeletedCacheResources.path "
        "FROM DeletedCacheResources "
        "LEFT JOIN CacheResourceData "
        "ON DeletedCacheResources.path = CacheResourceData.path "
        "WHERE (SELECT DeletedCacheResources.path == CacheResourceData.path) IS NULL"_s);
    
    if (!selectPaths)
        return;
    
    if (selectPaths->step() != SQLITE_ROW)
        return;
    
    do {
        String path = selectPaths->columnText(0);
        if (path.isEmpty())
            continue;
        
        String flatFileDirectory = FileSystem::pathByAppendingComponent(m_cacheDirectory, m_flatFileSubdirectoryName);
        String fullPath = FileSystem::pathByAppendingComponent(flatFileDirectory, path);
        
        // Don't exit the flatFileDirectory! This should only happen if the "path" entry contains a directory 
        // component, but protect against it regardless.
        if (FileSystem::parentPath(fullPath) != flatFileDirectory)
            continue;
        
        FileSystem::deleteFile(fullPath);
    } while (selectPaths->step() == SQLITE_ROW);
    
    executeSQLCommand("DELETE FROM DeletedCacheResources"_s);
}
    
long long ApplicationCacheStorage::flatFileAreaSize()
{
    openDatabase(false);
    if (!m_database.isOpen())
        return 0;
    
    auto selectPaths = m_database.prepareStatement("SELECT path FROM CacheResourceData WHERE path NOT NULL"_s);
    if (!selectPaths) {
        LOG_ERROR("Could not load flat file cache resource data, error \"%s\"", m_database.lastErrorMsg());
        return 0;
    }

    long long totalSize = 0;
    String flatFileDirectory = FileSystem::pathByAppendingComponent(m_cacheDirectory, m_flatFileSubdirectoryName);
    while (selectPaths->step() == SQLITE_ROW) {
        auto fullPath = FileSystem::pathByAppendingComponent(flatFileDirectory, selectPaths->columnText(0));
        totalSize += FileSystem::fileSize(fullPath).value_or(0);
    }
    
    return totalSize;
}

HashSet<SecurityOriginData> ApplicationCacheStorage::originsWithCache()
{
    auto urls = manifestURLs();
    if (!urls)
        return { };

    // Multiple manifest URLs might share the same SecurityOrigin, so we might be creating extra, wasted origins here.
    // The current schema doesn't allow for a more efficient way of building this list.
    HashSet<SecurityOriginData> origins;
    for (auto& url : *urls)
        origins.add(SecurityOriginData::fromURL(url));
    return origins;
}

void ApplicationCacheStorage::deleteAllEntries()
{
    empty();
    vacuumDatabaseFile();
}

void ApplicationCacheStorage::deleteAllCaches()
{
    auto origins = originsWithCache();
    for (auto& origin : origins)
        deleteCacheForOrigin(origin);

    vacuumDatabaseFile();
}

void ApplicationCacheStorage::deleteCacheForOrigin(const SecurityOriginData& securityOrigin)
{
    auto urls = manifestURLs();
    if (!urls) {
        LOG_ERROR("Failed to retrieve ApplicationCache manifest URLs");
        return;
    }

    URL originURL = securityOrigin.toURL();

    for (const auto& url : *urls) {
        if (!protocolHostAndPortAreEqual(url, originURL))
            continue;

        if (auto* group = findInMemoryCacheGroup(url))
            group->makeObsolete();
        else
            deleteCacheGroup(url.string());
    }
}

int64_t ApplicationCacheStorage::diskUsageForOrigin(const SecurityOriginData& securityOrigin)
{
    int64_t usage = 0;
    calculateUsageForOrigin(securityOrigin, usage);
    return usage;
}

ApplicationCacheStorage::ApplicationCacheStorage(const String& cacheDirectory, const String& flatFileSubdirectoryName)
    : m_cacheDirectory(cacheDirectory)
    , m_flatFileSubdirectoryName(flatFileSubdirectoryName)
{
}

} // namespace WebCore
