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
#include "FileSystemStorageError.h"
#include "OriginStorageManager.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <pal/SessionID.h>

namespace IPC {
class SharedFileHandle;
}

namespace WebCore {
struct ClientOrigin;
}

namespace WebKit {

class FileSystemStorageHandleRegistry;

class NetworkStorageManager final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<NetworkStorageManager> create(PAL::SessionID, const String& path);

    void startReceivingMessageFromConnection(IPC::Connection&);
    void stopReceivingMessageFromConnection(IPC::Connection&);

    PAL::SessionID sessionID() const { return m_sessionID; }
    void close();
    void clearStorageForTesting(CompletionHandler<void()>&&);

private:
    NetworkStorageManager(PAL::SessionID, const String& path);
    ~NetworkStorageManager();
    OriginStorageManager& localOriginStorageManager(const WebCore::ClientOrigin&);
    FileSystemStorageHandleRegistry& fileSystemStorageHandleRegistry();

    // IPC::MessageReceiver (implemented by generated code)
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    // Message handlers
    void persisted(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void persist(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void fileSystemGetDirectory(IPC::Connection&, const WebCore::ClientOrigin&, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void closeHandle(WebCore::FileSystemHandleIdentifier);
    void isSameEntry(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(bool)>&&);
    void move(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void getFileHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void getDirectoryHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void removeEntry(WebCore::FileSystemHandleIdentifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void resolve(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);

    using AccessHandleInfo = std::pair<WebCore::FileSystemSyncAccessHandleIdentifier, IPC::SharedFileHandle>;
    void createSyncAccessHandle(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<AccessHandleInfo, FileSystemStorageError>)>&&);
    void getSizeForAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void(Expected<uint64_t, FileSystemStorageError>)>&&);
    void truncateForAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, uint64_t size, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void flushForAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void closeAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void getHandleNames(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void getHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&&);

    PAL::SessionID m_sessionID;
    Ref<WorkQueue> m_queue;
    String m_path;
    FileSystem::Salt m_salt;
    bool m_closed { false };
    HashMap<WebCore::ClientOrigin, std::unique_ptr<OriginStorageManager>> m_localOriginStorageManagers;
    HashMap<WebCore::ClientOrigin, std::unique_ptr<OriginStorageManager>> m_sessionOriginStorageManagers;
    WeakHashSet<IPC::Connection> m_connections; // Main thread only.
    std::unique_ptr<FileSystemStorageHandleRegistry> m_fileSystemStorageHandleRegistry;
};

} // namespace WebKit
