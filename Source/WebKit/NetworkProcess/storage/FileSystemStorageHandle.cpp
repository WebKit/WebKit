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
#include "FileSystemStorageHandle.h"

#include "FileSystemStorageError.h"
#include "FileSystemStorageManager.h"
#include "SharedFileHandle.h"
#include <WebCore/FileSystemWriteCloseReason.h>
#include <WebCore/FileSystemWriteCommandType.h>
#include <wtf/FileSystem.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

#if OS(WINDOWS)
constexpr char pathSeparator = '\\';
#else
constexpr char pathSeparator = '/';
#endif
constexpr uint64_t defaultInitialCapacity = 1 * MB;
constexpr uint64_t defaultMaxCapacityForExponentialGrowth = 256 * MB;
constexpr uint64_t defaultCapacityStep = 128 * MB;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileSystemStorageHandle);

RefPtr<FileSystemStorageHandle> FileSystemStorageHandle::create(FileSystemStorageManager& manager, Type type, String&& path, String&& name)
{
    bool canAccess = false;
    switch (type) {
    case FileSystemStorageHandle::Type::Directory:
        canAccess = FileSystem::makeAllDirectories(path);
        break;
    case FileSystemStorageHandle::Type::File:
        if (auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite); handle)
            canAccess = true;
        break;
    case FileSystemStorageHandle::Type::Any:
        ASSERT_NOT_REACHED();
    }

    if (!canAccess)
        return nullptr;

    return adoptRef(*new FileSystemStorageHandle(manager, type, WTFMove(path), WTFMove(name)));
}

FileSystemStorageHandle::FileSystemStorageHandle(FileSystemStorageManager& manager, Type type, String&& path, String&& name)
    : m_manager(manager)
    , m_type(type)
    , m_path(WTFMove(path))
    , m_name(WTFMove(name))
{
    ASSERT(!m_path.isEmpty());
}

void FileSystemStorageHandle::close()
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return;

    if (m_activeSyncAccessHandle)
        closeSyncAccessHandle(m_activeSyncAccessHandle->identifier);

    auto activeWritableFileIdentifiers = copyToVector(m_activeWritableFiles.keys());
    for (auto identifier : activeWritableFileIdentifiers)
        closeWritable(identifier, WebCore::FileSystemWriteCloseReason::Aborted);
    ASSERT(m_activeWritableFiles.isEmpty());

    manager->closeHandle(*this);
}

bool FileSystemStorageHandle::isSameEntry(WebCore::FileSystemHandleIdentifier identifier)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return false;

    auto path = manager->getPath(identifier);
    if (path.isEmpty())
        return false;

    return m_path == path;
}

static bool isValidFileName(const String& directory, const String& name)
{
    // https://fs.spec.whatwg.org/#valid-file-name
    if (name.isEmpty() || (name == "."_s) || (name == ".."_s) || name.contains(pathSeparator))
        return false;

    return FileSystem::pathFileName(FileSystem::pathByAppendingComponent(directory, name)) == name;
}

Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> FileSystemStorageHandle::requestCreateHandle(IPC::Connection::UniqueID connection, Type type, String&& name, bool createIfNecessary)
{
    if (m_type != FileSystemStorageHandle::Type::Directory)
        return makeUnexpected(FileSystemStorageError::TypeMismatch);

    RefPtr manager = m_manager.get();
    if (!manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    if (!isValidFileName(m_path, name))
        return makeUnexpected(FileSystemStorageError::InvalidName);

    auto path = FileSystem::pathByAppendingComponent(m_path, name);
    return manager->createHandle(connection, type, WTFMove(path), WTFMove(name), createIfNecessary);
}

Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> FileSystemStorageHandle::getFileHandle(IPC::Connection::UniqueID connection, String&& name, bool createIfNecessary)
{
    return requestCreateHandle(connection, FileSystemStorageHandle::Type::File, WTFMove(name), createIfNecessary);
}

Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> FileSystemStorageHandle::getDirectoryHandle(IPC::Connection::UniqueID connection, String&& name, bool createIfNecessary)
{
    return requestCreateHandle(connection, FileSystemStorageHandle::Type::Directory, WTFMove(name), createIfNecessary);
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::removeEntry(const String& name, bool deleteRecursively)
{
    if (m_type != Type::Directory)
        return FileSystemStorageError::TypeMismatch;

    if (!isValidFileName(m_path, name))
        return FileSystemStorageError::InvalidName;

    auto path = FileSystem::pathByAppendingComponent(m_path, name);
    if (!FileSystem::fileExists(path))
        return FileSystemStorageError::FileNotFound;

    auto type = FileSystem::fileType(path);
    if (!type)
        return FileSystemStorageError::TypeMismatch;

    std::optional<FileSystemStorageError> result;
    switch (type.value()) {
    case FileSystem::FileType::Regular:
        if (!FileSystem::deleteFile(path))
            result = FileSystemStorageError::Unknown;
        break;
    case FileSystem::FileType::Directory:
        if (!deleteRecursively) {
            if (!FileSystem::deleteEmptyDirectory(path))
                result = FileSystemStorageError::Unknown;
        } else if (!FileSystem::deleteNonEmptyDirectory(path))
            result = FileSystemStorageError::Unknown;
        break;
    case FileSystem::FileType::SymbolicLink:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return result;
}

Expected<Vector<String>, FileSystemStorageError> FileSystemStorageHandle::resolve(WebCore::FileSystemHandleIdentifier identifier)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto path = manager->getPath(identifier);
    if (path.isEmpty())
        return makeUnexpected(FileSystemStorageError::Unknown);

    if (!path.startsWith(m_path))
        return Vector<String> { };

    auto restPath = path.substring(m_path.length());
    return restPath.split(pathSeparator);
}

Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError> FileSystemStorageHandle::createSyncAccessHandle()
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    bool acquired = manager->acquireLockForFile(m_path, FileSystemStorageManager::LockType::Exclusive);
    if (!acquired)
        return makeUnexpected(FileSystemStorageError::InvalidState);

    auto handle = FileSystem::openFile(m_path, FileSystem::FileOpenMode::ReadWrite);
    if (!handle)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto ipcHandle = IPC::SharedFileHandle::create(WTFMove(handle));
    if (!ipcHandle)
        return makeUnexpected(FileSystemStorageError::BackendNotSupported);

    ASSERT(!m_activeSyncAccessHandle);
    m_activeSyncAccessHandle = SyncAccessHandleInfo { WebCore::FileSystemSyncAccessHandleIdentifier::generate() };
    uint64_t initialCapacity = valueOrDefault(FileSystem::fileSize(m_path));
    return FileSystemSyncAccessHandleInfo { m_activeSyncAccessHandle->identifier, WTFMove(*ipcHandle), initialCapacity };
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::closeSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier)
{
    if (!m_activeSyncAccessHandle || m_activeSyncAccessHandle->identifier != accessHandleIdentifier)
        return FileSystemStorageError::Unknown;

    RefPtr manager = m_manager.get();
    if (!manager)
        return FileSystemStorageError::Unknown;

    manager->releaseLockForFile(m_path);
    m_activeSyncAccessHandle = std::nullopt;

    return std::nullopt;
}

Expected<WebCore::FileSystemWritableFileStreamIdentifier, FileSystemStorageError> FileSystemStorageHandle::createWritable(bool keepExistingData)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    if (!FileSystem::fileExists(m_path))
        return makeUnexpected(FileSystemStorageError::FileNotFound);

    bool acquired = manager->acquireLockForFile(m_path, FileSystemStorageManager::LockType::Shared);
    if (!acquired)
        return makeUnexpected(FileSystemStorageError::InvalidState);

    auto path = FileSystem::createTemporaryFile("FileSystemWritableStream"_s);
    if (keepExistingData)
        FileSystem::copyFile(path, m_path);

    auto streamIdentifier = WebCore::FileSystemWritableFileStreamIdentifier::generate();
    ASSERT(!m_activeWritableFiles.contains(streamIdentifier));

    auto activeWritableFile = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite);
    if (!activeWritableFile)
        return makeUnexpected(FileSystemStorageError::Unknown);

    m_activeWritableFiles.add(streamIdentifier, FileHandleWithPath { WTFMove(activeWritableFile), WTFMove(path) });
    return streamIdentifier;
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::closeWritable(WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCloseReason reason)
{
    auto iterator = m_activeWritableFiles.find(streamIdentifier);
    if (iterator == m_activeWritableFiles.end())
        return FileSystemStorageError::InvalidState;

    auto activeWritableFile = m_activeWritableFiles.take(iterator);
    RefPtr manager = m_manager.get();
    if (!manager)
        return FileSystemStorageError::Unknown;

    manager->releaseLockForFile(m_path);

    if (reason == WebCore::FileSystemWriteCloseReason::Aborted) {
        activeWritableFile.handle = { };
        FileSystem::deleteFile(activeWritableFile.path);
        return std::nullopt;
    }

    ASSERT(!activeWritableFile.path.isEmpty());
    if (!FileSystem::copyFile(m_path, activeWritableFile.path))
        return FileSystemStorageError::Unknown;

    return std::nullopt;
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::executeCommandForWritableInternal(WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCommandType type, std::optional<uint64_t> position, std::optional<uint64_t> size, std::span<const uint8_t> dataBytes, bool hasDataError)
{
    auto iterator = m_activeWritableFiles.find(streamIdentifier);
    if (iterator == m_activeWritableFiles.end())
        return FileSystemStorageError::InvalidState;

    auto& activeWritableFile = iterator->value;

    if (hasDataError)
        return FileSystemStorageError::InvalidDataType;

    switch (type) {
    case WebCore::FileSystemWriteCommandType::Write: {
        if (position) {
            auto result = activeWritableFile.handle.seek(*position, FileSystem::FileSeekOrigin::Beginning);
            if (!result)
                return FileSystemStorageError::Unknown;
        }

        if (!activeWritableFile.handle.write(dataBytes))
            return FileSystemStorageError::Unknown;

        return std::nullopt;
    }
    case WebCore::FileSystemWriteCommandType::Seek: {
        if (!position)
            return FileSystemStorageError::MissingArgument;

        auto result = activeWritableFile.handle.seek(*position, FileSystem::FileSeekOrigin::Beginning);
        if (!result)
            return FileSystemStorageError::Unknown;

        return std::nullopt;
    }
    case WebCore::FileSystemWriteCommandType::Truncate: {
        if (!size)
            return FileSystemStorageError::MissingArgument;

        bool truncated = activeWritableFile.handle.truncate(*size);
        if (!truncated)
            return FileSystemStorageError::Unknown;

        auto currentOffset = activeWritableFile.handle.seek(0, FileSystem::FileSeekOrigin::Current);
        if (!currentOffset || *currentOffset > *size)
            activeWritableFile.handle.seek(*size, FileSystem::FileSeekOrigin::Beginning);

        return std::nullopt;
    }
    }

    ASSERT_NOT_REACHED();
    return FileSystemStorageError::Unknown;
}

std::optional<size_t> FileSystemStorageHandle::computeCommandSpace(WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCommandType type, std::optional<uint64_t> position, std::optional<uint64_t> size, std::span<const uint8_t> dataBytes, bool hasDataError)
{
    if (hasDataError)
        return 0;
    if (type != WebCore::FileSystemWriteCommandType::Write && type != WebCore::FileSystemWriteCommandType::Truncate)
        return 0;

    auto iterator = m_activeWritableFiles.find(streamIdentifier);
    if (iterator == m_activeWritableFiles.end())
        return { };

    auto& activeWritableFile = iterator->value;

    auto fileSize = FileSystem::fileSize(m_path);
    if (!fileSize)
        return { };

    if (type == WebCore::FileSystemWriteCommandType::Truncate)
        return *size > *fileSize ? *size - *fileSize : 0;

    uint64_t finalSize;
    auto currentOffset = activeWritableFile.handle.seek(position.value_or(0), FileSystem::FileSeekOrigin::Current);
    if (!currentOffset)
        return { };

    if (!WTF::safeAdd(*currentOffset, dataBytes.size(), finalSize))
        return { };

    return finalSize > *fileSize ? finalSize - *fileSize : 0;
}

void FileSystemStorageHandle::executeCommandForWritable(WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCommandType type, std::optional<uint64_t> position, std::optional<uint64_t> size, std::span<const uint8_t> dataBytes, bool hasDataError, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    auto spaceRequired = computeCommandSpace(streamIdentifier, type, position, size, dataBytes, hasDataError);
    RefPtr manager = m_manager.get();
    if (!spaceRequired || !manager) {
        closeWritable(streamIdentifier, WebCore::FileSystemWriteCloseReason::Aborted);
        completionHandler(FileSystemStorageError::Unknown);
        return;
    }

    manager->requestSpace(*spaceRequired, [weakThis = WeakPtr { *this }, streamIdentifier, type, position, size, dataBytes = Vector<uint8_t>(dataBytes), completionHandler = WTFMove(completionHandler)](bool granted) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            completionHandler(FileSystemStorageError::Unknown);
            return;
        }

        if (!granted) {
            protectedThis->closeWritable(streamIdentifier, WebCore::FileSystemWriteCloseReason::Aborted);
            completionHandler(FileSystemStorageError::QuotaError);
            return;
        }

        auto error = protectedThis->executeCommandForWritableInternal(streamIdentifier, type, position, size, dataBytes.span(), false);
        if (error)
            protectedThis->closeWritable(streamIdentifier, WebCore::FileSystemWriteCloseReason::Aborted);

        completionHandler(error);
    });
}

Vector<WebCore::FileSystemWritableFileStreamIdentifier> FileSystemStorageHandle::writables() const
{
    return copyToVector(m_activeWritableFiles.keys());
}

Expected<Vector<String>, FileSystemStorageError> FileSystemStorageHandle::getHandleNames()
{
    if (m_type != Type::Directory)
        return makeUnexpected(FileSystemStorageError::TypeMismatch);

    return FileSystem::listDirectory(m_path);
}

Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError> FileSystemStorageHandle::getHandle(IPC::Connection::UniqueID connection, String&& name)
{
    bool createIfNecessary = false;
    auto result = requestCreateHandle(connection, FileSystemStorageHandle::Type::Any, WTFMove(name), createIfNecessary);
    if (!result)
        return makeUnexpected(result.error());

    RefPtr manager = m_manager.get();
    if (!manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto resultType = manager->getType(result.value());
    ASSERT(resultType != FileSystemStorageHandle::Type::Any);
    return std::pair { result.value(), resultType == FileSystemStorageHandle::Type::Directory };
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::move(WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return FileSystemStorageError::Unknown;

    // Do not move file if there is ongoing operation.
    if (m_activeSyncAccessHandle)
        return FileSystemStorageError::AccessHandleActive;

    if (manager->getType(destinationIdentifier) != Type::Directory)
        return FileSystemStorageError::TypeMismatch;

    auto path = manager->getPath(destinationIdentifier);
    if (path.isEmpty())
        return FileSystemStorageError::Unknown;

    if (!isValidFileName(path, newName))
        return FileSystemStorageError::InvalidName;

    auto destinationPath = FileSystem::pathByAppendingComponent(path, newName);
    if (!FileSystem::moveFile(m_path, destinationPath))
        return FileSystemStorageError::Unknown;

    m_path = destinationPath;
    m_name = newName;

    return std::nullopt;
}

std::optional<WebCore::FileSystemSyncAccessHandleIdentifier> FileSystemStorageHandle::activeSyncAccessHandle()
{
    if (!m_activeSyncAccessHandle)
        return std::nullopt;

    return m_activeSyncAccessHandle->identifier;
}

bool FileSystemStorageHandle::isActiveSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier)
{
    return m_activeSyncAccessHandle && m_activeSyncAccessHandle->identifier == accessHandleIdentifier;
}

uint64_t FileSystemStorageHandle::allocatedUnusedCapacity()
{
    if (!m_activeSyncAccessHandle)
        return 0;

    auto actualSize = valueOrDefault(FileSystem::fileSize(m_path));
    return actualSize > m_activeSyncAccessHandle->capacity ? 0 : m_activeSyncAccessHandle->capacity - actualSize;
}

void FileSystemStorageHandle::requestNewCapacityForSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    if (!isActiveSyncAccessHandle(accessHandleIdentifier))
        return completionHandler(std::nullopt);

    uint64_t currentCapacity = m_activeSyncAccessHandle->capacity;
    if (newCapacity <= currentCapacity)
        return completionHandler(currentCapacity);

    RefPtr manager = m_manager.get();
    if (!manager)
        return completionHandler(std::nullopt);

    if (newCapacity < defaultInitialCapacity)
        newCapacity = defaultInitialCapacity;
    else if (newCapacity < defaultMaxCapacityForExponentialGrowth)
        newCapacity = pow(2, (int)std::log2(newCapacity) + 1);
    else
        newCapacity = defaultCapacityStep * ((newCapacity / defaultCapacityStep) + 1);

    manager->requestSpace(newCapacity - currentCapacity, [weakThis = WeakPtr { *this }, accessHandleIdentifier, newCapacity, completionHandler = WTFMove(completionHandler)](bool granted) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(std::nullopt);

        if (!protectedThis->isActiveSyncAccessHandle(accessHandleIdentifier))
            return completionHandler(std::nullopt);

        if (granted)
            protectedThis->m_activeSyncAccessHandle->capacity = newCapacity;
        completionHandler(protectedThis->m_activeSyncAccessHandle->capacity);
    });
}

} // namespace WebKit
