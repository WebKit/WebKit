/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "MessageReceiverMap.h"
#include "MessageSender.h"
#include "SandboxExtension.h"
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/UserActivity.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

OBJC_CLASS NSDictionary;

namespace IPC {
class SharedBufferReference;
}

namespace WebKit {

class SandboxInitializationParameters;
struct AuxiliaryProcessInitializationParameters;
struct AuxiliaryProcessCreationParameters;

class AuxiliaryProcess : public IPC::Connection::Client, public IPC::MessageSender {
    WTF_MAKE_NONCOPYABLE(AuxiliaryProcess);

public:
    void initialize(const AuxiliaryProcessInitializationParameters&);

    // disable and enable termination of the process. when disableTermination is called, the
    // process won't terminate unless a corresponding enableTermination call is made.
    void disableTermination();
    void enableTermination();

    void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::ReceiverName, UInt128 destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::ReceiverName, UInt128 destinationID);
    void removeMessageReceiver(IPC::ReceiverName);
    void removeMessageReceiver(IPC::MessageReceiver&);
    
    template <typename T>
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, ObjectIdentifier<T> destinationID, IPC::MessageReceiver& receiver)
    {
        addMessageReceiver(messageReceiverName, destinationID.toUInt64(), receiver);
    }
    
    template <typename T>
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName, ObjectIdentifier<T> destinationID)
    {
        removeMessageReceiver(messageReceiverName, destinationID.toUInt64());
    }

    void mainThreadPing(CompletionHandler<void()>&&);
    void setProcessSuppressionEnabled(bool);

#if PLATFORM(COCOA)
    void setApplicationIsDaemon();
    void launchServicesCheckIn();
    void setQOS(int latencyQOS, int throughputQOS);
#endif

    static void applySandboxProfileForDaemon(const String& profilePath, const String& userDirectorySuffix);

    IPC::Connection* parentProcessConnection() const { return m_connection.get(); }

    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

#if PLATFORM(MAC)
    static bool isSystemWebKit();
#endif
    
#if PLATFORM(COCOA)
    bool parentProcessHasEntitlement(ASCIILiteral entitlement);
#endif

protected:
    explicit AuxiliaryProcess();
    virtual ~AuxiliaryProcess();

    virtual void initializeProcess(const AuxiliaryProcessInitializationParameters&);
    virtual void initializeProcessName(const AuxiliaryProcessInitializationParameters&);
    virtual void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&);
    virtual void initializeConnection(IPC::Connection*);

    virtual bool shouldTerminate() = 0;
    virtual void terminate();

    virtual void stopRunLoop();

#if USE(OS_STATE)
    void registerWithStateDumper(ASCIILiteral title);
    virtual RetainPtr<NSDictionary> additionalStateForDiagnosticReport() const { return { }; }
#endif // USE(OS_STATE)

#if USE(APPKIT)
    static void stopNSAppRunLoop();
#endif
    
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    static void stopNSRunLoop();
#endif

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

#if OS(LINUX)
    void didReceiveMemoryPressureEvent(bool isCritical);
#endif

protected:
#if ENABLE(CFPREFS_DIRECT_MODE)
    static id decodePreferenceValue(const std::optional<String>& encodedValue);
    static void setPreferenceValue(const String& domain, const String& key, id value);
    
    virtual void preferenceDidUpdate(const String& domain, const String& key, const std::optional<String>& encodedValue);
    virtual void handlePreferenceChange(const String& domain, const String& key, id value);
    virtual void dispatchSimulatedNotificationsForPreferenceChange(const String& key) { }
#endif
    void applyProcessCreationParameters(const AuxiliaryProcessCreationParameters&);

#if PLATFORM(MAC)
    void openDirectoryCacheInvalidated(SandboxExtension::Handle&&);
#endif

    void populateMobileGestaltCache(std::optional<SandboxExtension::Handle>&& mobileGestaltExtensionHandle);

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
    void consumeAudioComponentRegistrations(const IPC::SharedBufferReference&);
#endif

    // IPC::Connection::Client.
    void didClose(IPC::Connection&) override;

private:
    virtual bool shouldOverrideQuarantine() { return true; }

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::Connection::Client.
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final;

    void shutDown();

    void platformInitialize(const AuxiliaryProcessInitializationParameters&);
    void platformStopRunLoop();

    // A termination counter; when the counter reaches zero, the process will be terminated.
    unsigned m_terminationCounter;

    bool m_isInShutDown { false };

    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;

    UserActivity m_processSuppressionDisabled;
};

struct AuxiliaryProcessInitializationParameters {
    String uiProcessName;
    String clientIdentifier;
    String clientBundleIdentifier;
    std::optional<WebCore::ProcessIdentifier> processIdentifier;
    IPC::Connection::Identifier connectionIdentifier;
    HashMap<String, String> extraInitializationData;
    WebCore::AuxiliaryProcessType processType;
#if PLATFORM(COCOA)
    SDKAlignedBehaviors clientSDKAlignedBehaviors;
#endif
};

} // namespace WebKit

