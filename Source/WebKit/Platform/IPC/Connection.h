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
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

#if OS(DARWIN) && !USE(UNIX_DOMAIN_SOCKETS)
#include <mach/mach_port.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GSocketMonitor.h>
#endif

namespace IPC {

enum class SendOption {
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

class Connection : public ThreadSafeRefCounted<Connection, WTF::DestructionThread::MainRunLoop> {
public:
    class Client : public MessageReceiver {
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, MessageName) = 0;

    protected:
        virtual ~Client() { }
    };

    class WorkQueueMessageReceiver : public MessageReceiver, public ThreadSafeRefCounted<WorkQueueMessageReceiver> {
    };

    class ThreadMessageReceiver : public MessageReceiver {
    public:
        virtual void dispatchToThread(WTF::Function<void()>&&) = 0;

        void ref() { refMessageReceiver(); }
        void deref() { derefMessageReceiver(); }

    protected:
        virtual void refMessageReceiver() = 0;
        virtual void derefMessageReceiver() = 0;
    };

    class ThreadMessageReceiverRefCounted : public ThreadMessageReceiver, public ThreadSafeRefCounted<ThreadMessageReceiverRefCounted> {
    public:
        using ThreadSafeRefCounted::ref;
        using ThreadSafeRefCounted::deref;

    private:
        void refMessageReceiver() final { ThreadSafeRefCounted::ref(); }
        void derefMessageReceiver() final { ThreadSafeRefCounted::deref(); }
    };

#if ENABLE(IPC_TESTING_API)
    class MessageObserver : public CanMakeWeakPtr<MessageObserver> {
    public:
        virtual ~MessageObserver() = default;
        virtual void willSendMessage(const Encoder&, OptionSet<SendOption>) = 0;
        virtual void didReceiveMessage(const Decoder&) = 0;
    };
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
    typedef int Identifier;
    static bool identifierIsValid(Identifier identifier) { return identifier != -1; }

    struct SocketPair {
        int client;
        int server;
    };

    enum ConnectionOptions {
        SetCloexecOnClient = 1 << 0,
        SetCloexecOnServer = 1 << 1,
    };

    static Connection::SocketPair createPlatformConnection(unsigned options = SetCloexecOnClient | SetCloexecOnServer);
#elif OS(DARWIN)
    struct Identifier {
        Identifier()
        {
        }

        Identifier(mach_port_t port)
            : port(port)
        {
        }

        Identifier(mach_port_t port, OSObjectPtr<xpc_connection_t> xpcConnection)
            : port(port)
            , xpcConnection(WTFMove(xpcConnection))
        {
        }

        mach_port_t port { MACH_PORT_NULL };
        OSObjectPtr<xpc_connection_t> xpcConnection;
    };
    static bool identifierIsValid(Identifier identifier) { return MACH_PORT_VALID(identifier.port); }
    xpc_connection_t xpcConnection() const { return m_xpcConnection.get(); }
    Optional<audit_token_t> getAuditToken();
    pid_t remoteProcessID() const;
#elif OS(WINDOWS)
    typedef HANDLE Identifier;
    static bool createServerAndClientIdentifiers(Identifier& serverIdentifier, Identifier& clientIdentifier);
    static bool identifierIsValid(Identifier identifier) { return !!identifier; }
#endif

    static Ref<Connection> createServerConnection(Identifier, Client&);
    static Ref<Connection> createClientConnection(Identifier, Client&);
    ~Connection();

    Client& client() const { return m_client; }

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
    void addMessageReceiveQueue(MessageReceiveQueue&, ReceiverName, uint64_t destinationID = 0);

    void removeMessageReceiveQueue(ReceiverName, uint64_t destinationID = 0);

    // Adds a message receieve queue that dispatches through WorkQueue to WorkQueueMessageReceiver.
    // Keeps the WorkQueue and the WorkQueueMessageReceiver alive. Dispatched tasks keep WorkQueueMessageReceiver alive.
    void addWorkQueueMessageReceiver(ReceiverName, WorkQueue&, WorkQueueMessageReceiver*, uint64_t destinationID = 0);
    void removeWorkQueueMessageReceiver(ReceiverName receiverName, uint64_t destinationID = 0) { removeMessageReceiveQueue(receiverName, destinationID); }

    // Adds a message receieve queue that dispatches through ThreadMessageReceiver.
    // Keeps the ThreadMessageReceiver alive. Dispatched tasks keep the ThreadMessageReceiver alive.
    void addThreadMessageReceiver(ReceiverName, ThreadMessageReceiver*, uint64_t destinationID = 0);
    void removeThreadMessageReceiver(ReceiverName receiverName, uint64_t destinationID = 0) { removeMessageReceiveQueue(receiverName, destinationID); }

    bool open();
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void postConnectionDidCloseOnConnectionWorkQueue();
    template<typename T, typename C> uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T> bool send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }); // Thread-safe.
    // Sync senders should check the SendSyncResult for true/false in case they need to know if the result was really received.
    // Sync senders should hold on to the SendSyncResult in case they reference the contents of the reply via DataRefererence / ArrayReference.
    using SendSyncResult = std::unique_ptr<Decoder>;
    template<typename T> SendSyncResult sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, Seconds timeout = Seconds::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { }); // Main thread only.
    template<typename> bool waitForAndDispatchImmediately(uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions = { }); // Main thread only.
    template<typename> bool waitForAsyncCallbackAndDispatchImmediately(uint64_t callbackID, Seconds timeout); // Main thread only.

    // Thread-safe.
    template<typename T, typename C, typename U>
    uint64_t sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID = { }, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply<T, C>(WTFMove(message), WTFMove(completionHandler), destinationID.toUInt64(), sendOptions);
    }
    
    // Thread-safe.
    template<typename T, typename U>
    bool send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return send<T>(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    // Main thread only.
    template<typename T, typename U>
    SendSyncResult sendSync(T&& message, typename T::Reply&& reply, ObjectIdentifier<U> destinationID, Seconds timeout = Seconds::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(WTFMove(message), WTFMove(reply), destinationID.toUInt64(), timeout, sendSyncOptions);
    }
    
    // Main thread only.
    template<typename T, typename U>
    bool waitForAndDispatchImmediately(ObjectIdentifier<U> destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions = { })
    {
        return waitForAndDispatchImmediately<T>(destinationID.toUInt64(), timeout, waitForOptions);
    }

    bool sendMessage(std::unique_ptr<Encoder>, OptionSet<SendOption> sendOptions);
    std::unique_ptr<Encoder> createSyncMessageEncoder(MessageName, uint64_t destinationID, uint64_t& syncRequestID);
    std::unique_ptr<Decoder> sendSyncMessage(uint64_t syncRequestID, std::unique_ptr<Encoder>, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions);
    bool sendSyncReply(std::unique_ptr<Encoder>);

    void wakeUpRunLoop();

    void incrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { ++m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }
    void decrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { --m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }

    bool inSendSync() const { return m_inSendSyncCount; }

    Identifier identifier() const;

#if PLATFORM(COCOA)
    bool kill();
    void terminateSoon(Seconds);
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
#endif

    void dispatchMessageReceiverMessage(MessageReceiver&, std::unique_ptr<Decoder>&&);
    // Can be called from any thread.
    void dispatchDidReceiveInvalidMessage(MessageName);
private:
    Connection(Identifier, bool isServer, Client&);
    void platformInitialize(Identifier);
    void platformInvalidate();

    bool isIncomingMessagesThrottlingEnabled() const { return !!m_incomingMessagesThrottler; }
    
    std::unique_ptr<Decoder> waitForMessage(MessageName, uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption>);
    
    std::unique_ptr<Decoder> waitForSyncReply(uint64_t syncRequestID, MessageName, Seconds timeout, OptionSet<SendSyncOption>);

    // Called on the connection work queue.
    void processIncomingMessage(std::unique_ptr<Decoder>);
    void processIncomingSyncReply(std::unique_ptr<Decoder>);

    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(std::unique_ptr<Encoder>);
    void connectionDidClose();
    
    // Called on the listener thread.
    void dispatchOneIncomingMessage();
    void dispatchIncomingMessages();
    void dispatchMessage(std::unique_ptr<Decoder>);
    void dispatchMessage(Decoder&);
    void dispatchSyncMessage(Decoder&);
    void didFailToSendSyncMessage();

    // Can be called on any thread.
    void enqueueIncomingMessage(std::unique_ptr<Decoder>);
    size_t incomingMessagesDispatchingBatchSize() const;

    void willSendSyncMessage(OptionSet<SendSyncOption>);
    void didReceiveSyncReply(OptionSet<SendSyncOption>);

    Seconds timeoutRespectingIgnoreTimeoutsForTesting(Seconds) const;

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

    Client& m_client;
    UniqueID m_uniqueID;
    bool m_isServer;
    std::atomic<bool> m_isValid { true };
    std::atomic<uint64_t> m_syncRequestID;

    bool m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage;
    bool m_shouldExitOnSyncMessageSendFailure;
    DidCloseOnConnectionWorkQueueCallback m_didCloseOnConnectionWorkQueueCallback;

    bool m_isConnected;
    Ref<WorkQueue> m_connectionQueue;

    unsigned m_inSendSyncCount;
    unsigned m_inDispatchMessageCount;
    unsigned m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount;
    unsigned m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting { 0 };
    bool m_fullySynchronousModeIsAllowedForTesting { false };
    bool m_ignoreTimeoutsForTesting { false };
    bool m_didReceiveInvalidMessage;

    // Incoming messages.
    Lock m_incomingMessagesMutex;
    Deque<std::unique_ptr<Decoder>> m_incomingMessages;
    std::unique_ptr<MessagesThrottler> m_incomingMessagesThrottler;
    MessageReceiveQueueMap m_receiveQueues;

    // Outgoing messages.
    Lock m_outgoingMessagesMutex;
    Deque<std::unique_ptr<Encoder>> m_outgoingMessages;
    
    Condition m_waitForMessageCondition;
    Lock m_waitForMessageMutex;

    struct WaitForMessageState;
    WaitForMessageState* m_waitingForMessage { nullptr };

    class SyncMessageState;

    Lock m_syncReplyStateMutex;
    bool m_shouldWaitForSyncReplies;
    bool m_shouldWaitForMessages;
    struct PendingSyncReply;
    Vector<PendingSyncReply> m_pendingSyncReplies;

    Lock m_incomingSyncMessageCallbackMutex;
    HashMap<uint64_t, WTF::Function<void()>> m_incomingSyncMessageCallbacks;
    RefPtr<WorkQueue> m_incomingSyncMessageCallbackQueue;
    uint64_t m_nextIncomingSyncMessageCallbackID { 0 };

#if ENABLE(IPC_TESTING_API)
    Vector<WeakPtr<MessageObserver>> m_messageObservers;
    bool m_ignoreInvalidMessageForTesting { false };
#endif

#if HAVE(QOS_CLASSES)
    pthread_t m_mainThread { 0 };
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
    dispatch_source_t m_sendSource { nullptr };

    mach_port_t m_receivePort { MACH_PORT_NULL };
    dispatch_source_t m_receiveSource { nullptr };

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
};

template<typename T>
bool Connection::send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = makeUnique<Encoder>(T::name(), destinationID);
    *encoder << message.arguments();
    
    return sendMessage(WTFMove(encoder), sendOptions);
}

uint64_t nextAsyncReplyHandlerID();
void addAsyncReplyHandler(Connection&, uint64_t, CompletionHandler<void(Decoder*)>&&);
CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(Connection&, uint64_t);

template<typename T, typename C>
uint64_t Connection::sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = makeUnique<Encoder>(T::name(), destinationID);
    uint64_t listenerID = nextAsyncReplyHandlerID();
    addAsyncReplyHandler(*this, listenerID, CompletionHandler<void(Decoder*)>([completionHandler = WTFMove(completionHandler)] (Decoder* decoder) mutable {
        if (decoder && decoder->isValid())
            T::callReply(*decoder, WTFMove(completionHandler));
        else
            T::cancelReply(WTFMove(completionHandler));
    }, CompletionHandlerCallThread::MainThread));
    *encoder << listenerID;
    *encoder << message.arguments();
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

template<typename T> Connection::SendSyncResult Connection::sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    COMPILE_ASSERT(T::isSync, SyncMessageExpected);
    RELEASE_ASSERT(RunLoop::isMain());

    uint64_t syncRequestID = 0;
    std::unique_ptr<Encoder> encoder = createSyncMessageEncoder(T::name(), destinationID, syncRequestID);

    if (sendSyncOptions.contains(SendSyncOption::UseFullySynchronousModeForTesting)) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    *encoder << message.arguments();

    // Now send the message and wait for a reply.
    std::unique_ptr<Decoder> replyDecoder = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, sendSyncOptions);
    if (!replyDecoder)
        return { };

    // Decode the reply.
    Optional<typename T::ReplyArguments> replyArguments;
    *replyDecoder >> replyArguments;
    if (!replyArguments)
        return { };
    moveTuple(WTFMove(*replyArguments), reply);
    return replyDecoder;
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions)
{
    RELEASE_ASSERT(RunLoop::isMain());
    std::unique_ptr<Decoder> decoder = waitForMessage(T::name(), destinationID, timeout, waitForOptions);
    if (!decoder)
        return false;

    ASSERT(decoder->destinationID() == destinationID);
    m_client.didReceiveMessage(*this, *decoder);
    return true;
}

template<typename T> bool Connection::waitForAsyncCallbackAndDispatchImmediately(uint64_t destinationID, Seconds timeout)
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

} // namespace IPC
