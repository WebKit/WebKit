/*
 * Copyright (C) 2013, 2015, 2016 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "RemoteControllableTarget.h"

#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ProcessID.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "RemoteInspectorXPCConnection.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;
typedef RetainPtr<NSDictionary> TargetListing;
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/SocketConnection.h>
typedef GRefPtr<GVariant> TargetListing;
typedef struct _GCancellable GCancellable;
#endif

#if USE(INSPECTOR_SOCKET_SERVER)
#include "RemoteConnectionToTarget.h"
#include "RemoteInspectorConnectionClient.h"
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>

namespace Inspector {
using TargetListing = RefPtr<JSON::Object>;
}
#endif

namespace Inspector {

class RemoteAutomationTarget;
class RemoteConnectionToTarget;
class RemoteControllableTarget;
class RemoteInspectionTarget;
class RemoteInspectorClient;

class JS_EXPORT_PRIVATE RemoteInspector final
#if PLATFORM(COCOA)
    : public RemoteInspectorXPCConnection::Client
#elif USE(INSPECTOR_SOCKET_SERVER)
    : public RemoteInspectorConnectionClient
#endif
{
public:
    class Client {
    public:
        struct Capabilities {
            bool remoteAutomationAllowed : 1;
            String browserName;
            String browserVersion;
        };

        struct SessionCapabilities {
            bool acceptInsecureCertificates { false };
#if USE(GLIB)
            Vector<std::pair<String, String>> certificates;
            struct Proxy {
                String type;
                Optional<String> ftpURL;
                Optional<String> httpURL;
                Optional<String> httpsURL;
                Optional<String> socksURL;
                Vector<String> ignoreAddressList;
            };
            Optional<Proxy> proxy;
#endif
#if PLATFORM(COCOA)
            Optional<bool> allowInsecureMediaCapture;
            Optional<bool> suppressICECandidateFiltering;
#endif
        };

        virtual ~Client();
        virtual bool remoteAutomationAllowed() const = 0;
        virtual String browserName() const { return { }; }
        virtual String browserVersion() const { return { }; }
        virtual void requestAutomationSession(const String& sessionIdentifier, const SessionCapabilities&) = 0;
    };

    static void startDisabled();
    static RemoteInspector& singleton();
    friend class NeverDestroyed<RemoteInspector>;

    void registerTarget(RemoteControllableTarget*);
    void unregisterTarget(RemoteControllableTarget*);
    void updateTarget(RemoteControllableTarget*);
    void sendMessageToRemote(TargetID, const String& message);

    RemoteInspector::Client* client() const { return m_client; }
    void setClient(RemoteInspector::Client*);
    void clientCapabilitiesDidChange();
    Optional<RemoteInspector::Client::Capabilities> clientCapabilities() const { return m_clientCapabilities; }

    void setupFailed(TargetID);
    void setupCompleted(TargetID);
    bool waitingForAutomaticInspection(TargetID);
    void updateAutomaticInspectionCandidate(RemoteInspectionTarget*);

    bool enabled() const { return m_enabled; }
    bool hasActiveDebugSession() const { return m_hasActiveDebugSession; }

    void start();
    void stop();

#if PLATFORM(COCOA)
    bool hasParentProcessInformation() const { return m_parentProcessIdentifier != 0; }
    ProcessID parentProcessIdentifier() const { return m_parentProcessIdentifier; }
    RetainPtr<CFDataRef> parentProcessAuditData() const { return m_parentProcessAuditData; }
    void setParentProcessInformation(ProcessID, RetainPtr<CFDataRef> auditData);
    void setParentProcessInfomationIsDelayed();
#endif

    void updateTargetListing(TargetID);

#if USE(GLIB)
    void requestAutomationSession(const char* sessionID, const Client::SessionCapabilities&);
#endif
#if USE(GLIB) || USE(INSPECTOR_SOCKET_SERVER)
    void setup(TargetID);
    void sendMessageToTarget(TargetID, const char* message);
#endif
#if USE(INSPECTOR_SOCKET_SERVER)
    void requestAutomationSession(const String& sessionID, const Client::SessionCapabilities&);

    bool isConnected() const { return !!m_clientConnection; }
    void connect(ConnectionID);

    void setBackendCommandsPath(const String& backendCommandsPath) { m_backendCommandsPath = backendCommandsPath; }
#endif

private:
    RemoteInspector();

    TargetID nextAvailableTargetIdentifier();

    enum class StopSource { API, XPCMessage };
    void stopInternal(StopSource);

#if PLATFORM(COCOA)
    void setupXPCConnectionIfNeeded();
#endif
#if USE(GLIB)
    void setupConnection(Ref<SocketConnection>&&);
    static const SocketConnection::MessageHandlers& messageHandlers();

    void receivedGetTargetListMessage();
    void receivedSetupMessage(TargetID);
    void receivedDataMessage(TargetID, const char* message);
    void receivedCloseMessage(TargetID);
    void receivedAutomationSessionRequestMessage(const char* sessionID);
#endif

    TargetListing listingForTarget(const RemoteControllableTarget&) const;
    TargetListing listingForInspectionTarget(const RemoteInspectionTarget&) const;
    TargetListing listingForAutomationTarget(const RemoteAutomationTarget&) const;

    bool updateTargetMap(RemoteControllableTarget*);

    void pushListingsNow();
    void pushListingsSoon();

    void updateTargetListing(const RemoteControllableTarget&);

    void updateHasActiveDebugSession();
    void updateClientCapabilities();

    void sendAutomaticInspectionCandidateMessage();

#if PLATFORM(COCOA)
    void xpcConnectionReceivedMessage(RemoteInspectorXPCConnection*, NSString *messageName, NSDictionary *userInfo) override;
    void xpcConnectionFailed(RemoteInspectorXPCConnection*) override;
    void xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t) override;

    void receivedSetupMessage(NSDictionary *userInfo);
    void receivedDataMessage(NSDictionary *userInfo);
    void receivedDidCloseMessage(NSDictionary *userInfo);
    void receivedGetListingMessage(NSDictionary *userInfo);
    void receivedIndicateMessage(NSDictionary *userInfo);
    void receivedProxyApplicationSetupMessage(NSDictionary *userInfo);
    void receivedConnectionDiedMessage(NSDictionary *userInfo);
    void receivedAutomaticInspectionConfigurationMessage(NSDictionary *userInfo);
    void receivedAutomaticInspectionRejectMessage(NSDictionary *userInfo);
    void receivedAutomationSessionRequestMessage(NSDictionary *userInfo);
#endif
#if USE(INSPECTOR_SOCKET_SERVER)
    HashMap<String, CallHandler>& dispatchMap() override;
    void didClose(ConnectionID) override;

    void sendWebInspectorEvent(const String&);

    void setupInspectorClient(const Event&);
    void setupTarget(const Event&);
    void frontendDidClose(const Event&);
    void sendMessageToBackend(const Event&);
    void startAutomationSession(const Event&);

    void receivedAutomationSessionRequestMessage(const Event&);

    String backendCommands() const;
#endif
    static bool startEnabled;

    // Targets can be registered from any thread at any time.
    // Any target can send messages over the XPC connection.
    // So lock access to all maps and state as they can change
    // from any thread.
    Lock m_mutex;

    HashMap<TargetID, RemoteControllableTarget*> m_targetMap;
    HashMap<TargetID, RefPtr<RemoteConnectionToTarget>> m_targetConnectionMap;
    HashMap<TargetID, TargetListing> m_targetListingMap;

#if PLATFORM(COCOA)
    RefPtr<RemoteInspectorXPCConnection> m_relayConnection;
#endif
#if USE(GLIB)
    RefPtr<SocketConnection> m_socketConnection;
    GRefPtr<GCancellable> m_cancellable;
#endif

#if USE(INSPECTOR_SOCKET_SERVER)
    // Connection from RemoteInspectorClient or WebDriver.
    Optional<ConnectionID> m_clientConnection;
    bool m_readyToPushListings { false };

    String m_backendCommandsPath;
#endif

    RemoteInspector::Client* m_client { nullptr };
    Optional<RemoteInspector::Client::Capabilities> m_clientCapabilities;

#if PLATFORM(COCOA)
    dispatch_queue_t m_xpcQueue;
#endif
    TargetID m_nextAvailableTargetIdentifier { 1 };
    int m_notifyToken { 0 };
    bool m_enabled { false };
    bool m_hasActiveDebugSession { false };
    bool m_pushScheduled { false };

    ProcessID m_parentProcessIdentifier { 0 };
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> m_parentProcessAuditData;
#endif
    bool m_shouldSendParentProcessInformation { false };
    bool m_automaticInspectionEnabled { false };
    bool m_automaticInspectionPaused { false };
    TargetID m_automaticInspectionCandidateTargetIdentifier { 0 };
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
