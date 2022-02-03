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
#include <wtf/text/WTFString.h>

namespace WebCore {
struct ClientOrigin;
}

namespace WebKit {

class FileSystemStorageHandleRegistry;
class FileSystemStorageManager;
class LocalStorageManager;
class SessionStorageManager;
class StorageAreaRegistry;
enum class WebsiteDataType : uint32_t;

class OriginStorageManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    OriginStorageManager(String&& path, String&& localStoragePath);
    ~OriginStorageManager();

    void connectionClosed(IPC::Connection::UniqueID);
    bool persisted() const { return m_persisted; }
    void setPersisted(bool value);
    FileSystemStorageManager& fileSystemStorageManager(FileSystemStorageHandleRegistry&);
    LocalStorageManager& localStorageManager(StorageAreaRegistry&);
    LocalStorageManager* existingLocalStorageManager();
    SessionStorageManager& sessionStorageManager(StorageAreaRegistry&);
    SessionStorageManager* existingSessionStorageManager();
    bool isActive();
    bool isEmpty();
    OptionSet<WebsiteDataType> fetchDataTypesInList(OptionSet<WebsiteDataType>);
    void deleteData(OptionSet<WebsiteDataType>, WallTime);
    void moveData(const String& newPath, const String& localStoragePath);

private:
    enum class StorageBucketMode : bool;
    class StorageBucket;
    StorageBucket& defaultBucket();

    void createOriginFileIfNecessary(const WebCore::ClientOrigin&);
    void deleteOriginFileIfNecessary();

    std::unique_ptr<StorageBucket> m_defaultBucket;
    String m_path;
    bool m_persisted { false };
    String m_localStoragePath;
};

} // namespace WebKit

