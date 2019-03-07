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

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheIOChannel.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/StorageQuotaManager.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

namespace CacheStorage {
using namespace WebCore::DOMCacheEngine;
using namespace NetworkCache;

static inline String cachesListFilename(const String& cachesRootPath)
{
    return FileSystem::pathByAppendingComponent(cachesRootPath, "cacheslist"_s);
}

static inline String cachesOriginFilename(const String& cachesRootPath)
{
    return FileSystem::pathByAppendingComponent(cachesRootPath, "origin"_s);
}

Caches::Caches(Engine& engine, WebCore::ClientOrigin&& origin, String&& rootPath, StorageQuotaManager& quotaManager)
    : m_engine(&engine)
    , m_origin(WTFMove(origin))
    , m_rootPath(WTFMove(rootPath))
    , m_quotaManager(makeWeakPtr(quotaManager))
{
    quotaManager.addUser(*this);
}

Caches::~Caches()
{
    ASSERT(m_pendingWritingCachesToDiskCallbacks.isEmpty());

    if (m_quotaManager)
        m_quotaManager->removeUser(*this);
}

void Caches::retrieveOriginFromDirectory(const String& folderPath, WorkQueue& queue, WTF::CompletionHandler<void(Optional<WebCore::ClientOrigin>&&)>&& completionHandler)
{
    queue.dispatch([completionHandler = WTFMove(completionHandler), filename = cachesOriginFilename(folderPath)]() mutable {
        if (!FileSystem::fileExists(filename)) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(WTF::nullopt);
            });
            return;
        }

        auto channel = IOChannel::open(filename, IOChannel::Type::Read);
        channel->read(0, std::numeric_limits<size_t>::max(), nullptr, [completionHandler = WTFMove(completionHandler)](const Data& data, int error) mutable {
            ASSERT(RunLoop::isMain());
            if (error) {
                RELEASE_LOG_ERROR(CacheStorage, "Caches::retrieveOriginFromDirectory failed reading channel with error %d", error);
                completionHandler(WTF::nullopt);
                return;
            }
            completionHandler(readOrigin(data));
        });
    });
}

void Caches::storeOrigin(CompletionCallback&& completionHandler)
{
    WTF::Persistence::Encoder encoder;
    encoder << m_origin.topOrigin.protocol;
    encoder << m_origin.topOrigin.host;
    encoder << m_origin.topOrigin.port;
    encoder << m_origin.clientOrigin.protocol;
    encoder << m_origin.clientOrigin.host;
    encoder << m_origin.clientOrigin.port;
    m_engine->writeFile(cachesOriginFilename(m_rootPath), Data { encoder.buffer(), encoder.bufferSize() }, [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (Optional<Error>&& error) mutable {
        completionHandler(WTFMove(error));
    });
}

Optional<WebCore::ClientOrigin> Caches::readOrigin(const Data& data)
{
    // FIXME: We should be able to use modern decoders for persistent data.
    WebCore::SecurityOriginData topOrigin, clientOrigin;
    WTF::Persistence::Decoder decoder(data.data(), data.size());

    if (!decoder.decode(topOrigin.protocol))
        return WTF::nullopt;
    if (!decoder.decode(topOrigin.host))
        return WTF::nullopt;
    if (!decoder.decode(topOrigin.port))
        return WTF::nullopt;
    if (!decoder.decode(clientOrigin.protocol))
        return WTF::nullopt;
    if (!decoder.decode(clientOrigin.host))
        return WTF::nullopt;
    if (!decoder.decode(clientOrigin.port))
        return WTF::nullopt;
    return WebCore::ClientOrigin { WTFMove(topOrigin), WTFMove(clientOrigin) };
}

void Caches::initialize(WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (m_isInitialized) {
        callback(WTF::nullopt);
        return;
    }

    if (m_rootPath.isNull()) {
        makeDirty();
        m_isInitialized = true;
        callback(WTF::nullopt);
        return;
    }

    if (m_storage) {
        m_pendingInitializationCallbacks.append(WTFMove(callback));
        return;
    }

    auto storage = Storage::open(m_rootPath, Storage::Mode::AvoidRandomness);
    if (!storage) {
        RELEASE_LOG_ERROR(CacheStorage, "Caches::initialize failed opening storage");
        callback(Error::WriteDisk);
        return;
    }

    m_pendingInitializationCallbacks.append(WTFMove(callback));
    m_storage = storage.releaseNonNull();
    m_storage->writeWithoutWaiting();

    storeOrigin([this] (Optional<Error>&& error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::initialize failed storing origin with error %d", static_cast<int>(*error));

            auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
            for (auto& callback : pendingCallbacks)
                callback(Error::WriteDisk);

            m_storage = nullptr;
            return;
        }

        readCachesFromDisk([this](Expected<Vector<Cache>, Error>&& result) mutable {
            makeDirty();

            if (!result.has_value()) {
                RELEASE_LOG_ERROR(CacheStorage, "Caches::initialize failed reading caches from disk with error %d", static_cast<int>(result.error()));

                auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
                for (auto& callback : pendingCallbacks)
                    callback(result.error());

                m_storage = nullptr;
                return;
            }
            m_caches = WTFMove(result.value());

            initializeSize();
        });
    });
}

void Caches::initializeSize()
{
    if (!m_storage) {
        auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
        for (auto& callback : pendingCallbacks)
            callback(Error::Internal);
        return;
    }

    uint64_t size = 0;
    m_storage->traverse({ }, { }, [protectedThis = makeRef(*this), this, protectedStorage = makeRef(*m_storage), size](const auto* storage, const auto& information) mutable {
        if (!storage) {
            if (m_pendingInitializationCallbacks.isEmpty()) {
                // Caches was cleared so let's not get initialized.
                m_storage = nullptr;
                return;
            }
            m_size = size;
            m_isInitialized = true;
            auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
            for (auto& callback : pendingCallbacks)
                callback(WTF::nullopt);

            return;
        }
        auto decoded = Cache::decodeRecordHeader(*storage);
        if (decoded)
            size += decoded->size;
    });
}

void Caches::detach()
{
    m_engine = nullptr;
    m_rootPath = { };
    clearPendingWritingCachesToDiskCallbacks();
}

void Caches::clear(CompletionHandler<void()>&& completionHandler)
{
    if (m_isWritingCachesToDisk) {
        m_pendingWritingCachesToDiskCallbacks.append([this, completionHandler = WTFMove(completionHandler)] (auto&& error) mutable {
            this->clear(WTFMove(completionHandler));
        });
        return;
    }

    auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
    for (auto& callback : pendingCallbacks)
        callback(Error::Internal);

    if (m_engine)
        m_engine->removeFile(cachesListFilename(m_rootPath));
    if (m_storage) {
        m_storage->clear(String { }, -WallTime::infinity(), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
            ASSERT(RunLoop::isMain());
            protectedThis->clearMemoryRepresentation();
            completionHandler();
        });
        return;
    }
    clearMemoryRepresentation();
    clearPendingWritingCachesToDiskCallbacks();
    completionHandler();
}

void Caches::clearPendingWritingCachesToDiskCallbacks()
{
    auto pendingWritingCachesToDiskCallbacks = WTFMove(m_pendingWritingCachesToDiskCallbacks);
    for (auto& callback : pendingWritingCachesToDiskCallbacks)
        callback(Error::Internal);
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

    if (m_isWritingCachesToDisk) {
        m_pendingWritingCachesToDiskCallbacks.append([this, name, callback = WTFMove(callback)] (auto&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }
            this->open(name, WTFMove(callback));
        });
        return;
    }

    if (auto* cache = find(name)) {
        cache->open([cacheIdentifier = cache->identifier(), callback = WTFMove(callback)](Optional<Error>&& error) mutable {
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

    writeCachesToDisk([callback = WTFMove(callback), cacheIdentifier](Optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { cacheIdentifier, !!error });
    });
}

void Caches::remove(uint64_t identifier, CacheIdentifierCallback&& callback)
{
    ASSERT(m_isInitialized);
    ASSERT(m_engine);

    if (m_isWritingCachesToDisk) {
        m_pendingWritingCachesToDiskCallbacks.append([this, identifier, callback = WTFMove(callback)] (auto&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }
            this->remove(identifier, WTFMove(callback));
        });
        return;
    }

    auto position = m_caches.findMatching([&](const auto& item) { return item.identifier() == identifier; });

    if (position == notFound) {
        ASSERT(m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == identifier; }) != notFound);
        callback(CacheIdentifierOperationResult { 0, false });
        return;
    }

    makeDirty();

    m_removedCaches.append(WTFMove(m_caches[position]));
    m_caches.remove(position);

    writeCachesToDisk([callback = WTFMove(callback), identifier](Optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { identifier, !!error });
    });
}

bool Caches::hasActiveCache() const
{
    if (m_removedCaches.size())
        return true;
    return m_caches.findMatching([](const auto& item) { return item.isActive(); }) != notFound;
}

void Caches::dispose(Cache& cache)
{
    auto position = m_removedCaches.findMatching([&](const auto& item) { return item.identifier() == cache.identifier(); });
    if (position != notFound) {
        if (m_storage)
            m_storage->remove(cache.keys(), [] { });

        m_removedCaches.remove(position);
        return;
    }
    ASSERT(m_caches.findMatching([&](const auto& item) { return item.identifier() == cache.identifier(); }) != notFound);
    cache.clearMemoryRepresentation();

    if (!hasActiveCache())
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

static inline Expected<Vector<std::pair<String, String>>, Error> decodeCachesNames(const Data& data)
{
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
    if (!FileSystem::fileExists(filename)) {
        callback(Vector<Cache> { });
        return;
    }

    m_engine->readFile(filename, [protectedThis = makeRef(*this), this, callback = WTFMove(callback)](const Data& data, int error) mutable {
        if (!m_engine) {
            callback(Vector<Cache> { });
            return;
        }

        if (error) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::readCachesFromDisk failed reading caches from disk with error %d", error);
            callback(makeUnexpected(Error::ReadDisk));
            return;
        }

        auto result = decodeCachesNames(data);
        if (!result.has_value()) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::decodeCachesNames failed decoding caches with error %d", static_cast<int>(result.error()));
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
    ASSERT(!m_isWritingCachesToDisk);
    ASSERT(m_isInitialized);
    if (!shouldPersist()) {
        callback(WTF::nullopt);
        return;
    }

    ASSERT(m_engine);

    if (m_caches.isEmpty()) {
        m_engine->removeFile(cachesListFilename(m_rootPath));
        callback(WTF::nullopt);
        return;
    }

    m_isWritingCachesToDisk = true;
    m_engine->writeFile(cachesListFilename(m_rootPath), encodeCacheNames(m_caches), [this, protectedThis = makeRef(*this), callback = WTFMove(callback)](Optional<Error>&& error) mutable {
        m_isWritingCachesToDisk = false;
        if (error)
            RELEASE_LOG_ERROR(CacheStorage, "Caches::writeCachesToDisk failed writing caches to disk with error %d", static_cast<int>(*error));

        callback(WTFMove(error));
        while (!m_pendingWritingCachesToDiskCallbacks.isEmpty() && !m_isWritingCachesToDisk)
            m_pendingWritingCachesToDiskCallbacks.takeFirst()(WTF::nullopt);
    });
}

void Caches::readRecordsList(Cache& cache, NetworkCache::Storage::TraverseHandler&& callback)
{
    ASSERT(m_isInitialized);

    if (!m_storage) {
        callback(nullptr, { });
        return;
    }
    m_storage->traverse(cache.uniqueName(), { }, [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](const auto* storage, const auto& information) {
        callback(storage, information);
    });
}

void Caches::requestSpace(uint64_t spaceRequired, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (!m_quotaManager) {
        callback(Error::QuotaExceeded);
        return;
    }

    m_quotaManager->requestSpace(spaceRequired, [callback = WTFMove(callback)](auto decision) {
        switch (decision) {
        case StorageQuotaManager::Decision::Deny:
            callback(Error::QuotaExceeded);
            return;
        case StorageQuotaManager::Decision::Grant:
            callback({ });
        };
    });
}

void Caches::writeRecord(const Cache& cache, const RecordInformation& recordInformation, Record&& record, uint64_t previousRecordSize, CompletionCallback&& callback)
{
    ASSERT(m_isInitialized);

    ASSERT(m_size >= previousRecordSize);
    m_size += recordInformation.size;
    m_size -= previousRecordSize;

    if (!shouldPersist()) {
        m_volatileStorage.set(recordInformation.key, WTFMove(record));
        callback(WTF::nullopt);
        return;
    }

    m_storage->store(Cache::encode(recordInformation, record), { }, [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](int error) {
        if (error) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::writeRecord failed with error %d", error);
            callback(Error::WriteDisk);
            return;
        }
        callback(WTF::nullopt);
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

    m_storage->retrieve(key, 4, [protectedStorage = makeRef(*m_storage), callback = WTFMove(callback)](std::unique_ptr<Storage::Record> storage, const Storage::Timings&) mutable {
        if (!storage) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::readRecord failed reading record from disk");
            callback(makeUnexpected(Error::ReadDisk));
            return false;
        }

        auto record = Cache::decode(*storage);
        if (!record) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::readRecord failed decoding record from disk");
            callback(makeUnexpected(Error::ReadDisk));
            return false;
        }

        callback(WTFMove(record.value()));
        return true;
    });
}

void Caches::removeRecord(const RecordInformation& record)
{
    ASSERT(m_isInitialized);

    ASSERT(m_size >= record.size);
    m_size -= record.size;
    removeCacheEntry(record.key);
}

void Caches::removeCacheEntry(const NetworkCache::Key& key)
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
    if (!m_isInitialized) {
        ASSERT(!m_storage || !hasActiveCache() || !m_pendingInitializationCallbacks.isEmpty());
        // m_storage might not be null in case Caches is being initialized. This is fine as nullify it below is a memory optimization.
        m_caches.clear();
        return;
    }

    makeDirty();
    m_caches.clear();
    m_isInitialized = false;

    // Clear storages as a memory optimization.
    m_storage = nullptr;
    m_volatileStorage.clear();
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

void Caches::cacheInfos(uint64_t updateCounter, CacheInfosCallback&& callback)
{
    if (m_isWritingCachesToDisk) {
        m_pendingWritingCachesToDiskCallbacks.append([this, updateCounter, callback = WTFMove(callback)] (auto&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }
            this->cacheInfos(updateCounter, WTFMove(callback));
        });
        return;
    }

    Vector<CacheInfo> cacheInfos;
    if (isDirty(updateCounter)) {
        cacheInfos.reserveInitialCapacity(m_caches.size());
        for (auto& cache : m_caches)
            cacheInfos.uncheckedAppend(CacheInfo { cache.identifier(), cache.name() });
    }
    callback(CacheInfos { WTFMove(cacheInfos), m_updateCounter });
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

uint64_t Caches::storageSize() const
{
    ASSERT(m_isInitialized);
    if (!shouldPersist())
        return 0;
    return m_storage->approximateSize();
}

} // namespace CacheStorage

} // namespace WebKit
