/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
#include "CacheStorageEngine.h"

#include "NetworkCacheCoders.h"
#include <wtf/UUID.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore::DOMCacheEngine;
using namespace WebKit::NetworkCache;

namespace WebKit {

namespace CacheStorage {

static inline String cachesRootPath(Engine& engine, const String& origin)
{
    if (!engine.shouldPersist())
        return { };

    Key key(origin, { }, { }, { }, engine.salt());
    return WebCore::pathByAppendingComponent(engine.rootPath(), key.partitionHashAsString());
}

static inline String cachesListFilename(const String& cachesRootPath)
{
    return WebCore::pathByAppendingComponent(cachesRootPath, ASCIILiteral("cacheslist"));
}

Caches::Caches(Engine& engine, String&& origin)
    : m_engine(&engine)
    , m_origin(WTFMove(origin))
    , m_rootPath(cachesRootPath(engine, m_origin))
{
}

void Caches::initialize(WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (m_isInitialized) {
        callback(std::nullopt);
        return;
    }

    if (m_rootPath.isNull()) {
        makeDirty();
        m_isInitialized = true;
        callback(std::nullopt);
        return;
    }

    if (m_storage) {
        m_pendingInitializationCallbacks.append(WTFMove(callback));
        return;
    }

    auto storage = Storage::open(m_rootPath, Storage::Mode::Normal);
    if (!storage) {
        callback(Error::WriteDisk);
        return;
    }
    m_storage = storage.releaseNonNull();
    m_storage->writeWithoutWaiting();
    readCachesFromDisk([this, callback = WTFMove(callback)](Expected<Vector<Cache>, Error>&& result) {
        makeDirty();

        if (!result.hasValue()) {
            callback(result.error());

            auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
            for (auto& callback : pendingCallbacks)
                callback(result.error());
            return;
        }
        m_caches = WTFMove(result.value());
        m_isInitialized = true;
        callback(std::nullopt);

        auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
        for (auto& callback : pendingCallbacks)
            callback(std::nullopt);
    });
}

void Caches::detach()
{
    m_engine = nullptr;
    m_rootPath = { };
}

Cache* Caches::find(const String& name)
{
    auto position = m_caches.findMatching([&](const auto& item) { return item.name() == name; });
    return (position != notFound) ? &m_caches[position] : nullptr;
}

Cache* Caches::find(uint64_t identifier)
{
    auto position = m_caches.findMatching([&](const auto& item) { return item.identifier() == identifier; });
    if (position != notFound)
        return &m_caches[position];

    position = m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == identifier; });
    return (position != notFound) ? &m_removedCaches[position] : nullptr;
}

void Caches::open(const String& name, CacheIdentifierCallback&& callback)
{
    ASSERT(m_isInitialized);
    ASSERT(m_engine);

    if (auto* cache = find(name)) {
        cache->open([cacheIdentifier = cache->identifier(), callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }
            callback(CacheIdentifierOperationResult { cacheIdentifier, false });
        });
        return;
    }

    makeDirty();

    uint64_t cacheIdentifier = m_engine->nextCacheIdentifier();
    m_caches.append(Cache { *this, cacheIdentifier, Cache::State::Open, String { name }, createCanonicalUUIDString() });

    writeCachesToDisk([callback = WTFMove(callback), cacheIdentifier](std::optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { cacheIdentifier, !!error });
    });
}

void Caches::remove(uint64_t identifier, CacheIdentifierCallback&& callback)
{
    ASSERT(m_isInitialized);
    ASSERT(m_engine);

    auto position = m_caches.findMatching([&](const auto& item) { return item.identifier() == identifier; });

    if (position == notFound) {
        ASSERT(m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == identifier; }) != notFound);
        callback(CacheIdentifierOperationResult { identifier, false });
        return;
    }

    makeDirty();

    m_removedCaches.append(WTFMove(m_caches[position]));
    m_caches.remove(position);

    writeCachesToDisk([callback = WTFMove(callback), identifier](std::optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { identifier, !!error });
    });
}

void Caches::dispose(Cache& cache)
{
    auto position = m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == cache.identifier(); });
    if (position != notFound) {
        if (m_storage)
            m_storage->remove(cache.keys(), { });

        m_removedCaches.remove(position);
        return;
    }
    ASSERT(m_caches.findMatching([&](const auto& item) { return item.identifier() == cache.identifier(); }) != notFound);
    cache.clearMemoryRepresentation();

    if (m_caches.findMatching([](const auto& item) { return item.isActive(); }) == notFound)
        clearMemoryRepresentation();
}

static inline Data encodeCacheNames(const Vector<Cache>& caches)
{
    WTF::Persistence::Encoder encoder;

    uint64_t size = caches.size();
    encoder << size;
    for (auto& cache : caches) {
        encoder << cache.name();
        encoder << cache.uniqueName();
    }

    return Data { encoder.buffer(), encoder.bufferSize() };
}

static inline Expected<Vector<std::pair<String, String>>, Error> decodeCachesNames(const Data& data, int error)
{
    if (error)
        return makeUnexpected(Error::ReadDisk);

    WTF::Persistence::Decoder decoder(data.data(), data.size());
    uint64_t count;
    if (!decoder.decode(count))
        return makeUnexpected(Error::ReadDisk);

    Vector<std::pair<String, String>> names;
    names.reserveInitialCapacity(count);
    for (size_t index = 0; index < count; ++index) {
        String name;
        if (!decoder.decode(name))
            return makeUnexpected(Error::ReadDisk);
        String uniqueName;
        if (!decoder.decode(uniqueName))
            return makeUnexpected(Error::ReadDisk);

        names.uncheckedAppend(std::pair<String, String> { WTFMove(name), WTFMove(uniqueName) });
    }
    return names;
}

void Caches::readCachesFromDisk(WTF::Function<void(Expected<Vector<Cache>, Error>&&)>&& callback)
{
    ASSERT(m_engine);
    ASSERT(!m_isInitialized);
    ASSERT(m_caches.isEmpty());

    if (!shouldPersist()) {
        callback(Vector<Cache> { });
        return;
    }

    auto filename = cachesListFilename(m_rootPath);
    if (!WebCore::fileExists(filename)) {
        callback(Vector<Cache> { });
        return;
    }

    m_engine->readFile(filename, [protectedThis = makeRef(*this), this, callback = WTFMove(callback)](const Data& data, int error) mutable {
        if (!m_engine) {
            callback(Vector<Cache> { });
            return;
        }

        auto result = decodeCachesNames(data, error);
        if (!result.hasValue()) {
            callback(makeUnexpected(result.error()));
            return;
        }
        callback(WTF::map(WTFMove(result.value()), [this] (auto&& pair) {
            return Cache { *this, m_engine->nextCacheIdentifier(), Cache::State::Uninitialized, WTFMove(pair.first), WTFMove(pair.second) };
        }));
    });
}

void Caches::writeCachesToDisk(CompletionCallback&& callback)
{
    if (!shouldPersist()) {
        callback(std::nullopt);
        return;
    }

    ASSERT(m_engine);

    if (m_caches.isEmpty()) {
        m_engine->removeFile(cachesListFilename(m_rootPath));
        callback(std::nullopt);
        return;
    }

    m_engine->writeFile(cachesListFilename(m_rootPath), encodeCacheNames(m_caches), [callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
        callback(WTFMove(error));
    });
}

void Caches::readRecordsList(Cache& cache, NetworkCache::Storage::TraverseHandler&& callback)
{
    ASSERT(m_isInitialized);

    if (!m_storage) {
        callback(nullptr, { });
        return;
    }
    m_storage->traverse(cache.uniqueName(), 0, [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](const auto* storage, const auto& information) {
        callback(storage, information);
    });
}

void Caches::writeRecord(const Cache& cache, const RecordInformation& recordInformation, Record&& record, CompletionCallback&& callback)
{
    ASSERT(m_isInitialized);

    // FIXME: Check for storage quota.
    if (!shouldPersist()) {
        m_volatileStorage.set(recordInformation.key, WTFMove(record));
        return;
    }

    m_storage->store(Cache::encode(recordInformation, record), [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](const Data&) {
        callback(std::nullopt);
    });
}

void Caches::readRecord(const NetworkCache::Key& key, WTF::Function<void(Expected<Record, Error>&&)>&& callback)
{
    ASSERT(m_isInitialized);

    if (!shouldPersist()) {
        auto iterator = m_volatileStorage.find(key);
        if (iterator == m_volatileStorage.end()) {
            callback(makeUnexpected(Error::Internal));
            return;
        }
        callback(iterator->value.copy());
        return;
    }

    m_storage->retrieve(key, 4, [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](std::unique_ptr<Storage::Record> storage) mutable {
        if (!storage) {
            callback(makeUnexpected(Error::ReadDisk));
            return false;
        }

        auto record = Cache::decode(*storage);
        if (!record) {
            callback(makeUnexpected(Error::ReadDisk));
            return false;
        }

        callback(WTFMove(record.value()));
        return true;
    });
}

void Caches::removeRecord(const NetworkCache::Key& key)
{
    ASSERT(m_isInitialized);

    if (!shouldPersist()) {
        m_volatileStorage.remove(key);
        return;
    }
    m_storage->remove(key);
}

void Caches::clearMemoryRepresentation()
{
    makeDirty();
    m_caches.clear();
    m_isInitialized = false;
    m_storage = nullptr;
}

bool Caches::isDirty(uint64_t updateCounter) const
{
    ASSERT(m_updateCounter >= updateCounter);
    return m_updateCounter != updateCounter;
}

const NetworkCache::Salt& Caches::salt() const
{
    if (m_engine)
        return m_engine->salt();

    if (!m_volatileSalt)
        m_volatileSalt = Salt { };

    return m_volatileSalt.value();
}

CacheInfos Caches::cacheInfos(uint64_t updateCounter) const
{
    Vector<CacheInfo> cacheInfos;
    if (isDirty(updateCounter)) {
        cacheInfos.reserveInitialCapacity(m_caches.size());
        for (auto& cache : m_caches)
            cacheInfos.uncheckedAppend(CacheInfo { cache.identifier(), cache.name() });
    }
    return { WTFMove(cacheInfos), m_updateCounter };
}

void Caches::appendRepresentation(StringBuilder& builder) const
{
    builder.append("{ \"persistent\": [");

    bool isFirst = true;
    for (auto& cache : m_caches) {
        if (!isFirst)
            builder.append(", ");
        isFirst = false;
        builder.append("\"");
        builder.append(cache.name());
        builder.append("\"");
    }

    builder.append("], \"removed\": [");

    isFirst = true;
    for (auto& cache : m_removedCaches) {
        if (!isFirst)
            builder.append(", ");
        isFirst = false;
        builder.append("\"");
        builder.append(cache.name());
        builder.append("\"");
    }
    builder.append("]}\n");
}

} // namespace CacheStorage

} // namespace WebKit
