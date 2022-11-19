/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/StorageArea.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOrigin;
class StorageMap;
struct ClientOrigin;
}

namespace WebKit {

class StorageAreaImpl;
class StorageNamespaceImpl;

class StorageAreaMap final : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StorageAreaMap(StorageNamespaceImpl&, Ref<const WebCore::SecurityOrigin>&&);
    ~StorageAreaMap();

    WebCore::StorageType type() const { return m_type; }

    unsigned length();
    String key(unsigned index);
    String item(const String& key);
    void setItem(WebCore::Frame& sourceFrame, StorageAreaImpl* sourceArea, const String& key, const String& value, bool& quotaException);
    void removeItem(WebCore::Frame& sourceFrame, StorageAreaImpl* sourceArea, const String& key);
    void clear(WebCore::Frame& sourceFrame, StorageAreaImpl* sourceArea);
    bool contains(const String& key);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    const WebCore::SecurityOrigin& securityOrigin() const { return m_securityOrigin.get(); }
    StorageAreaMapIdentifier identifier() const { return m_identifier; }

    void connect();
    void disconnect();
    void incrementUseCount();
    void decrementUseCount();

private:
    void didSetItem(uint64_t mapSeed, const String& key, bool hasError, HashMap<String, String>&&);
    void didRemoveItem(uint64_t mapSeed, const String& key);
    void didClear(uint64_t mapSeed);

    // Message handlers.
    void dispatchStorageEvent(const std::optional<StorageAreaImplIdentifier>& sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString, uint64_t messageIdentifier);
    void clearCache(uint64_t messageIdentifier);

    void syncOneItem(const String& key, const String& value);
    void syncItems(HashMap<String, String>&&);
    WebCore::StorageMap& ensureMap();
    WebCore::StorageType computeStorageType() const;
    WebCore::ClientOrigin clientOrigin() const;

    void applyChange(const String& key, const String& newValue);
    void dispatchSessionStorageEvent(const std::optional<StorageAreaImplIdentifier>&, const String& key, const String& oldValue, const String& newValue, const String& urlString);
    void dispatchLocalStorageEvent(const std::optional<StorageAreaImplIdentifier>&, const String& key, const String& oldValue, const String& newValue, const String& urlString);

    enum class SendMode : bool { Async, Sync };
    void sendConnectMessage(SendMode);
    void connectSync();
    void didConnect(StorageAreaIdentifier, HashMap<String, String>&&, uint64_t messageIdentifier);

    StorageAreaMapIdentifier m_identifier;
    uint64_t m_lastHandledMessageIdentifier { 0 };
    StorageNamespaceImpl& m_namespace;
    Ref<const WebCore::SecurityOrigin> m_securityOrigin;
    std::unique_ptr<WebCore::StorageMap> m_map;
    std::optional<StorageAreaIdentifier> m_remoteAreaIdentifier;
    HashCountedSet<String> m_pendingValueChanges;
    uint64_t m_currentSeed { 1 };
    unsigned m_quotaInBytes;
    WebCore::StorageType m_type;
    uint64_t m_useCount { 0 };
    bool m_hasPendingClear { false };
    bool m_isWaitingForConnectReply { false };
};

} // namespace WebKit
