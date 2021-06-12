/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "Capabilities.h"
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/SocketConnection.h>
typedef struct _GSubprocess GSubprocess;
#elif USE(INSPECTOR_SOCKET_SERVER)
#include <JavaScriptCore/RemoteInspectorConnectionClient.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#endif

namespace WebDriver {

struct ConnectToBrowserAsyncData;

class SessionHost
#if USE(INSPECTOR_SOCKET_SERVER)
    : public Inspector::RemoteInspectorConnectionClient
#endif
{
    WTF_MAKE_FAST_ALLOCATED(SessionHost);
public:
    explicit SessionHost(Capabilities&& capabilities)
        : m_capabilities(WTFMove(capabilities))
    {
    }
    ~SessionHost();

#if USE(INSPECTOR_SOCKET_SERVER)
    void setHostAddress(const String& ip, uint16_t port) { m_targetIp = ip; m_targetPort = port; }
#endif
    bool isConnected() const;

    const String& sessionID() const { return m_sessionID; }
    const Capabilities& capabilities() const { return m_capabilities; }

    void connectToBrowser(Function<void (std::optional<String> error)>&&);
    void startAutomationSession(Function<void (bool, std::optional<String>)>&&);

    struct CommandResponse {
        RefPtr<JSON::Object> responseObject;
        bool isError { false };
    };
    long sendCommandToBackend(const String&, RefPtr<JSON::Object>&& parameters, Function<void (CommandResponse&&)>&&);

private:
    struct Target {
        uint64_t id { 0 };
        CString name;
        bool paired { false };
    };

    void inspectorDisconnected();
    void sendMessageToBackend(const String&);
    void dispatchMessage(const String&);

#if USE(GLIB)
    static const SocketConnection::MessageHandlers& messageHandlers();
    void connectionDidClose();
    void launchBrowser(Function<void (std::optional<String> error)>&&);
    void connectToBrowser(std::unique_ptr<ConnectToBrowserAsyncData>&&);
    bool matchCapabilities(GVariant*);
    bool buildSessionCapabilities(GVariantBuilder*) const;
    void setupConnection(Ref<SocketConnection>&&);
    void didStartAutomationSession(GVariant*);
    void setTargetList(uint64_t connectionID, Vector<Target>&&);
    void sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message);
#elif USE(INSPECTOR_SOCKET_SERVER)
    HashMap<String, CallHandler>& dispatchMap() override;
    void didClose(Inspector::RemoteInspectorSocketEndpoint&, Inspector::ConnectionID) final;
    void sendWebInspectorEvent(const String&);

    void receivedSetTargetList(const struct Event&);
    void receivedSendMessageToFrontend(const struct Event&);
    void receivedStartAutomationSessionReturn(const struct Event&);

    std::optional<Vector<Target>> parseTargetList(const struct Event&);
    void setTargetList(uint64_t connectionID, Vector<Target>&&);
#endif

    Capabilities m_capabilities;

    String m_sessionID;
    uint64_t m_connectionID { 0 };
    Target m_target;

    HashMap<long, Function<void (CommandResponse&&)>> m_commandRequests;

#if USE(GLIB)
    Function<void (bool, std::optional<String>)> m_startSessionCompletionHandler;
    GRefPtr<GSubprocess> m_browser;
    RefPtr<SocketConnection> m_socketConnection;
    GRefPtr<GCancellable> m_cancellable;
#elif USE(INSPECTOR_SOCKET_SERVER)
    String m_targetIp;
    uint16_t m_targetPort { 0 };
    Function<void(bool, std::optional<String>)> m_startSessionCompletionHandler;
    std::optional<Inspector::ConnectionID> m_clientID;
#endif
};

} // namespace WebDriver
