/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionIdentifier.h"
#include <WebCore/LegacyCDMSession.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

namespace WebCore {
class LegacyCDM;
class LegacyCDMSession;
class SharedBuffer;
}

namespace WebKit {

class RemoteLegacyCDMProxy;
class RemoteMediaPlayerProxy;

class RemoteLegacyCDMSessionProxy : public IPC::MessageReceiver, public WebCore::LegacyCDMSessionClient, public RefCounted<RemoteLegacyCDMSessionProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteLegacyCDMSessionProxy> create(RemoteLegacyCDMFactoryProxy&, uint64_t logIdentifier, RemoteLegacyCDMSessionIdentifier, WebCore::LegacyCDM&);
    ~RemoteLegacyCDMSessionProxy();

    void invalidate();

    RemoteLegacyCDMFactoryProxy* factory() const { return m_factory.get(); }
    WebCore::LegacyCDMSession* session() const { return m_session.get(); }
    RefPtr<WebCore::LegacyCDMSession> protectedSession() const;

    void setPlayer(WeakPtr<RemoteMediaPlayerProxy>);

    RefPtr<ArrayBuffer> getCachedKeyForKeyId(const String&);
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    friend class RemoteLegacyCDMFactoryProxy;
    RemoteLegacyCDMSessionProxy(RemoteLegacyCDMFactoryProxy&, uint64_t logIdentifier, RemoteLegacyCDMSessionIdentifier, WebCore::LegacyCDM&);

    RefPtr<RemoteLegacyCDMFactoryProxy> protectedFactory() const { return m_factory.get(); }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // LegacyCDMSessionClient
    void sendMessage(Uint8Array*, String destinationURL) final;
    void sendError(MediaKeyErrorCode, uint32_t systemCode) final;
    String mediaKeysStorageDirectory() const final;
#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "RemoteLegacyCDMSessionProxy"_s; }
    WTFLogChannel& logChannel() const;
#endif

    // Messages
    using GenerateKeyCallback = CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&, const String&, unsigned short, uint32_t)>;
    void generateKeyRequest(const String& mimeType, RefPtr<WebCore::SharedBuffer>&& initData, GenerateKeyCallback&&);
    void releaseKeys();
    using UpdateCallback = CompletionHandler<void(bool, RefPtr<WebCore::SharedBuffer>&&, unsigned short, uint32_t)>;
    void update(RefPtr<WebCore::SharedBuffer>&& update, UpdateCallback&&);
    using CachedKeyForKeyIDCallback = CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>;
    void cachedKeyForKeyID(String keyId, CachedKeyForKeyIDCallback&&);

    WeakPtr<RemoteLegacyCDMFactoryProxy> m_factory;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

    RemoteLegacyCDMSessionIdentifier m_identifier;
    RefPtr<WebCore::LegacyCDMSession> m_session;
    WeakPtr<RemoteMediaPlayerProxy> m_player;
};

}

#endif
