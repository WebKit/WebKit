/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "NetworkStorageManager.h"

#include "CacheStorageManager.h"
#include "IDBStorageConnectionToClient.h"
#include "Logging.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkStorageManagerMessages.h"
#include "OriginStorageManager.h"
#include "StorageUtilities.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebsiteDataType.h"
#include <WebCore/SecurityOriginData.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/SuspendableWorkQueue.h>
#include <wtf/text/Base64.h>

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
static const Seconds defaultBackupExclusionPeriod { 24_h };
#endif

static String encode(const String& string, FileSystem::Salt salt)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto utf8String = string.utf8();
    crypto->addBytes(utf8String.data(), utf8String.length());
    crypto->addBytes(salt.data(), salt.size());
    auto hash = crypto->computeHash();
    return base64URLEncodeToString(hash.data(), hash.size());
}

static String originDirectoryPath(const String& rootPath, const WebCore::ClientOrigin& origin, FileSystem::Salt salt)
{
    if (rootPath.isEmpty())
        return emptyString();

    auto encodedTopOrigin = encode(origin.topOrigin.toString(), salt);
    auto encodedOpeningOrigin = encode(origin.clientOrigin.toString(), salt);
    return FileSystem::pathByAppendingComponents(rootPath, { encodedTopOrigin, encodedOpeningOrigin });
}

static String originFilePath(const String& directory)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, OriginStorageManager::originFileIdentifier());
}

static bool isEmptyOriginDirectory(const String& directory)
{
    auto children = FileSystem::listDirectory(directory);
    if (children.isEmpty())
        return true;

    if (children.size() >= 2)
        return false;

    HashSet<String> invalidFileNames {
        OriginStorageManager::originFileIdentifier()
#if PLATFORM(COCOA)
        , ".DS_Store"_s
#endif
    };
    return WTF::allOf(children, [&] (auto& child) {
        return invalidFileNames.contains(child);
    });
}

static void deleteEmptyOriginDirectory(const String& directory)
{
    if (directory.isEmpty())
        return;

    if (isEmptyOriginDirectory(directory))
        FileSystem::deleteFile(originFilePath(directory));

    FileSystem::deleteEmptyDirectory(directory);
    FileSystem::deleteEmptyDirectory(FileSystem::parentPath(directory));
}

Ref<NetworkStorageManager> NetworkStorageManager::create(PAL::SessionID sessionID, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, UnifiedOriginStorageLevel level)
{
    return adoptRef(*new NetworkStorageManager(sessionID, connection, path, customLocalStoragePath, customIDBStoragePath, customCacheStoragePath, defaultOriginQuota, defaultThirdPartyOriginQuota, level));
}

NetworkStorageManager::NetworkStorageManager(PAL::SessionID sessionID, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, UnifiedOriginStorageLevel level)
    : m_sessionID(sessionID)
    , m_queue(SuspendableWorkQueue::create("com.apple.WebKit.Storage", SuspendableWorkQueue::QOS::Default, SuspendableWorkQueue::ShouldLog::Yes))
    , m_defaultOriginQuota(defaultOriginQuota)
    , m_defaultThirdPartyOriginQuota(defaultThirdPartyOriginQuota)
    , m_parentConnection(connection)
#if PLATFORM(IOS_FAMILY)
    , m_backupExclusionPeriod(defaultBackupExclusionPeriod)
#endif
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, path = path.isolatedCopy(), customLocalStoragePath = crossThreadCopy(customLocalStoragePath), customIDBStoragePath = crossThreadCopy(customIDBStoragePath), customCacheStoragePath = crossThreadCopy(customCacheStoragePath), level]() mutable {
        m_unifiedOriginStorageLevel = level;
        m_path = path;
        m_customLocalStoragePath = customLocalStoragePath;
        m_customIDBStoragePath = customIDBStoragePath;
        m_customCacheStoragePath = customCacheStoragePath;
        if (!m_path.isEmpty()) {
            auto saltPath = FileSystem::pathByAppendingComponent(m_path, "salt"_s);
            m_salt = valueOrDefault(FileSystem::readOrMakeSalt(saltPath));
        }
#if PLATFORM(IOS_FAMILY)
        // Exclude LocalStorage directory to reduce backup traffic. See https://webkit.org/b/168388.
        if (m_unifiedOriginStorageLevel == UnifiedOriginStorageLevel::None  && !m_customLocalStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(m_customLocalStoragePath);
            FileSystem::setExcludedFromBackup(m_customLocalStoragePath, true);
        }
#endif
    });
}

NetworkStorageManager::~NetworkStorageManager()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_closed);
}

bool NetworkStorageManager::canHandleTypes(OptionSet<WebsiteDataType> types)
{
    return types.contains(WebsiteDataType::LocalStorage)
        || types.contains(WebsiteDataType::SessionStorage)
        || types.contains(WebsiteDataType::FileSystem)
        || types.contains(WebsiteDataType::IndexedDBDatabases)
        || types.contains(WebsiteDataType::DOMCache);
}

void NetworkStorageManager::close(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_closed = true;
    m_connections.forEach([] (auto& connection) {
        connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    });

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        m_originStorageManagers.clear();
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::startReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    connection.addWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName(), m_queue.get(), *this);
    m_connections.add(connection);
}

void NetworkStorageManager::stopReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    
    if (!m_connections.remove(connection))
        return;

    connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    m_queue->dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID()]() mutable {
        assertIsCurrent(workQueue());
        m_originStorageManagers.removeIf([&](auto& entry) {
            auto& manager = entry.value;
            manager->connectionClosed(connection);
            bool shouldRemove = !manager->isActive();
            if (shouldRemove) {
                manager->deleteEmptyDirectory();
                auto originDirectory = originDirectoryPath(m_path, entry.key, m_salt);
                deleteEmptyOriginDirectory(originDirectory);
            }
            return shouldRemove;
        });
        m_temporaryBlobPathsByConnection.remove(connection);
    });
}

#if PLATFORM(IOS_FAMILY)

void NetworkStorageManager::includeOriginInBackupIfNecessary(const WebCore::ClientOrigin& origin, OriginStorageManager& manager)
{
    if (manager.includedInBackup())
        return;

    auto originFileCreationTimestamp = manager.originFileCreationTimestamp();
    if (!originFileCreationTimestamp)
        return;

    if (WallTime::now() - originFileCreationTimestamp.value() < m_backupExclusionPeriod)
        return;

    auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
    FileSystem::setExcludedFromBackup(originDirectory, false);
    manager.markIncludedInBackup();
}

#endif

void NetworkStorageManager::writeOriginToFileIfNecessary(const WebCore::ClientOrigin& origin, ShouldCheckIfDirectoryEmpty shouldCheckIfDirectoryEmpty)
{
    auto* manager = m_originStorageManagers.get(origin);
    if (!manager)
        return;

    if (manager->originFileCreationTimestamp()) {
#if PLATFORM(IOS_FAMILY)
        includeOriginInBackupIfNecessary(origin, *manager);
#endif
        return;
    }
    auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
    if (originDirectory.isEmpty())
        return;

    if (shouldCheckIfDirectoryEmpty == ShouldCheckIfDirectoryEmpty::Yes && isEmptyOriginDirectory(originDirectory))
        return;

    auto originFile = originFilePath(originDirectory);
    bool didWrite = writeOriginToFile(originFile, origin);
    auto timestamp = FileSystem::fileCreationTime(originFile);
    manager->setOriginFileCreationTimestamp(timestamp);
#if PLATFORM(IOS_FAMILY)
    if (didWrite)
        FileSystem::setExcludedFromBackup(originDirectory, true);
    else
        includeOriginInBackupIfNecessary(origin, *manager);
#else
    UNUSED_PARAM(didWrite);
#endif
}

OriginStorageManager& NetworkStorageManager::originStorageManager(const WebCore::ClientOrigin& origin, ShouldWriteOriginFile shouldWriteOriginFile)
{
    ASSERT(!RunLoop::isMain());

    auto& originStorageManager = *m_originStorageManagers.ensure(origin, [&] {
        auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
        auto localStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, origin);
        auto idbStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, origin);
        auto cacheStoragePath = CacheStorageManager::cacheStorageOriginDirectory(m_customCacheStoragePath, origin);
        CacheStorageManager::copySaltFileToOriginDirectory(m_customCacheStoragePath, cacheStoragePath);
        uint64_t quota = origin.topOrigin == origin.clientOrigin ? m_defaultOriginQuota : m_defaultThirdPartyOriginQuota;
        QuotaManager::IncreaseQuotaFunction increaseQuotaFunction = [sessionID = m_sessionID, origin, connection = m_parentConnection] (auto identifier, auto currentQuota, auto currentUsage, auto requestedIncrease) mutable {
            IPC::Connection::send(connection, Messages::NetworkProcessProxy::IncreaseQuota(sessionID, origin, identifier, currentQuota, currentUsage, requestedIncrease), 0);
        };
        return makeUnique<OriginStorageManager>(quota, WTFMove(increaseQuotaFunction), origin, WTFMove(originDirectory), WTFMove(localStoragePath), WTFMove(idbStoragePath), WTFMove(cacheStoragePath), m_unifiedOriginStorageLevel);
    }).iterator->value;

    if (shouldWriteOriginFile == ShouldWriteOriginFile::Yes)
        writeOriginToFileIfNecessary(origin);

    return originStorageManager;
}

OriginStorageManager* NetworkStorageManager::existingOriginStorageManager(const WebCore::ClientOrigin& origin)
{
    if (auto iterator = m_originStorageManagers.find(origin); iterator != m_originStorageManagers.end())
        return iterator->value.get();

    return nullptr;
}

bool NetworkStorageManager::removeOriginStorageManagerIfPossible(const WebCore::ClientOrigin& origin)
{
    auto iterator = m_originStorageManagers.find(origin);
    if (iterator == m_originStorageManagers.end())
        return true;

    auto& manager = iterator->value;
    if (manager->isActive())
        return false;

    manager->deleteEmptyDirectory();
    auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
    deleteEmptyOriginDirectory(originDirectory);

    m_originStorageManagers.remove(iterator);
    return true;
}

void NetworkStorageManager::persisted(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(originStorageManager(origin).persisted());
}

void NetworkStorageManager::persist(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    originStorageManager(origin).setPersisted(true);
    completionHandler(true);
}

void NetworkStorageManager::estimate(const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<WebCore::StorageEstimate>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(originStorageManager(origin).estimate());
}

void NetworkStorageManager::resetStoragePersistedState(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        // Reset persisted value.
        for (auto& manager : m_originStorageManagers.values())
            manager->setPersisted(false);

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::clearStorageForWebPage(WebPageProxyIdentifier pageIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, pageIdentifier]() mutable {
        for (auto& manager : m_originStorageManagers.values())
            manager->clearSessionStorage(makeObjectIdentifier<StorageNamespaceIdentifierType>(pageIdentifier.toUInt64()));
    });
}

void NetworkStorageManager::cloneSessionStorageForWebPage(WebPageProxyIdentifier fromIdentifier, WebPageProxyIdentifier toIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, fromIdentifier, toIdentifier]() mutable {
        cloneSessionStorageNamespace(makeObjectIdentifier<StorageNamespaceIdentifierType>(fromIdentifier.toUInt64()), makeObjectIdentifier<StorageNamespaceIdentifierType>(toIdentifier.toUInt64()));
    });
}

void NetworkStorageManager::didIncreaseQuota(WebCore::ClientOrigin&& origin, QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(WTFMove(origin)), identifier, newQuota]() mutable {
        if (auto manager = m_originStorageManagers.get(origin))
            manager->didIncreaseQuota(identifier, newQuota);
    });
}

void NetworkStorageManager::fileSystemGetDirectory(IPC::Connection& connection, WebCore::ClientOrigin&& origin, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    originStorageManager(origin).fileSystemGetDirectory(connection, WTFMove(completionHandler));
}

void NetworkStorageManager::closeHandle(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        manager->fileSystemCloseHandle(identifier);
}

void NetworkStorageManager::isSameEntry(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemIsSameEntry(identifier, targetIdentifier, WTFMove(completionHandler));

    completionHandler(false);
}

void NetworkStorageManager::move(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemMove(identifier, destinationIdentifier, newName, WTFMove(completionHandler));

    completionHandler(FileSystemStorageError::Unknown);
}

void NetworkStorageManager::getFileHandle(IPC::Connection& connection, const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemGetFileHandle(connection, identifier, WTFMove(name), createIfNecessary, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::getDirectoryHandle(IPC::Connection& connection, const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemGetDirectoryHandle(connection, identifier, WTFMove(name), createIfNecessary, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::removeEntry(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemRemoveEntry(identifier, name, deleteRecursively, WTFMove(completionHandler));

    completionHandler(FileSystemStorageError::Unknown);
}

void NetworkStorageManager::resolve(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemResolve(identifier, targetIdentifier, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::getFile(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemGetFile(identifier, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::createSyncAccessHandle(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemCreateSyncAccessHandle(identifier, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::closeSyncAccessHandle(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemCloseSyncAccessHandle(identifier, accessHandleIdentifier, WTFMove(completionHandler));

    completionHandler();
}

void NetworkStorageManager::requestNewCapacityForSyncAccessHandle(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemRequestNewCapacityForSyncAccessHandle(identifier, accessHandleIdentifier, newCapacity, WTFMove(completionHandler));

    completionHandler(std::nullopt);
}

void NetworkStorageManager::getHandleNames(const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemGetHandleNames(identifier, WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::getHandle(IPC::Connection& connection, const WebCore::ClientOrigin& origin, WebCore::FileSystemHandleIdentifier identifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        return manager->fileSystemGetHandle(connection, identifier, WTFMove(name), WTFMove(completionHandler));

    completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
}

void NetworkStorageManager::forEachOriginDirectory(const Function<void(const String&)>& apply)
{
    for (auto& topOrigin : FileSystem::listDirectory(m_path)) {
        auto topOriginDirectory = FileSystem::pathByAppendingComponent(m_path, topOrigin);
        auto openingOrigins = FileSystem::listDirectory(topOriginDirectory);
        if (openingOrigins.isEmpty()) {
            FileSystem::deleteEmptyDirectory(topOriginDirectory);
            continue;
        }

        for (auto& openingOrigin : openingOrigins) {
            if (openingOrigin.startsWith('.'))
                continue;

            auto openingOriginDirectory = FileSystem::pathByAppendingComponent(topOriginDirectory, openingOrigin);
            apply(openingOriginDirectory);
        }
    }
}

HashSet<WebCore::ClientOrigin> NetworkStorageManager::getAllOrigins()
{
    HashSet<WebCore::ClientOrigin> allOrigins;
    for (auto& origin : m_originStorageManagers.keys())
        allOrigins.add(origin);

    forEachOriginDirectory([&](auto directory) {
        if (auto origin = readOriginFromFile(originFilePath(directory)))
            allOrigins.add(*origin);
    });

    for (auto& origin : LocalStorageManager::originsOfLocalStorageData(m_customLocalStoragePath))
        allOrigins.add(WebCore::ClientOrigin { origin, origin });

    for (auto& origin : IDBStorageManager::originsOfIDBStorageData(m_customIDBStoragePath))
        allOrigins.add(origin);

    for (auto& origin : CacheStorageManager::originsOfCacheStorageData(m_customCacheStoragePath))
        allOrigins.add(origin);

    return allOrigins;
}

static void updateOriginData(HashMap<WebCore::SecurityOriginData, OriginStorageManager::DataTypeSizeMap>& originTypes, const WebCore::SecurityOriginData& origin, const OriginStorageManager::DataTypeSizeMap& newTypeSizeMap)
{
    auto& typeSizeMap = originTypes.add(origin, OriginStorageManager::DataTypeSizeMap { }).iterator->value;
    for (auto [type, size] : newTypeSizeMap) {
        auto& currentSize = typeSizeMap.add(type, 0).iterator->value;
        currentSize += size;
    }
}

Vector<WebsiteData::Entry> NetworkStorageManager::fetchDataFromDisk(OptionSet<WebsiteDataType> targetTypes, ShouldComputeSize shouldComputeSize)
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::SecurityOriginData, OriginStorageManager::DataTypeSizeMap> originTypes;
    for (auto& origin : getAllOrigins()) {
        auto typeSizeMap = originStorageManager(origin).fetchDataTypesInList(targetTypes, shouldComputeSize == ShouldComputeSize::Yes);
        updateOriginData(originTypes, origin.clientOrigin, typeSizeMap);
        if (origin.clientOrigin != origin.topOrigin)
            updateOriginData(originTypes, origin.topOrigin, typeSizeMap);

        removeOriginStorageManagerIfPossible(origin);
    }

    Vector<WebsiteData::Entry> entries;
    for (auto [origin, types] : originTypes) {
        for (auto [type, size] : types)
            entries.append({ WebsiteData::Entry { origin, type, size } });
    }

    return entries;
}

void NetworkStorageManager::fetchData(OptionSet<WebsiteDataType> types, ShouldComputeSize shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, shouldComputeSize, completionHandler = WTFMove(completionHandler)]() mutable {
        auto entries = fetchDataFromDisk(types, shouldComputeSize);
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), entries = crossThreadCopy(WTFMove(entries))]() mutable {
            completionHandler(WTFMove(entries));
        });
    });
}

HashSet<WebCore::ClientOrigin> NetworkStorageManager::deleteDataOnDisk(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, const Function<bool(const WebCore::ClientOrigin&)>& filter)
{
    ASSERT(!RunLoop::isMain());

    HashSet<WebCore::ClientOrigin> deletedOrigins;
    for (auto& origin : getAllOrigins()) {
        if (!filter(origin))
            continue;

        auto existingDataTypes = originStorageManager(origin).fetchDataTypesInList(types, false);
        if (!existingDataTypes.isEmpty()) {
            deletedOrigins.add(origin);
            originStorageManager(origin).deleteData(types, modifiedSinceTime);
        }
        removeOriginStorageManagerIfPossible(origin);
    }

    return deletedOrigins;
}

void NetworkStorageManager::deleteData(OptionSet<WebsiteDataType> types, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, origins = crossThreadCopy(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        HashSet<WebCore::SecurityOriginData> originSet;
        originSet.reserveInitialCapacity(origins.size());
        for (auto origin : origins)
            originSet.add(WTFMove(origin));

        deleteDataOnDisk(types, -WallTime::infinity(), [&originSet](auto origin) {
            return originSet.contains(origin.topOrigin) || originSet.contains(origin.clientOrigin);
        });
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteData(OptionSet<WebsiteDataType> types, const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, originToDelete = origin.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        deleteDataOnDisk(types, -WallTime::infinity(), [originToDelete = WTFMove(originToDelete)](auto& origin) {
            return origin == originToDelete;
        });
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataModifiedSince(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, modifiedSinceTime, completionHandler = WTFMove(completionHandler)]() mutable {
        deleteDataOnDisk(types, modifiedSinceTime, [](auto&) {
            return true;
        });

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataForRegistrableDomains(OptionSet<WebsiteDataType> types, const Vector<WebCore::RegistrableDomain>& domains, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)]() mutable {
        auto deletedOrigins = deleteDataOnDisk(types, -WallTime::infinity(), [&domains](auto& origin) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            return domains.contains(domain);
        });

        HashSet<WebCore::RegistrableDomain> deletedDomains;
        for (auto origin : deletedOrigins) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            deletedDomains.add(domain);
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), domains = crossThreadCopy(WTFMove(deletedDomains))]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void NetworkStorageManager::moveData(OptionSet<WebsiteDataType> types, WebCore::SecurityOriginData&& source, WebCore::SecurityOriginData&& target, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, source = crossThreadCopy(WTFMove(source)), target = crossThreadCopy(WTFMove(target)), completionHandler = WTFMove(completionHandler)]() mutable {
        auto sourceOrigin = WebCore::ClientOrigin { source, source };
        auto targetOrigin = WebCore::ClientOrigin { target, target };
        m_originStorageManagers.remove(sourceOrigin);
        m_originStorageManagers.remove(targetOrigin);
        auto sourceOriginDirectory = originDirectoryPath(m_path, sourceOrigin, m_salt);
        auto targetOriginDirectory = originDirectoryPath(m_path, sourceOrigin, m_salt);
        FileSystem::deleteNonEmptyDirectory(targetOriginDirectory);
        FileSystem::moveFile(sourceOriginDirectory, targetOriginDirectory);
        // Force update origin file.
        auto originFile = originFilePath(targetOriginDirectory);
        writeOriginToFile(originFile, targetOrigin);

        if (types.contains(WebsiteDataType::LocalStorage)) {
            auto sourceLocalStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, sourceOrigin);
            auto targetLocalStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, targetOrigin);
            FileSystem::deleteFile(targetLocalStoragePath);
            FileSystem::moveFile(sourceLocalStoragePath, targetLocalStoragePath);
        }

        if (types.contains(WebsiteDataType::IndexedDBDatabases)) {
            auto sourceIDBStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, sourceOrigin);
            auto targetIDBStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, targetOrigin);
            FileSystem::deleteFile(targetIDBStoragePath);
            FileSystem::moveFile(sourceIDBStoragePath, targetIDBStoragePath);
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::getOriginDirectory(WebCore::ClientOrigin&& origin, WebsiteDataType type, CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, type, origin = crossThreadCopy(WTFMove(origin)), completionHandler = WTFMove(completionHandler)]() mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), directory = crossThreadCopy(originStorageManager(origin).resolvedPath(type))]() mutable {
            completionHandler(WTFMove(directory));
        });
        removeOriginStorageManagerIfPossible(origin);
    });
}

void NetworkStorageManager::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_sessionID.isEphemeral())
        return completionHandler();

    RELEASE_LOG(ProcessSuspension, "%p - NetworkStorageManager::suspend()", this);
    m_queue->suspend([this, protectedThis = Ref { *this }] {
        for (auto& manager : m_originStorageManagers.values())
            manager->suspend();
    }, WTFMove(completionHandler));
}

void NetworkStorageManager::resume()
{
    ASSERT(RunLoop::isMain());

    if (m_sessionID.isEphemeral())
        return;

    RELEASE_LOG(ProcessSuspension, "%p - NetworkStorageManager::resume()", this);
    m_queue->resume();
}

void NetworkStorageManager::handleLowMemoryWarning()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }] {
        for (auto& manager : m_originStorageManagers.values())
            manager->handleLowMemoryWarning();
    });
}

void NetworkStorageManager::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& manager : m_originStorageManagers.values())
            manager->syncLocalStorage();

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::registerTemporaryBlobFilePaths(IPC::Connection& connection, const Vector<String>& filePaths)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, connectionID = connection.uniqueID(), filePaths = crossThreadCopy(filePaths)] {
        assertIsCurrent(workQueue());
        auto& temporaryBlobPaths = m_temporaryBlobPathsByConnection.ensure(connectionID, [] {
            return HashSet<String> { };
        }).iterator->value;
        temporaryBlobPaths.add(filePaths.begin(), filePaths.end());
    });
}

void NetworkStorageManager::requestSpace(const WebCore::ClientOrigin& origin, uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin), size, completionHandler = WTFMove(completionHandler)]() mutable {
        originStorageManager(origin).requestSpace(size, [completionHandler = WTFMove(completionHandler)](bool granted) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), granted]() mutable {
                completionHandler(granted);
            });
        });
    });
}

void NetworkStorageManager::resetQuotaForTesting(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& manager : m_originStorageManagers.values())
            manager->resetQuotaForTesting();
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::resetQuotaUpdatedBasedOnUsageForTesting(WebCore::ClientOrigin&& origin)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(WTFMove(origin))]() mutable {
        if (auto manager = m_originStorageManagers.get(origin))
            manager->resetQuotaUpdatedBasedOnUsageForTesting();
    });
}

#if PLATFORM(IOS_FAMILY)

void NetworkStorageManager::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, period, completionHandler = WTFMove(completionHandler)]() mutable {
        m_backupExclusionPeriod = period;
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

#endif

void NetworkStorageManager::connectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    // StorageArea may be connected due to LocalStorage prewarming, so do not write origin file eagerly.
    originStorageManager(origin, ShouldWriteOriginFile::No).webStorageConnectToStorageArea(connection, type, sourceIdentifier, namespaceIdentifier, m_queue.copyRef(), WTFMove(completionHandler));
    writeOriginToFileIfNecessary(origin, ShouldCheckIfDirectoryEmpty::Yes);
}

void NetworkStorageManager::connectToStorageAreaSync(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    originStorageManager(origin, ShouldWriteOriginFile::No).webStorageConnectToStorageAreaSync(connection, type, sourceIdentifier, namespaceIdentifier, m_queue.copyRef(), WTFMove(completionHandler));
}

void NetworkStorageManager::cancelConnectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->webStorageCancelConnectToStorageArea(connection, type, namespaceIdentifier);
}

void NetworkStorageManager::disconnectFromStorageArea(IPC::Connection& connection, const WebCore::ClientOrigin& origin, StorageAreaIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin))
        manager->webStorageDisconnectFromStorageArea(connection, identifier);
}

void NetworkStorageManager::cloneSessionStorageNamespace(StorageNamespaceIdentifier fromIdentifier, StorageNamespaceIdentifier toIdentifier)
{
    ASSERT(!RunLoop::isMain());

    for (auto& manager : m_originStorageManagers.values())
        manager->webStorageCloneSessionStorageNamespace(fromIdentifier, toIdentifier);
}

void NetworkStorageManager::setItem(IPC::Connection& connection, const WebCore::ClientOrigin& origin, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin)) {
        manager->webStorageSetItem(connection, identifier, implIdentifier, WTFMove(key), WTFMove(value), WTFMove(urlString), WTFMove(completionHandler));
        writeOriginToFileIfNecessary(origin, ShouldCheckIfDirectoryEmpty::Yes);
        return;
    }

    completionHandler(true, { });
}

void NetworkStorageManager::removeItem(IPC::Connection& connection, const WebCore::ClientOrigin& origin, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin)) {
        manager->webStorageRemoveItem(connection, identifier, implIdentifier, WTFMove(key), WTFMove(urlString), WTFMove(completionHandler));
        writeOriginToFileIfNecessary(origin, ShouldCheckIfDirectoryEmpty::Yes);
        return;
    }

    completionHandler();
}

void NetworkStorageManager::clear(IPC::Connection& connection, const WebCore::ClientOrigin& origin, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto* manager = existingOriginStorageManager(origin)) {
        manager->webStorageClear(connection, identifier, implIdentifier, WTFMove(urlString), WTFMove(completionHandler));
        writeOriginToFileIfNecessary(origin, ShouldCheckIfDirectoryEmpty::Yes);
        return;
    }

    completionHandler();
}

void NetworkStorageManager::openDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    originStorageManager(requestData.databaseIdentifier().origin()).idbOpenDatabase(connection, requestData);
}

void NetworkStorageManager::openDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    originStorageManager(requestData.databaseIdentifier().origin()).idbOpenDBRequestCancelled(requestData);
}

void NetworkStorageManager::deleteDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    originStorageManager(requestData.databaseIdentifier().origin()).idbDeleteDatabase(connection, requestData);
}

void NetworkStorageManager::establishTransaction(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbEstablishTransaction(databaseConnectionIdentifier, transactionInfo);
}

void NetworkStorageManager::databaseConnectionPendingClose(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDatabaseConnectionPendingClose(databaseConnectionIdentifier);
}

void NetworkStorageManager::databaseConnectionClosed(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDatabaseConnectionClosed(databaseConnectionIdentifier);
}

void NetworkStorageManager::abortOpenAndUpgradeNeeded(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbAbortOpenAndUpgradeNeeded(databaseConnectionIdentifier, transactionIdentifier);
}

void NetworkStorageManager::didFireVersionChangeEvent(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDidFireVersionChangeEvent(databaseConnectionIdentifier, requestIdentifier, connectionClosed);
}

void NetworkStorageManager::abortTransaction(const WebCore::ClientOrigin& origin, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbAbortTransaction(transactionIdentifier);
}

void NetworkStorageManager::commitTransaction(const WebCore::ClientOrigin& origin, const WebCore::IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbCommitTransaction(transactionIdentifier, pendingRequestCount);
}

void NetworkStorageManager::didFinishHandlingVersionChangeTransaction(const WebCore::ClientOrigin& origin, uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDidFinishHandlingVersionChangeTransaction(databaseConnectionIdentifier, transactionIdentifier);
}

void NetworkStorageManager::createObjectStore(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbCreateObjectStore(requestData, objectStoreInfo);
}

void NetworkStorageManager::deleteObjectStore(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDeleteObjectStore(requestData, objectStoreName);
}

void NetworkStorageManager::renameObjectStore(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbRenameObjectStore(requestData, objectStoreIdentifier, newName);
}

void NetworkStorageManager::clearObjectStore(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbClearObjectStore(requestData, objectStoreIdentifier);
}

void NetworkStorageManager::createIndex(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbCreateIndex(requestData, indexInfo);
}

void NetworkStorageManager::deleteIndex(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDeleteIndex(requestData, objectStoreIdentifier, indexName);
}

void NetworkStorageManager::renameIndex(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbRenameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void NetworkStorageManager::putOrAdd(IPC::Connection& connection, const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, WebCore::IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    assertIsCurrent(workQueue());
    if (value.blobURLs().size() != value.blobFilePaths().size()) {
        RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: Number of blob URLs doesn't match the number of blob file paths.");
        ASSERT_NOT_REACHED();
        return;
    }

    // Validate temporary blob paths in |value| to make sure they belong to the source process.
    if (!value.blobFilePaths().isEmpty()) {
        auto it = m_temporaryBlobPathsByConnection.find(connection.uniqueID());
        if (it == m_temporaryBlobPathsByConnection.end()) {
            RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: IDBValue contains blob paths but none are allowed for this process");
            ASSERT_NOT_REACHED();
            return;
        }

        auto& temporaryBlobPathsForConnection = it->value;
        for (auto& blobFilePath : value.blobFilePaths()) {
            if (!temporaryBlobPathsForConnection.remove(blobFilePath)) {
                RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: Blob path was not created for this WebProcess");
                ASSERT_NOT_REACHED();
                return;
            }
        }
    }

    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbPutOrAdd(connection, requestData, keyData, value, overwriteMode);
}

void NetworkStorageManager::getRecord(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbGetRecord(requestData, getRecordData);
}

void NetworkStorageManager::getAllRecords(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbGetAllRecords(requestData, getAllRecordsData);
}

void NetworkStorageManager::getCount(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbGetCount(requestData, keyRangeData);
}

void NetworkStorageManager::deleteRecord(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbDeleteRecord(requestData, keyRangeData);
}

void NetworkStorageManager::openCursor(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbOpenCursor(requestData, cursorInfo);
}

void NetworkStorageManager::iterateCursor(const WebCore::ClientOrigin& origin, const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& cursorData)
{
    if (auto* manager = existingOriginStorageManager(origin))
        manager->idbIterateCursor(requestData, cursorData);
}

void NetworkStorageManager::getAllDatabaseNamesAndVersions(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::ClientOrigin& origin)
{
    originStorageManager(origin).idbGetAllDatabaseNamesAndVersions(connection, requestIdentifier);
}

void NetworkStorageManager::cacheStorageOpenCache(const WebCore::ClientOrigin& origin, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    originStorageManager(origin).cacheStorageOpenCache(m_queue.copyRef(), cacheName, WTFMove(callback));
}

void NetworkStorageManager::cacheStorageRemoveCache(const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStorageRemoveCache(cacheIdentifier, WTFMove(callback));

    callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

void NetworkStorageManager::cacheStorageAllCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    originStorageManager(origin).cacheStorageAllCaches(m_queue.copyRef(), updateCounter, WTFMove(callback));
}

void NetworkStorageManager::cacheStorageReference(IPC::Connection& connection, const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStorageReference(connection, cacheIdentifier);
}

void NetworkStorageManager::cacheStorageDereference(IPC::Connection& connection, const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStorageDereference(connection, cacheIdentifier);
}

void NetworkStorageManager::cacheStorageRetrieveRecords(const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::RecordsCallback&& callback)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStorageRetrieveRecords(cacheIdentifier, WTFMove(options), WTFMove(callback));

    callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

void NetworkStorageManager::cacheStorageRemoveRecords(const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStorageRemoveRecords(cacheIdentifier, WTFMove(request), WTFMove(options), WTFMove(callback));

    callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

void NetworkStorageManager::cacheStoragePutRecords(const WebCore::ClientOrigin& origin, WebCore::DOMCacheIdentifier cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    if (auto* manager = existingOriginStorageManager(origin))
        return manager->cacheStoragePutRecords(cacheIdentifier, WTFMove(records), WTFMove(callback));

    callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

void NetworkStorageManager::cacheStorageClearMemoryRepresentation(const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Error>&&)>&& callback)
{
    auto* manager = existingOriginStorageManager(origin);
    if (!manager)
        return callback(std::nullopt);

    manager->cacheStorageClearMemoryRepresentation(WTFMove(callback));
}

void NetworkStorageManager::cacheStorageRepresentation(CompletionHandler<void(String&&)>&& callback)
{
    Vector<String> originStrings;
    auto targetTypes = OptionSet<WebsiteDataType> { WebsiteDataType::DOMCache };
    for (auto& origin : getAllOrigins()) {
        auto fetchedTypes = originStorageManager(origin).fetchDataTypesInList(targetTypes, false);
        if (!fetchedTypes.isEmpty()) {
            StringBuilder originBuilder;
            originBuilder.append("\n{ \"origin\" : { \"topOrigin\" : \"", origin.topOrigin.toString(), "\", \"clientOrigin\": \"", origin.clientOrigin.toString(), "\" }, \"caches\" : ");
            originBuilder.append(originStorageManager(origin).cacheStorageRepresentation(m_queue.copyRef()));
            originBuilder.append('}');
            originStrings.append(originBuilder.toString());
        }
        removeOriginStorageManagerIfPossible(origin);
    }

    std::sort(originStrings.begin(), originStrings.end(), [](auto& a, auto& b) {
        return codePointCompareLessThan(a, b);
    });
    StringBuilder builder;
    builder.append("{ \"path\": \"", m_customCacheStoragePath, "\", \"origins\": [");
    const char* divider = "";
    for (auto& origin : originStrings) {
        builder.append(divider, origin);
        divider = ",";
    }
    builder.append("]}");
    callback(builder.toString());
}

} // namespace WebKit

