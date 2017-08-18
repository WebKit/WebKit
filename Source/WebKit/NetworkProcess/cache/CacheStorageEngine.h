/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "WebCoreArgumentCoders.h"
#include <WebCore/CacheStorageConnection.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class SessionID;
}

namespace WebKit {

class NetworkConnectionToWebProcess;

class CacheStorageEngine {
public:
    static CacheStorageEngine& from(PAL::SessionID);
    static void destroyEngine(PAL::SessionID);

    enum class Error {
        Internal
    };
    using Record = WebCore::CacheStorageConnection::Record;

    using CacheIdentifierOrError = Expected<uint64_t, Error>;
    using CacheIdentifierCallback = Function<void(CacheIdentifierOrError&&)>;

    using CacheInfosOrError = Expected<Vector<WebCore::CacheStorageConnection::CacheInfo>, Error>;
    using CacheInfosCallback = Function<void(CacheInfosOrError&&)>;

    using RecordsOrError = Expected<Vector<Record>, Error>;
    using RecordsCallback = Function<void(RecordsOrError&&)>;

    using RecordIdentifiersOrError = Expected<Vector<uint64_t>, Error>;
    using RecordIdentifiersCallback = Function<void(RecordIdentifiersOrError&&)>;

    using CompletionCallback = Function<void(std::optional<Error>&&)>;

    void open(const String& origin, const String& cacheName, CacheIdentifierCallback&&);
    void remove(uint64_t cacheIdentifier, CacheIdentifierCallback&&);
    void retrieveCaches(const String& origin, CacheInfosCallback&&);

    void retrieveRecords(uint64_t cacheIdentifier, RecordsCallback&&);
    void putRecords(uint64_t cacheIdentifier, Vector<Record>&&, RecordIdentifiersCallback&&);
    void deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, RecordIdentifiersCallback&&);

private:
    static CacheStorageEngine& defaultEngine();

    void writeCachesToDisk(CompletionCallback&&);
    void readCachesFromDisk(CompletionCallback&&);

    struct Cache {
        uint64_t identifier;
        String name;
        Vector<Record> records;
        uint64_t nextRecordIdentifier { 0 };
    };

    using CacheOrError = Expected<std::reference_wrapper<Cache>, Error>;
    using CacheCallback = Function<void(CacheOrError&&)>;

    Vector<WebCore::CacheStorageConnection::CacheInfo> caches(const String& origin) const;

    void readCache(uint64_t cacheIdentifier, CacheCallback&&);
    void writeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&&, RecordIdentifiersCallback&&);
    void removeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&&, RecordIdentifiersCallback&&);

    Vector<uint64_t> queryCache(const Vector<Record>&, const WebCore::ResourceRequest&, const WebCore::CacheQueryOptions&);

    Cache* cache(uint64_t cacheIdentifier);

    HashMap<String, Vector<Cache>> m_caches;
    Vector<Cache> m_removedCaches;
    uint64_t m_nextCacheIdentifier { 0 };
};

}

namespace WTF {
template<> struct EnumTraits<WebKit::CacheStorageEngine::Error> {
    using values = EnumValues<
        WebKit::CacheStorageEngine::Error,
        WebKit::CacheStorageEngine::Error::Internal
    >;
};
}
