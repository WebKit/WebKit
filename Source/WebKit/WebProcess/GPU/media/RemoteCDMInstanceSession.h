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

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include "MessageReceiver.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMInstanceSessionIdentifier.h"
#include <WebCore/CDMInstanceSession.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class RemoteCDMInstanceSession final : private IPC::MessageReceiver, public WebCore::CDMInstanceSession {
public:
    static Ref<RemoteCDMInstanceSession> create(WeakPtr<RemoteCDMFactory>&&, RemoteCDMInstanceSessionIdentifier&&);
    virtual ~RemoteCDMInstanceSession() = default;

    RemoteCDMInstanceSessionIdentifier identifier() const { return m_identifier; }

private:
    friend class RemoteCDMFactory;
    RemoteCDMInstanceSession(WeakPtr<RemoteCDMFactory>&&, RemoteCDMInstanceSessionIdentifier&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void updateKeyStatuses(KeyStatusVector&&);
    void sendMessage(WebCore::CDMMessageType, RefPtr<WebCore::SharedBuffer>&&);
    void sessionIdChanged(const String&);

    void setClient(WeakPtr<WebCore::CDMInstanceSessionClient>&& client) final { m_client = WTFMove(client); }
    void clearClient() final { m_client = nullptr; }
    void requestLicense(LicenseType, const AtomString& initDataType, Ref<WebCore::SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String& sessionId, LicenseType, Ref<WebCore::SharedBuffer>&& response, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String& sessionId, const String& origin, LoadSessionCallback&&) final;
    void closeSession(const String& sessionId, CloseSessionCallback&&) final;
    void removeSessionData(const String& sessionId, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String& sessionId) final;

    WeakPtr<RemoteCDMFactory> m_factory;
    RemoteCDMInstanceSessionIdentifier m_identifier;
    WeakPtr<WebCore::CDMInstanceSessionClient> m_client;
};

} // namespace WebCore

#endif
