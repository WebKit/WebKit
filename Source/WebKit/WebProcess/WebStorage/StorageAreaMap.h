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
}

namespace WebKit {

class StorageAreaImpl;
class StorageNamespaceImpl;

class StorageAreaMap final : private IPC::MessageReceiver, public CanMakeWeakPtr<StorageAreaMap> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StorageAreaMap(StorageNamespaceImpl&, Ref<WebCore::SecurityOrigin>&&);
    ~StorageAreaMap();

    WebCore::StorageType type() const { return m_type; }

    unsigned length();
    String key(unsigned index);
    String item(const String& key);
    void setItem(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key, const String& value, bool& quotaException);
    void removeItem(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea, const String& key);
    void clear(WebCore::Frame* sourceFrame, StorageAreaImpl* sourceArea);
    bool contains(const String& key);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    const WebCore::SecurityOrigin& securityOrigin() const { return m_securityOrigin.get(); }
    const Optional<StorageAreaIdentifier>& identifier() const { return m_mapID; }

    void disconnect();

    void incrementUseCount();
    void decrementUseCount();

private:
    void didSetItem(uint64_t mapSeed, const String& key, bool quotaError);
    void didRemoveItem(uint64_t mapSeed, const String& key);
    void didClear(uint64_t mapSeed);

    void dispatchStorageEvent(const Optional<StorageAreaImplIdentifier>& sourceStorageAreaID, const String& key, const String& oldValue, const String& newValue, const String& urlString);
    void clearCache();

    void resetValues();
    WebCore::StorageMap& ensureMap();

    bool shouldApplyChangeForKey(const String& key) const;
    void applyChange(const String& key, const String& newValue);

    void dispatchSessionStorageEvent(const Optional<StorageAreaImplIdentifier>&, const String& key, const String& oldValue, const String& newValue, const String& urlString);
    void dispatchLocalStorageEvent(const Optional<StorageAreaImplIdentifier>&, const String& key, const String& oldValue, const String& newValue, const String& urlString);

    void connect();

    StorageNamespaceImpl& m_namespace;
    Ref<WebCore::SecurityOrigin> m_securityOrigin;
    RefPtr<WebCore::StorageMap> m_map;
    Optional<StorageAreaIdentifier> m_mapID;
    HashCountedSet<String> m_pendingValueChanges;
    uint64_t m_currentSeed { 0 };
    unsigned m_quotaInBytes;
    WebCore::StorageType m_type;
    uint64_t m_useCount { 0 };
    bool m_hasPendingClear { false };
};

} // namespace WebKit
