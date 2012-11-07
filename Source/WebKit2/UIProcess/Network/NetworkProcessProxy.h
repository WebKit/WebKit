/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetworkProcessProxy_h
#define NetworkProcessProxy_h

#if ENABLE(NETWORK_PROCESS)

#include "Connection.h"
#include "ProcessLauncher.h"
#include "WebProcessProxyMessages.h"
#include <wtf/Deque.h>

namespace WebKit {

class NetworkProcessManager;
struct NetworkProcessCreationParameters;

class NetworkProcessProxy : public RefCounted<NetworkProcessProxy>, CoreIPC::Connection::Client, ProcessLauncher::Client {
public:
    static PassRefPtr<NetworkProcessProxy> create(NetworkProcessManager*);
    ~NetworkProcessProxy();

    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);

    bool isValid() const { return m_connection; }

#if PLATFORM(MAC)
    void setApplicationIsOccluded(bool);
#endif

private:
    NetworkProcessProxy(NetworkProcessManager*);

    void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);

    void networkProcessCrashedOrFailedToLaunch();

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName);
    virtual void syncMessageSendTimedOut(CoreIPC::Connection*);

    // Message handlers
    void didReceiveNetworkProcessProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void didCreateNetworkConnectionToWebProcess(const CoreIPC::Attachment&);
    
    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier);

    // The connection to the network process.
    RefPtr<CoreIPC::Connection> m_connection;

    // The process launcher for the network process.
    RefPtr<ProcessLauncher> m_processLauncher;

    NetworkProcessManager* m_networkProcessManager;
    
    unsigned m_numPendingConnectionRequests;
    Deque<RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> > m_pendingConnectionReplies;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkProcessProxy_h
