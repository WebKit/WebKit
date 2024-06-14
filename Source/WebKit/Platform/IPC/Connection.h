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

#include "ConnectionHandle.h"
#include "MessageReceiveQueueMap.h"
#include "MessageReceiver.h"
#include "ReceiverMatcher.h"
#include "Timeout.h"
#include <wtf/Assertions.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NativePromise.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

#if OS(DARWIN)
#include <mach/mach_port.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
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
#if ENABLE(IPC_TESTING_API)
    IPCTestingMessage = 1 << 3,
#endif
};

enum class SendSyncOption : uint8_t {
    UseFullySynchronousModeForTesting = 1 << 0,
    ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply = 1 << 1,
    MaintainOrderingWithAsyncMessages = 1 << 2,
};

enum class WaitForOption {
    // Use this to make waitForMessage be interrupted immediately by any incoming sync messages.
    InterruptWaitingIfSyncMessageArrives = 1 << 0,
    DispatchIncomingSyncMessagesWhileWaiting = 1 << 1,
};

enum class Error : uint8_t {
    NoError = 0,
    InvalidConnection,
    NoConnectionForIdentifier,
    NoMessageSenderConnection,
    Timeout,
    Unspecified,
    MultipleWaitingClients,
    AttemptingToWaitOnClosedConnection,
    WaitingOnAlreadyDispatchedMessage,
    AttemptingToWaitInsideSyncMessageHandling,
    SyncMessageInterruptedWait,
    CantWaitForSyncReplies,
    FailedToEncodeMessageArguments,
    FailedToDecodeReplyArguments,
    FailedToFindReplyHandler,
    FailedToAcquireBufferSpan,
    FailedToAcquireReplyBufferSpan,
    StreamConnectionEncodingError,
};

extern const char* errorAsString(Error);

#define CONNECTION_STRINGIFY(line) #line
#define CONNECTION_STRINGIFY_MACRO(line) CONNECTION_STRINGIFY(line)

#define MESSAGE_CHECK_BASE(assertion, connection) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, (void)0)

#define MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion) do { \
    if (UNLIKELY(!(assertion))) { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %s", WTF_PRETTY_FUNCTION); \
        (connection)->markCurrentlyDispatchedMessageAsInvalid(); \
        { completion; } \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, connection, returnValue) do { \
    if (UNLIKELY(!(assertion))) { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING, WTF_PRETTY_FUNCTION); \
        (connection)->markCurrentlyDispatchedMessageAsInvalid(); \
        return (returnValue); \
    } \
} while (0)

template<typename AsyncReplyResult> struct AsyncReplyError {
    static AsyncReplyResult create() { return AsyncReplyResult { }; };
};

class Decoder;
class Encoder;
class MachMessage;
class UnixMessage;
class WorkQueueMessageReceiver;

struct AsyncReplyIDType;
using AsyncReplyID = AtomicObjectIdentifier<AsyncReplyIDType>;

// Sync message sender is expected to hold this instance alive as long as the reply() is being
// accessed. View type data types in replies, such as std::span, refer to data stored in
// ConnectionSendSyncResult.
template<typename T> class ConnectionSendSyncResult {
public:
    ConnectionSendSyncResult(Error error)
        : value(makeUnexpected(error))
    {
        ASSERT(value.error() != Error::NoError);
    }

    ConnectionSendSyncResult(UniqueRef<Decoder>&& decoder, typename T::ReplyArguments&& replyArguments)
        : value({ WTFMove(decoder), WTFMove(replyArguments) })
    {
    }

    bool succeeded() const { return value.has_value(); }
    Error error() const { return value.has_value() ? Error::NoError : value.error(); }

    typename T::ReplyArguments& reply()
    {
        return value.value().reply;
    }

    typename T::ReplyArguments takeReply()
    {
        return WTFMove(value.value().reply);
    }

    template<typename... U>
    typename T::ReplyArguments takeReplyOr(U&&... defaultValues)
    {
        if (!value.has_value())
            return { std::forward<U>(defaultValues)... };
        return takeReply();
    }
private:
    struct ReplyData {
        UniqueRef<Decoder> decoder; // Owns the memory for reply.
        typename T::ReplyArguments reply;
    };
    Expected<ReplyData, Error> value;
};

struct ConnectionAsyncReplyHandler {
    CompletionHandler<void(Decoder*)> completionHandler;
    AsyncReplyID replyID;
};

enum class ConnectionSyncRequestIDType { };
using ConnectionSyncRequestID = AtomicObjectIdentifier<ConnectionSyncRequestIDType>;

class Connection : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Connection, WTF::DestructionThread::MainRunLoop> {
public:
    enum class SyncRequestIDType { };
    using SyncRequestID = ConnectionSyncRequestID;
    using AsyncReplyID = IPC::AsyncReplyID;

    class Client : public MessageReceiver {
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, MessageName) = 0;

    protected:
        virtual ~Client() { }
    };

    using Handle = ConnectionHandle;

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
            : Identifier(handle.leak())
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
        explicit operator bool() const { return MACH_PORT_VALID(port); }
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
    using UniqueID = AtomicObjectIdentifier<UniqueIDType>;
    using DecoderOrError = Expected<UniqueRef<Decoder>, Error>;

    static RefPtr<Connection> connection(UniqueID);
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

    using OutgoingMessageQueueIsGrowingLargeCallback = Function<void()>;
    void setOutgoingMessageQueueIsGrowingLargeCallback(OutgoingMessageQueueIsGrowingLargeCallback&&);

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

    bool open(Client&, SerialFunctionDispatcher& = RunLoop::current());
    // Ensures that messages sent prior to the call are not affected by invalidate() or crash done after the call returns.
    Error flushSentMessages(Timeout);
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    template<typename T, typename C> AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T> Ref<typename T::Promise> sendWithPromisedReply(T&& message, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T> Error send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.
    template<typename T> static Error send(UniqueID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.

    // Sync senders should check the SendSyncResult for true/false in case they need to know if the result was really received.
    // Sync senders should hold on to the SendSyncResult in case they reference the contents of the reply via DataRefererence / ArrayReference.

    template<typename T> using SendSyncResult = ConnectionSendSyncResult<T>;
    template<typename T> SendSyncResult<T> sendSync(T&& message, uint64_t destinationID, Timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { }); // Main thread only.

    template<typename> Error waitForAndDispatchImmediately(uint64_t destinationID, Timeout, OptionSet<WaitForOption> waitForOptions = { }); // Main thread only.
    template<typename> Error waitForAsyncReplyAndDispatchImmediately(AsyncReplyID, Timeout); // Main thread only.

    // Thread-safe.
    template<typename T, typename C>
    AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, const ObjectIdentifierGenericBase& destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply<T, C>(std::forward<T>(message), std::forward<C>(completionHandler), destinationID.toUInt64(), sendOptions);
    }

    template<typename T>
    Ref<typename T::Promise> sendWithPromisedReply(T&& message, const ObjectIdentifierGenericBase& destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithPromisedReply<T>(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    // Thread-safe.
    template<typename T>
    Error send(T&& message, const ObjectIdentifierGenericBase& destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt)
    {
        return send<T>(std::forward<T>(message), destinationID.toUInt64(), sendOptions, qos);
    }

    // Main thread only.
    template<typename T>
    SendSyncResult<T> sendSync(T&& message, const ObjectIdentifierGenericBase& destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(std::forward<T>(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    // Main thread only.
    template<typename T>
    Error waitForAndDispatchImmediately(const ObjectIdentifierGenericBase& destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions = { })
    {
        return waitForAndDispatchImmediately<T>(destinationID.toUInt64(), timeout, waitForOptions);
    }

    Error sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);

    using AsyncReplyHandler = ConnectionAsyncReplyHandler;
    Error sendMessageWithAsyncReply(UniqueRef<Encoder>&&, AsyncReplyHandler, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);
    UniqueRef<Encoder> createSyncMessageEncoder(MessageName, uint64_t destinationID, SyncRequestID&);
    DecoderOrError sendSyncMessage(SyncRequestID, UniqueRef<Encoder>&&, Timeout, OptionSet<SendSyncOption> sendSyncOptions);
    Error sendSyncReply(UniqueRef<Encoder>&&);

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
    void dispatchIncomingMessageForTesting(UniqueRef<Decoder>&&);
    DecoderOrError waitForMessageForTesting(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);
#endif

    void dispatchMessageReceiverMessage(MessageReceiver&, UniqueRef<Decoder>&&);
    // Can be called from any thread.
    void dispatchDidReceiveInvalidMessage(MessageName);
    void dispatchDidCloseAndInvalidate();

    size_t pendingMessageCountForTesting() const;
    void dispatchOnReceiveQueueForTesting(Function<void()>&&);

    template<typename T, typename C> static AsyncReplyHandler makeAsyncReplyHandler(C&& completionHandler, ThreadLikeAssertion callThread = CompletionHandlerCallThread::AnyThread);
    template<typename T> static AsyncReplyHandler makeAsyncReplyHandler(typename T::Promise::Producer&&, ThreadLikeAssertion callThread = CompletionHandlerCallThread::AnyThread);

    CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(AsyncReplyID);

    template<typename T, typename C> static void callReply(IPC::Decoder&, C&& completionHandler);
    template<typename T, typename C> static void cancelReply(C&& completionHandler);

private:
    Connection(Identifier, bool isServer);
    void platformInitialize(Identifier);
    bool platformPrepareForOpen();
    void platformOpen();
    void platformInvalidate();

    bool isIncomingMessagesThrottlingEnabled() const { return m_incomingMessagesThrottlingLevel.has_value(); }

    static HashMap<IPC::Connection::UniqueID, ThreadSafeWeakPtr<Connection>>& connectionMap() WTF_REQUIRES_LOCK(s_connectionMapLock);

    DecoderOrError waitForMessage(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);

    SyncRequestID makeSyncRequestID() { return SyncRequestID::generate(); }
    bool pushPendingSyncRequestID(SyncRequestID);
    void popPendingSyncRequestID(SyncRequestID);
    DecoderOrError waitForSyncReply(SyncRequestID, MessageName, Timeout, OptionSet<SendSyncOption>);

    void enqueueMatchingMessagesToMessageReceiveQueue(MessageReceiveQueue&, const ReceiverMatcher&) WTF_REQUIRES_LOCK(m_incomingMessagesLock);

    // Called on the connection work queue.
    void processIncomingMessage(UniqueRef<Decoder>);
    void processIncomingSyncReply(UniqueRef<Decoder>);

    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(UniqueRef<Encoder>&&);
    void connectionDidClose();

    // Called on the connection run loop.
    void dispatchSyncStateMessages();
    void dispatchOneIncomingMessage();
    void dispatchIncomingMessages();
    void dispatchMessage(UniqueRef<Decoder>);
    void dispatchMessage(Decoder&);
    void dispatchSyncMessage(Decoder&);
    void didFailToSendSyncMessage(Error);

    // Can be called on any thread.
    void enqueueIncomingMessage(UniqueRef<Decoder>) WTF_REQUIRES_LOCK(m_incomingMessagesLock);
    size_t incomingMessagesDispatchingBatchSize() const;

    Timeout timeoutRespectingIgnoreTimeoutsForTesting(Timeout) const;

#if PLATFORM(COCOA)
    bool sendMessage(std::unique_ptr<MachMessage>);
#endif
    template<typename F>
    void dispatchToClient(F&& clientRunLoopTask);

    size_t numberOfMessagesToProcess(size_t totalMessages);
    bool isThrottlingIncomingMessages() const { return *m_incomingMessagesThrottlingLevel > 0; }

    // Only valid between open() and invalidate().
    SerialFunctionDispatcher& dispatcher();

    class SyncMessageState;
    struct SyncMessageStateRelease {
        void operator()(SyncMessageState*) const;
    };
    void addAsyncReplyHandler(AsyncReplyHandler&&);
    void cancelAsyncReplyHandlers();

    static Lock s_connectionMapLock;
    Client* m_client { nullptr };
    std::unique_ptr<SyncMessageState, SyncMessageStateRelease> m_syncState;
    UniqueID m_uniqueID;
    bool m_isServer;
    std::atomic<bool> m_isValid { true };

    bool m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage { false };
    bool m_shouldExitOnSyncMessageSendFailure { false };
    DidCloseOnConnectionWorkQueueCallback m_didCloseOnConnectionWorkQueueCallback { nullptr };
    OutgoingMessageQueueIsGrowingLargeCallback m_outgoingMessageQueueIsGrowingLargeCallback;
    MonotonicTime m_lastOutgoingMessageQueueIsGrowingLargeCallbackCallTime WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);

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
    std::optional<uint8_t> m_incomingMessagesThrottlingLevel;

    // Incoming messages.
    mutable Lock m_incomingMessagesLock;
    Deque<UniqueRef<Decoder>> m_incomingMessages WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);
    MessageReceiveQueueMap m_receiveQueues WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);

    // Outgoing messages.
    Lock m_outgoingMessagesLock;
    Deque<UniqueRef<Encoder>> m_outgoingMessages WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);
    Condition m_outgoingMessagesEmptyCondition WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);

    Condition m_waitForMessageCondition;
    Lock m_waitForMessageLock;

    struct WaitForMessageState;
    WaitForMessageState* m_waitingForMessage WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { nullptr }; // NOLINT

    Lock m_syncReplyStateLock;
    bool m_shouldWaitForSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock) { true };
    bool m_shouldWaitForMessages WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { true };
    struct PendingSyncReply;
    Vector<PendingSyncReply> m_pendingSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock);

    Lock m_incomingSyncMessageCallbackLock;
    HashMap<uint64_t, WTF::Function<void()>> m_incomingSyncMessageCallbacks WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    RefPtr<WorkQueue> m_incomingSyncMessageCallbackQueue WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    uint64_t m_nextIncomingSyncMessageCallbackID WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock) { 0 };

    using AsyncReplyHandlerMap = HashMap<AsyncReplyID, CompletionHandler<void(Decoder*)>>;
    AsyncReplyHandlerMap m_asyncReplyHandlers WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);

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
    void cancelSendSource();

    mach_port_t m_sendPort { MACH_PORT_NULL };
    OSObjectPtr<dispatch_source_t> m_sendSource;

    mach_port_t m_receivePort { MACH_PORT_NULL };
    OSObjectPtr<dispatch_source_t> m_receiveSource;

    std::unique_ptr<MachMessage> m_pendingOutgoingMachMessage;

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
Error Connection::send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    static_assert(!T::isSync, "Async message expected");

    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    encoder.get() << message.arguments();

    return sendMessage(WTFMove(encoder), sendOptions, qos);
}

template<typename T>
Error Connection::send(UniqueID connectionID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    Locker locker { s_connectionMapLock };
    RefPtr connection = connectionMap().get(connectionID).get();
    if (!connection)
        return Error::NoConnectionForIdentifier;
    return connection->send(std::forward<T>(message), destinationID, sendOptions, qos);
}

template<typename T, typename C>
Connection::AsyncReplyID Connection::sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");
    auto handler = makeAsyncReplyHandler<T>(std::forward<C>(completionHandler));
    auto replyID = handler.replyID;
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    encoder.get() << message.arguments();
    if (sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(handler), sendOptions) == Error::NoError)
        return replyID;
    // FIXME: Propagate the error back.
    return { };
}

template<typename T>
Ref<typename T::Promise> Connection::sendWithPromisedReply(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");
    typename T::Promise::Producer producer(WTF::PromiseDispatchMode::RunSynchronouslyOnTarget);
    Ref<typename T::Promise> promise = producer;
    auto handler = makeAsyncReplyHandler<T>(WTFMove(producer));
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    encoder.get() << message.arguments();
    sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(handler), sendOptions);
    // The promise will be rejected in the handler should an error occur.
    return promise;
}

template<typename T> Connection::SendSyncResult<T> Connection::sendSync(T&& message, uint64_t destinationID, Timeout timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    static_assert(T::isSync, "Sync message expected");
    SyncRequestID syncRequestID;
    auto encoder = createSyncMessageEncoder(T::name(), destinationID, syncRequestID);

    if (sendSyncOptions.contains(SendSyncOption::UseFullySynchronousModeForTesting)) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    encoder.get() << message.arguments();

    // Now send the message and wait for a reply.
    auto replyDecoderOrError = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, sendSyncOptions);
    if (!replyDecoderOrError.has_value()) {
        ASSERT(replyDecoderOrError.error() != Error::NoError);
        return { replyDecoderOrError.error() };
    }

    std::optional<typename T::ReplyArguments> replyArguments;
    *replyDecoderOrError.value() >> replyArguments;
    if (!replyArguments)
        return { Error::FailedToDecodeReplyArguments };

    return SendSyncResult<T> { WTFMove(replyDecoderOrError.value()), WTFMove(*replyArguments) };
}

template<typename T> Error Connection::waitForAndDispatchImmediately(uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions)
{
    static_assert(T::canDispatchOutOfOrder, "Can only use waitForAndDispatchImmediately on messages declared with CanDispatchOutOfOrder");
    auto decoderOrError = waitForMessage(T::name(), destinationID, timeout, waitForOptions);
    if (!decoderOrError.has_value())
        return decoderOrError.error();

    if (!isValid())
        return Error::InvalidConnection;

    ASSERT(decoderOrError.value()->destinationID() == destinationID);
    m_client->didReceiveMessage(*this, decoderOrError.value());
    return Error::NoError;
}

template<typename T> Error Connection::waitForAsyncReplyAndDispatchImmediately(AsyncReplyID replyID, Timeout timeout)
{
    static_assert(T::replyCanDispatchOutOfOrder, "Can only use waitForAsyncReplyAndDispatchImmediately on messages declared with ReplyCanDispatchOutOfOrder");
    auto decoderOrError = waitForMessage(T::asyncMessageReplyName(), replyID.toUInt64(), timeout, { });
    if (!decoderOrError.has_value())
        return decoderOrError.error();

    ASSERT(decoderOrError.value()->messageReceiverName() == ReceiverName::AsyncReply);
    ASSERT(decoderOrError.value()->destinationID() == replyID.toUInt64());
    auto handler = takeAsyncReplyHandler(AtomicObjectIdentifier<AsyncReplyIDType>(decoderOrError.value()->destinationID()));
    if (!handler) {
        ASSERT_NOT_REACHED();
        return Error::FailedToFindReplyHandler;
    }
    handler(&decoderOrError.value().get());
    return Error::NoError;
}

#if ENABLE(IPC_TESTING_API)
inline auto Connection::waitForMessageForTesting(MessageName messageName, uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> options) -> DecoderOrError
{
    return waitForMessage(messageName, destinationID, timeout, options);
}
#endif

template<typename T, typename C>
Connection::AsyncReplyHandler Connection::makeAsyncReplyHandler(C&& completionHandler, ThreadLikeAssertion callThread)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=248947): callThread by default uses AnyThread because the
    // API contract on invalid sends does not make sense.
    return AsyncReplyHandler {
        {
            [completionHandler = WTFMove(completionHandler)] (Decoder* decoder) mutable {
                if (decoder && decoder->isValid())
                    callReply<T>(*decoder, WTFMove(completionHandler));
                else
                    cancelReply<T>(WTFMove(completionHandler));
            }, callThread
        },
        AsyncReplyID::generate()
    };
}

template<typename T, typename C>
void Connection::callReply(Decoder& decoder, C&& completionHandler)
{
    if constexpr (!std::tuple_size_v<typename T::ReplyArguments>) {
        // Nothing to decode in case of no reply arguments, so just invoke the completion handler in that case.
        completionHandler();
    } else {
        if (auto arguments = decoder.decode<typename T::ReplyArguments>()) {
            std::apply(std::forward<C>(completionHandler), WTFMove(*arguments));
            return;
        }

        ASSERT_NOT_REACHED();
        cancelReply<T>(std::forward<C>(completionHandler));
    }
}

template<typename T, typename C>
void Connection::cancelReply(C&& completionHandler)
{
    [&]<size_t... Indices>(std::index_sequence<Indices...>)
    {
        completionHandler(AsyncReplyError<std::tuple_element_t<Indices, typename T::ReplyArguments>>::create()...);
    }(std::make_index_sequence<std::tuple_size_v<typename T::ReplyArguments>> { });
}

template<typename T>
Connection::AsyncReplyHandler Connection::makeAsyncReplyHandler(typename T::Promise::Producer&& producer, ThreadLikeAssertion callThread)
{
    return AsyncReplyHandler {
        {
            [producer = WTFMove(producer)] (Decoder* decoder) mutable {
                if (!decoder) {
                    producer.reject(Error::InvalidConnection);
                    return;
                }
                if (!decoder->isValid()) {
                    producer.reject(Error::FailedToDecodeReplyArguments);
                    return;
                }
                if constexpr (!std::tuple_size_v<typename T::ReplyArguments>) {
                    producer.resolve();
                    return;
                } else if (auto arguments = decoder->decode<typename T::ReplyArguments>()) {
                    if constexpr (std::tuple_size_v<typename T::ReplyArguments> == 1)
                        producer.resolve(std::get<0>(WTFMove(*arguments)));
                    else
                        producer.resolve(WTFMove(*arguments));
                    return;
                }
                ASSERT_NOT_REACHED();
                producer.reject(Error::FailedToDecodeReplyArguments);
            }, callThread
        },
        AsyncReplyID::generate()
    };
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
