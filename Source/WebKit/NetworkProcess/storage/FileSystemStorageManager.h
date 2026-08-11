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

#pragma once

#include "FileSystemStorageHandle.h"
#include "FileSystemStorageManagerLock.h"
#include <WebCore/FileSystemHandleGlobalIdentifier.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemHandleKind.h>
#include <WebCore/FileSystemHandleRecord.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class FileSystemStorageHandle;
class FileSystemStorageHandleRegistry;

class FileSystemStorageManager final : public RefCountedAndCanMakeWeakPtr<FileSystemStorageManager> {
    WTF_MAKE_TZONE_ALLOCATED(FileSystemStorageManager);
public:
    using QuotaCheckFunction = Function<void(uint64_t spaceRequested, CompletionHandler<void(bool)>&&)>;
    static Ref<FileSystemStorageManager> create(String&& path, FileSystemStorageHandleRegistry&, QuotaCheckFunction&&);
    ~FileSystemStorageManager();

    bool NODELETE isActive() const;
    uint64_t allocatedUnusedCapacity() const;
    Expected<std::pair<WebCore::FileSystemHandleGlobalIdentifier, WebCore::FileSystemHandleIdentifier>, FileSystemStorageError> createHandle(IPC::Connection::UniqueID, FileSystemStorageHandle::Type, String&& path, String&& name, bool createIfNecessary);
    const String& NODELETE getPath(WebCore::FileSystemHandleIdentifier);
    const String& rootPath() const LIFETIME_BOUND { return m_path; }
    FileSystemStorageHandle::Type NODELETE getType(WebCore::FileSystemHandleIdentifier);
    void closeHandle(FileSystemStorageHandle&);
    void connectionClosed(IPC::Connection::UniqueID);
    Expected<std::pair<WebCore::FileSystemHandleGlobalIdentifier, WebCore::FileSystemHandleIdentifier>, FileSystemStorageError> getDirectory(IPC::Connection::UniqueID);

    struct GlobalIdentifierEntry {
        WebCore::FileSystemHandleKind kind;
        String path;
        String name;
        CheckedUint32 refcount { 0 };
    };

    void addGlobalIdentifierReference(WebCore::FileSystemHandleGlobalIdentifier);
    void removeGlobalIdentifierReferences(std::span<const WebCore::FileSystemHandleGlobalIdentifier>);
    Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> resolveGlobalIdentifier(IPC::Connection::UniqueID, WebCore::FileSystemHandleGlobalIdentifier);

    std::optional<WebCore::FileSystemHandleRecord> lookupHandle(WebCore::FileSystemHandleGlobalIdentifier);
    std::optional<Vector<WebCore::FileSystemHandleRecord>> lookupHandles(std::span<const WebCore::FileSystemHandleGlobalIdentifier>);
    void registerPersistedHandle(WebCore::FileSystemHandleGlobalIdentifier, WebCore::FileSystemHandleKind, String&& path, String&& name);
    void registerPersistedHandlesAndAddReferences(const Vector<WebCore::FileSystemHandleRecord>&);

    enum class LockType : bool { Exclusive, Shared };
    bool acquireLockForFile(const String& path, LockType);
    bool releaseLockForFile(const String& path);
    bool hasActiveLock(const String& path) const;
    void requestSpace(uint64_t spaceRequested, CompletionHandler<void(bool)>&&);

private:
    FileSystemStorageManager(String&& path, FileSystemStorageHandleRegistry&, QuotaCheckFunction&&);

    void close();
    void removeGlobalIdentifierReference(WebCore::FileSystemHandleGlobalIdentifier);

    using Lock = FileSystemStorageManagerLock;

    String m_path;
    WeakPtr<FileSystemStorageHandleRegistry> m_registry;
    QuotaCheckFunction m_quotaCheckFunction;
    HashMap<IPC::Connection::UniqueID, HashSet<WebCore::FileSystemHandleIdentifier>> m_handlesByConnection;
    HashMap<WebCore::FileSystemHandleIdentifier, Ref<FileSystemStorageHandle>> m_handles;
    HashMap<String, Lock> m_lockMap;
    HashMap<WebCore::FileSystemHandleGlobalIdentifier, GlobalIdentifierEntry> m_globalIdentifierRegistry;
};

} // namespace WebKit
