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

#include "CacheStorageEngine.h"
#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"
#include "IDBStorageConnectionToClient.h"
#include "IDBStorageManager.h"
#include "IDBStorageRegistry.h"
#include "LocalStorageManager.h"
#include "Logging.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkStorageManagerMessages.h"
#include "OriginStorageManager.h"
#include "QuotaManager.h"
#include "SessionStorageManager.h"
#include "StorageAreaBase.h"
#include "StorageAreaMapMessages.h"
#include "StorageAreaRegistry.h"
#include "WebsiteDataType.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <WebCore/UniqueIDBDatabaseTransaction.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/Scope.h>
#include <wtf/SuspendableWorkQueue.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/Base64.h>

namespace WebKit {

static std::optional<WebCore::ClientOrigin> readOriginFromFile(const String& filePath)
{
    ASSERT(!RunLoop::isMain());

    if (filePath.isEmpty() || !FileSystem::fileExists(filePath))
        return std::nullopt;

    auto originFileHandle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Read);
    auto closeFile = makeScopeExit([&] {
        FileSystem::closeFile(originFileHandle);
    });

    if (!FileSystem::isHandleValid(originFileHandle))
        return std::nullopt;

    auto originContent = FileSystem::readEntireFile(originFileHandle);
    if (!originContent)
        return std::nullopt;

    WTF::Persistence::Decoder decoder({ originContent->data(), originContent->size() });
    std::optional<WebCore::ClientOrigin> origin;
    decoder >> origin;
    return origin;
}

static void writeOriginToFileIfNecessary(const String& filePath, const WebCore::ClientOrigin& origin)
{
    if (filePath.isEmpty() || FileSystem::fileExists(filePath))
        return;

    FileSystem::makeAllDirectories(FileSystem::parentPath(filePath));
    auto originFileHandle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::ReadWrite);
    auto closeFile = makeScopeExit([&] {
        FileSystem::closeFile(originFileHandle);
    });

    if (!FileSystem::isHandleValid(originFileHandle)) {
        LOG_ERROR("writeOriginToFileIfNecessary: Failed to open origin file '%s'", filePath.utf8().data());
        return;
    }

    WTF::Persistence::Encoder encoder;
    encoder << origin;
    FileSystem::writeToFile(originFileHandle, encoder.buffer(), encoder.bufferSize());
}

static void deleteOriginFileIfNecessary(const String& filePath)
{
    auto parentPath = FileSystem::parentPath(filePath);
    auto children = FileSystem::listDirectory(parentPath);
    if (children.size() == 1)
        FileSystem::deleteFile(filePath);
}

Ref<NetworkStorageManager> NetworkStorageManager::create(PAL::SessionID sessionID, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, bool shouldUseCustomPaths)
{
    return adoptRef(*new NetworkStorageManager(sessionID, connection, path, customLocalStoragePath, customIDBStoragePath, customCacheStoragePath, defaultOriginQuota, defaultThirdPartyOriginQuota, shouldUseCustomPaths));
}

NetworkStorageManager::NetworkStorageManager(PAL::SessionID sessionID, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, bool shouldUseCustomPaths)
    : m_sessionID(sessionID)
    , m_queue(SuspendableWorkQueue::create("com.apple.WebKit.Storage"))
    , m_defaultOriginQuota(defaultOriginQuota)
    , m_defaultThirdPartyOriginQuota(defaultThirdPartyOriginQuota)
    , m_parentConnection(connection)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, path = path.isolatedCopy(), customLocalStoragePath = crossThreadCopy(customLocalStoragePath), customIDBStoragePath = crossThreadCopy(customIDBStoragePath), customCacheStoragePath = crossThreadCopy(customCacheStoragePath), shouldUseCustomPaths]() mutable {
        m_fileSystemStorageHandleRegistry = makeUnique<FileSystemStorageHandleRegistry>();
        m_storageAreaRegistry = makeUnique<StorageAreaRegistry>();
        m_idbStorageRegistry = makeUnique<IDBStorageRegistry>();
        m_shouldUseCustomPaths = shouldUseCustomPaths;
        m_path = path;
        m_customLocalStoragePath = customLocalStoragePath;
        m_customIDBStoragePath = customIDBStoragePath;
        m_customCacheStoragePath = customCacheStoragePath;
        if (!m_path.isEmpty()) {
            auto saltPath = FileSystem::pathByAppendingComponent(m_path, "salt");
            m_salt = valueOrDefault(FileSystem::readOrMakeSalt(saltPath));
        }
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
        || types.contains(WebsiteDataType::IndexedDBDatabases);
}

void NetworkStorageManager::close()
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return;
    m_closed = true;

    for (auto& connection : m_connections)
        connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());

    m_queue->dispatch([this, protectedThis = Ref { *this }]() mutable {
        m_localOriginStorageManagers.clear();
        m_fileSystemStorageHandleRegistry = nullptr;

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis)] { });
    });
}

void NetworkStorageManager::startReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    connection.addWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName(), m_queue.get(), this);
    m_connections.add(connection);
}

void NetworkStorageManager::stopReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    
    if (!m_connections.remove(connection))
        return;

    connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    m_queue->dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID()]() mutable {
        m_idbStorageRegistry->removeConnectionToClient(connection);
        m_localOriginStorageManagers.removeIf([&](auto& entry) {
            auto& manager = entry.value;
            manager->connectionClosed(connection);
            return !manager->isActive();
        });

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis)] { });
    });
}

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

OriginStorageManager& NetworkStorageManager::localOriginStorageManager(const WebCore::ClientOrigin& origin, ShouldWriteOriginFile shouldWriteOriginFile)
{
    ASSERT(!RunLoop::isMain());

    return *m_localOriginStorageManagers.ensure(origin, [&] {
        auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
        if (!originDirectory.isEmpty() && shouldWriteOriginFile == ShouldWriteOriginFile::Yes)
            writeOriginToFileIfNecessary(originFilePath(originDirectory), origin);
        auto localStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, origin);
        auto idbStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, origin);
        auto cacheStoragePath = CacheStorage::Engine::storagePath(m_customCacheStoragePath, origin);
        uint64_t quota = origin.topOrigin == origin.clientOrigin ? m_defaultOriginQuota : m_defaultThirdPartyOriginQuota;
        QuotaManager::IncreaseQuotaFunction increaseQuotaFunction = [sessionID = m_sessionID, origin, connection = m_parentConnection] (auto identifier, auto currentQuota, auto currentUsage, auto requestedIncrease) mutable {
            IPC::Connection::send(connection, Messages::NetworkProcessProxy::IncreaseQuota(sessionID, origin, identifier, currentQuota, currentUsage, requestedIncrease), 0);
        };
        return makeUnique<OriginStorageManager>(quota, WTFMove(increaseQuotaFunction), WTFMove(originDirectory), WTFMove(localStoragePath), WTFMove(idbStoragePath), WTFMove(cacheStoragePath), m_shouldUseCustomPaths);
    }).iterator->value;
}

bool NetworkStorageManager::removeOriginStorageManagerIfPossible(const WebCore::ClientOrigin& origin)
{
    auto iterator = m_localOriginStorageManagers.find(origin);
    if (iterator == m_localOriginStorageManagers.end())
        return true;

    if (iterator->value->isActive())
        return false;

    m_localOriginStorageManagers.remove(iterator);
    return true;
}

void NetworkStorageManager::deleteOriginDirectoryIfPossible(const WebCore::ClientOrigin& origin)
{
    bool isEmpty = localOriginStorageManager(origin).isEmpty();
    bool removed = removeOriginStorageManagerIfPossible(origin);
    if (!removed || !isEmpty)
        return;

    auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
    auto filePath = originFilePath(originDirectory);
    deleteOriginFileIfNecessary(filePath);
    FileSystem::deleteEmptyDirectory(originDirectory);
}

void NetworkStorageManager::persisted(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(localOriginStorageManager(origin).persisted());
}

void NetworkStorageManager::persist(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    localOriginStorageManager(origin).setPersisted(true);
    completionHandler(true);
}

void NetworkStorageManager::clearStorageForTesting(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        // Reset persisted value.
        for (auto& manager : m_localOriginStorageManagers.values())
            manager->setPersisted(false);

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::didIncreaseQuota(const WebCore::ClientOrigin& origin, QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin), identifier, newQuota]() mutable {
        if (auto manager = m_localOriginStorageManagers.get(origin))
            manager->quotaManager().didIncreaseQuota(identifier, newQuota);
    });
}

void NetworkStorageManager::fileSystemGetDirectory(IPC::Connection& connection, const WebCore::ClientOrigin& origin, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(localOriginStorageManager(origin).fileSystemStorageManager(*m_fileSystemStorageHandleRegistry).getDirectory(connection.uniqueID()));
}

void NetworkStorageManager::closeHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier))
        handle->close();
}

void NetworkStorageManager::isSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(false);

    completionHandler(handle->isSameEntry(targetIdentifier));
}

void NetworkStorageManager::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->move(destinationIdentifier, newName));
}

void NetworkStorageManager::getFileHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getFileHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void NetworkStorageManager::getDirectoryHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getDirectoryHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void NetworkStorageManager::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->removeEntry(name, deleteRecursively));
}

void NetworkStorageManager::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->resolve(targetIdentifier));
}

void NetworkStorageManager::getFile(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->path());
}

void NetworkStorageManager::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<AccessHandleInfo, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->createSyncAccessHandle());
}

void NetworkStorageManager::closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->closeSyncAccessHandle(accessHandleIdentifier));
}

void NetworkStorageManager::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getHandleNames());
}

void NetworkStorageManager::getHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getHandle(connection.uniqueID(), WTFMove(name)));
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
    for (auto& origin : m_localOriginStorageManagers.keys())
        allOrigins.add(origin);

    forEachOriginDirectory([&](auto directory) {
        if (auto origin = readOriginFromFile(originFilePath(directory)))
            allOrigins.add(*origin);
    });

    for (auto& origin : LocalStorageManager::originsOfLocalStorageData(m_customLocalStoragePath))
        allOrigins.add(WebCore::ClientOrigin { origin, origin });

    for (auto& origin : IDBStorageManager::originsOfIDBStorageData(m_customIDBStoragePath))
        allOrigins.add(origin);

    return allOrigins;
}

Vector<WebsiteData::Entry> NetworkStorageManager::fetchDataFromDisk(OptionSet<WebsiteDataType> targetTypes)
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::SecurityOriginData, OptionSet<WebsiteDataType>> originTypes;
    for (auto& origin : getAllOrigins()) {
        auto types = localOriginStorageManager(origin).fetchDataTypesInList(targetTypes);
        originTypes.add(origin.clientOrigin, OptionSet<WebsiteDataType> { }).iterator->value.add(types);
        originTypes.add(origin.topOrigin, OptionSet<WebsiteDataType> { }).iterator->value.add(types);
        removeOriginStorageManagerIfPossible(origin);
    }

    Vector<WebsiteData::Entry> entries;
    for (auto [origin, types] : originTypes) {
        for (auto type : types)
            entries.append({ WebsiteData::Entry { origin, type, 0 } });
    }

    return entries;
}

void NetworkStorageManager::fetchData(OptionSet<WebsiteDataType> types, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, completionHandler = WTFMove(completionHandler)]() mutable {
        auto entries = fetchDataFromDisk(types);
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

        auto existingDataTypes = localOriginStorageManager(origin).fetchDataTypesInList(types);
        if (!existingDataTypes.isEmpty()) {
            deletedOrigins.add(origin);
            localOriginStorageManager(origin).deleteData(types, modifiedSinceTime);
        }
        deleteOriginDirectoryIfPossible(origin);
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
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host);
            return domains.contains(domain);
        });

        HashSet<WebCore::RegistrableDomain> deletedDomains;
        for (auto origin : deletedOrigins) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host);
            deletedDomains.add(domain);
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), domains = crossThreadCopy(WTFMove(deletedDomains))]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void NetworkStorageManager::moveData(OptionSet<WebsiteDataType> types, const WebCore::SecurityOriginData& source, const WebCore::SecurityOriginData& target, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, source = crossThreadCopy(source), target = crossThreadCopy(target), completionHandler = WTFMove(completionHandler)]() mutable {
        auto sourceOrigin = WebCore::ClientOrigin { source, source };
        auto targetOrigin = WebCore::ClientOrigin { target, target };
        
        // Clear existing data of target origin.
        localOriginStorageManager(targetOrigin).deleteData(types, -WallTime::infinity());

        // Move data from source origin to target origin.
        localOriginStorageManager(sourceOrigin).moveData(types, localOriginStorageManager(targetOrigin).resolvedLocalStoragePath(), localOriginStorageManager(targetOrigin).resolvedIDBStoragePath());

        removeOriginStorageManagerIfPossible(targetOrigin);
        removeOriginStorageManagerIfPossible(sourceOrigin);

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->suspend([this, protectedThis = Ref { *this }] {
        for (auto& manager : m_localOriginStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
            if (auto idbStorageManager = manager->existingIDBStorageManager())
                idbStorageManager->stopDatabaseActivitiesForSuspend();
        }
    }, WTFMove(completionHandler));
}

void NetworkStorageManager::resume()
{
    ASSERT(RunLoop::isMain());

    m_queue->resume();
}

void NetworkStorageManager::handleLowMemoryWarning()
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }] {
        for (auto& manager : m_localOriginStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->handleLowMemoryWarning();
        }
    });
}

void NetworkStorageManager::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& manager : m_localOriginStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::registerTemporaryBlobFilePaths(IPC::Connection& connection, const Vector<String>& filePaths)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, connectionID = connection.uniqueID(), filePaths = crossThreadCopy(filePaths)] {
        auto& temporaryBlobPaths = m_temporaryBlobPathsByConnection.ensure(connectionID, [] {
            return HashSet<String> { };
        }).iterator->value;
        temporaryBlobPaths.add(filePaths.begin(), filePaths.end());
    });
}

void NetworkStorageManager::requestSpace(const WebCore::ClientOrigin& origin, uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin), size, completionHandler = WTFMove(completionHandler)]() mutable {
        localOriginStorageManager(origin).quotaManager().requestSpace(size, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), decision]() mutable {
                completionHandler(decision == QuotaManager::Decision::Grant);
            });
        });
    });
}

void NetworkStorageManager::resetQuotaForTesting(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        for (auto& manager : m_localOriginStorageManagers.values())
            manager->quotaManager().resetQuotaForTesting();
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::resetQuotaUpdatedBasedOnUsageForTesting(const WebCore::ClientOrigin& origin)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin)]() mutable {
        if (auto manager = m_localOriginStorageManagers.get(origin))
            manager->quotaManager().resetQuotaUpdatedBasedOnUsageForTesting();
    });
}

void NetworkStorageManager::connectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, StorageNamespaceIdentifier namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto connectionIdentifier = connection.uniqueID();
    bool willCreateOriginStorageManager = !m_localOriginStorageManagers.contains(origin);
    // Avoid delay in replying sync message by writing origin file after replying message.
    auto& originStorageManager = localOriginStorageManager(origin, ShouldWriteOriginFile::No);
    auto writeOriginFile = makeScopeExit([&] {
        if (!willCreateOriginStorageManager)
            return;

        if (auto originDirectory = originDirectoryPath(m_path, origin, m_salt); !originDirectory.isEmpty())
            writeOriginToFileIfNecessary(originFilePath(originDirectory), origin);
    });

    StorageAreaIdentifier resultIdentifier;
    switch (type) {
    case WebCore::StorageType::Local:
        resultIdentifier = originStorageManager.localStorageManager(*m_storageAreaRegistry).connectToLocalStorageArea(connectionIdentifier, sourceIdentifier, origin, m_queue.copyRef());
        break;
    case WebCore::StorageType::TransientLocal:
        resultIdentifier = originStorageManager.localStorageManager(*m_storageAreaRegistry).connectToTransientLocalStorageArea(connectionIdentifier, sourceIdentifier, origin);
        break;
    case WebCore::StorageType::Session:
        resultIdentifier = originStorageManager.sessionStorageManager(*m_storageAreaRegistry).connectToSessionStorageArea(connectionIdentifier, sourceIdentifier, origin, namespaceIdentifier);
    }

    if (auto storageArea = m_storageAreaRegistry->getStorageArea(resultIdentifier))
        return completionHandler(resultIdentifier, storageArea->allItems(), StorageAreaBase::nextMessageIdentifier());

    return completionHandler(resultIdentifier, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
}

void NetworkStorageManager::connectToStorageAreaSync(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, StorageNamespaceIdentifier namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    connectToStorageArea(connection, type, sourceIdentifier, namespaceIdentifier, origin, WTFMove(completionHandler));
}

void NetworkStorageManager::disconnectFromStorageArea(IPC::Connection& connection, StorageAreaIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return;

    if (storageArea->storageType() == StorageAreaBase::StorageType::Local)
        localOriginStorageManager(storageArea->origin()).localStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
    else
        localOriginStorageManager(storageArea->origin()).sessionStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
}

void NetworkStorageManager::cloneSessionStorageNamespace(IPC::Connection& connection, StorageNamespaceIdentifier fromIdentifier, StorageNamespaceIdentifier toIdentifier)
{
    ASSERT(!RunLoop::isMain());

    for (auto& manager : m_localOriginStorageManagers.values()) {
        if (auto* sessionStorageManager = manager->existingSessionStorageManager())
            sessionStorageManager->cloneStorageArea(connection.uniqueID(), fromIdentifier, toIdentifier);
    }
}

void NetworkStorageManager::setItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool hasQuotaError = false;
    if (auto storageArea = m_storageAreaRegistry->getStorageArea(identifier)) {
        auto result = storageArea->setItem(connection.uniqueID(), implIdentifier, String { key }, WTFMove(value), WTFMove(urlString));
        if (!result)
            hasQuotaError = (result.error() == StorageError::QuotaExceeded);
    }

    completionHandler(hasQuotaError);
}

void NetworkStorageManager::removeItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto storageArea = m_storageAreaRegistry->getStorageArea(identifier))
        storageArea->removeItem(connection.uniqueID(), implIdentifier, WTFMove(key), WTFMove(urlString));

    completionHandler();
}

void NetworkStorageManager::clear(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto storageArea = m_storageAreaRegistry->getStorageArea(identifier))
        storageArea->clear(connection.uniqueID(), implIdentifier, WTFMove(urlString));

    completionHandler();
}

void NetworkStorageManager::openDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    localOriginStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).openDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::openDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    localOriginStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).openDBRequestCancelled(requestData);
}

void NetworkStorageManager::deleteDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    localOriginStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).deleteDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->establishTransaction(transactionInfo);
}

void NetworkStorageManager::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionPendingCloseFromClient();
}

void NetworkStorageManager::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void NetworkStorageManager::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->abortWithoutCallback();

    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void NetworkStorageManager::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void NetworkStorageManager::abortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->abort();
}

void NetworkStorageManager::commitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->commit(pendingRequestCount);
}

void NetworkStorageManager::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void NetworkStorageManager::createObjectStore(const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->createObjectStore(requestData, objectStoreInfo);
    }
}

void NetworkStorageManager::deleteObjectStore(const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->deleteObjectStore(requestData, objectStoreName);
    }
}

void NetworkStorageManager::renameObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
    }
}

void NetworkStorageManager::clearObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void NetworkStorageManager::createIndex(const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->createIndex(requestData, indexInfo);
}

void NetworkStorageManager::deleteIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void NetworkStorageManager::renameIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void NetworkStorageManager::putOrAdd(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, WebCore::IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
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

    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->putOrAdd(requestData, keyData, value, overwriteMode);
}

void NetworkStorageManager::getRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getRecord(requestData, getRecordData);
}

void NetworkStorageManager::getAllRecords(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getAllRecords(requestData, getAllRecordsData);
}

void NetworkStorageManager::getCount(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getCount(requestData, keyRangeData);
}

void NetworkStorageManager::deleteRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteRecord(requestData, keyRangeData);
}

void NetworkStorageManager::openCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->openCursor(requestData, cursorInfo);
}

void NetworkStorageManager::iterateCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& cursorData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->iterateCursor(requestData, cursorData);
}

void NetworkStorageManager::getAllDatabaseNamesAndVersions(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::ClientOrigin& origin)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestIdentifier.connectionIdentifier());
    auto result = localOriginStorageManager(origin).idbStorageManager(*m_idbStorageRegistry).getAllDatabaseNamesAndVersions();
    connectionToClient.didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(result));
}

} // namespace WebKit

