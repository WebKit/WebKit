/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "Decoder.h"
#include "Encoder.h"
#include "MessageReceiveQueueMap.h"
#include "MessageReceiver.h"
#include "ReceiverMatcher.h"
#include "Timeout.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

#if USE(UNIX_DOMAIN_SOCKETS)
#include <wtf/unix/UnixFileDescriptor.h>
#endif

#if OS(DARWIN)
#include <mach/mach_port.h>
#include <wtf/MachSendRight.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if OS(WINDOWS)
#include <wtf/win/Win32Handle.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GSocketMonitor.h>
#endif

#if ENABLE(IPC_TESTING_API)
#include "MessageObserver.h"
#endif

namespace IPC {

enum class SendOption : uint8_t {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
    DispatchMessageEvenWhenWaitingForUnboundedSyncReply = 1 << 1,
    IgnoreFullySynchronousMode = 1 << 2,
};

enum class SendSyncOption {
    // Use this to inform that this sync call will suspend this process until the user responds with input.
    InformPlatformProcessWillSuspend = 1 << 0,
    UseFullySynchronousModeForTesting = 1 << 1,
    ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply = 1 << 2,
    MaintainOrderingWithAsyncMessages = 1 << 3,
};

enum class WaitForOption {
    // Use this to make waitForMessage be interrupted immediately by any incoming sync messages.
    InterruptWaitingIfSyncMessageArrives = 1 << 0,
    DispatchIncomingSyncMessagesWhileWaiting = 1 << 1,
};

#define MESSAGE_CHECK_BASE(assertion, connection) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, (void)0)

#define MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion) do { \
    if (UNLIKELY(!(assertion))) { \
        (connection)->markCurrentlyDispatchedMessageAsInvalid(); \
        { completion; } \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, connection, returnValue) do { \
    if (UNLIKELY(!(assertion))) { \
        (connection)->markCurrentlyDispatchedMessageAsInvalid(); \
        return (returnValue); \
    } \
} while (0)

template<typename AsyncReplyResult> struct AsyncReplyError {
    static AsyncReplyResult create() { return AsyncReplyResult { }; };
};

class MachMessage;
class UnixMessage;
class WorkQueueMessageReceiver;

class Connection : public ThreadSafeRefCounted<Connection, WTF::DestructionThread::MainRunLoop>, public CanMakeWeakPtr<Connection> {
public:
    enum SyncRequestIDType { };
    using SyncRequestID = ObjectIdentifier<SyncRequestIDType>;

    class Client : public MessageReceiver {
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, MessageName) = 0;

    protected:
        virtual ~Client() { }
    };

#if USE(UNIX_DOMAIN_SOCKETS)
    using Handle = UnixFileDescriptor;
#elif OS(WINDOWS)
    using Handle = Win32Handle;
#elif OS(DARWIN)
    using Handle = MachSendRight;
#endif

    struct Identifier {
        Identifier() = default;
#if USE(UNIX_DOMAIN_SOCKETS)
        explicit Identifier(Handle&& handle)
            : Identifier(handle.release())
        {
        }
        explicit Identifier(int handle)
            : handle(handle)
        {
        }
        operator bool() const { return handle != -1; }
        int handle { -1 };
#elif OS(WINDOWS)
        explicit Identifier(Handle&& handle)
            : Identifier(handle.release())
        {
        }
        explicit Identifier(HANDLE handle)
            : handle(handle)
        {
        }
        operator bool() const { return !!handle; }
        HANDLE handle { 0 };
#elif OS(DARWIN)
        explicit Identifier(Handle&& handle)
            : Identifier(handle.leakSendRight())
        {
        }
        explicit Identifier(mach_port_t port)
            : port(port)
        {
        }
        Identifier(mach_port_t port, OSObjectPtr<xpc_connection_t> xpcConnection)
            : port(port)
            , xpcConnection(WTFMove(xpcConnection))
        {
        }
        operator bool() const { return MACH_PORT_VALID(port); }
        mach_port_t port { MACH_PORT_NULL };
        OSObjectPtr<xpc_connection_t> xpcConnection;
#endif
    };

#if OS(DARWIN)
    xpc_connection_t xpcConnection() const { return m_xpcConnection.get(); }
    std::optional<audit_token_t> getAuditToken();
    pid_t remoteProcessID() const;
#endif

    static Ref<Connection> createServerConnection(Identifier);
    static Ref<Connection> createClientConnection(Identifier);

    struct ConnectionIdentifierPair {
        IPC::Connection::Identifier server;
        IPC::Connection::Handle client;
    };
    static std::optional<ConnectionIdentifierPair> createConnectionIdentifierPair();

    ~Connection();

    Client* client() const { return m_client; }

    enum UniqueIDType { };
    using UniqueID = ObjectIdentifier<UniqueIDType>;

    static Connection* connection(UniqueID);
    UniqueID uniqueID() const { return m_uniqueID; }

    void setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(bool);
    void setShouldExitOnSyncMessageSendFailure(bool);

    // The set callback will be called on the connection work queue when the connection is closed, 
    // before didCall is called on the client thread. Must be called before the connection is opened.
    // In the future we might want a more generic way to handle sync or async messages directly
    // on the work queue, for example if we want to handle them on some other thread we could avoid
    // handling the message on the client thread first.
    typedef void (*DidCloseOnConnectionWorkQueueCallback)(Connection*);
    void setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback);

    // Adds a message receive queue. The client should make sure the instance is removed before it goes
    // out of scope.
    // std::nullopt ReceiverMatchSpec matches all receivers.
    void addMessageReceiveQueue(MessageReceiveQueue&, const ReceiverMatcher&);
    void removeMessageReceiveQueue(const ReceiverMatcher&);

    // Adds a message receive queue that dispatches through WorkQueue to WorkQueueMessageReceiver.
    // Keeps the WorkQueue and the WorkQueueMessageReceiver alive. Dispatched tasks keep WorkQueueMessageReceiver alive.
    // destinationID == 0 matches all ids.
    void addWorkQueueMessageReceiver(ReceiverName, WorkQueue&, WorkQueueMessageReceiver&, uint64_t destinationID = 0);
    void removeWorkQueueMessageReceiver(ReceiverName, uint64_t destinationID = 0);

    // Adds a message receive queue that dispatches through FunctionDispatcher.
    // `FunctionDispatcher` will be used in any thread.
    // `FunctionDispatcher` will be used to dispatch `MessageReceiver` functions
    // until `removeMessageReceiver()` for same receiver name, destination id returns.
    // The caller is responsible for making sure the `MessageReceiver` is alive when the dispatched functions
    // are run.
    void addMessageReceiver(FunctionDispatcher&, MessageReceiver&, ReceiverName, uint64_t destinationID = 0);
    void removeMessageReceiver(ReceiverName, uint64_t destinationID = 0);

    bool open(Client&);
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void postConnectionDidCloseOnConnectionWorkQueue();
    template<typename T, typename C> uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T> bool send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.
    template<typename T> static bool send(UniqueID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.

    // Sync senders should check the SendSyncResult for true/false in case they need to know if the result was really received.
    // Sync senders should hold on to the SendSyncResult in case they reference the contents of the reply via DataRefererence / ArrayReference.

    template<typename T>
    struct SendSyncResult {
        std::unique_ptr<Decoder> decoder;
        std::optional<typename T::ReplyArguments> replyArguments;

        explicit operator bool() const { return !!decoder; }

        typename T::ReplyArguments& reply()
        {
            ASSERT(!!replyArguments);
            return *replyArguments;
        }

        typename T::ReplyArguments takeReply()
        {
            ASSERT(!!replyArguments);
            return WTFMove(replyArguments).value();
        }

        template<typename... U>
        typename T::ReplyArguments takeReplyOr(U&&... defaultValues)
        {
            return WTFMove(replyArguments).value_or(typename T::ReplyArguments { std::forward<U>(defaultValues)... });
        }
    };

    template<typename T> SendSyncResult<T> sendSync(T&& message, uint64_t destinationID, Timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { }); // Main thread only.

    template<typename> bool waitForAndDispatchImmediately(uint64_t destinationID, Timeout, OptionSet<WaitForOption> waitForOptions = { }); // Main thread only.
    template<typename> bool waitForAsyncCallbackAndDispatchImmediately(uint64_t callbackID, Timeout); // Main thread only.

    // Thread-safe.
    template<typename T, typename C, typename U>
    uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID = { }, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply<T, C>(WTFMove(message), WTFMove(completionHandler), destinationID.toUInt64(), sendOptions);
    }
    
    // Thread-safe.
    template<typename T, typename U>
    bool send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt)
    {
        return send<T>(WTFMove(message), destinationID.toUInt64(), sendOptions, qos);
    }

    // Main thread only.
    template<typename T, typename U>
    SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(WTFMove(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }
    
    // Main thread only.
    template<typename T, typename U>
    bool waitForAndDispatchImmediately(ObjectIdentifier<U> destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions = { })
    {
        return waitForAndDispatchImmediately<T>(destinationID.toUInt64(), timeout, waitForOptions);
    }

    bool sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);
    UniqueRef<Encoder> createSyncMessageEncoder(MessageName, uint64_t destinationID, SyncRequestID&);
    std::unique_ptr<Decoder> sendSyncMessage(SyncRequestID, UniqueRef<Encoder>&&, Timeout, OptionSet<SendSyncOption> sendSyncOptions);
    bool sendSyncReply(UniqueRef<Encoder>&&);

    void wakeUpRunLoop();

    void incrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { ++m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }
    void decrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { --m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }

    bool inSendSync() const { return m_inSendSyncCount; }

    Identifier identifier() const;

#if PLATFORM(COCOA)
    bool kill();
#endif

    bool isValid() const { return m_isValid; }

    uint64_t installIncomingSyncMessageCallback(WTF::Function<void()>&&);
    void uninstallIncomingSyncMessageCallback(uint64_t);
    bool hasIncomingSyncMessage();

    void allowFullySynchronousModeForTesting() { m_fullySynchronousModeIsAllowedForTesting = true; }

    void ignoreTimeoutsForTesting() { m_ignoreTimeoutsForTesting = true; }

    void enableIncomingMessagesThrottling();

#if ENABLE(IPC_TESTING_API)
    void addMessageObserver(const MessageObserver&);

    void setIgnoreInvalidMessageForTesting() { m_ignoreInvalidMessageForTesting = true; }
    bool ignoreInvalidMessageForTesting() const { return m_ignoreInvalidMessageForTesting; }
    void dispatchIncomingMessageForTesting(std::unique_ptr<Decoder>&&);
    std::unique_ptr<Decoder> waitForMessageForTesting(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);
#endif

    void dispatchMessageReceiverMessage(MessageReceiver&, std::unique_ptr<Decoder>&&);
    // Can be called from any thread.
    void dispatchDidReceiveInvalidMessage(MessageName);
private:
    Connection(Identifier, bool isServer);
    void platformInitialize(Identifier);
    void platformInvalidate();

    bool isIncomingMessagesThrottlingEnabled() const { return !!m_incomingMessagesThrottler; }

    static HashMap<IPC::Connection::UniqueID, Connection*>& connectionMap() WTF_REQUIRES_LOCK(s_connectionMapLock);
    
    std::unique_ptr<Decoder> waitForMessage(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);

    SyncRequestID makeSyncRequestID() { return SyncRequestID::generateThreadSafe(); }
    bool pushPendingSyncRequestID(SyncRequestID);
    void popPendingSyncRequestID(SyncRequestID);
    std::unique_ptr<Decoder> waitForSyncReply(SyncRequestID, MessageName, Timeout, OptionSet<SendSyncOption>);

    void enqueueMatchingMessagesToMessageReceiveQueue(MessageReceiveQueue&, const ReceiverMatcher&) WTF_REQUIRES_LOCK(m_incomingMessagesLock);

    // Called on the connection work queue.
    void processIncomingMessage(std::unique_ptr<Decoder>);
    void processIncomingSyncReply(std::unique_ptr<Decoder>);

    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(UniqueRef<Encoder>&&);
    void connectionDidClose();
    
    // Called on the listener thread.
    void dispatchOneIncomingMessage();
    void dispatchIncomingMessages();
    void dispatchMessage(std::unique_ptr<Decoder>);
    void dispatchMessage(Decoder&);
    void dispatchSyncMessage(Decoder&);
    void didFailToSendSyncMessage();

    // Can be called on any thread.
    void enqueueIncomingMessage(std::unique_ptr<Decoder>) WTF_REQUIRES_LOCK(m_incomingMessagesLock);
    size_t incomingMessagesDispatchingBatchSize() const;

    void willSendSyncMessage(OptionSet<SendSyncOption>);
    void didReceiveSyncReply(OptionSet<SendSyncOption>);

    Timeout timeoutRespectingIgnoreTimeoutsForTesting(Timeout) const;

#if PLATFORM(COCOA)
    bool sendMessage(std::unique_ptr<MachMessage>);
#endif

    class MessagesThrottler {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        typedef void (Connection::*DispatchMessagesFunction)();
        MessagesThrottler(Connection&, DispatchMessagesFunction);

        size_t numberOfMessagesToProcess(size_t totalMessages);
        void scheduleMessagesDispatch();

    private:
        RunLoop::Timer<Connection> m_dispatchMessagesTimer;
        Connection& m_connection;
        DispatchMessagesFunction m_dispatchMessages;
        unsigned m_throttlingLevel { 0 };
    };

    static Lock s_connectionMapLock;
    Client* m_client { nullptr };
    UniqueID m_uniqueID;
    bool m_isServer;
    std::atomic<bool> m_isValid { true };

    bool m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage { false };
    bool m_shouldExitOnSyncMessageSendFailure { false };
    DidCloseOnConnectionWorkQueueCallback m_didCloseOnConnectionWorkQueueCallback { nullptr };

    Ref<WorkQueue> m_connectionQueue;
    bool m_isConnected { false };

    unsigned m_inSendSyncCount { 0 };
    unsigned m_inDispatchMessageCount { 0 };
    unsigned m_inDispatchSyncMessageCount { 0 };
    unsigned m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount { 0 };
    unsigned m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting { 0 };
    bool m_fullySynchronousModeIsAllowedForTesting { false };
    bool m_ignoreTimeoutsForTesting { false };
    bool m_didReceiveInvalidMessage { false };

    // Incoming messages.
    Lock m_incomingMessagesLock;
    Deque<std::unique_ptr<Decoder>> m_incomingMessages WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);
    std::unique_ptr<MessagesThrottler> m_incomingMessagesThrottler;
    MessageReceiveQueueMap m_receiveQueues WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);

    // Outgoing messages.
    Lock m_outgoingMessagesLock;
    Deque<UniqueRef<Encoder>> m_outgoingMessages WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);

    Condition m_waitForMessageCondition;
    Lock m_waitForMessageLock;

    struct WaitForMessageState;
    WaitForMessageState* m_waitingForMessage WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { nullptr }; // NOLINT

    class SyncMessageState;

    Lock m_syncReplyStateLock;
    bool m_shouldWaitForSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock) { true };
    bool m_shouldWaitForMessages WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { true };
    struct PendingSyncReply;
    Vector<PendingSyncReply> m_pendingSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock);

    Lock m_incomingSyncMessageCallbackLock;
    HashMap<uint64_t, WTF::Function<void()>> m_incomingSyncMessageCallbacks WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    RefPtr<WorkQueue> m_incomingSyncMessageCallbackQueue WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    uint64_t m_nextIncomingSyncMessageCallbackID WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock) { 0 };

#if ENABLE(IPC_TESTING_API)
    Vector<WeakPtr<MessageObserver>> m_messageObservers;
    bool m_ignoreInvalidMessageForTesting { false };
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
    // Called on the connection queue.
    void readyReadHandler();
    bool processMessage();
    bool sendOutputMessage(UnixMessage&);

    Vector<uint8_t> m_readBuffer;
    Vector<int> m_fileDescriptors;
    int m_socketDescriptor;
    std::unique_ptr<UnixMessage> m_pendingOutputMessage;
#if USE(GLIB)
    GRefPtr<GSocket> m_socket;
    GSocketMonitor m_readSocketMonitor;
    GSocketMonitor m_writeSocketMonitor;
#endif
#if PLATFORM(PLAYSTATION)
    RefPtr<WTF::Thread> m_socketMonitor;
#endif
#elif OS(DARWIN)
    // Called on the connection queue.
    void receiveSourceEventHandler();
    void initializeSendSource();
    void resumeSendSource();
    void cancelReceiveSource();

    mach_port_t m_sendPort { MACH_PORT_NULL };
    OSObjectPtr<dispatch_source_t> m_sendSource;

    mach_port_t m_receivePort { MACH_PORT_NULL };
    OSObjectPtr<dispatch_source_t> m_receiveSource;

    std::unique_ptr<MachMessage> m_pendingOutgoingMachMessage;
    bool m_isInitializingSendSource { false };

    OSObjectPtr<xpc_connection_t> m_xpcConnection;
    bool m_wasKilled { false };
#elif OS(WINDOWS)
    // Called on the connection queue.
    void readEventHandler();
    void writeEventHandler();
    void invokeReadEventHandler();
    void invokeWriteEventHandler();

    class EventListener {
    public:
        void open(Function<void()>&&);
        void close();

        OVERLAPPED& state() { return m_state; }

    private:
        static void WINAPI callback(void*, BOOLEAN);

        OVERLAPPED m_state;
        HANDLE m_waitHandle { INVALID_HANDLE_VALUE };
        Function<void()> m_handler;
    };

    Vector<uint8_t> m_readBuffer;
    EventListener m_readListener;
    std::unique_ptr<Encoder> m_pendingWriteEncoder;
    EventListener m_writeListener;
    HANDLE m_connectionPipe { INVALID_HANDLE_VALUE };
#endif
    friend class StreamClientConnection;
};

template<typename T>
bool Connection::send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    static_assert(!T::isSync, "Async message expected");

    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    encoder.get() << message.arguments();
    
    return sendMessage(WTFMove(encoder), sendOptions, qos);
}

template<typename T>
bool Connection::send(UniqueID connectionID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    Locker locker { s_connectionMapLock };
    auto* connection = connectionMap().get(connectionID);
    if (!connection)
        return false;
    return connection->send(WTFMove(message), destinationID, sendOptions, qos);
}

uint64_t nextAsyncReplyHandlerID();
void addAsyncReplyHandler(Connection&, uint64_t, CompletionHandler<void(Decoder*)>&&);
CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(Connection&, uint64_t);

template<typename T, typename C>
uint64_t Connection::sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");

    if (!isValid()) {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
            T::cancelReply(WTFMove(completionHandler));
        });
        return 0;
    }

    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    uint64_t listenerID = nextAsyncReplyHandlerID();
    addAsyncReplyHandler(*this, listenerID, CompletionHandler<void(Decoder*)>([completionHandler = WTFMove(completionHandler)] (Decoder* decoder) mutable {
        if (decoder && decoder->isValid())
            T::callReply(*decoder, WTFMove(completionHandler));
        else
            T::cancelReply(WTFMove(completionHandler));
    }, CompletionHandlerCallThread::MainThread));
    encoder.get() << listenerID;
    encoder.get() << message.arguments();
    sendMessage(WTFMove(encoder), sendOptions);
    return listenerID;
}

template<size_t i, typename A, typename B> struct TupleMover {
    static void move(A&& a, B& b)
    {
        std::get<i - 1>(b) = WTFMove(std::get<i - 1>(a));
        TupleMover<i - 1, A, B>::move(WTFMove(a), b);
    }
};

template<typename A, typename B> struct TupleMover<0, A, B> {
    static void move(A&&, B&) { }
};

template<typename... A, typename... B>
void moveTuple(std::tuple<A...>&& a, std::tuple<B...>& b)
{
    static_assert(sizeof...(A) == sizeof...(B), "Should be used with two tuples of same size");
    TupleMover<sizeof...(A), std::tuple<A...>, std::tuple<B...>>::move(WTFMove(a), b);
}

template<typename T> Connection::SendSyncResult<T> Connection::sendSync(T&& message, uint64_t destinationID, Timeout timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    static_assert(T::isSync, "Sync message expected");
    RELEASE_ASSERT(RunLoop::isMain());

    SyncRequestID syncRequestID;
    auto encoder = createSyncMessageEncoder(T::name(), destinationID, syncRequestID);

    if (sendSyncOptions.contains(SendSyncOption::UseFullySynchronousModeForTesting)) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    encoder.get() << message.arguments();

    // Now send the message and wait for a reply.
    std::unique_ptr<Decoder> replyDecoder = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, sendSyncOptions);
    if (!replyDecoder)
        return { };

    SendSyncResult<T> result;
    *replyDecoder >> result.replyArguments;
    if (!result.replyArguments)
        return { };
    result.decoder = WTFMove(replyDecoder);
    return result;
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions)
{
    RELEASE_ASSERT(RunLoop::isMain());
    std::unique_ptr<Decoder> decoder = waitForMessage(T::name(), destinationID, timeout, waitForOptions);
    if (!decoder)
        return false;
    if (!isValid())
        return false;
    ASSERT(decoder->destinationID() == destinationID);
    m_client->didReceiveMessage(*this, *decoder);
    return true;
}

template<typename T> bool Connection::waitForAsyncCallbackAndDispatchImmediately(uint64_t destinationID, Timeout timeout)
{
    RELEASE_ASSERT(RunLoop::isMain());
    std::unique_ptr<Decoder> decoder = waitForMessage(T::asyncMessageReplyName(), destinationID, timeout, { });
    if (!decoder)
        return false;

    ASSERT(decoder->messageReceiverName() == ReceiverName::AsyncReply);
    ASSERT(decoder->destinationID() == destinationID);
    auto handler = takeAsyncReplyHandler(*this, decoder->destinationID());
    if (!handler) {
        ASSERT_NOT_REACHED();
        return false;
    }
    handler(decoder.get());
    return true;
}

#if ENABLE(IPC_TESTING_API)
inline std::unique_ptr<Decoder> Connection::waitForMessageForTesting(MessageName messageName, uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> options)
{
    return waitForMessage(messageName, destinationID, timeout, options);
}
#endif


class UnboundedSynchronousIPCScope {
public:
    UnboundedSynchronousIPCScope()
    {
        ASSERT(RunLoop::isMain());
        ++unboundedSynchronousIPCCount;
    }

    ~UnboundedSynchronousIPCScope()
    {
        ASSERT(RunLoop::isMain());
        ASSERT(unboundedSynchronousIPCCount);
        --unboundedSynchronousIPCCount;
    }

    static bool hasOngoingUnboundedSyncIPC()
    {
        return unboundedSynchronousIPCCount.load() > 0;
    }

private:
    static std::atomic<unsigned> unboundedSynchronousIPCCount;
};

void AccessibilityProcessSuspendedNotification(bool suspended);

} // namespace IPC
