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
#include <wtf/text/StringBuilder.h>

using namespace WebCore::DOMCacheEngine;
using namespace WebKit::NetworkCache;

namespace WebKit {

namespace CacheStorage {

static inline String cachesRootPath(Engine& engine, const String& origin)
{
    if (!engine.shouldPersist())
        return { };

    Key key(engine.rootPath(), { }, { }, origin, engine.salt());
    return WebCore::pathByAppendingComponent(engine.rootPath(), key.partitionHashAsString());
}

static inline String cachesListFilename(const String& cachesRootPath)
{
    return WebCore::pathByAppendingComponent(cachesRootPath, ASCIILiteral("cacheslist"));
}

Caches::Caches(Engine& engine, const String& origin)
    : m_engine(&engine)
    , m_rootPath(cachesRootPath(engine, origin))
{
}

void Caches::initialize(WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (m_isInitialized || m_rootPath.isNull()) {
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

const Cache* Caches::find(const String& name) const
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
    ASSERT(m_engine);

    if (auto* cache = find(name)) {
        callback(CacheIdentifierOperationResult { cache->identifier(), false });
        return;
    }

    makeDirty();

    uint64_t cacheIdentifier = m_engine->nextCacheIdentifier();
    m_caches.append(Cache { cacheIdentifier, String { name } });
    writeCachesToDisk([callback = WTFMove(callback), cacheIdentifier](std::optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { cacheIdentifier, !!error });
    });
}

void Caches::remove(uint64_t identifier, CacheIdentifierCallback&& callback)
{
    ASSERT(m_engine);

    auto position = m_caches.findMatching([&](const auto& item) { return item.identifier() == identifier; });

    if (position == notFound) {
        ASSERT(m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == identifier; }) != notFound);
        callback(CacheIdentifierOperationResult { identifier, false });
        return;
    }

    makeDirty();

    auto cache = WTFMove(m_caches[position]);
    m_caches.remove(position);
    m_removedCaches.append(WTFMove(cache));

    writeCachesToDisk([callback = WTFMove(callback), identifier](std::optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { identifier, !!error });
    });
}

static inline Data encodeCacheNames(const Vector<Cache>& caches)
{
    WTF::Persistence::Encoder encoder;

    uint64_t size = caches.size();
    encoder << size;
    for (auto& cache : caches)
        encoder << cache.name();

    return Data { encoder.buffer(), encoder.bufferSize() };
}

static inline Expected<Vector<String>, Error> decodeCachesNames(const Data& data, int error)
{
    if (error)
        return makeUnexpected(Error::ReadDisk);

    WTF::Persistence::Decoder decoder(data.data(), data.size());
    uint64_t count;
    if (!decoder.decode(count))
        return makeUnexpected(Error::ReadDisk);

    Vector<String> names;
    names.reserveInitialCapacity(count);
    for (size_t index = 0; index < count; ++index) {
        String name;
        if (!decoder.decode(name))
            return makeUnexpected(Error::ReadDisk);

        names.uncheckedAppend(WTFMove(name));
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
        Vector<Cache> caches;
        caches.reserveInitialCapacity(result.value().size());
        for (auto& name : result.value())
            caches.uncheckedAppend(Cache { m_engine->nextCacheIdentifier(), WTFMove(name) });

        callback(WTFMove(caches));
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

} // namespace CacheStorage

} // namespace WebKit
