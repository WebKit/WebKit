/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CacheStorageManager.h"

#include "CacheStorageCache.h"
#include "CacheStorageRegistry.h"
#include "StorageUtilities.h"
#include <WebCore/ClientOrigin.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

static constexpr auto cachesListFileName = "cacheslist"_s;
static constexpr auto sizeFileName = "estimatedsize"_s;
static constexpr auto originFileName = "origin"_s;
static constexpr auto originSaltFileName = "salt"_s;

static uint64_t nextUpdateNumber()
{
    static std::atomic<uint64_t> currentUpdateNumber;
    return ++currentUpdateNumber;
}

static std::optional<Vector<std::pair<String, String>>> readCachesList(const String& cachesListDirectoryPath)
{
    Vector<std::pair<String, String>> result;
    if (cachesListDirectoryPath.isEmpty())
        return result;

    auto cachesListFilePath = FileSystem::pathByAppendingComponent(cachesListDirectoryPath, cachesListFileName);
    if (!FileSystem::fileExists(cachesListFilePath))
        return result;

    auto cachesListHandle = FileSystem::openFile(cachesListFilePath, FileSystem::FileOpenMode::ReadWrite);
    if (!FileSystem::isHandleValid(cachesListHandle))
        return std::nullopt;
    auto closeFileOnExit = makeScopeExit([&cachesListHandle]() mutable {
        FileSystem::closeFile(cachesListHandle);
    });

    auto cachesList = FileSystem::readEntireFile(cachesListHandle);
    if (!cachesList)
        return std::nullopt;

    WTF::Persistence::Decoder decoder({ cachesList->data(), cachesList->size() });
    std::optional<uint64_t> count;
    decoder >> count;
    if (!count)
        return std::nullopt;

    result.reserveInitialCapacity(*count);
    for (size_t index = 0; index < *count; ++index) {
        std::optional<String> name;
        decoder >> name;
        if (!name)
            return std::nullopt;

        std::optional<String> uniqueName;
        decoder >> uniqueName;
        if (!uniqueName)
            return std::nullopt;

        result.uncheckedAppend({ WTFMove(*name), WTFMove(*uniqueName) });
    }

    return result;
}

static bool writeCachesList(const String& cachesListDirectoryPath, const Vector<std::unique_ptr<CacheStorageCache>>& caches, std::optional<size_t> skippedCachesIndex = std::nullopt)
{
    if (cachesListDirectoryPath.isEmpty())
        return true;

    auto cachesListFilePath = FileSystem::pathByAppendingComponent(cachesListDirectoryPath, cachesListFileName);
    if (caches.isEmpty()) {
        FileSystem::deleteFile(cachesListFilePath);
        return true;
    }

    FileSystem::makeAllDirectories(FileSystem::parentPath(cachesListFilePath));
    auto cachesListHandle = FileSystem::openFile(cachesListFilePath, FileSystem::FileOpenMode::ReadWrite);
    if (!FileSystem::isHandleValid(cachesListHandle))
        return false;
    auto closeFileOnExit = makeScopeExit([&cachesListHandle]() mutable {
        FileSystem::closeFile(cachesListHandle);
    });

    if (!FileSystem::truncateFile(cachesListHandle, 0))
        return false;

    WTF::Persistence::Encoder encoder;
    uint64_t count = caches.size();
    if (skippedCachesIndex && *skippedCachesIndex < caches.size())
        --count;
    encoder << count;
    for (size_t index = 0; index < caches.size(); ++index) {
        if (skippedCachesIndex && index == *skippedCachesIndex)
            continue;
        encoder << caches[index]->name();
        encoder << caches[index]->uniqueName();
    }
    FileSystem::writeToFile(cachesListHandle, encoder.buffer(), encoder.bufferSize());
    return true;
}

static std::optional<uint64_t> readSizeFile(const String& sizeDirectoryPath)
{
    if (sizeDirectoryPath.isEmpty())
        return std::nullopt;

    auto sizeFilePath = FileSystem::pathByAppendingComponent(sizeDirectoryPath, sizeFileName);
    if (!FileSystem::fileExists(sizeFilePath))
        return std::nullopt;

    auto buffer = FileSystem::readEntireFile(sizeFilePath);
    if (!buffer)
        return std::nullopt;

    return parseInteger<uint64_t>({ buffer->data(), static_cast<unsigned>(buffer->size()) });
}

static bool writeSizeFile(const String& sizeDirectoryPath, uint64_t size)
{
    if (sizeDirectoryPath.isEmpty())
        return true;

    auto sizeFilePath = FileSystem::pathByAppendingComponent(sizeDirectoryPath, sizeFileName);
    auto value = String::number(size).utf8();
    return FileSystem::overwriteEntireFile(sizeFilePath, Span { reinterpret_cast<uint8_t*>(const_cast<char*>(value.data())), value.length() }) != -1;
}

String CacheStorageManager::cacheStorageOriginDirectory(const String& rootDirectory, const WebCore::ClientOrigin& origin)
{
    if (rootDirectory.isEmpty())
        return emptyString();

    auto saltFilePath = FileSystem::pathByAppendingComponent(rootDirectory, "salt"_s);
    auto salt = valueOrDefault(FileSystem::readOrMakeSalt(saltFilePath));
    NetworkCache::Key key(origin.topOrigin.toString(), origin.clientOrigin.toString(), { }, { }, salt);
    return FileSystem::pathByAppendingComponent(rootDirectory, key.hashAsString());
}

void CacheStorageManager::copySaltFileToOriginDirectory(const String& rootDirectory, const String& originDirectory)
{
    if (!FileSystem::fileExists(originDirectory))
        return;

    auto targetFilePath = FileSystem::pathByAppendingComponent(originDirectory, "salt"_s);
    if (FileSystem::fileExists(targetFilePath))
        return;

    auto sourceFilePath = FileSystem::pathByAppendingComponent(rootDirectory, "salt"_s);
    FileSystem::hardLinkOrCopyFile(sourceFilePath, targetFilePath);
}

HashSet<WebCore::ClientOrigin> CacheStorageManager::originsOfCacheStorageData(const String& rootDirectory)
{
    HashSet<WebCore::ClientOrigin> result;
    for (auto& originName : FileSystem::listDirectory(rootDirectory)) {
        auto originFile = FileSystem::pathByAppendingComponents(rootDirectory, { originName, originFileName });
        if (auto origin = readOriginFromFile(originFile))
            result.add(*origin);
    }

    return result;
}

static uint64_t getDirectorySize(const String& originDirectory)
{
    uint64_t directorySize = 0;
    Deque<String> paths;
    paths.append(originDirectory);
    while (!paths.isEmpty()) {
        auto path = paths.takeFirst();
        if (FileSystem::fileType(path) == FileSystem::FileType::Directory) {
            auto fileNames = FileSystem::listDirectory(path);
            for (auto& fileName : fileNames) {
                // Files in /Blobs directory are hard link.
                if (fileName == "Blobs"_s)
                    continue;
                paths.append(FileSystem::pathByAppendingComponent(path, fileName));
            }
            continue;
        }
        directorySize += FileSystem::fileSize(path).value_or(0);
    }
    return directorySize;
}

uint64_t CacheStorageManager::cacheStorageSize(const String& originDirectory)
{
    if (auto sizeFromFile = readSizeFile(originDirectory))
        return *sizeFromFile;

    return getDirectorySize(originDirectory);
}

bool CacheStorageManager::hasCacheList(const String& cacheStorageDirectory)
{
    return FileSystem::fileExists(FileSystem::pathByAppendingComponent(cacheStorageDirectory, cachesListFileName));
}

void CacheStorageManager::makeDirty()
{
    m_updateCounter = nextUpdateNumber();
}

String CacheStorageManager::saltFilePath() const
{
    return FileSystem::pathByAppendingComponent(m_path, originSaltFileName);
}

CacheStorageManager::CacheStorageManager(const String& path, CacheStorageRegistry& registry, const std::optional<WebCore::ClientOrigin>& origin, QuotaCheckFunction&& quotaCheckFunction, Ref<WorkQueue>&& queue)
    : m_updateCounter(nextUpdateNumber())
    , m_path(path)
    , m_salt(valueOrDefault(FileSystem::readOrMakeSalt(saltFilePath())))
    , m_registry(registry)
    , m_quotaCheckFunction(WTFMove(quotaCheckFunction))
    , m_queue(WTFMove(queue))
{
    if (m_path.isEmpty() || !origin)
        return;

    auto originFile = FileSystem::pathByAppendingComponent(m_path, originFileName);
    writeOriginToFile(originFile, *origin);
}

CacheStorageManager::~CacheStorageManager()
{
    for (auto& request : m_pendingSpaceRequests)
        request.second(false);

    for (auto& cache : m_caches)
        m_registry.unregisterCache(cache->identifier());

    for (auto& identifier : m_removedCaches.keys())
        m_registry.unregisterCache(identifier);
}

bool CacheStorageManager::initializeCaches()
{
    if (m_isInitialized)
        return true;

    auto cachesList = readCachesList(m_path);
    if (!cachesList)
        return false;

    m_isInitialized = true;
    for (auto& [name, uniqueName] : *cachesList) {
        auto cache = makeUnique<CacheStorageCache>(*this, name, uniqueName, m_path, m_queue.copyRef());
        m_registry.registerCache(cache->identifier(), *cache.get());
        m_caches.append(WTFMove(cache));
    }

    return true;
}

void CacheStorageManager::openCache(const String& name, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    if (!initializeCaches())
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::ReadDisk));

    auto index = m_caches.findIf([&](auto& cache) {
        return cache->name() == name;
    });
    if (index != notFound)
        return m_caches[index]->open(WTFMove(callback));

    m_caches.append(makeUnique<CacheStorageCache>(*this, name, createVersion4UUIDString(), m_path, m_queue.copyRef()));
    bool written = writeCachesList(m_path, m_caches);
    if (!written) {
        m_caches.removeLast();
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));
    }

    makeDirty();
    auto& cache = m_caches.last();
    m_registry.registerCache(cache->identifier(), *cache);
    cache->open(WTFMove(callback));
}

void CacheStorageManager::removeCache(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)
{
    auto index = m_caches.findIf([&](auto& cache) {
        return cache->identifier() == cacheIdentifier;
    });
    if (index == notFound)
        return callback(false);

    if (!writeCachesList(m_path, m_caches, index))
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

    makeDirty();
    m_removedCaches.set(cacheIdentifier, WTFMove(m_caches[index]));
    m_caches.remove(index);
    return callback(true);
}

void CacheStorageManager::allCaches(uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    if (!initializeCaches())
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::ReadDisk));

    auto cacheInfos = WTF::map(m_caches, [](const auto& cache) {
        return WebCore::DOMCacheEngine::CacheInfo { cache->identifier(), cache->name() };
    });
    auto callbackAggregator = CallbackAggregator::create([callback = WTFMove(callback), cacheInfos = WTFMove(cacheInfos), updateCounter = m_updateCounter]() mutable {
        callback(WebCore::DOMCacheEngine::CacheInfos { WTFMove(cacheInfos), updateCounter });
    });
    for (auto& cache : m_caches)
        cache->open([callbackAggregator](auto) { });
}

void CacheStorageManager::initializeCacheSize(CacheStorageCache& cache)
{
    cache.getSize([this, weakThis = WeakPtr { *this }](auto size) mutable {
        if (!weakThis)
            return;

        m_pendingSize.first += size;
        if (!--m_pendingSize.second)
            finishInitializingSize();
    });
}

void CacheStorageManager::finishInitializingSize()
{
    m_size = std::exchange(m_pendingSize.first, 0);
    writeSizeFile(m_path, *m_size);

    while (!m_pendingSpaceRequests.isEmpty()) {
        auto [size, callback] = m_pendingSpaceRequests.takeFirst();
        m_quotaCheckFunction(size, WTFMove(callback));
    }
}

void CacheStorageManager::requestSpaceAfterInitializingSize(uint64_t spaceRequested, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_size)
        return m_quotaCheckFunction(spaceRequested, WTFMove(completionHandler));

    m_pendingSpaceRequests.append({ spaceRequested, WTFMove(completionHandler) });
    if (m_pendingSpaceRequests.size() > 1)
        return;

    m_pendingSize = { 0, m_caches.size() + m_removedCaches.size() };
    for (auto& cache : m_caches)
        initializeCacheSize(*cache);

    for (auto& cache : m_removedCaches.values())
        initializeCacheSize(*cache);
}

void CacheStorageManager::requestSpace(uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    requestSpaceAfterInitializingSize(size, WTFMove(completionHandler));
}

void CacheStorageManager::sizeIncreased(uint64_t amount)
{
    if (!m_size || !amount)
        return;

    m_size = *m_size + amount;
    writeSizeFile(m_path, *m_size);
}

void CacheStorageManager::sizeDecreased(uint64_t amount)
{
    if (!m_size || !amount)
        return;

    m_size = *m_size - amount;
    writeSizeFile(m_path, *m_size);
}

void CacheStorageManager::reference(IPC::Connection::UniqueID connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto& references = m_cacheRefConnections.ensure(cacheIdentifier, []() {
        return Vector<IPC::Connection::UniqueID> { };
    }).iterator->value;
    references.append(connection);
}

void CacheStorageManager::dereference(IPC::Connection::UniqueID connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto iter = m_cacheRefConnections.find(cacheIdentifier);
    if (iter == m_cacheRefConnections.end())
        return;

    auto& refConnections = iter->value;
    auto index = refConnections.findIf([&](auto& refConnection) {
        return refConnection == connection;
    });
    if (index == notFound)
        return;

    refConnections.remove(index);
    if (!refConnections.isEmpty())
        return;

    removeUnusedCache(cacheIdentifier);
}

void CacheStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    HashSet<WebCore::DOMCacheIdentifier> unusedCacheIdentifers;
    for (auto& [identifier, refConnections] : m_cacheRefConnections) {
        refConnections.removeAllMatching([&](auto refConnection) {
            return refConnection == connection;
        });
        if (refConnections.isEmpty()) {
            removeUnusedCache(identifier);
            unusedCacheIdentifers.add(identifier);
        }
    }

    for (auto& identifier : unusedCacheIdentifers)
        m_cacheRefConnections.remove(identifier);
}

void CacheStorageManager::removeUnusedCache(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto cache = m_removedCaches.take(cacheIdentifier);
    if (cache) {
        cache->removeAllRecords();
        return;
    }

    for (auto& cache : m_caches) {
        if (cache->identifier() == cacheIdentifier) {
            cache->close();
            return;
        }
    }
}

bool CacheStorageManager::hasDataInMemory()
{
    // Cache data is stored on disk.
    if (!m_path.isEmpty())
        return false;

    return !m_caches.isEmpty() || !m_removedCaches.isEmpty();
}

bool CacheStorageManager::isActive()
{
    return !m_cacheRefConnections.isEmpty();
}

String CacheStorageManager::representationString()
{
    StringBuilder builder;
    builder.append("{ \"persistent\": [");

    bool isFirst = true;
    for (auto& cache : m_caches) {
        if (!isFirst)
            builder.append(", ");
        isFirst = false;
        builder.append("\"");
        builder.append(cache->name());
        builder.append("\"");
    }

    builder.append("], \"removed\": [");
    isFirst = true;
    for (auto& cache : m_removedCaches.values()) {
        if (!isFirst)
            builder.append(", ");
        isFirst = false;
        builder.append("\"");
        builder.append(cache->name());
        builder.append("\"");
    }
    builder.append("]}\n");
    return builder.toString();
}

} // namespace WebKit
