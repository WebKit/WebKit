/*
 * Copyright (C) 2008, 2010, 2011 Apple Inc. All Rights Reserved.
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

#ifndef ApplicationCacheStorage_h
#define ApplicationCacheStorage_h

#include "SecurityOriginHash.h"
#include "SQLiteDatabase.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ApplicationCache;
class ApplicationCacheGroup;
class ApplicationCacheHost;
class ApplicationCacheResource;
class URL;
class SecurityOrigin;
class SharedBuffer;
template <class T> class StorageIDJournal;

class ApplicationCacheStorage : public RefCounted<ApplicationCacheStorage> {
public:
    enum FailureReason {
        OriginQuotaReached,
        TotalQuotaReached,
        DiskOrOperationFailure
    };

    // FIXME: Migrate off of this singleton and towards a world where each page has a storage.
    WEBCORE_EXPORT static ApplicationCacheStorage& singleton();

    WEBCORE_EXPORT static Ref<ApplicationCacheStorage> create(const String& cacheDirectory, const String& flatFileSubdirectoryName);

    WEBCORE_EXPORT void setCacheDirectory(const String&);
    const String& cacheDirectory() const;
    
    WEBCORE_EXPORT void setMaximumSize(int64_t size);
    WEBCORE_EXPORT int64_t maximumSize() const;
    bool isMaximumSizeReached() const;
    int64_t spaceNeeded(int64_t cacheToSave);

    int64_t defaultOriginQuota() const { return m_defaultOriginQuota; }
    WEBCORE_EXPORT void setDefaultOriginQuota(int64_t quota);
    WEBCORE_EXPORT bool calculateUsageForOrigin(const SecurityOrigin*, int64_t& usage);
    WEBCORE_EXPORT bool calculateQuotaForOrigin(const SecurityOrigin*, int64_t& quota);
    bool calculateRemainingSizeForOriginExcludingCache(const SecurityOrigin*, ApplicationCache*, int64_t& remainingSize);
    WEBCORE_EXPORT bool storeUpdatedQuotaForOrigin(const SecurityOrigin*, int64_t quota);
    bool checkOriginQuota(ApplicationCacheGroup*, ApplicationCache* oldCache, ApplicationCache* newCache, int64_t& totalSpaceNeeded);

    ApplicationCacheGroup* cacheGroupForURL(const URL&); // Cache to load a main resource from.
    ApplicationCacheGroup* fallbackCacheGroupForURL(const URL&); // Cache that has a fallback entry to load a main resource from if normal loading fails.

    ApplicationCacheGroup* findOrCreateCacheGroup(const URL& manifestURL);
    ApplicationCacheGroup* findInMemoryCacheGroup(const URL& manifestURL) const;
    void cacheGroupDestroyed(ApplicationCacheGroup*);
    void cacheGroupMadeObsolete(ApplicationCacheGroup*);
        
    bool storeNewestCache(ApplicationCacheGroup*, ApplicationCache* oldCache, FailureReason& failureReason);
    bool storeNewestCache(ApplicationCacheGroup*); // Updates the cache group, but doesn't remove old cache.
    bool store(ApplicationCacheResource*, ApplicationCache*);
    bool storeUpdatedType(ApplicationCacheResource*, ApplicationCache*);

    // Removes the group if the cache to be removed is the newest one (so, storeNewestCache() needs to be called beforehand when updating).
    void remove(ApplicationCache*);
    
    WEBCORE_EXPORT void empty();
    
    bool getManifestURLs(Vector<URL>* urls);
    bool cacheGroupSize(const String& manifestURL, int64_t* size);
    bool deleteCacheGroup(const String& manifestURL);
    WEBCORE_EXPORT void vacuumDatabaseFile();

    WEBCORE_EXPORT void getOriginsWithCache(HashSet<RefPtr<SecurityOrigin>, SecurityOriginHash>&);
    WEBCORE_EXPORT void deleteAllEntries();

    // FIXME: This should be consolidated with deleteAllEntries().
    WEBCORE_EXPORT void deleteAllCaches();

    // FIXME: This should be consolidated with deleteCacheGroup().
    WEBCORE_EXPORT void deleteCacheForOrigin(const SecurityOrigin&);

    // FIXME: This should be consolidated with calculateUsageForOrigin().
    WEBCORE_EXPORT int64_t diskUsageForOrigin(const SecurityOrigin&);

    static int64_t unknownQuota() { return -1; }
    static int64_t noQuota() { return std::numeric_limits<int64_t>::max(); }
private:
    ApplicationCacheStorage(const String& cacheDirectory, const String& flatFileSubdirectoryName);

    PassRefPtr<ApplicationCache> loadCache(unsigned storageID);
    ApplicationCacheGroup* loadCacheGroup(const URL& manifestURL);
    
    typedef StorageIDJournal<ApplicationCacheResource> ResourceStorageIDJournal;
    typedef StorageIDJournal<ApplicationCacheGroup> GroupStorageIDJournal;

    bool store(ApplicationCacheGroup*, GroupStorageIDJournal*);
    bool store(ApplicationCache*, ResourceStorageIDJournal*);
    bool store(ApplicationCacheResource*, unsigned cacheStorageID);
    bool deleteCacheGroupRecord(const String& manifestURL);

    bool ensureOriginRecord(const SecurityOrigin*);
    bool shouldStoreResourceAsFlatFile(ApplicationCacheResource*);
    void deleteTables();
    bool writeDataToUniqueFileInDirectory(SharedBuffer&, const String& directory, String& outFilename, const String& fileExtension);

    void loadManifestHostHashes();
    
    void verifySchemaVersion();
    
    void openDatabase(bool createIfDoesNotExist);
    
    bool executeStatement(SQLiteStatement&);
    bool executeSQLCommand(const String&);

    void checkForMaxSizeReached();
    void checkForDeletedResources();
    long long flatFileAreaSize();

    String m_cacheDirectory;
    String m_cacheFile;
    const String m_flatFileSubdirectoryName;

    int64_t m_maximumSize;
    bool m_isMaximumSizeReached;

    int64_t m_defaultOriginQuota;

    SQLiteDatabase m_database;

    // In order to quickly determine if a given resource exists in an application cache,
    // we keep a hash set of the hosts of the manifest URLs of all non-obsolete cache groups.
    HashCountedSet<unsigned, AlreadyHashed> m_cacheHostSet;
    
    typedef HashMap<String, ApplicationCacheGroup*> CacheGroupMap;
    CacheGroupMap m_cachesInMemory; // Excludes obsolete cache groups.

    friend class WTF::NeverDestroyed<ApplicationCacheStorage>;
};

} // namespace WebCore

#endif // ApplicationCacheStorage_h
