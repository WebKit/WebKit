/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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
#include "ProcessLauncher.h"

#include <WebCore/ProcessIdentifier.h>
#include <wtf/ProcessID.h>
#include <wtf/SystemTracing.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

class ChildProcessProxy : ProcessLauncher::Client, public IPC::Connection::Client {
    WTF_MAKE_NONCOPYABLE(ChildProcessProxy);

protected:
    explicit ChildProcessProxy(bool alwaysRunsAtBackgroundPriority = false);

public:
    virtual ~ChildProcessProxy();

    void connect();
    void terminate();

    template<typename T> bool send(T&& message, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions = { });
    template<typename T> bool sendSync(T&& message, typename T::Reply&&, uint64_t destinationID, Seconds timeout = 1_s, OptionSet<IPC::SendSyncOption> sendSyncOptions = { });
    template<typename T, typename... Args> void sendWithAsyncReply(T&&, CompletionHandler<void(Args...)>&&);

    IPC::Connection* connection() const
    {
        ASSERT(m_connection);
        return m_connection.get();
    }
    
    bool hasConnection(const IPC::Connection& connection) const
    {
        return m_connection == &connection;
    }

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);
    void removeMessageReceiver(IPC::StringReference messageReceiverName);

    enum class State {
        Launching,
        Running,
        Terminated,
    };
    State state() const;

    ProcessID processIdentifier() const { return m_processLauncher ? m_processLauncher->processIdentifier() : 0; }

    bool canSendMessage() const { return state() != State::Terminated;}
    bool sendMessage(std::unique_ptr<IPC::Encoder>, OptionSet<IPC::SendOption>);

    void shutDownProcess();

    WebCore::ProcessIdentifier coreProcessIdentifier() const { return m_processIdentifier; }

    void setProcessSuppressionEnabled(bool);

protected:
    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);
    
    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&);
    virtual void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&) { };

private:
    virtual void connectionWillOpen(IPC::Connection&);
    virtual void processWillShutDown(IPC::Connection&) = 0;

    Vector<std::pair<std::unique_ptr<IPC::Encoder>, OptionSet<IPC::SendOption>>> m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    bool m_alwaysRunsAtBackgroundPriority { false };
    WebCore::ProcessIdentifier m_processIdentifier { generateObjectIdentifier<WebCore::ProcessIdentifierType>() };
};

template<typename T>
bool ChildProcessProxy::send(T&& message, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = std::make_unique<IPC::Encoder>(T::receiverName(), T::name(), destinationID);
    encoder->encode(message.arguments());

    return sendMessage(WTFMove(encoder), sendOptions);
}

template<typename U> 
bool ChildProcessProxy::sendSync(U&& message, typename U::Reply&& reply, uint64_t destinationID, Seconds timeout, OptionSet<IPC::SendSyncOption> sendSyncOptions)
{
    COMPILE_ASSERT(U::isSync, SyncMessageExpected);

    if (!m_connection)
        return false;

    TraceScope scope(SyncMessageStart, SyncMessageEnd);

    return connection()->sendSync(std::forward<U>(message), WTFMove(reply), destinationID, timeout, sendSyncOptions);
}

template<typename T, typename... Args>
void ChildProcessProxy::sendWithAsyncReply(T&& message, CompletionHandler<void(Args...)>&& completionHandler)
{
    if (!m_connection) {
        T::cancelReply(WTFMove(completionHandler));
        return;
    }

    connection()->sendWithAsyncReply(std::forward<T>(message), WTFMove(completionHandler));
}
    
} // namespace WebKit
