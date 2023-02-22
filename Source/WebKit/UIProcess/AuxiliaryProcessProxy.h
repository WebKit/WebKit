/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#include "ProcessThrottler.h"
#include "ResponsivenessTimer.h"
#include "SandboxExtension.h"
#include <WebCore/ProcessIdentifier.h>
#include <memory>
#include <wtf/ProcessID.h>
#include <wtf/SystemTracing.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class ProcessThrottler;
class ProcessAssertion;

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

    enum class ShouldStartProcessThrottlerActivity : bool { No, Yes };

    class AsyncMessageOptions {
    public:
        template<typename T, typename = void> struct IsOptionType : std::false_type { };
        template<typename... OptionTypes, typename = std::enable_if_t<std::conjunction_v<IsOptionType<OptionTypes>...>>>
        AsyncMessageOptions(const OptionTypes&... options)
        {
            (..., setOption(options));
        }

        auto destinationID() const { return m_destinationID; }
        auto sendOptions() const { return m_sendOptions; }
        auto shouldStartProcessThrottlerActivity() const { return m_shouldStartProcessThrottlerActivity; }

    private:
        void setOption(uint64_t destinationID) { m_destinationID = destinationID; }
        template<typename T> void setOption(ObjectIdentifier<T> destinationID) { m_destinationID = destinationID.toUInt64(); }
        void setOption(OptionSet<IPC::SendOption> sendOptions) { m_sendOptions = sendOptions; }
        void setOption(ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity) { m_shouldStartProcessThrottlerActivity = shouldStartProcessThrottlerActivity; }

        uint64_t m_destinationID { 0 };
        OptionSet<IPC::SendOption> m_sendOptions { };
        ShouldStartProcessThrottlerActivity m_shouldStartProcessThrottlerActivity { ShouldStartProcessThrottlerActivity::Yes };
    };

    template<typename T, typename... ArgumentTypes>
    bool send(AsyncMessageOptions, ArgumentTypes&&...);

    template<typename T>
    auto send(T&& message, uint64_t destinationID, OptionSet<IPC::SendOption> sendOptions = { })
    {
        return std::apply([&](auto&&... arguments) {
            return send<T>({ destinationID, sendOptions }, arguments...);
        }, message.arguments());
    }

    template<typename T, typename U>
    auto send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<IPC::SendOption> sendOptions = { })
    {
        return send(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    template<typename T> using SendSyncResult = IPC::Connection::SendSyncResult<T>;
    template<typename T, typename... ArgumentTypes>
    SendSyncResult<T> sendSync(IPC::Connection::SyncMessageOptions, ArgumentTypes&&...);

    template<typename T>
    auto sendSync(T&& message, uint64_t destinationID, IPC::Timeout timeout = 1_s, OptionSet<IPC::SendSyncOption> sendSyncOptions = { })
    {
        return std::apply([&](auto&&... arguments) {
            return sendSync<T>({ destinationID, timeout, sendSyncOptions }, arguments...);
        }, message.arguments());
    }

    template<typename T, typename U>
    auto sendSync(T&& message, ObjectIdentifier<U> destinationID, IPC::Timeout timeout = 1_s, OptionSet<IPC::SendSyncOption> sendSyncOptions = { })
    {
        return sendSync(WTFMove(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    using AsyncReplyID = IPC::Connection::AsyncReplyID;
    template<typename T, typename C, typename... ArgumentTypes>
    AsyncReplyID sendWithAsyncReply(AsyncMessageOptions, C&&, ArgumentTypes&&...);

    template<typename T, typename C>
    auto sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID = 0, OptionSet<IPC::SendOption> sendOptions = { }, ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity = ShouldStartProcessThrottlerActivity::Yes)
    {
        return std::apply([&](auto&&... arguments) {
            return sendWithAsyncReply<T>({ destinationID, sendOptions, shouldStartProcessThrottlerActivity }, WTFMove(completionHandler), arguments...);
        }, message.arguments());
    }

    template<typename T, typename C, typename U>
    auto sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID, OptionSet<IPC::SendOption> sendOptions = { }, ShouldStartProcessThrottlerActivity shouldStartProcessThrottlerActivity = ShouldStartProcessThrottlerActivity::Yes)
    {
        return std::apply([&](auto&&... arguments) {
            return sendWithAsyncReply<T>({ destinationID, sendOptions, shouldStartProcessThrottlerActivity }, WTFMove(completionHandler), arguments...);
        }, message.arguments());
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
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>, std::optional<IPC::Connection::AsyncReplyHandler> = std::nullopt, ShouldStartProcessThrottlerActivity = ShouldStartProcessThrottlerActivity::Yes);

    void replyToPendingMessages();

    void shutDownProcess();

    WebCore::ProcessIdentifier coreProcessIdentifier() const { return m_processIdentifier; }

    void setProcessSuppressionEnabled(bool);
    bool platformIsBeingDebugged() const;

#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    void setRunningBoardThrottlingEnabled();
#endif

    enum class UseLazyStop : bool { No, Yes };
    void startResponsivenessTimer(UseLazyStop = UseLazyStop::No);
    void stopResponsivenessTimer();

    void checkForResponsiveness(CompletionHandler<void()>&& = nullptr, UseLazyStop = UseLazyStop::No);

    ResponsivenessTimer& responsivenessTimer() { return m_responsivenessTimer; }
    const ResponsivenessTimer& responsivenessTimer() const { return m_responsivenessTimer; }

    void ref() final { ThreadSafeRefCounted::ref(); }
    void deref() final { ThreadSafeRefCounted::deref(); }

    bool operator==(const AuxiliaryProcessProxy& other) const { return (this == &other); }
    bool operator!=(const AuxiliaryProcessProxy& other) const { return !(this == &other); }

    std::optional<SandboxExtension::Handle> createMobileGestaltSandboxExtensionIfNeeded() const;

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
        std::optional<IPC::Connection::AsyncReplyHandler> asyncReplyHandler;
    };

    virtual bool shouldSendPendingMessage(const PendingMessage&) { return true; }

    void beginResponsivenessChecks();

    // ResponsivenessTimer::Client.
    void didBecomeUnresponsive() override;
    void didBecomeResponsive() override { }
    void willChangeIsResponsive() override { }
    void didChangeIsResponsive() override { }
    bool mayBecomeUnresponsive() override;

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
    static RefPtr<WebCore::SharedBuffer> fetchAudioComponentServerRegistrations();
#endif

private:
    virtual void connectionWillOpen(IPC::Connection&);
    virtual void processWillShutDown(IPC::Connection&) = 0;

    void populateOverrideLanguagesLaunchOptions(ProcessLauncher::LaunchOptions&) const;
    Vector<String> platformOverrideLanguages() const;
    void platformStartConnectionTerminationWatchdog();

    ResponsivenessTimer m_responsivenessTimer;
    Vector<PendingMessage> m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    bool m_alwaysRunsAtBackgroundPriority { false };
    bool m_didBeginResponsivenessChecks { false };
    WebCore::ProcessIdentifier m_processIdentifier { WebCore::ProcessIdentifier::generate() };
    std::optional<UseLazyStop> m_delayedResponsivenessCheck;
    MonotonicTime m_processStart;
#if PLATFORM(MAC) && USE(RUNNINGBOARD)
    std::unique_ptr<ProcessThrottler::ForegroundActivity> m_lifetimeActivity;
#endif
};

template<typename T> struct AuxiliaryProcessProxy::AsyncMessageOptions::IsOptionType<T, std::void_t<decltype(std::declval<AuxiliaryProcessProxy::AsyncMessageOptions>().setOption(std::declval<T>()))>> : std::true_type { };

template<typename T, typename... ArgumentTypes>
bool AuxiliaryProcessProxy::send(AsyncMessageOptions messageOptions, ArgumentTypes&&... arguments)
{
    static_assert(!T::isSync);
    static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

    auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), messageOptions.destinationID());
    (encoder.get() << ... << std::forward<ArgumentTypes>(arguments));
    return sendMessage(WTFMove(encoder), messageOptions.sendOptions());
}

template<typename T, typename... ArgumentTypes>
AuxiliaryProcessProxy::SendSyncResult<T> AuxiliaryProcessProxy::sendSync(IPC::Connection::SyncMessageOptions messageOptions, ArgumentTypes&&... arguments)
{
    static_assert(T::isSync);
    static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

    if (!m_connection)
        return { };

    TraceScope scope(SyncMessageStart, SyncMessageEnd);
    return m_connection->sendSync<T>(messageOptions, std::forward<ArgumentTypes>(arguments)...);
}

template<typename T, typename C, typename... ArgumentTypes>
AuxiliaryProcessProxy::AsyncReplyID AuxiliaryProcessProxy::sendWithAsyncReply(AsyncMessageOptions messageOptions, C&& completionHandler, ArgumentTypes&&... arguments)
{
    static_assert(!T::isSync);
    static_assert(std::is_same_v<std::tuple<std::remove_cvref_t<ArgumentTypes>...>, typename T::Arguments>);

    auto encoder = makeUniqueRef<IPC::Encoder>(T::name(), messageOptions.destinationID());
    (encoder.get() << ... << std::forward<ArgumentTypes>(arguments));
    auto handler = IPC::Connection::makeAsyncReplyHandler<T>(WTFMove(completionHandler));
    auto replyID = handler.replyID;
    if (sendMessage(WTFMove(encoder), messageOptions.sendOptions(), WTFMove(handler), messageOptions.shouldStartProcessThrottlerActivity()))
        return replyID;
    return { };
}
    
} // namespace WebKit
