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
#include <wtf/Scope.h>

namespace WebKit {

#if OS(WINDOWS)
constexpr char pathSeparator = '\\';
#else
constexpr char pathSeparator = '/';
#endif

FileSystemStorageHandle::FileSystemStorageHandle(FileSystemStorageManager& manager, Type type, String&& path, String&& name)
    : m_identifier(WebCore::FileSystemHandleIdentifier::generateThreadSafe())
    , m_manager(manager)
    , m_type(type)
    , m_path(WTFMove(path))
    , m_name(WTFMove(name))
{
    ASSERT(!m_path.isEmpty());

    switch (m_type) {
    case FileSystemStorageHandle::Type::Directory:
        FileSystem::makeAllDirectories(m_path);
        return;
    case FileSystemStorageHandle::Type::File:
        if (!FileSystem::fileExists(m_path)) {
            auto handle = FileSystem::openFile(m_path, FileSystem::FileOpenMode::Write);
            FileSystem::closeFile(handle);
        }
        return;
    case FileSystemStorageHandle::Type::Any:
        ASSERT_NOT_REACHED();
    }
}

void FileSystemStorageHandle::close()
{
    if (!m_manager)
        return;

    if (m_activeSyncAccessHandle)
        closeSyncAccessHandle(*m_activeSyncAccessHandle);
    m_manager->closeHandle(*this);
}

bool FileSystemStorageHandle::isSameEntry(WebCore::FileSystemHandleIdentifier identifier)
{
    auto path = m_manager->getPath(identifier);
    if (path.isEmpty())
        return false;

    return m_path == path;
}

static bool isValidFileName(const String& name)
{
    return name != "." && name != ".." && !name.contains(pathSeparator);
}

Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> FileSystemStorageHandle::requestCreateHandle(IPC::Connection::UniqueID connection, Type type, String&& name, bool createIfNecessary)
{
    if (m_type != FileSystemStorageHandle::Type::Directory)
        return makeUnexpected(FileSystemStorageError::TypeMismatch);

    if (!m_manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    // https://wicg.github.io/file-system-access/#valid-file-name
    if (!isValidFileName(name))
        return makeUnexpected(FileSystemStorageError::InvalidName);

    auto path = FileSystem::pathByAppendingComponent(m_path, name);
    return m_manager->createHandle(connection, type, WTFMove(path), WTFMove(name), createIfNecessary);
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

    auto path = FileSystem::pathByAppendingComponent(m_path, name);
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
    if (!m_manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto path = m_manager->getPath(identifier);
    if (path.isEmpty())
        return makeUnexpected(FileSystemStorageError::Unknown);

    if (!path.startsWith(m_path))
        return Vector<String> { };

    auto restPath = path.substring(m_path.length());
    return restPath.split(pathSeparator);
}

Expected<FileSystemStorageHandle::AccessHandleInfo, FileSystemStorageError> FileSystemStorageHandle::createSyncAccessHandle()
{
    if (!m_manager)
        return makeUnexpected(FileSystemStorageError::Unknown);

    bool acquired = m_manager->acquireLockForFile(m_path, m_identifier);
    if (!acquired)
        return makeUnexpected(FileSystemStorageError::InvalidState);

    auto handle = FileSystem::openFile(m_path, FileSystem::FileOpenMode::ReadWrite);
    if (handle == FileSystem::invalidPlatformFileHandle)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto ipcHandle = IPC::SharedFileHandle::create(std::exchange(handle, FileSystem::invalidPlatformFileHandle));
    if (!ipcHandle) {
        FileSystem::closeFile(handle);
        return makeUnexpected(FileSystemStorageError::BackendNotSupported);
    }

    ASSERT(!m_activeSyncAccessHandle);
    m_activeSyncAccessHandle = WebCore::FileSystemSyncAccessHandleIdentifier::generateThreadSafe();
    return std::pair { *m_activeSyncAccessHandle, WTFMove(*ipcHandle) };
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::closeSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier)
{
    if (!m_activeSyncAccessHandle || *m_activeSyncAccessHandle != accessHandleIdentifier)
        return FileSystemStorageError::Unknown;

    if (!m_manager)
        return FileSystemStorageError::Unknown;

    m_manager->releaseLockForFile(m_path, m_identifier);
    m_activeSyncAccessHandle = std::nullopt;

    return std::nullopt;
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

    auto resultType = m_manager->getType(result.value());
    ASSERT(resultType != FileSystemStorageHandle::Type::Any);
    return std::pair { result.value(), resultType == FileSystemStorageHandle::Type::Directory };
}

std::optional<FileSystemStorageError> FileSystemStorageHandle::move(WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName)
{
    if (!m_manager)
        return FileSystemStorageError::Unknown;

    // Do not move file if there is ongoing operation.
    if (m_activeSyncAccessHandle)
        return FileSystemStorageError::AccessHandleActive;

    if (m_manager->getType(destinationIdentifier) != Type::Directory)
        return FileSystemStorageError::TypeMismatch;

    auto path = m_manager->getPath(destinationIdentifier);
    if (path.isEmpty())
        return FileSystemStorageError::Unknown;

    if (!isValidFileName(newName))
        return FileSystemStorageError::InvalidName;

    auto destinationPath = FileSystem::pathByAppendingComponent(path, newName);
    if (FileSystem::fileExists(destinationPath))
        return FileSystemStorageError::Unknown;

    if (!FileSystem::moveFile(m_path, destinationPath))
        return FileSystemStorageError::Unknown;

    m_path = destinationPath;
    m_name = newName;

    return std::nullopt;
}

} // namespace WebKit
