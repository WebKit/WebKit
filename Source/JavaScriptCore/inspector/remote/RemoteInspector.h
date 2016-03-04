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

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteInspector_h
#define RemoteInspector_h

#import "RemoteInspectorXPCConnection.h"
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;

namespace Inspector {

class RemoteAutomationTarget;
class RemoteConnectionToTarget;
class RemoteControllableTarget;
class RemoteInspectionTarget;
class RemoteInspectorClient;

class JS_EXPORT_PRIVATE RemoteInspector final : public RemoteInspectorXPCConnection::Client {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual bool remoteAutomationAllowed() const = 0;
        virtual void requestAutomationSession(const String& sessionIdentifier) = 0;
    };

    static void startDisabled();
    static RemoteInspector& singleton();
    friend class NeverDestroyed<RemoteInspector>;

    void registerTarget(RemoteControllableTarget*);
    void unregisterTarget(RemoteControllableTarget*);
    void updateTarget(RemoteControllableTarget*);
    void sendMessageToRemote(unsigned targetIdentifier, const String& message);

    void updateAutomaticInspectionCandidate(RemoteInspectionTarget*);
    void setRemoteInspectorClient(RemoteInspector::Client*);

    void setupFailed(unsigned targetIdentifier);
    void setupCompleted(unsigned targetIdentifier);
    bool waitingForAutomaticInspection(unsigned targetIdentifier);
    void clientCapabilitiesDidChange() { pushListingsSoon(); }

    bool enabled() const { return m_enabled; }
    bool hasActiveDebugSession() const { return m_hasActiveDebugSession; }

    void start();
    void stop();

    bool hasParentProcessInformation() const { return m_parentProcessIdentifier != 0; }
    pid_t parentProcessIdentifier() const { return m_parentProcessIdentifier; }
    RetainPtr<CFDataRef> parentProcessAuditData() const { return m_parentProcessAuditData; }
    void setParentProcessInformation(pid_t, RetainPtr<CFDataRef> auditData);
    void setParentProcessInfomationIsDelayed();

private:
    RemoteInspector();

    unsigned nextAvailableTargetIdentifier();

    enum class StopSource { API, XPCMessage };
    void stopInternal(StopSource);

    void setupXPCConnectionIfNeeded();

    RetainPtr<NSDictionary> listingForTarget(const RemoteControllableTarget&) const;
    RetainPtr<NSDictionary> listingForInspectionTarget(const RemoteInspectionTarget&) const;
    RetainPtr<NSDictionary> listingForAutomationTarget(const RemoteAutomationTarget&) const;
    void pushListingsNow();
    void pushListingsSoon();

    void updateHasActiveDebugSession();

    void sendAutomaticInspectionCandidateMessage();

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

    static bool startEnabled;

    // Targets can be registered from any thread at any time.
    // Any target can send messages over the XPC connection.
    // So lock access to all maps and state as they can change
    // from any thread.
    Lock m_mutex;

    HashMap<unsigned, RemoteControllableTarget*> m_targetMap;
    HashMap<unsigned, RetainPtr<NSDictionary>> m_targetListingMap;
    HashMap<unsigned, RefPtr<RemoteConnectionToTarget>> m_targetConnectionMap;

    RefPtr<RemoteInspectorXPCConnection> m_relayConnection;

    RemoteInspector::Client* m_client { nullptr };

    dispatch_queue_t m_xpcQueue;
    unsigned m_nextAvailableTargetIdentifier { 1 };
    int m_notifyToken { 0 };
    bool m_enabled { false };
    bool m_hasActiveDebugSession { false };
    bool m_pushScheduled { false };

    pid_t m_parentProcessIdentifier { 0 };
    RetainPtr<CFDataRef> m_parentProcessAuditData;
    bool m_shouldSendParentProcessInformation { false };
    bool m_automaticInspectionEnabled { false };
    bool m_automaticInspectionPaused { false };
    unsigned m_automaticInspectionCandidateTargetIdentifier { 0 };
};

} // namespace Inspector

#endif // RemoteInspector_h

#endif // ENABLE(REMOTE_INSPECTOR)
