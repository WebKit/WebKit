/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef ChildProcess_h
#define ChildProcess_h

#include "Connection.h"
#include "MessageReceiverMap.h"
#include "MessageSender.h"
#include <WebCore/UserActivity.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class SandboxInitializationParameters;

struct ChildProcessInitializationParameters {
    String uiProcessName;
    String clientIdentifier;
    IPC::Connection::Identifier connectionIdentifier;
    HashMap<String, String> extraInitializationData;
};

class ChildProcess : protected IPC::Connection::Client, public IPC::MessageSender {
    WTF_MAKE_NONCOPYABLE(ChildProcess);

public:
    void initialize(const ChildProcessInitializationParameters&);

    // disable and enable termination of the process. when disableTermination is called, the
    // process won't terminate unless a corresponding disableTermination call is made.
    void disableTermination();
    void enableTermination();

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);

    void setProcessSuppressionEnabled(bool);

#if PLATFORM(COCOA)
    void setApplicationIsDaemon();
    void setQOS(int latencyQOS, int throughputQOS);
#endif

    IPC::Connection* parentProcessConnection() const { return m_connection.get(); }

    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }

protected:
    explicit ChildProcess();
    virtual ~ChildProcess();

    void setTerminationTimeout(double seconds) { m_terminationTimeout = seconds; }

    virtual void initializeProcess(const ChildProcessInitializationParameters&);
    virtual void initializeProcessName(const ChildProcessInitializationParameters&);
    virtual void initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&);
    virtual void initializeConnection(IPC::Connection*);

    virtual bool shouldTerminate() = 0;
    virtual void terminate();

    virtual void stopRunLoop();

#if USE(APPKIT)
    static void stopNSAppRunLoop();
#endif

private:
    // IPC::MessageSender
    virtual IPC::Connection* messageSenderConnection() override;
    virtual uint64_t messageSenderDestinationID() override;

    void terminationTimerFired();

    void platformInitialize();

    // The timeout, in seconds, before this process will be terminated if termination
    // has been enabled. If the timeout is 0 seconds, the process will be terminated immediately.
    double m_terminationTimeout;

    // A termination counter; when the counter reaches zero, the process will be terminated
    // after a given period of time.
    unsigned m_terminationCounter;

    RunLoop::Timer<ChildProcess> m_terminationTimer;

    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;

    UserActivity m_processSuppressionDisabled;
};

} // namespace WebKit

#endif // ChildProcess_h
