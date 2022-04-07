/*
 * Copyright (C) 2022 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorClient.h"
#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {

class RemoteInspectorHTTPServer final : public RemoteInspectorObserver {
public:
    static RemoteInspectorHTTPServer& singleton();
    ~RemoteInspectorHTTPServer() = default;

    bool start(GRefPtr<GSocketAddress>&&, unsigned inspectorPort);
    bool isRunning() const { return !!m_server; }
    const String& inspectorServerAddress() const;

    void sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const String& message) const;
    void targetDidClose(uint64_t connectionID, uint64_t targetID);

private:
    unsigned handleRequest(const char*, SoupMessageHeaders*, SoupMessageBody*) const;
    void handleWebSocket(const char*, SoupWebsocketConnection*);

    void sendMessageToBackend(SoupWebsocketConnection*, const String&) const;
    void didCloseWebSocket(SoupWebsocketConnection*);

    void targetListChanged(RemoteInspectorClient&) override { }
    void connectionClosed(RemoteInspectorClient&) override { }

    GRefPtr<SoupServer> m_server;
    std::unique_ptr<RemoteInspectorClient> m_client;
    HashMap<std::pair<uint64_t, uint64_t>, GRefPtr<SoupWebsocketConnection>> m_webSocketConnectionMap;
    HashMap<SoupWebsocketConnection*, std::pair<uint64_t, uint64_t>> m_webSocketConnectionToTargetMap;
};

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
