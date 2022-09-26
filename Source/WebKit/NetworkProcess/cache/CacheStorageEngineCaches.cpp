/*
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
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

String Caches::cachesSizeFilename(const String& cachesRootsPath)
{
    return FileSystem::pathByAppendingComponent(cachesRootsPath, "estimatedsize"_s);
}

Ref<Caches> Caches::create(Engine& engine, WebCore::ClientOrigin&& origin, String&& rootPath)
{
    return adoptRef(*new Caches { engine, WTFMove(origin), WTFMove(rootPath) });
}

Caches::Caches(Engine& engine, WebCore::ClientOrigin&& origin, String&& rootPath)
    : m_engine(&engine)
    , m_origin(WTFMove(origin))
    , m_rootPath(WTFMove(rootPath))
{
}

Caches::~Caches()
{
    ASSERT(m_pendingWritingCachesToDiskCallbacks.isEmpty());
}

void Caches::retrieveOriginFromDirectory(const String& folderPath, WorkQueue& queue, WTF::CompletionHandler<void(std::optional<WebCore::ClientOrigin>&&)>&& completionHandler)
{
    queue.dispatch([completionHandler = WTFMove(completionHandler), filename = cachesOriginFilename(folderPath)]() mutable {
        if (!FileSystem::fileExists(filename)) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(std::nullopt);
            });
            return;
        }

        auto channel = IOChannel::open(WTFMove(filename), IOChannel::Type::Read);
        channel->read(0, std::numeric_limits<size_t>::max(), WorkQueue::main(), [completionHandler = WTFMove(completionHandler)](const Data& data, int error) mutable {
            ASSERT(RunLoop::isMain());
            if (error) {
                RELEASE_LOG_ERROR(CacheStorage, "Caches::retrieveOriginFromDirectory failed reading channel with error %d", error);
                completionHandler(std::nullopt);
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
    m_engine->writeFile(cachesOriginFilename(m_rootPath), Data { encoder.buffer(), encoder.bufferSize() }, [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (std::optional<Error>&& error) mutable {
        completionHandler(WTFMove(error));
    });
}

std::optional<WebCore::ClientOrigin> Caches::readOrigin(const Data& data)
{
    WTF::Persistence::Decoder decoder(data.span());

    std::optional<WebCore::SecurityOriginData> topOrigin;
    decoder >> topOrigin;
    if (!topOrigin)
        return std::nullopt;

    std::optional<WebCore::SecurityOriginData> clientOrigin;
    decoder >> clientOrigin;
    if (!clientOrigin)
        return std::nullopt;

    return {{
        WTFMove(*topOrigin),
        WTFMove(*clientOrigin)
    }};
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

    size_t capacity = std::numeric_limits<size_t>::max(); // We use a per-origin quota instead of a global capacity.
    auto storage = Storage::open(m_rootPath, Storage::Mode::AvoidRandomness, capacity);
    if (!storage) {
        RELEASE_LOG_ERROR(CacheStorage, "Caches::initialize failed opening storage");
        callback(Error::WriteDisk);
        return;
    }

    m_pendingInitializationCallbacks.append(WTFMove(callback));
    m_storage = storage.releaseNonNull();
    m_storage->writeWithoutWaiting();

    storeOrigin([this] (std::optional<Error>&& error) mutable {
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

void Caches::updateSizeFile(CompletionHandler<void()>&& completionHandler)
{
    if (!m_engine) {
        completionHandler();
        return;
    }

    m_engine->writeSizeFile(cachesSizeFilename(m_rootPath), m_size, WTFMove(completionHandler));
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
    m_storage->traverse({ }, { }, [protectedThis = Ref { *this }, this, protectedStorage = Ref { *m_storage }, size](const auto* storage, const auto& information) mutable {
        if (!storage) {
            if (m_pendingInitializationCallbacks.isEmpty()) {
                // Caches was cleared so let's not get initialized.
                m_storage = nullptr;
                return;
            }
            m_size = size;
            updateSizeFile([this, protectedThis = WTFMove(protectedThis)]() mutable {
                m_isInitialized = true;
                auto pendingCallbacks = WTFMove(m_pendingInitializationCallbacks);
                for (auto& callback : pendingCallbacks)
                    callback(std::nullopt);
            });
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
        String anyType;
        m_storage->clear(WTFMove(anyType), -WallTime::infinity(), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
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
    auto position = m_caches.findIf([&](const auto& item) { return item.name() == name; });
    return (position != notFound) ? &m_caches[position] : nullptr;
}

Cache* Caches::find(WebCore::DOMCacheIdentifier identifier)
{
    auto position = m_caches.findIf([&](const auto& item) { return item.identifier() == identifier; });
    if (position != notFound)
        return &m_caches[position];

    position = m_removedCaches.findIf([&](const auto& item) { return item.identifier() == identifier; });
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

    auto cacheIdentifier = WebCore::DOMCacheIdentifier::generate();
    m_caches.append(Cache { *this, cacheIdentifier, Cache::State::Open, String { name }, createVersion4UUIDString() });

    writeCachesToDisk([callback = WTFMove(callback), cacheIdentifier](std::optional<Error>&& error) mutable {
        callback(CacheIdentifierOperationResult { cacheIdentifier, !!error });
    });
}

void Caches::remove(WebCore::DOMCacheIdentifier identifier, RemoveCacheIdentifierCallback&& callback)
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

    auto position = m_caches.findIf([&](const auto& item) { return item.identifier() == identifier; });

    if (position == notFound) {
        ASSERT(m_removedCaches.findIf([&](const auto& item) { return item.identifier() == identifier; }) != notFound);
        callback(false);
        return;
    }

    makeDirty();

    m_removedCaches.append(WTFMove(m_caches[position]));
    m_caches.remove(position);

    writeCachesToDisk([callback = WTFMove(callback)](std::optional<Error>&&) mutable {
        callback(true);
    });
}

bool Caches::hasActiveCache() const
{
    if (m_removedCaches.size())
        return true;
    return m_caches.findIf([](const auto& item) { return item.isActive(); }) != notFound;
}

void Caches::dispose(Cache& cache)
{
    auto position = m_removedCaches.findIf([&](const auto& item) { return item.identifier() == cache.identifier(); });
    if (position != notFound) {
        if (m_storage)
            m_storage->remove(cache.keys(), [] { });

        m_removedCaches.remove(position);
        return;
    }
    ASSERT(m_caches.findIf([&](const auto& item) { return item.identifier() == cache.identifier(); }) != notFound);

    // We cannot clear the memory representation in ephemeral sessions since we would loose all data.
    if (!shouldPersist())
        return;

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
    WTF::Persistence::Decoder decoder(data.span());
    std::optional<uint64_t> count;
    decoder >> count;
    if (!count)
        return makeUnexpected(Error::ReadDisk);

    Vector<std::pair<String, String>> names;
    if (!names.tryReserveCapacity(*count))
        return makeUnexpected(Error::ReadDisk);

    for (size_t index = 0; index < *count; ++index) {
        std::optional<String> name;
        decoder >> name;
        if (!name)
            return makeUnexpected(Error::ReadDisk);
        std::optional<String> uniqueName;
        decoder >> uniqueName;
        if (!uniqueName)
            return makeUnexpected(Error::ReadDisk);

        names.uncheckedAppend({ WTFMove(*name), WTFMove(*uniqueName) });
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

    m_engine->readFile(WTFMove(filename), [protectedThis = Ref { *this }, this, callback = WTFMove(callback)](const Data& data, int error) mutable {
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
            return Cache { *this, WebCore::DOMCacheIdentifier::generate(), Cache::State::Uninitialized, WTFMove(pair.first), WTFMove(pair.second) };
        }));
    });
}

void Caches::writeCachesToDisk(CompletionCallback&& callback)
{
    ASSERT(!m_isWritingCachesToDisk);
    ASSERT(m_isInitialized);
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

    m_isWritingCachesToDisk = true;
    m_engine->writeFile(cachesListFilename(m_rootPath), encodeCacheNames(m_caches), [this, protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
        m_isWritingCachesToDisk = false;
        if (error)
            RELEASE_LOG_ERROR(CacheStorage, "Caches::writeCachesToDisk failed writing caches to disk with error %d", static_cast<int>(*error));

        callback(WTFMove(error));
        while (!m_pendingWritingCachesToDiskCallbacks.isEmpty() && !m_isWritingCachesToDisk)
            m_pendingWritingCachesToDiskCallbacks.takeFirst()(std::nullopt);
    });
}

void Caches::readRecordsList(Cache& cache, NetworkCache::Storage::TraverseHandler&& callback)
{
    ASSERT(m_isInitialized);

    if (!m_storage) {
        callback(nullptr, { });
        return;
    }
    m_storage->traverse(cache.uniqueName(), { }, [protectedStorage = Ref { *m_storage }, callback = WTFMove(callback)](const auto* storage, const auto& information) {
        callback(storage, information);
    });
}

void Caches::requestSpace(uint64_t spaceRequired, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (!m_engine) {
        callback(Error::QuotaExceeded);
        return;
    }

    m_engine->requestSpace(m_origin, spaceRequired, [callback = WTFMove(callback)](bool granted) mutable {
        if (!granted)
            return callback(Error::QuotaExceeded);
        callback({ });
    });
}

void Caches::writeRecord(const Cache& cache, const RecordInformation& recordInformation, Record&& record, uint64_t previousRecordSize, CompletionCallback&& callback)
{
    ASSERT(m_isInitialized);

    ASSERT(m_size >= previousRecordSize);
    m_size += recordInformation.size;
    m_size -= previousRecordSize;

    if (!shouldPersist()) {
        m_volatileStorage.set(recordInformation.key, makeUnique<Record>(WTFMove(record)));
        callback(std::nullopt);
        return;
    }

    if (!m_storage)
        return callback(std::nullopt);

    m_storage->store(Cache::encode(recordInformation, record), { }, [this, protectedThis = Ref { *this }, protectedStorage = Ref { *m_storage }, callback = WTFMove(callback)](int error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(CacheStorage, "Caches::writeRecord failed with error %d", error);
            callback(Error::WriteDisk);
            return;
        }
        updateSizeFile([callback = WTFMove(callback)]() mutable {
            callback(std::nullopt);
        });
    });
}

void Caches::readRecord(const NetworkCache::Key& key, WTF::Function<void(Expected<Record, Error>&&)>&& callback)
{
    ASSERT(m_isInitialized);

    if (!shouldPersist()) {
        if (auto* record = m_volatileStorage.get(key))
            return callback(record->copy());
        return callback(makeUnexpected(Error::Internal));
    }

    if (!m_storage)
        return callback(makeUnexpected(Error::Internal));

    m_storage->retrieve(key, 4, [protectedStorage = Ref { *m_storage }, callback = WTFMove(callback)](std::unique_ptr<Storage::Record> storage, const Storage::Timings&) mutable {
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
    updateSizeFile([] { });

    removeCacheEntry(record.key);
}

void Caches::removeCacheEntry(const NetworkCache::Key& key)
{
    ASSERT(m_isInitialized);

    if (!shouldPersist()) {
        m_volatileStorage.remove(key);
        return;
    }
    if (m_storage)
        m_storage->remove(key);
}

void Caches::clearMemoryRepresentation()
{
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
        cacheInfos = m_caches.map([](auto& cache) {
            return CacheInfo { cache.identifier(), cache.name() };
        });
    }
    callback(CacheInfos { WTFMove(cacheInfos), m_updateCounter });
}

void Caches::appendRepresentation(StringBuilder& builder) const
{
    ASSERT(m_pendingInitializationCallbacks.isEmpty());
    ASSERT(m_pendingWritingCachesToDiskCallbacks.isEmpty());

    builder.append("{ \"persistent\": [");

    bool isFirst = true;
    for (auto& cache : m_caches) {
        ASSERT(!cache.hasPendingOpeningCallbacks());
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
