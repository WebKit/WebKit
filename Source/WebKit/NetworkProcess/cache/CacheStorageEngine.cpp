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
#include "NetworkCacheFileSystem.h"
#include "NetworkCacheIOChannel.h"
#include "NetworkProcess.h"
#include "WebsiteDataType.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/RetrieveRecordsOptions.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

namespace CacheStorage {

using namespace WebCore::DOMCacheEngine;
using namespace NetworkCache;

static Lock globalSizeFileLock;

String Engine::cachesRootPath(const WebCore::ClientOrigin& origin)
{
    if (!shouldPersist() || !m_salt)
        return { };

    Key key(origin.topOrigin.toString(), origin.clientOrigin.toString(), { }, { }, salt());
    return FileSystem::pathByAppendingComponent(rootPath(), key.hashAsString());
}

Engine::~Engine()
{
    for (auto& caches : m_caches.values())
        caches->detach();

    auto pendingClearCallbacks = WTFMove(m_pendingClearCallbacks);
    for (auto& callback : pendingClearCallbacks)
        callback(Error::Internal);

    auto initializationCallbacks = WTFMove(m_initializationCallbacks);
    for (auto& callback : initializationCallbacks)
        callback(Error::Internal);

    auto writeCallbacks = WTFMove(m_pendingWriteCallbacks);
    for (auto& callback : writeCallbacks.values())
        callback(Error::Internal);

    auto readCallbacks = WTFMove(m_pendingReadCallbacks);
    for (auto& callback : readCallbacks.values())
        callback(Data { }, 1);
}

void Engine::from(NetworkProcess& networkProcess, PAL::SessionID sessionID, Function<void(Engine&)>&& callback)
{
    if (auto* engine = networkProcess.findCacheEngine(sessionID)) {
        callback(*engine);
        return;
    }

    networkProcess.cacheStorageRootPath(sessionID, [networkProcess = makeRef(networkProcess), sessionID, callback = WTFMove(callback)] (auto&& rootPath) mutable {
        callback(networkProcess->ensureCacheEngine(sessionID, [&] {
            return adoptRef(*new Engine { sessionID, networkProcess.get(), WTFMove(rootPath) });
        }));
    });
}

void Engine::destroyEngine(NetworkProcess& networkProcess, PAL::SessionID sessionID)
{
#if !USE(SOUP) && !USE(CURL)
    // cURL and Soup based ports destroy the default session right before the process exits to avoid leaking
    // network resources like the cookies database.
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());
#endif

    networkProcess.removeCacheEngine(sessionID);
}

void Engine::fetchEntries(NetworkProcess& networkProcess, PAL::SessionID sessionID, bool shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    from(networkProcess, sessionID, [shouldComputeSize, completionHandler = WTFMove(completionHandler)] (auto& engine) mutable {
        engine.fetchEntries(shouldComputeSize, WTFMove(completionHandler));
    });
}

void Engine::open(NetworkProcess& networkProcess, PAL::SessionID sessionID, WebCore::ClientOrigin&& origin, String&& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    from(networkProcess, sessionID, [origin = WTFMove(origin), cacheName = WTFMove(cacheName), callback = WTFMove(callback)](auto& engine) mutable {
        engine.open(origin, cacheName, WTFMove(callback));
    });
}

void Engine::remove(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    from(networkProcess, sessionID, [cacheIdentifier, callback = WTFMove(callback)](auto& engine) mutable {
        engine.remove(cacheIdentifier, WTFMove(callback));
    });
}

void Engine::retrieveCaches(NetworkProcess& networkProcess, PAL::SessionID sessionID, WebCore::ClientOrigin&& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    from(networkProcess, sessionID, [origin = WTFMove(origin), updateCounter, callback = WTFMove(callback)](auto& engine) mutable {
        engine.retrieveCaches(origin, updateCounter, WTFMove(callback));
    });
}


void Engine::retrieveRecords(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::RecordsCallback&& callback)
{
    from(networkProcess, sessionID, [cacheIdentifier, options = WTFMove(options), callback = WTFMove(callback)](auto& engine) mutable {
        engine.retrieveRecords(cacheIdentifier, WTFMove(options), WTFMove(callback));
    });
}

void Engine::putRecords(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    from(networkProcess, sessionID, [cacheIdentifier, records = WTFMove(records), callback = WTFMove(callback)](auto& engine) mutable {
        engine.putRecords(cacheIdentifier, WTFMove(records), WTFMove(callback));
    });
}

void Engine::deleteMatchingRecords(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    from(networkProcess, sessionID, [cacheIdentifier, request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)](auto& engine) mutable {
        engine.deleteMatchingRecords(cacheIdentifier, WTFMove(request), WTFMove(options), WTFMove(callback));
    });
}

void Engine::lock(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier)
{
    from(networkProcess, sessionID, [cacheIdentifier](auto& engine) mutable {
        engine.lock(cacheIdentifier);
    });
}

void Engine::unlock(NetworkProcess& networkProcess, PAL::SessionID sessionID, uint64_t cacheIdentifier)
{
    from(networkProcess, sessionID, [cacheIdentifier](auto& engine) mutable {
        engine.unlock(cacheIdentifier);
    });
}

void Engine::clearMemoryRepresentation(NetworkProcess& networkProcess, PAL::SessionID sessionID, WebCore::ClientOrigin&& origin, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    from(networkProcess, sessionID, [origin = WTFMove(origin), callback = WTFMove(callback)](auto& engine) mutable {
        engine.clearMemoryRepresentation(origin, WTFMove(callback));
    });
}

void Engine::representation(NetworkProcess& networkProcess, PAL::SessionID sessionID, CompletionHandler<void(String&&)>&& callback)
{
    from(networkProcess, sessionID, [callback = WTFMove(callback)](auto& engine) mutable {
        callback(engine.representation());
    });
}

void Engine::clearAllCaches(NetworkProcess& networkProcess, PAL::SessionID sessionID, CompletionHandler<void()>&& completionHandler)
{
    from(networkProcess, sessionID, [completionHandler = WTFMove(completionHandler)](auto& engine) mutable {
        engine.clearAllCaches(WTFMove(completionHandler));
    });
}

void Engine::clearCachesForOrigin(NetworkProcess& networkProcess, PAL::SessionID sessionID, WebCore::SecurityOriginData&& originData, CompletionHandler<void()>&& completionHandler)
{
    from(networkProcess, sessionID, [originData = WTFMove(originData), completionHandler = WTFMove(completionHandler)](auto& engine) mutable {
        engine.clearCachesForOrigin(originData, WTFMove(completionHandler));
    });
}

static uint64_t getDirectorySize(const String& directoryPath)
{
    ASSERT(!isMainRunLoop());

    uint64_t directorySize = 0;
    Deque<String> paths;
    paths.append(directoryPath);
    while (!paths.isEmpty()) {
        auto path = paths.takeFirst();
        if (FileSystem::fileType(path) == FileSystem::FileType::Directory) {
            auto fileNames = FileSystem::listDirectory(path);
            for (auto& fileName : fileNames) {
                // Files in /Blobs directory are hard link.
                if (fileName == "Blobs")
                    continue;
                paths.append(FileSystem::pathByAppendingComponent(path, fileName));
            }
            continue;
        }
        directorySize += FileSystem::fileSize(path).value_or(0);
    }
    return directorySize;
}

uint64_t Engine::diskUsage(const String& rootPath, const WebCore::ClientOrigin& origin)
{
    ASSERT(!isMainRunLoop());

    if (rootPath.isEmpty())
        return 0;

    String saltPath = FileSystem::pathByAppendingComponent(rootPath, "salt"_s);
    auto salt = FileSystem::readOrMakeSalt(saltPath);
    if (!salt)
        return 0;

    Key key(origin.topOrigin.toString(), origin.clientOrigin.toString(), { }, { }, *salt);
    String directoryPath = FileSystem::pathByAppendingComponent(rootPath, key.hashAsString());

    String sizeFilePath = Caches::cachesSizeFilename(directoryPath);
    if (auto recordedSize = readSizeFile(sizeFilePath))
        return *recordedSize;

    return getDirectorySize(directoryPath);
}

void Engine::requestSpace(const WebCore::ClientOrigin& origin, uint64_t spaceRequested, CompletionHandler<void(WebCore::StorageQuotaManager::Decision)>&& callback)
{
    ASSERT(isMainThread());

    if (!m_networkProcess)
        callback(WebCore::StorageQuotaManager::Decision::Deny);

    RefPtr<WebCore::StorageQuotaManager> storageQuotaManager = m_networkProcess->storageQuotaManager(m_sessionID, origin);
    if (!storageQuotaManager)
        callback(WebCore::StorageQuotaManager::Decision::Deny);

    storageQuotaManager->requestSpaceOnMainThread(spaceRequested, WTFMove(callback));
}

Engine::Engine(PAL::SessionID sessionID, NetworkProcess& process, String&& rootPath)
    : m_sessionID(sessionID)
    , m_networkProcess(makeWeakPtr(process))
    , m_rootPath(WTFMove(rootPath))
{
    if (!m_rootPath.isNull())
        m_ioQueue = WorkQueue::create("com.apple.WebKit.CacheStorageEngine.serial.default", WorkQueue::Type::Serial, WorkQueue::QOS::Default);
}

void Engine::open(const WebCore::ClientOrigin& origin, const String& cacheName, CacheIdentifierCallback&& callback)
{
    readCachesFromDisk(origin, [cacheName, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.has_value()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        cachesOrError.value().get().open(cacheName, WTFMove(callback));
    });
}

void Engine::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
{
    Caches* cachesToModify = nullptr;
    for (auto& caches : m_caches.values()) {
        auto* cacheToRemove = caches->find(cacheIdentifier);
        if (cacheToRemove) {
            cachesToModify = caches.get();
            break;
        }
    }
    if (!cachesToModify) {
        callback(makeUnexpected(Error::Internal));
        return;
    }

    cachesToModify->remove(cacheIdentifier, WTFMove(callback));
}

void Engine::retrieveCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    readCachesFromDisk(origin, [updateCounter, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.has_value()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        cachesOrError.value().get().cacheInfos(updateCounter, WTFMove(callback));
    });
}

void Engine::retrieveRecords(uint64_t cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, RecordsCallback&& callback)
{
    readCache(cacheIdentifier, [options = WTFMove(options), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }
        result.value().get().retrieveRecords(options, WTFMove(callback));
    });
}

void Engine::putRecords(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [records = WTFMove(records), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        result.value().get().put(WTFMove(records), WTFMove(callback));
    });
}

void Engine::deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        result.value().get().remove(WTFMove(request), WTFMove(options), WTFMove(callback));
    });
}

void Engine::initialize(CompletionCallback&& callback)
{
    if (m_clearTaskCounter || !m_pendingClearCallbacks.isEmpty()) {
        m_pendingClearCallbacks.append(WTFMove(callback));
        return;
    }

    if (m_salt) {
        callback(std::nullopt);
        return;
    }

    if (!shouldPersist()) {
        m_salt = NetworkCache::Salt { };
        callback(std::nullopt);
        return;
    }

    bool shouldComputeSalt = m_initializationCallbacks.isEmpty();
    m_initializationCallbacks.append(WTFMove(callback));

    if (!shouldComputeSalt)
        return;

    m_ioQueue->dispatch([this, weakThis = makeWeakPtr(this), rootPath = m_rootPath.isolatedCopy()] () mutable {
        FileSystem::makeAllDirectories(rootPath);
        String saltPath = FileSystem::pathByAppendingComponent(rootPath, "salt"_s);
        RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), salt = FileSystem::readOrMakeSalt(saltPath)]() mutable {
            if (!weakThis)
                return;

            m_salt = WTFMove(salt);

            auto callbacks = WTFMove(m_initializationCallbacks);
            for (auto& callback : callbacks)
                callback(m_salt ? std::nullopt : std::make_optional(Error::WriteDisk));
        });
    });
}

void Engine::readCachesFromDisk(const WebCore::ClientOrigin& origin, CachesCallback&& callback)
{
    initialize([this, origin, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }

        auto& caches = m_caches.ensure(origin, [&origin, this] {
            auto path = cachesRootPath(origin);
            return Caches::create(*this, WebCore::ClientOrigin { origin }, WTFMove(path));
        }).iterator->value;

        if (caches->isInitialized()) {
            callback(std::reference_wrapper<Caches> { *caches });
            return;
        }

        caches->initialize([callback = WTFMove(callback), caches](std::optional<Error>&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }

            callback(std::reference_wrapper<Caches> { *caches });
        });
    });
}

void Engine::readCache(uint64_t cacheIdentifier, CacheCallback&& callback)
{
    auto* cache = this->cache(cacheIdentifier);
    if (!cache) {
        callback(makeUnexpected(Error::Internal));
        return;
    }
    if (!cache->isOpened()) {
        cache->open([this, protectedThis = makeRef(*this), cacheIdentifier, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }

            auto* cache = this->cache(cacheIdentifier);
            if (!cache) {
                callback(makeUnexpected(Error::Internal));
                return;
            }
            ASSERT(cache->isOpened());
            callback(std::reference_wrapper<Cache> { *cache });
        });
        return;
    }
    callback(std::reference_wrapper<Cache> { *cache });
}

Cache* Engine::cache(uint64_t cacheIdentifier)
{
    Cache* result = nullptr;
    for (auto& caches : m_caches.values()) {
        if ((result = caches->find(cacheIdentifier)))
            break;
    }
    return result;
}

void Engine::writeFile(const String& filename, NetworkCache::Data&& data, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (!shouldPersist()) {
        callback(std::nullopt);
        return;
    }

    m_pendingWriteCallbacks.add(++m_pendingCallbacksCounter, WTFMove(callback));
    m_ioQueue->dispatch([this, weakThis = makeWeakPtr(this), identifier = m_pendingCallbacksCounter, data = WTFMove(data), filename = filename.isolatedCopy()]() mutable {

        String directoryPath = FileSystem::parentPath(filename);
        if (!FileSystem::fileExists(directoryPath))
            FileSystem::makeAllDirectories(directoryPath);

        auto channel = IOChannel::open(filename, IOChannel::Type::Create, WorkQueue::QOS::Default);
        channel->write(0, data, WorkQueue::main(), [this, weakThis = WTFMove(weakThis), identifier](int error) mutable {
            ASSERT(RunLoop::isMain());
            if (!weakThis)
                return;

            auto callback = m_pendingWriteCallbacks.take(identifier);
            if (error) {
                RELEASE_LOG_ERROR(CacheStorage, "CacheStorage::Engine::writeFile failed with error %d", error);

                callback(Error::WriteDisk);
                return;
            }
            callback(std::nullopt);
        });
    });
}

void Engine::readFile(const String& filename, CompletionHandler<void(const NetworkCache::Data&, int error)>&& callback)
{
    if (!shouldPersist()) {
        callback(Data { }, 0);
        return;
    }

    m_pendingReadCallbacks.add(++m_pendingCallbacksCounter, WTFMove(callback));
    m_ioQueue->dispatch([this, weakThis = makeWeakPtr(this), identifier = m_pendingCallbacksCounter, filename = filename.isolatedCopy()]() mutable {
        auto channel = IOChannel::open(filename, IOChannel::Type::Read);
        if (!channel->isOpened()) {
            RunLoop::main().dispatch([this, weakThis = WTFMove(weakThis), identifier]() mutable {
                if (!weakThis)
                    return;

                m_pendingReadCallbacks.take(identifier)(Data { }, 0);
            });
            return;
        }

        channel->read(0, std::numeric_limits<size_t>::max(), WorkQueue::main(), [this, weakThis = WTFMove(weakThis), identifier](const Data& data, int error) mutable {
            RELEASE_LOG_ERROR_IF(error, CacheStorage, "CacheStorage::Engine::readFile failed with error %d", error);

            // FIXME: We should do the decoding in the background thread.
            ASSERT(RunLoop::isMain());

            if (!weakThis)
                return;

            m_pendingReadCallbacks.take(identifier)(data, error);
        });
    });
}

void Engine::removeFile(const String& filename)
{
    if (!shouldPersist())
        return;

    m_ioQueue->dispatch([filename = filename.isolatedCopy()]() mutable {
        FileSystem::deleteFile(filename);
    });
}

void Engine::writeSizeFile(const String& path, uint64_t size, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (!shouldPersist())
        return completionHandler();

    m_ioQueue->dispatch([path = path.isolatedCopy(), size, completionHandler = WTFMove(completionHandler)]() mutable {
        Locker locker { globalSizeFileLock };
        auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::Write);

        if (FileSystem::isHandleValid(fileHandle)) {
            FileSystem::truncateFile(fileHandle, 0);

            auto value = String::number(size).utf8();
            FileSystem::writeToFile(fileHandle, value.data(), value.length());

            FileSystem::closeFile(fileHandle);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

std::optional<uint64_t> Engine::readSizeFile(const String& path)
{
    ASSERT(!RunLoop::isMain());

    Locker locker { globalSizeFileLock };
    auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    auto closeFileHandle = makeScopeExit([&] {
        FileSystem::closeFile(fileHandle);
    });

    if (!FileSystem::isHandleValid(fileHandle))
        return std::nullopt;

    auto fileSize = FileSystem::fileSize(path).value_or(0);
    if (!fileSize)
        return std::nullopt;

    unsigned bytesToRead;
    if (!WTF::convertSafely(fileSize, bytesToRead))
        return std::nullopt;

    // FIXME: No reason we need a heap buffer to read an arbitrary number of bytes when we only support small files that contain numerals.
    Vector<char> buffer(bytesToRead);
    unsigned totalBytesRead = FileSystem::readFromFile(fileHandle, buffer.data(), buffer.size());
    if (totalBytesRead != bytesToRead)
        return std::nullopt;

    return parseInteger<uint64_t>({ buffer.data(), totalBytesRead });
}

class ReadOriginsTaskCounter : public RefCounted<ReadOriginsTaskCounter> {
public:
    static Ref<ReadOriginsTaskCounter> create(CompletionHandler<void(Vector<WebsiteData::Entry>)>&& callback)
    {
        return adoptRef(*new ReadOriginsTaskCounter(WTFMove(callback)));
    }

    ~ReadOriginsTaskCounter()
    {
        m_callback(WTFMove(m_entries));
    }

    void addOrigin(WebCore::SecurityOriginData&& origin, uint64_t size)
    {
        m_entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::DOMCache, size });
    }

private:
    explicit ReadOriginsTaskCounter(CompletionHandler<void(Vector<WebsiteData::Entry>)>&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    CompletionHandler<void(Vector<WebsiteData::Entry>)> m_callback;
    Vector<WebsiteData::Entry> m_entries;
};

void Engine::getDirectories(CompletionHandler<void(const Vector<String>&)>&& completionHandler)
{
    m_ioQueue->dispatch([path = m_rootPath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<String> folderPaths;
        for (auto& fileName : FileSystem::listDirectory(path)) {
            auto filePath = FileSystem::pathByAppendingComponent(path, fileName);
            if (FileSystem::fileType(filePath) == FileSystem::FileType::Directory)
                folderPaths.append(filePath.isolatedCopy());
        }

        RunLoop::main().dispatch([folderPaths = WTFMove(folderPaths), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(folderPaths);
        });
    });
}

void Engine::fetchEntries(bool shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    if (!shouldPersist()) {
        auto entries = WTF::map(m_caches, [] (auto& pair) {
            return WebsiteData::Entry { pair.value->origin().clientOrigin, WebsiteDataType::DOMCache, 0 };
        });
        completionHandler(WTFMove(entries));
        return;
    }

    getDirectories([this, weakThis = makeWeakPtr(this), path = m_rootPath.isolatedCopy(), shouldComputeSize, completionHandler = WTFMove(completionHandler)](const auto& folderPaths) mutable {
        if (!weakThis)
            return completionHandler({ });
        fetchDirectoryEntries(shouldComputeSize, folderPaths, WTFMove(completionHandler));
    });
}

void Engine::fetchDirectoryEntries(bool shouldComputeSize, const Vector<String>& folderPaths, CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    auto taskCounter = ReadOriginsTaskCounter::create(WTFMove(completionHandler));
    for (auto& folderPath : folderPaths) {
        Caches::retrieveOriginFromDirectory(folderPath, *m_ioQueue, [protectedThis = makeRef(*this), shouldComputeSize, taskCounter] (auto&& origin) mutable {
            ASSERT(RunLoop::isMain());
            if (!origin)
                return;

            if (!shouldComputeSize) {
                taskCounter->addOrigin(WTFMove(origin->topOrigin), 0);
                taskCounter->addOrigin(WTFMove(origin->clientOrigin), 0);
                return;
            }

            protectedThis->readCachesFromDisk(origin.value(), [origin = origin.value(), taskCounter = WTFMove(taskCounter)] (CachesOrError&& result) mutable {
                if (!result.has_value())
                    return;
                taskCounter->addOrigin(WTFMove(origin.topOrigin), 0);
                taskCounter->addOrigin(WTFMove(origin.clientOrigin), result.value().get().storageSize());
            });
        });
    }
}

CompletionHandler<void()> Engine::createClearTask(CompletionHandler<void()>&& completionHandler)
{
    ++m_clearTaskCounter;
    return [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
        if (!--m_clearTaskCounter) {
            auto callbacks = WTFMove(m_pendingClearCallbacks);
            for (auto& callback : callbacks)
                initialize(WTFMove(callback));
        }
    };
}

void Engine::clearAllCaches(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([this, completionHandler = createClearTask(WTFMove(completionHandler))]() mutable {
        if (!this->shouldPersist())
            return completionHandler();
        
        this->clearAllCachesFromDisk(WTFMove(completionHandler));
    });

    for (auto& caches : m_caches.values())
        caches->clear([callbackAggregator] { });
}

void Engine::clearAllCachesFromDisk(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_ioQueue->dispatch([path = m_rootPath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        Locker locker { globalSizeFileLock };
        for (auto& fileName : FileSystem::listDirectory(path)) {
            auto filePath = FileSystem::pathByAppendingComponent(path, fileName);
            if (FileSystem::fileType(filePath) == FileSystem::FileType::Directory)
                FileSystem::deleteNonEmptyDirectory(filePath);
        }
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void Engine::clearCachesForOrigin(const WebCore::SecurityOriginData& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create([this, origin, completionHandler = createClearTask(WTFMove(completionHandler))]() mutable {
        if (!this->shouldPersist())
            return completionHandler();

        this->clearCachesForOriginFromDisk(origin, [completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });

    for (auto& keyValue : m_caches) {
        if (keyValue.key.topOrigin == origin || keyValue.key.clientOrigin == origin)
            keyValue.value->clear([callbackAggregator] { });
    }
}

void Engine::clearCachesForOriginFromDisk(const WebCore::SecurityOriginData& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    getDirectories([this, weakThis = makeWeakPtr(this), origin, completionHandler = WTFMove(completionHandler)](const auto& folderPaths) mutable {
        if (!weakThis)
            return completionHandler();
        clearCachesForOriginFromDirectories(folderPaths, origin, WTFMove(completionHandler));
    });
}

void Engine::clearCachesForOriginFromDirectories(const Vector<String>& folderPaths, const WebCore::SecurityOriginData& origin, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& folderPath : folderPaths) {
        Caches::retrieveOriginFromDirectory(folderPath, *m_ioQueue, [this, protectedThis = makeRef(*this), origin, callbackAggregator, folderPath] (std::optional<WebCore::ClientOrigin>&& folderOrigin) mutable {
            if (!folderOrigin)
                return;
            if (folderOrigin->topOrigin != origin && folderOrigin->clientOrigin != origin)
                return;

            // If cache salt is initialized and the paths do not match, some cache files have probably be removed or partially corrupted.
            ASSERT(!m_salt || folderPath == cachesRootPath(*folderOrigin));
            deleteNonEmptyDirectoryOnBackgroundThread(folderPath, [callbackAggregator = WTFMove(callbackAggregator)] { });
        });
    }
}

void Engine::deleteNonEmptyDirectoryOnBackgroundThread(const String& path, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_ioQueue->dispatch([path = path.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        Locker locker { globalSizeFileLock };
        FileSystem::deleteNonEmptyDirectory(path);

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void Engine::clearMemoryRepresentation(const WebCore::ClientOrigin& origin, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    readCachesFromDisk(origin, [callback = WTFMove(callback)](CachesOrError&& result) mutable {
        if (!result.has_value()) {
            callback(result.error());
            return;
        }
        result.value().get().clearMemoryRepresentation();
        callback(std::nullopt);
    });
}

void Engine::lock(uint64_t cacheIdentifier)
{
    auto& counter = m_cacheLocks.ensure(cacheIdentifier, []() {
        return 0;
    }).iterator->value;

    ++counter;
}

void Engine::unlock(uint64_t cacheIdentifier)
{
    auto lockCount = m_cacheLocks.find(cacheIdentifier);
    if (lockCount == m_cacheLocks.end())
        return;

    ASSERT(lockCount->value);
    if (--lockCount->value)
        return;

    auto* cache = this->cache(cacheIdentifier);
    if (!cache)
        return;

    cache->dispose();
}

String Engine::representation()
{
    ASSERT(m_pendingClearCallbacks.isEmpty());
    ASSERT(m_initializationCallbacks.isEmpty());

    bool isFirst = true;
    StringBuilder builder;
    builder.append("{ \"path\": \"");
    builder.append(m_rootPath);
    builder.append("\", \"origins\": [");
    for (auto& keyValue : m_caches) {
        if (!isFirst)
            builder.append(",");
        isFirst = false;

        builder.append("\n{ \"origin\" : { \"topOrigin\" : \"");
        builder.append(keyValue.key.topOrigin.toString());
        builder.append("\", \"clientOrigin\": \"");
        builder.append(keyValue.key.clientOrigin.toString());
        builder.append("\" }, \"caches\" : ");
        keyValue.value->appendRepresentation(builder);
        builder.append("}");
    }
    builder.append("]}");
    return builder.toString();
}

} // namespace CacheStorage

} // namespace WebKit
