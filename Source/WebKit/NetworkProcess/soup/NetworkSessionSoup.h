/*
 * Copyright (C) 2016 Igalia S.L.
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

#include "NetworkSession.h"
#include "SoupCookiePersistentStorageType.h"
#include "WebPageProxyIdentifier.h"

typedef struct _SoupSession SoupSession;

namespace WebCore {
class SoupNetworkSession;
struct SoupNetworkProxySettings;
}

namespace WebKit {

class NetworkSocketChannel;
class WebSocketTask;
struct NetworkSessionCreationParameters;

class NetworkSessionSoup final : public NetworkSession {
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    {
        return makeUnique<NetworkSessionSoup>(networkProcess, parameters);
    }
    NetworkSessionSoup(NetworkProcess&, const NetworkSessionCreationParameters&);
    ~NetworkSessionSoup();

    WebCore::SoupNetworkSession& soupNetworkSession() const { return *m_networkSession; }
    SoupSession* soupSession() const;

    void setCookiePersistentStorage(const String& storagePath, SoupCookiePersistentStorageType);

    void setPersistentCredentialStorageEnabled(bool enabled) { m_persistentCredentialStorageEnabled = enabled; }
    bool persistentCredentialStorageEnabled() const { return m_persistentCredentialStorageEnabled; }

    void setIgnoreTLSErrors(bool);
    void setProxySettings(const WebCore::SoupNetworkProxySettings&);

private:
    std::unique_ptr<WebSocketTask> createWebSocketTask(WebPageProxyIdentifier, NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&, bool, bool, OptionSet<WebCore::NetworkConnectionIntegrity>) final;
    void clearCredentials() final;

    std::unique_ptr<WebCore::SoupNetworkSession> m_networkSession;
    bool m_persistentCredentialStorageEnabled { true };
};

} // namespace WebKit
