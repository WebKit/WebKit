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
#include <WebCore/UserActivity.h>
#include <wtf/AbstractRefCounted.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>
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

class AuxiliaryProcess : public IPC::Connection::Client, public IPC::MessageSender, public AbstractRefCounted {
    WTF_MAKE_NONCOPYABLE(AuxiliaryProcess);
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AuxiliaryProcess);
public:
    void initialize(const AuxiliaryProcessInitializationParameters&);

    // disable and enable termination of the process. when disableTermination is called, the
    // process won't terminate unless a corresponding enableTermination call is made.
    void disableTermination();
    void enableTermination();

    void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::ReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::ReceiverName, uint64_t destinationID);
    void removeMessageReceiver(IPC::ReceiverName);
    void removeMessageReceiver(IPC::MessageReceiver&);
    
    template<typename RawValue>
    void addMessageReceiver(IPC::ReceiverName messageReceiverName, const ObjectIdentifierGenericBase<RawValue>& destinationID, IPC::MessageReceiver& receiver)
    {
        addMessageReceiver(messageReceiverName, destinationID.toUInt64(), receiver);
    }
    
    template<typename RawValue>
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName, const ObjectIdentifierGenericBase<RawValue>& destinationID)
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
    RefPtr<IPC::Connection> protectedParentProcessConnection() const { return parentProcessConnection(); }

    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

#if PLATFORM(COCOA)
    static bool isSystemWebKit();
#endif
    
#if PLATFORM(COCOA)
    bool parentProcessHasEntitlement(ASCIILiteral entitlement);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    virtual void preferenceDidUpdate(const String& domain, const String& key, const std::optional<String>& encodedValue);
#endif
    static void setNotifyOptions();

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
    virtual bool filterUnhandledMessage(IPC::Connection&, IPC::Decoder&);

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
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

#if OS(LINUX)
    void didReceiveMemoryPressureEvent(bool isCritical);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    static id decodePreferenceValue(const std::optional<String>& encodedValue);
    static void setPreferenceValue(const String& domain, const String& key, id value);
    virtual void handlePreferenceChange(const String& domain, const String& key, id value);
    virtual void dispatchSimulatedNotificationsForPreferenceChange(const String& key) { }
    virtual void accessibilitySettingsDidChange() { }
#endif

    void applyProcessCreationParameters(const AuxiliaryProcessCreationParameters&);

#if PLATFORM(MAC)
    static void openDirectoryCacheInvalidated(SandboxExtension::Handle&&);
#endif

    void populateMobileGestaltCache(std::optional<SandboxExtension::Handle>&& mobileGestaltExtensionHandle);

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
    void consumeAudioComponentRegistrations(const IPC::SharedBufferReference&);
#endif

    // IPC::Connection::Client.
    void didClose(IPC::Connection&) override;

private:
#if ENABLE(CFPREFS_DIRECT_MODE)
    void handleAXPreferenceChange(const String& domain, const String& key, id value);
#endif

    virtual bool shouldOverrideQuarantine() { return true; }

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::Connection::Client.
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) final;

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
    WTF::AuxiliaryProcessType processType;
};

} // namespace WebKit

