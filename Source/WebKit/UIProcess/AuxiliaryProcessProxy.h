/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "AuxiliaryProcessCreationParameters.h"
#include "Connection.h"
#include "MessageReceiverMap.h"
#include "ProcessLauncher.h"
#include "ResponsivenessTimer.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/ProcessID.h>
#include <wtf/SystemTracing.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class ProcessThrottler;

class AuxiliaryProcessProxy : public ThreadSafeRefCounted<AuxiliaryProcessProxy, WTF::DestructionThread::MainRunLoop>, public ResponsivenessTimer::Client, private ProcessLauncher::Client, public IPC::Connection::Client {
    WTF_MAKE_NONCOPYABLE(AuxiliaryProcessProxy);

protected:
    AuxiliaryProcessProxy(bool alwaysRunsAtBackgroundPriority = false, Seconds responsivenessTimeout = ResponsivenessTimer::defaultResponsivenessTimeout);

public:
    virtual ~AuxiliaryProcessProxy();

    static AuxiliaryProcessCreationParameters auxiliaryProcessParameters();

    void connect();
    virtual void terminate();

    virtual ProcessThrottler& throttler() = 0;

    template<typename T> bool send(T&& message, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions = { });
    using SendSyncResult = IPC::Connection::SendSyncResult;
    template<typename T> SendSyncResult sendSync(T&& message, typename T::Reply&&, uint64_t destinationID, IPC::Timeout = 1_s, OptionSet<IPC::SendSyncOption> sendSyncOptions = { });

    enum class ShouldStartProcessThrottlerActivity : bool { No, Yes };
    template<typename T, typename C> uint64_t sendWithAsyncReply(T&&, C&&, uint64_t destinationID = 0, OptionSet<IPC::SendOption> = { }, ShouldStartProcessThrottlerActivity = ShouldStartProcessThrottlerActivity::Yes);
    
    template<typename T, typename U>
    bool send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<IPC::SendOption> sendOptions = { })
    {
        return send<T>(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }
    
    template<typename T, typename U>
    SendSyncResult sendSync(T&& message, typename T::Reply&& reply, ObjectIdentifier<U> destinationID, IPC::Timeout timeout = 1_s, OptionSet<IPC::SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(WTFMove(message), WTFMove(reply), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    IPC::Connection* connection() const
    {
        ASSERT(m_connection);
        return m_connection.get();
    }

    bool hasConnection() const
    {
        return !!m_connection;
    }
    
    bool hasConnection(const IPC::Connection& connection) const
    {
        return m_connection == &connection;
    }

    void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::ReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::ReceiverName, uint64_t destinationID);
    void removeMessageReceiver(IPC::ReceiverName);
    
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

    enum class State {
        Launching,
        Running,
        Terminated,
    };
    State state() const;
    String stateString() const;
    bool isLaunching() const { return state() == State::Launching; }
    bool wasTerminated() const;

    ProcessID processIdentifier() const { return m_processLauncher ? m_processLauncher->processIdentifier() : 0; }

    bool canSendMessage() const { return state() != State::Terminated;}
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>, std::optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&& asyncReplyInfo = std::nullopt, ShouldStartProcessThrottlerActivity = ShouldStartProcessThrottlerActivity::Yes);

    void replyToPendingMessages();

    void shutDownProcess();

    WebCore::ProcessIdentifier coreProcessIdentifier() const { return m_processIdentifier; }

    void setProcessSuppressionEnabled(bool);
    bool platformIsBeingDebugged() const;

    enum class UseLazyStop : bool { No, Yes };
    void startResponsivenessTimer(UseLazyStop = UseLazyStop::No);
    void stopResponsivenessTimer();

    void checkForResponsiveness(CompletionHandler<void()>&& = nullptr, UseLazyStop = UseLazyStop::No);

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }
    const ResponsivenessTimer& responsivenessTimer() const { return m_responsivenessTimer; }

    void ref() final { ThreadSafeRefCounted::ref(); }
    void deref() final { ThreadSafeRefCounted::deref(); }

protected:
    // ProcessLauncher::Client
    void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void logInvalidMessage(IPC::Connection&, IPC::MessageName);
    virtual ASCIILiteral processName() const = 0;

    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&);
    virtual void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&) { };

    struct PendingMessage {
        UniqueRef<IPC::Encoder> encoder;
        OptionSet<IPC::SendOption> sendOptions;
        std::optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>> asyncReplyInfo;
    };

    virtual bool shouldSendPendingMessage(const PendingMessage&) { return true; }

    // ResponsivenessTimer::Client.
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override { }
    void willChangeIsResponsive() override { }
    void didChangeIsResponsive() override { }
    bool mayBecomeUnresponsive() override;

private:
    virtual void connectionWillOpen(IPC::Connection&);
    virtual void processWillShutDown(IPC::Connection&) = 0;

    ResponsivenessTimer m_responsivenessTimer;
    Vector<PendingMessage> m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    bool m_alwaysRunsAtBackgroundPriority { false };
    WebCore::ProcessIdentifier m_processIdentifier { WebCore::ProcessIdentifier::generate() };
    std::optional<UseLazyStop> m_shouldStartResponsivenessTimerWhenLaunched;
};

template<typename T>
bool AuxiliaryProcessProxy::send(T&& message, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), destinationID);
    encoder.get() << message.arguments();

    return sendMessage(WTFMove(encoder), sendOptions);
}

template<typename U> 
AuxiliaryProcessProxy::SendSyncResult AuxiliaryProcessProxy::sendSync(U&& message, typename U::Reply&& reply, uint64_t destinationID, IPC::Timeout timeout, OptionSet<IPC::SendSyncOption> sendSyncOptions)
{
    COMPILE_ASSERT(U::isSync, SyncMessageExpected);

    if (!m_connection)
        return { };

    TraceScope scope(SyncMessageStart, SyncMessageEnd);

    return connection()->sendSync(std::forward<U>(message), WTFMove(reply), destinationID, timeout, sendSyncOptions);
}

template<typename T, typename C>
uint64_t AuxiliaryProcessProxy::sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions, ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), destinationID);
    uint64_t listenerID = IPC::nextAsyncReplyHandlerID();
    encoder.get() << listenerID;
    encoder.get() << message.arguments();
    sendMessage(WTFMove(encoder), sendOptions, {{ [completionHandler = WTFMove(completionHandler)] (IPC::Decoder* decoder) mutable {
        if (decoder && decoder->isValid())
            T::callReply(*decoder, WTFMove(completionHandler));
        else
            T::cancelReply(WTFMove(completionHandler));
    }, listenerID }}, shouldStartProcessThrottlerActivity);
    return listenerID;
}
    
} // namespace WebKit
