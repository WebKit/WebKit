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
typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GSubprocess GSubprocess;
#endif

namespace WebDriver {

struct ConnectToBrowserAsyncData;

class SessionHost {
    WTF_MAKE_FAST_ALLOCATED(SessionHost);
public:
    explicit SessionHost(Capabilities&& capabilities)
        : m_capabilities(WTFMove(capabilities))
    {
    }
    ~SessionHost();

    bool isConnected() const;

    const String& sessionID() const { return m_sessionID; }
    const Capabilities& capabilities() const { return m_capabilities; }

    void connectToBrowser(Function<void (Optional<String> error)>&&);
    void startAutomationSession(Function<void (bool, Optional<String>)>&&);

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
    void sendMessageToBackend(long, const String&);
    void dispatchMessage(const String&);

#if USE(GLIB)
    static void dbusConnectionClosedCallback(SessionHost*);
    static const GDBusInterfaceVTable s_interfaceVTable;
    void launchBrowser(Function<void (Optional<String> error)>&&);
    void connectToBrowser(std::unique_ptr<ConnectToBrowserAsyncData>&&);
    bool matchCapabilities(GVariant*);
    bool buildSessionCapabilities(GVariantBuilder*) const;
    void setupConnection(GRefPtr<GDBusConnection>&&);
    void setTargetList(uint64_t connectionID, Vector<Target>&&);
    void sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message);
#endif

    Capabilities m_capabilities;

    String m_sessionID;
    uint64_t m_connectionID { 0 };
    Target m_target;

    HashMap<long, Function<void (CommandResponse&&)>> m_commandRequests;

#if USE(GLIB)
    Function<void (bool, Optional<String>)> m_startSessionCompletionHandler;
    GRefPtr<GSubprocess> m_browser;
    GRefPtr<GDBusConnection> m_dbusConnection;
    GRefPtr<GCancellable> m_cancellable;
#endif
};

} // namespace WebDriver
