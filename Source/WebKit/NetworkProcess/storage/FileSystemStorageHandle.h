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

#include "Connection.h"
#include "FileSystemSyncAccessHandleInfo.h"
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class SharedFileHandle;
}

namespace WebKit {

class FileSystemStorageManager;
enum class FileSystemStorageError : uint8_t;

class FileSystemStorageHandle : public CanMakeWeakPtr<FileSystemStorageHandle, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : uint8_t { File, Directory, Any };
    static std::unique_ptr<FileSystemStorageHandle> create(FileSystemStorageManager&, Type, String&& path, String&& name);

    WebCore::FileSystemHandleIdentifier identifier() const { return m_identifier; }
    const String& path() const { return m_path; }
    Type type() const { return m_type; }
    uint64_t allocatedUnusedCapacity();

    void close();
    bool isSameEntry(WebCore::FileSystemHandleIdentifier);
    std::optional<FileSystemStorageError> move(WebCore::FileSystemHandleIdentifier, const String& newName);
    Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> getFileHandle(IPC::Connection::UniqueID, String&& name, bool createIfNecessary);
    Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> getDirectoryHandle(IPC::Connection::UniqueID, String&& name, bool createIfNecessary);
    std::optional<FileSystemStorageError> removeEntry(const String& name, bool deleteRecursively);
    Expected<Vector<String>, FileSystemStorageError> resolve(WebCore::FileSystemHandleIdentifier);
    Expected<Vector<String>, FileSystemStorageError> getHandleNames();
    Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError> getHandle(IPC::Connection::UniqueID, String&& name);
    void requestNewCapacityForSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&&);

    Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError> createSyncAccessHandle();
    std::optional<FileSystemStorageError> closeSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier);
    std::optional<WebCore::FileSystemSyncAccessHandleIdentifier> activeSyncAccessHandle();

private:
    FileSystemStorageHandle(FileSystemStorageManager&, Type, String&& path, String&& name);
    Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError> requestCreateHandle(IPC::Connection::UniqueID, Type, String&& name, bool createIfNecessary);
    bool isActiveSyncAccessHandle(WebCore::FileSystemSyncAccessHandleIdentifier);

    WebCore::FileSystemHandleIdentifier m_identifier;
    WeakPtr<FileSystemStorageManager> m_manager;
    Type m_type;
    String m_path;
    String m_name;
    struct SyncAccessHandleInfo {
        WebCore::FileSystemSyncAccessHandleIdentifier identifier;
        uint64_t capacity { 0 };
    };
    std::optional<SyncAccessHandleInfo> m_activeSyncAccessHandle;
};

} // namespace WebKit
