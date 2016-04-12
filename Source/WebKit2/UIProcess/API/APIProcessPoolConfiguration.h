/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APIContextConfiguration_h
#define APIContextConfiguration_h

#include "APIObject.h"
#include "CacheModel.h"
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace API {

class ProcessPoolConfiguration final : public ObjectImpl<Object::Type::ProcessPoolConfiguration> {
public:
    static Ref<ProcessPoolConfiguration> create();
    static Ref<ProcessPoolConfiguration> createWithLegacyOptions();
    
    explicit ProcessPoolConfiguration();
    virtual ~ProcessPoolConfiguration();
    
    Ref<ProcessPoolConfiguration> copy();

    bool shouldHaveLegacyDataStore() const { return m_shouldHaveLegacyDataStore; }
    void setShouldHaveLegacyDataStore(bool shouldHaveLegacyDataStore) { m_shouldHaveLegacyDataStore = shouldHaveLegacyDataStore; }

    unsigned maximumProcessCount() const { return m_maximumProcessCount; }
    void setMaximumProcessCount(unsigned maximumProcessCount) { m_maximumProcessCount = maximumProcessCount; } 

    bool diskCacheSpeculativeValidationEnabled() const { return m_diskCacheSpeculativeValidationEnabled; }
    void setDiskCacheSpeculativeValidationEnabled(bool enabled) { m_diskCacheSpeculativeValidationEnabled = enabled; }

    WebKit::CacheModel cacheModel() const { return m_cacheModel; }
    void setCacheModel(WebKit::CacheModel cacheModel) { m_cacheModel = cacheModel; }

    int64_t diskCacheSizeOverride() const { return m_diskCacheSizeOverride; }
    void setDiskCacheSizeOverride(int64_t size) { m_diskCacheSizeOverride = size; }

    const WTF::String& applicationCacheDirectory() const { return m_applicationCacheDirectory; }
    void setApplicationCacheDirectory(const WTF::String& applicationCacheDirectory) { m_applicationCacheDirectory = applicationCacheDirectory; }

    const WTF::String& diskCacheDirectory() const { return m_diskCacheDirectory; }
    void setDiskCacheDirectory(const WTF::String& diskCacheDirectory) { m_diskCacheDirectory = diskCacheDirectory; }

    const WTF::String& mediaCacheDirectory() const { return m_mediaCacheDirectory; }
    void setMediaCacheDirectory(const WTF::String& mediaCacheDirectory) { m_mediaCacheDirectory = mediaCacheDirectory; }
    
    const WTF::String& indexedDBDatabaseDirectory() const { return m_indexedDBDatabaseDirectory; }
    void setIndexedDBDatabaseDirectory(const WTF::String& indexedDBDatabaseDirectory) { m_indexedDBDatabaseDirectory = indexedDBDatabaseDirectory; }

    const WTF::String& injectedBundlePath() const { return m_injectedBundlePath; }
    void setInjectedBundlePath(const WTF::String& injectedBundlePath) { m_injectedBundlePath = injectedBundlePath; }

    const WTF::String& localStorageDirectory() const { return m_localStorageDirectory; }
    void setLocalStorageDirectory(const WTF::String& localStorageDirectory) { m_localStorageDirectory = localStorageDirectory; }

    const WTF::String& webSQLDatabaseDirectory() const { return m_webSQLDatabaseDirectory; }
    void setWebSQLDatabaseDirectory(const WTF::String& webSQLDatabaseDirectory) { m_webSQLDatabaseDirectory = webSQLDatabaseDirectory; }

    const WTF::String& mediaKeysStorageDirectory() const { return m_mediaKeysStorageDirectory; }
    void setMediaKeysStorageDirectory(const WTF::String& mediaKeysStorageDirectory) { m_mediaKeysStorageDirectory = mediaKeysStorageDirectory; }

    const Vector<WTF::String>& cachePartitionedURLSchemes() { return m_cachePartitionedURLSchemes; }
    void setCachePartitionedURLSchemes(Vector<WTF::String>&& cachePartitionedURLSchemes) { m_cachePartitionedURLSchemes = WTFMove(cachePartitionedURLSchemes); }

    const Vector<WTF::String>& alwaysRevalidatedURLSchemes() { return m_alwaysRevalidatedURLSchemes; }
    void setAlwaysRevalidatedURLSchemes(Vector<WTF::String>&& alwaysRevalidatedURLSchemes) { m_alwaysRevalidatedURLSchemes = WTFMove(alwaysRevalidatedURLSchemes); }

    bool fullySynchronousModeIsAllowedForTesting() const { return m_fullySynchronousModeIsAllowedForTesting; }
    void setFullySynchronousModeIsAllowedForTesting(bool allowed) { m_fullySynchronousModeIsAllowedForTesting = allowed; }

    const Vector<WTF::String>& overrideLanguages() const { return m_overrideLanguages; }
    void setOverrideLanguages(Vector<WTF::String>&& languages) { m_overrideLanguages = WTFMove(languages); }

private:
    bool m_shouldHaveLegacyDataStore { false };

    unsigned m_maximumProcessCount { 0 };
    bool m_diskCacheSpeculativeValidationEnabled { false };
    WebKit::CacheModel m_cacheModel { WebKit::CacheModelPrimaryWebBrowser };
    int64_t m_diskCacheSizeOverride { -1 };

    WTF::String m_applicationCacheDirectory;
    WTF::String m_diskCacheDirectory;
    WTF::String m_mediaCacheDirectory;
    WTF::String m_indexedDBDatabaseDirectory;
    WTF::String m_injectedBundlePath;
    WTF::String m_localStorageDirectory;
    WTF::String m_webSQLDatabaseDirectory;
    WTF::String m_mediaKeysStorageDirectory;
    WTF::String m_resourceLoadStatisticsDirectory;
    Vector<WTF::String> m_cachePartitionedURLSchemes;
    Vector<WTF::String> m_alwaysRevalidatedURLSchemes;
    bool m_fullySynchronousModeIsAllowedForTesting { false };
    Vector<WTF::String> m_overrideLanguages;
};

} // namespace API

#endif // APIContextConfiguration_h
