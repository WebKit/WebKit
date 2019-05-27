/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/UserActivity.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class SandboxInitializationParameters;
struct AuxiliaryProcessInitializationParameters;

class AuxiliaryProcess : protected IPC::Connection::Client, public IPC::MessageSender {
    WTF_MAKE_NONCOPYABLE(AuxiliaryProcess);

public:
    enum class ProcessType : uint8_t {
        WebContent,
        Network,
        Plugin
    };

    void initialize(const AuxiliaryProcessInitializationParameters&);

    // disable and enable termination of the process. when disableTermination is called, the
    // process won't terminate unless a corresponding disableTermination call is made.
    void disableTermination();
    void enableTermination();

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);
    void removeMessageReceiver(IPC::StringReference messageReceiverName);
    void removeMessageReceiver(IPC::MessageReceiver&);
    
    template <typename T>
    void addMessageReceiver(IPC::StringReference messageReceiverName, ObjectIdentifier<T> destinationID, IPC::MessageReceiver& receiver)
    {
        addMessageReceiver(messageReceiverName, destinationID.toUInt64(), receiver);
    }
    
    template <typename T>
    void removeMessageReceiver(IPC::StringReference messageReceiverName, ObjectIdentifier<T> destinationID)
    {
        removeMessageReceiver(messageReceiverName, destinationID.toUInt64());
    }

    void setProcessSuppressionEnabled(bool);

#if PLATFORM(COCOA)
    void setApplicationIsDaemon();
    void launchServicesCheckIn();
    void setQOS(int latencyQOS, int throughputQOS);
#endif

    IPC::Connection* parentProcessConnection() const { return m_connection.get(); }

    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

#if PLATFORM(MAC)
    static bool isSystemWebKit();
#endif
    
#if PLATFORM(COCOA)
    bool parentProcessHasEntitlement(const char* entitlement);
#endif

protected:
    explicit AuxiliaryProcess();
    virtual ~AuxiliaryProcess();

    void setTerminationTimeout(Seconds seconds) { m_terminationTimeout = seconds; }

    virtual void initializeProcess(const AuxiliaryProcessInitializationParameters&);
    virtual void initializeProcessName(const AuxiliaryProcessInitializationParameters&);
    virtual void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&);
    virtual void initializeConnection(IPC::Connection*);

    virtual bool shouldTerminate() = 0;
    virtual void terminate();

    virtual void stopRunLoop();

#if USE(APPKIT)
    static void stopNSAppRunLoop();
#endif
    
#if PLATFORM(MAC) && ENABLE(WEBPROCESS_NSRUNLOOP)
    static void stopNSRunLoop();
#endif

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void registerURLSchemeServiceWorkersCanHandle(const String&) const;
#if OS(LINUX)
    void didReceiveMemoryPressureEvent(bool isCritical);
#endif

private:
    virtual bool shouldOverrideQuarantine() { return true; }

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // IPC::Connection::Client.
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) final;
    void didClose(IPC::Connection&) override;

    void shutDown();

    void terminationTimerFired();

    void platformInitialize();
    void platformStopRunLoop();

    // The timeout, in seconds, before this process will be terminated if termination
    // has been enabled. If the timeout is 0 seconds, the process will be terminated immediately.
    Seconds m_terminationTimeout;

    // A termination counter; when the counter reaches zero, the process will be terminated
    // after a given period of time.
    unsigned m_terminationCounter;

    RunLoop::Timer<AuxiliaryProcess> m_terminationTimer;

    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;

    UserActivity m_processSuppressionDisabled;

#if PLATFORM(COCOA)
    OSObjectPtr<xpc_object_t> m_priorityBoostMessage;
#endif
};

struct AuxiliaryProcessInitializationParameters {
    String uiProcessName;
    String clientIdentifier;
    Optional<WebCore::ProcessIdentifier> processIdentifier;
    IPC::Connection::Identifier connectionIdentifier;
    HashMap<String, String> extraInitializationData;
    AuxiliaryProcess::ProcessType processType;
#if PLATFORM(COCOA)
    OSObjectPtr<xpc_object_t> priorityBoostMessage;
#endif
};

} // namespace WebKit

