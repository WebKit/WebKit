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

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "MessageReceiver.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMIdentifier.h"
#include <WebCore/LegacyCDM.h>
#include <wtf/Forward.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class RemoteLegacyCDMProxy
    : public IPC::MessageReceiver
    , public WebCore::LegacyCDMClient {
public:
    static std::unique_ptr<RemoteLegacyCDMProxy> create(WeakPtr<RemoteLegacyCDMFactoryProxy>, WebCore::MediaPlayerIdentifier&&, std::unique_ptr<WebCore::LegacyCDM>&&);
    ~RemoteLegacyCDMProxy();

    RemoteLegacyCDMFactoryProxy* factory() const { return m_factory.get(); }

private:
    friend class RemoteLegacyCDMFactoryProxy;
    RemoteLegacyCDMProxy(WeakPtr<RemoteLegacyCDMFactoryProxy>&&, WebCore::MediaPlayerIdentifier&&, std::unique_ptr<WebCore::LegacyCDM>&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // Messages
    using SupportsMIMETypeCallback = CompletionHandler<void(bool)>;
    void supportsMIMEType(const String&, SupportsMIMETypeCallback&&);
    using CreateSessionCallback = CompletionHandler<void(RemoteLegacyCDMSessionIdentifier&&)>;
    void createSession(const String&, CreateSessionCallback&&);
    void setPlayerId(std::optional<WebCore::MediaPlayerIdentifier>&&);

    // LegacyCDMClient
    RefPtr<WebCore::MediaPlayer> cdmMediaPlayer(const WebCore::LegacyCDM*) const final;

    WeakPtr<RemoteLegacyCDMFactoryProxy> m_factory;
    WebCore::MediaPlayerIdentifier m_playerId;
    std::unique_ptr<WebCore::LegacyCDM> m_cdm;
};

}

#endif
