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
#include "FileSystemStorageManager.h"

#include "FileSystemStorageError.h"
#include "FileSystemStorageHandleRegistry.h"
#include "WebFileSystemStorageConnectionMessages.h"
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FileSystemStorageManager);

Ref<FileSystemStorageManager> FileSystemStorageManager::create(String&& path, FileSystemStorageHandleRegistry& registry, QuotaCheckFunction&& quotaCheckFunction)
{
    return adoptRef(*new FileSystemStorageManager(WTF::move(path), registry, WTF::move(quotaCheckFunction)));
}

FileSystemStorageManager::FileSystemStorageManager(String&& path, FileSystemStorageHandleRegistry& registry, QuotaCheckFunction&& quotaCheckFunction)
    : m_path(WTF::move(path))
    , m_registry(registry)
    , m_quotaCheckFunction(WTF::move(quotaCheckFunction))
{
    ASSERT(!RunLoop::isMain());
}

FileSystemStorageManager::~FileSystemStorageManager()
{
    ASSERT(!RunLoop::isMain());

    close();
}

bool FileSystemStorageManager::isActive() const
{
    return !m_handles.isEmpty();
}

uint64_t FileSystemStorageManager::allocatedUnusedCapacity() const
{
    CheckedUint64 result = 0;
    for (auto& handle : m_handles.values())
        result += handle->allocatedUnusedCapacity();

    if (result.hasOverflowed())
        return 0;

    return result;
}

Expected<std::pair<WebCore::FileSystemHandleGlobalIdentifier, WebCore::FileSystemHandleIdentifier>, FileSystemStorageError> FileSystemStorageManager::createHandle(IPC::Connection::UniqueID connection, FileSystemStorageHandle::Type type, String&& path, String&& name, bool createIfNecessary)
{
    ASSERT(!RunLoop::isMain());

    if (path.isEmpty())
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto fileExists = FileSystem::fileExists(path);
    if (!createIfNecessary && !fileExists)
        return makeUnexpected(FileSystemStorageError::FileNotFound);

    if (fileExists) {
        auto existingFileType = FileSystem::fileType(path);
        if (!existingFileType)
            return makeUnexpected(FileSystemStorageError::Unknown);

        auto existingHandleType = (existingFileType.value() == FileSystem::FileType::Regular) ? FileSystemStorageHandle::Type::File : FileSystemStorageHandle::Type::Directory;
        if (type == FileSystemStorageHandle::Type::Any)
            type = existingHandleType;
        else {
            // Requesting type and existing type should be a match.
            if (existingHandleType != type)
                return makeUnexpected(FileSystemStorageError::TypeMismatch);
        }
    }

    RefPtr newHandle = FileSystemStorageHandle::create(*this, type, String { path }, String { name });
    if (!newHandle)
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto globalIdentifier = WebCore::FileSystemHandleGlobalIdentifier::generate();
    newHandle->setGlobalIdentifier(globalIdentifier);

    auto newHandleIdentifier = newHandle->identifier();
    auto kind = newHandle->type() == FileSystemStorageHandle::Type::Directory ? WebCore::FileSystemHandleKind::Directory : WebCore::FileSystemHandleKind::File;
    m_handlesByConnection.ensure(connection, [&] {
        return HashSet<WebCore::FileSystemHandleIdentifier> { };
    }).iterator->value.add(newHandleIdentifier);
    if (RefPtr registry = m_registry.get())
        registry->registerHandle(newHandleIdentifier, *newHandle);
    m_handles.add(newHandleIdentifier, newHandle.releaseNonNull());

    m_globalIdentifierRegistry.add(globalIdentifier, GlobalIdentifierEntry { kind, WTF::move(path), WTF::move(name), CheckedUint32 { 1 } });

    return std::pair { globalIdentifier, newHandleIdentifier };
}

const String& FileSystemStorageManager::getPath(WebCore::FileSystemHandleIdentifier identifier)
{
    auto handle = m_handles.find(identifier);
    return handle == m_handles.end() ? emptyString() : handle->value->path();
}

FileSystemStorageHandle::Type FileSystemStorageManager::getType(WebCore::FileSystemHandleIdentifier identifier)
{
    auto handle = m_handles.find(identifier);
    return handle == m_handles.end() ? FileSystemStorageHandle::Type::Any : handle->value->type();
}

void FileSystemStorageManager::closeHandle(FileSystemStorageHandle& handle)
{
    auto identifier = handle.identifier();
    if (auto globalIdentifier = handle.globalIdentifier())
        removeGlobalIdentifierReference(*globalIdentifier);
    auto takenHandle = m_handles.take(identifier);
    ASSERT(takenHandle.get() == &handle);
    for (auto& handles : m_handlesByConnection.values()) {
        if (handles.remove(identifier))
            break;
    }
    if (RefPtr registry = m_registry.get())
        registry->unregisterHandle(identifier);
}

void FileSystemStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    ASSERT(!RunLoop::isMain());

    auto connectionHandles = m_handlesByConnection.find(connection);
    if (connectionHandles == m_handlesByConnection.end())
        return;

    auto identifiers = connectionHandles->value;
    for (auto identifier : identifiers) {
        if (RefPtr handle = m_handles.get(identifier))
            handle->close();
    }

    m_handlesByConnection.remove(connectionHandles);
}

Expected<std::pair<WebCore::FileSystemHandleGlobalIdentifier, WebCore::FileSystemHandleIdentifier>, FileSystemStorageError> FileSystemStorageManager::getDirectory(IPC::Connection::UniqueID connection)
{
    ASSERT(!RunLoop::isMain());

    return createHandle(connection, FileSystemStorageHandle::Type::Directory, String { m_path }, { }, true);
}

// https://fs.spec.whatwg.org/#file-entry-lock-take
bool FileSystemStorageManager::acquireLockForFile(const String& path, LockType lockType)
{
    auto iterator = m_lockMap.ensure(path, [] {
        return Lock { };
    }).iterator;

    auto& lock = iterator->value;
    if (lockType == LockType::Exclusive) {
        if (lock.state == Lock::State::Open) {
            lock.state = Lock::State::TakenExclusive;
            return true;
        }
    } else if (lockType == LockType::Shared) {
        if (lock.state == Lock::State::Open) {
            lock.state = Lock::State::TakenShared;
            lock.count = 1;
            return true;
        }
        if (lock.state == Lock::State::TakenShared) {
            ++lock.count;
            return true;
        }
    }

    return false;
}

// https://fs.spec.whatwg.org/#file-entry-lock-release
bool FileSystemStorageManager::releaseLockForFile(const String& path)
{
    auto iterator = m_lockMap.find(path);
    if (iterator == m_lockMap.end())
        return false;

    auto& lock = iterator->value;
    if (lock.state == Lock::State::TakenShared) {
        if (--lock.count)
            return false;
    }
    m_lockMap.remove(iterator);
    return true;
}

bool FileSystemStorageManager::hasActiveLock(const String& path) const
{
    auto prefix = makeString(path, FileSystem::pathSeparator);
    for (auto& [lockedPath, lock] : m_lockMap) {
        if (lock.state == Lock::State::Open)
            continue;
        if (lockedPath == path || lockedPath.startsWith(prefix))
            return true;
    }
    return false;
}

void FileSystemStorageManager::close()
{
    ASSERT(!RunLoop::isMain());

    for (auto& [connectionID, identifiers] : m_handlesByConnection) {
        for (auto identifier : identifiers) {
            auto takenHandle = m_handles.take(identifier);
            if (RefPtr registry = m_registry.get())
                registry->unregisterHandle(identifier);

            // Send messages to web process to invalidate active sync access handle and writables.
            if (auto accessHandleIdentifier = takenHandle->activeSyncAccessHandle())
                IPC::Connection::send(connectionID, Messages::WebFileSystemStorageConnection::InvalidateAccessHandle(*accessHandleIdentifier), 0);

            for (auto writableIdentifier : takenHandle->writables())
                IPC::Connection::send(connectionID, Messages::WebFileSystemStorageConnection::InvalidateWritable(writableIdentifier), 0);
        }
    }

    ASSERT(m_handles.isEmpty());
    m_handlesByConnection.clear();
    m_lockMap.clear();
}

void FileSystemStorageManager::requestSpace(uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    m_quotaCheckFunction(size, WTF::move(completionHandler));
}

void FileSystemStorageManager::addGlobalIdentifierReference(WebCore::FileSystemHandleGlobalIdentifier globalIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto it = m_globalIdentifierRegistry.find(globalIdentifier);
    if (it != m_globalIdentifierRegistry.end() && !it->value.refcount.hasOverflowed())
        it->value.refcount++;
}

void FileSystemStorageManager::removeGlobalIdentifierReference(WebCore::FileSystemHandleGlobalIdentifier globalIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto it = m_globalIdentifierRegistry.find(globalIdentifier);
    if (it == m_globalIdentifierRegistry.end())
        return;
    auto& entry = it->value;
    if (entry.refcount.hasOverflowed() || !entry.refcount)
        return;
    --entry.refcount;
    if (!entry.refcount)
        m_globalIdentifierRegistry.remove(it);
}

void FileSystemStorageManager::removeGlobalIdentifierReferences(std::span<const WebCore::FileSystemHandleGlobalIdentifier> identifiers)
{
    ASSERT(!RunLoop::isMain());

    for (auto& identifier : identifiers)
        removeGlobalIdentifierReference(identifier);
}

Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> FileSystemStorageManager::resolveGlobalIdentifier(IPC::Connection::UniqueID connection, WebCore::FileSystemHandleGlobalIdentifier globalIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto it = m_globalIdentifierRegistry.find(globalIdentifier);
    if (it == m_globalIdentifierRegistry.end())
        return makeUnexpected(FileSystemStorageError::Unknown);

    auto& entry = it->value;
    auto handleType = entry.kind == WebCore::FileSystemHandleKind::Directory ? FileSystemStorageHandle::Type::Directory : FileSystemStorageHandle::Type::File;
    auto result = createHandle(connection, handleType, String { entry.path }, String { entry.name }, false);
    if (!result)
        return makeUnexpected(result.error());

    auto& [autoGlobalIdentifier, newIdentifier] = result.value();
    // Replace the auto-generated global identifier with the existing one being resolved.
    m_globalIdentifierRegistry.remove(autoGlobalIdentifier);
    if (auto handleIt = m_handles.find(newIdentifier); handleIt != m_handles.end())
        handleIt->value->setGlobalIdentifier(globalIdentifier);
    addGlobalIdentifierReference(globalIdentifier);
    return newIdentifier;
}

std::optional<WebCore::FileSystemHandleRecord> FileSystemStorageManager::lookupHandle(WebCore::FileSystemHandleGlobalIdentifier globalIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto it = m_globalIdentifierRegistry.find(globalIdentifier);
    if (it == m_globalIdentifierRegistry.end())
        return std::nullopt;

    auto& entry = it->value;
    return WebCore::FileSystemHandleRecord { globalIdentifier, entry.kind, entry.path, entry.name };
}

std::optional<Vector<WebCore::FileSystemHandleRecord>> FileSystemStorageManager::lookupHandles(std::span<const WebCore::FileSystemHandleGlobalIdentifier> identifiers)
{
    ASSERT(!RunLoop::isMain());

    Vector<WebCore::FileSystemHandleRecord> records;
    records.reserveInitialCapacity(identifiers.size());
    for (auto& identifier : identifiers) {
        auto record = lookupHandle(identifier);
        if (!record)
            return std::nullopt;
        records.append(WTF::move(*record));
    }
    return records;
}

void FileSystemStorageManager::registerPersistedHandle(WebCore::FileSystemHandleGlobalIdentifier globalIdentifier, WebCore::FileSystemHandleKind kind, String&& path, String&& name)
{
    ASSERT(!RunLoop::isMain());

    m_globalIdentifierRegistry.ensure(globalIdentifier, [&] {
        return GlobalIdentifierEntry { kind, WTF::move(path), WTF::move(name), CheckedUint32 { 0 } };
    });
}

void FileSystemStorageManager::registerPersistedHandlesAndAddReferences(const Vector<WebCore::FileSystemHandleRecord>& records)
{
    ASSERT(!RunLoop::isMain());

    for (auto& record : records) {
        registerPersistedHandle(record.identifier, record.kind, String { record.path }, String { record.name });
        addGlobalIdentifierReference(record.identifier);
    }
}

} // namespace WebKit
