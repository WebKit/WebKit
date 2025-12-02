/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
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
#include "SyncRequestID.h"
#include "Timeout.h"
#include <atomic>
#include <new>
#include <tuple>
#include <wtf/Assertions.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Expected.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NativePromise.h>
#include <wtf/Noncopyable.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainReleaseSwift.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/ThreadSafetyAnalysis.h>
#include <wtf/Threading.h>
#include <wtf/Unexpected.h>
#include <wtf/UniqueRef.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CString.h>

#if OS(ANDROID)
#include <wtf/android/RefPtrAndroid.h>
#endif

#if OS(DARWIN)
#include <mach/mach_port.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/darwin/XPCObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#if HAVE(XPC_API)
#include <xpc/xpc.h>
#endif
#endif // OS(DARWIN)

#if USE(GLIB)
#include <wtf/glib/GSocketMonitor.h>
#endif

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/TZoneHeap.h>
#include <bmalloc/bmalloc.h>
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
#include <wtf/unix/UnixFileDescriptor.h>
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
    SyncMessageCancelled,
    CantWaitForSyncReplies,
    FailedToEncodeMessageArguments,
    FailedToDecodeReplyArguments,
    FailedToFindReplyHandler,
    FailedToAcquireBufferSpan,
    FailedToAcquireReplyBufferSpan,
    StreamConnectionEncodingError,
};

extern ASCIILiteral errorAsString(Error);

#define CONNECTION_STRINGIFY(line) #line
#define CONNECTION_STRINGIFY_MACRO(line) CONNECTION_STRINGIFY(line)

#if ENABLE(IPC_TESTING_API)
#define CRASH_IF_TESTING
#else
#define CRASH_IF_TESTING if (IPC::Connection::shouldCrashOnMessageCheckFailure()) { CRASH(); }
#endif

#define MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, connection, message) do { \
    if (!(assertion)) [[unlikely]] { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING ": " message, WTF_PRETTY_FUNCTION); \
        IPC::markCurrentlyDispatchedMessageAsInvalid(connection, "Message check failed: " #assertion " - " #message ## _s); \
        CRASH_IF_TESTING \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_BASE(assertion, connection) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, (void)0)
#define MESSAGE_CHECK_BASE_COROUTINE(assertion, connection) MESSAGE_CHECK_COMPLETION_BASE_COROUTINE(assertion, connection, (void)0)

#define MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection) do { \
    if (!(assertion)) [[unlikely]] { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING, WTF_PRETTY_FUNCTION); \
        IPC::markCurrentlyDispatchedMessageAsInvalid(connection, "Message check failed: " #assertion ## _s); \
        CRASH_IF_TESTING \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion) do { \
    if (!(assertion)) [[unlikely]] { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING, WTF_PRETTY_FUNCTION); \
        IPC::markCurrentlyDispatchedMessageAsInvalid(connection, "Message check failed: " #assertion ## _s); \
        CRASH_IF_TESTING \
        { completion; } \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_COMPLETION_BASE_COROUTINE(assertion, connection, completion) do { \
    if (!(assertion)) [[unlikely]] { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING, WTF_PRETTY_FUNCTION); \
        IPC::markCurrentlyDispatchedMessageAsInvalid(connection, "Message check failed: " #assertion ## _s); \
        CRASH_IF_TESTING \
        { completion; } \
        co_return { }; \
    } \
} while (0)

#define MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, connection, returnValue) do { \
    if (!(assertion)) [[unlikely]] { \
        RELEASE_LOG_FAULT(IPC, __FILE__ " " CONNECTION_STRINGIFY_MACRO(__LINE__) ": Invalid message dispatched %" PUBLIC_LOG_STRING, WTF_PRETTY_FUNCTION); \
        IPC::markCurrentlyDispatchedMessageAsInvalid(connection, "Message check failed: " #assertion ## _s); \
        CRASH_IF_TESTING \
        return (returnValue); \
    } \
} while (0)

template<typename AsyncReplyResult> struct AsyncReplyError {
    static AsyncReplyResult create() { return AsyncReplyResult { }; };
};

template<typename T, typename E> struct AsyncReplyError<Expected<T, E>> {
    static Expected<T, E> create() { return makeUnexpected<E>(AsyncReplyError<E>::create()); };
};

class Decoder;
class MachMessage;
class UnixMessage;
class WorkQueueMessageReceiverBase;

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
    CompletionHandler<void(Connection*, Decoder*)> completionHandler;
    Markable<AsyncReplyID> replyID;
};

class Connection : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Connection, WTF::DestructionThread::MainRunLoop> {
public:
    using SyncRequestID = IPC::SyncRequestID;
    using AsyncReplyID = IPC::AsyncReplyID;

    class Client : public MessageReceiver, public CanMakeThreadSafeCheckedPtr<Client> {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Client);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(Client);
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) = 0;
        virtual void requestRemoteProcessTermination() { }

    protected:
        virtual ~Client() { }
    };

    using Handle = ConnectionHandle;

    struct Identifier {
        WTF_MAKE_NONCOPYABLE(Identifier);

        Identifier() = default;
        Identifier(Identifier&&) = default;
        Identifier& operator=(Identifier&&) = default;

#if USE(UNIX_DOMAIN_SOCKETS)
        explicit Identifier(Handle&& handle)
            : Identifier({ handle.release(), UnixFileDescriptor::Adopt })
        {
        }
        explicit Identifier(UnixFileDescriptor&& fd)
            : handle(WTFMove(fd))
        {
        }
        operator bool() const { return !!handle; }
        UnixFileDescriptor handle;
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
        Identifier(mach_port_t port, XPCObjectPtr<xpc_connection_t> xpcConnection)
            : port(port)
            , xpcConnection(WTFMove(xpcConnection))
        {
        }
        operator bool() const { return MACH_PORT_VALID(port); }
        mach_port_t port { MACH_PORT_NULL };
        XPCObjectPtr<xpc_connection_t> xpcConnection;
#endif
    };

#if OS(DARWIN)
    xpc_connection_t xpcConnection() const { return m_xpcConnection.get(); }
    XPCObjectPtr<xpc_connection_t> protectedXPCConnection() const { return xpcConnection(); }
    std::optional<audit_token_t> getAuditToken();
    pid_t remoteProcessID() const;
#endif

#if USE(GLIB)
    void sendCredentials() const;
    static pid_t remoteProcessID(GSocket*);
#endif

    static Ref<Connection> createServerConnection(Identifier&&, Thread::QOS = Thread::QOS::Default);
    static Ref<Connection> createClientConnection(Identifier&&);

    struct ConnectionIdentifierPair {
        IPC::Connection::Identifier server;
        IPC::Connection::Handle client;
    };
    static std::optional<ConnectionIdentifierPair> createConnectionIdentifierPair();

    ~Connection();

    Client* client() const { return m_client.get(); }
    RefPtr<Client> protectedClient() const { return m_client.get(); }

    enum UniqueIDType { };
    using UniqueID = AtomicObjectIdentifier<UniqueIDType>;
#ifndef __swift__ // rdar://152496447
    using DecoderOrError = Expected<UniqueRef<Decoder>, Error>;
#endif

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
    void addWorkQueueMessageReceiver(ReceiverName, WorkQueue&, WorkQueueMessageReceiverBase&, uint64_t destinationID = 0);
    void removeWorkQueueMessageReceiver(ReceiverName, uint64_t destinationID = 0);

    // Adds a message receive queue that dispatches through FunctionDispatcher.
    // `FunctionDispatcher` will be used in any thread.
    // `FunctionDispatcher` will be used to dispatch `MessageReceiver` functions
    // until `removeMessageReceiver()` for same receiver name, destination id returns.
    // The caller is responsible for making sure the `MessageReceiver` is alive when the dispatched functions
    // are run.
    void addMessageReceiver(FunctionDispatcher&, MessageReceiver&, ReceiverName, uint64_t destinationID = 0);
    void removeMessageReceiver(ReceiverName, uint64_t destinationID = 0);

    bool open(Client&, SerialFunctionDispatcher& = RunLoop::currentSingleton());
    // Ensures that messages sent prior to the call are not affected by invalidate() or crash done after the call returns.
    Error flushSentMessages(Timeout);
    void invalidate();

    template<typename PC, typename BasePromise>
    struct ConvertedPromise {
        template <typename T, typename E>
        struct Promise
        {
            using Type = NativePromise<T, E>;
        };

        template <typename T, typename E>
        struct Promise<Expected<T, E>, E>
        {
            using Type = NativePromise<T, E>;
        };

        using RejectValueType = std::remove_reference_t<decltype(PC::convertError(std::declval<IPC::Error>()).value())>;
        using Type = typename Promise<typename BasePromise::ResolveValueType, RejectValueType>::Type;
    };
    struct NoOpPromiseConverter {
        static auto convertError(IPC::Error error) { return makeUnexpected(error); }
    };

    template<typename T, typename C> std::optional<AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe, but the reply will be called on the Connection's dispatcher
    template<typename PC = NoOpPromiseConverter, typename T, typename Promise = typename ConvertedPromise<PC, typename T::Promise>::Type> Ref<Promise> sendWithPromisedReply(T&& message, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T, typename C> std::optional<AsyncReplyID> sendWithAsyncReplyOnDispatcher(T&& message, GuaranteedSerialFunctionDispatcher&, C&& completionHandler, uint64_t destinationID = 0, OptionSet<SendOption> = { }); // Thread-safe.
    template<typename T> Error send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.
    template<typename T> static Error send(UniqueID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt); // Thread-safe.

    // Sync senders should check the SendSyncResult for true/false in case they need to know if the result was really received.
    // Sync senders should hold on to the SendSyncResult in case they reference the contents of the reply via DataRefererence / ArrayReference.

    template<typename T> using SendSyncResult = ConnectionSendSyncResult<T>;
    template<typename T> SendSyncResult<T> sendSync(T&& message, uint64_t destinationID, Timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { }); // Main thread only.

    template<typename> Error waitForAndDispatchImmediately(uint64_t destinationID, Timeout, OptionSet<WaitForOption> waitForOptions = { }); // Main thread only.
    template<typename> Error waitForAsyncReplyAndDispatchImmediately(AsyncReplyID, Timeout); // Main thread only.

    // // Thread-safe, but the reply will be called on the Connection's dispatcher
    template<typename T, typename C, typename RawValue>
    std::optional<AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler, const ObjectIdentifierGenericBase<RawValue>& destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithAsyncReply<T, C>(std::forward<T>(message), std::forward<C>(completionHandler), destinationID.toUInt64(), sendOptions);
    }

    // Thread-safe.
    template<typename PC = NoOpPromiseConverter, typename T, typename Promise = typename ConvertedPromise<PC, typename T::Promise>::Type, typename RawValue>
    Ref<Promise> sendWithPromisedReply(T&& message, const ObjectIdentifierGenericBase<RawValue>& destinationID, OptionSet<SendOption> sendOptions = { })
    {
        return sendWithPromisedReply<PC, T, Promise>(WTFMove(message), destinationID.toUInt64(), sendOptions);
    }

    // Thread-safe.
    template<typename T, typename RawValue>
    Error send(T&& message, const ObjectIdentifierGenericBase<RawValue>& destinationID, OptionSet<SendOption> sendOptions = { }, std::optional<Thread::QOS> qos = std::nullopt)
    {
        return send<T>(std::forward<T>(message), destinationID.toUInt64(), sendOptions, qos);
    }

    // Main thread only.
    template<typename T, typename RawValue>
    SendSyncResult<T> sendSync(T&& message, const ObjectIdentifierGenericBase<RawValue>& destinationID, Timeout timeout = Timeout::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { })
    {
        return sendSync<T>(std::forward<T>(message), destinationID.toUInt64(), timeout, sendSyncOptions);
    }

    // Main thread only.
    template<typename T, typename RawValue>
    Error waitForAndDispatchImmediately(const ObjectIdentifierGenericBase<RawValue>& destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions = { })
    {
        return waitForAndDispatchImmediately<T>(destinationID.toUInt64(), timeout, waitForOptions);
    }

    Error sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);

    using AsyncReplyHandler = ConnectionAsyncReplyHandler;
    Error sendMessageWithAsyncReply(UniqueRef<Encoder>&&, AsyncReplyHandler, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);
    std::pair<UniqueRef<Encoder>, SyncRequestID> createSyncMessageEncoder(MessageName, uint64_t destinationID);
#ifndef __swift__ // rdar://152496447
    DecoderOrError sendSyncMessage(SyncRequestID, UniqueRef<Encoder>&&, Timeout, OptionSet<SendSyncOption> sendSyncOptions);
#endif
    Error sendSyncReply(UniqueRef<Encoder>&&);
    template<typename T, typename... Arguments>
    void sendAsyncReply(AsyncReplyID, Arguments&&...);

    void wakeUpRunLoop();

    bool inSendSync() const { return m_inSendSyncCount; }
    unsigned inDispatchSyncMessageCount() const { return m_inDispatchSyncMessageCount; }

#if PLATFORM(COCOA)
    Identifier identifier() const;
#endif

#if PLATFORM(COCOA) && !USE(EXTENSIONKIT_PROCESS_TERMINATION)
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
#ifndef __swift__ // rdar://152496447
    DecoderOrError waitForMessageForTesting(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);
#endif
#endif

    template<typename MessageReceiverType> void dispatchMessageReceiverMessage(MessageReceiverType&, UniqueRef<Decoder>&&);
    // Can be called from any thread.
    void dispatchDidReceiveInvalidMessage(MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding);
    void dispatchDidCloseAndInvalidate();

    size_t pendingMessageCountForTesting() const;
    void dispatchOnReceiveQueueForTesting(Function<void()>&&);

    template<typename T, typename C> static AsyncReplyHandler makeAsyncReplyHandler(C&& completionHandler, ThreadLikeAssertion callThread = CompletionHandlerCallThread::AnyThread);

    CompletionHandler<void(Connection*, Decoder*)> takeAsyncReplyHandler(AsyncReplyID);

    template<typename T, typename C> static void callReply(Connection*, Decoder&, C&&);
    template<typename T, typename C> static void cancelReply(C&&);

    void markCurrentlyDispatchedMessageAsInvalid(ASCIILiteral error);

#if ENABLE(CORE_IPC_SIGNPOSTS)
    static bool signpostsEnabled();
    static void forceEnableSignposts();
#endif

    static bool shouldCrashOnMessageCheckFailure();
    static void setShouldCrashOnMessageCheckFailure(bool);

#if ENABLE(IPC_TESTING_API)
    bool hasErrorString() const { return !m_errorString.isNull(); }
    void setErrorString(ASCIILiteral error)
    {
        if (!hasErrorString())
            m_errorString = error;
    }
    ASCIILiteral takeErrorString() { return std::exchange(m_errorString, { }); }
#endif

private:
    Connection(Identifier&&, bool isServer, Thread::QOS = Thread::QOS::Default);
    Connection();
    void platformInitialize(Identifier&&);
    bool platformPrepareForOpen();
    void platformOpen();
    void platformInvalidate();

    struct AsyncReplyHandlerWithDispatcher {
        CompletionHandler<void(Connection*, std::unique_ptr<Decoder>&&)> completionHandler;
        Markable<AsyncReplyID> replyID;
    };

    bool isAsyncReplyHandlerWithDispatcher(AsyncReplyID);
    CompletionHandler<void(Connection*, std::unique_ptr<Decoder>&&)> takeAsyncReplyHandlerWithDispatcher(AsyncReplyID);
    template<typename T, typename C> static AsyncReplyHandlerWithDispatcher makeAsyncReplyHandlerWithDispatcher(C&& completionHandler, GuaranteedSerialFunctionDispatcher&);
    template<typename T, typename PC, typename Promise> static AsyncReplyHandlerWithDispatcher makeAsyncReplyHandlerWithDispatcher(typename Promise::Producer&&);

    Error sendMessageWithAsyncReplyWithDispatcher(UniqueRef<Encoder>&&, AsyncReplyHandlerWithDispatcher&&, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);
    // Utility methods to avoid code duplication.
    template<typename T, typename C> static CompletionHandler<void(Connection*, Decoder*)> makeAsyncReplyCompletionHandler(C&& completionHandler, ThreadLikeAssertion);
    template<typename T> static CompletionHandler<void(Decoder*)> makeAsyncReplyCompletionHandler(typename T::Promise::Producer&&, ThreadLikeAssertion);

    bool isIncomingMessagesThrottlingEnabled() const { return m_incomingMessagesThrottlingLevel.has_value(); }

#ifndef __swift__ // rdar://152496447
    DecoderOrError waitForMessage(MessageName, uint64_t destinationID, Timeout, OptionSet<WaitForOption>);
#endif

    SyncRequestID makeSyncRequestID() { return SyncRequestID::generate(); }
    bool pushPendingSyncRequestID(SyncRequestID);
    void popPendingSyncRequestID(SyncRequestID);
#ifndef __swift__ // rdar://152496447
    DecoderOrError waitForSyncReply(SyncRequestID, MessageName, Timeout, OptionSet<SendSyncOption>);
#endif

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
    CompletionHandler<void(Connection*, std::unique_ptr<Decoder>&&)> takeAsyncReplyHandlerWithDispatcherWithLockHeld(AsyncReplyID);

    Timeout timeoutRespectingIgnoreTimeoutsForTesting(Timeout) const;

    Error sendMessageImpl(UniqueRef<Encoder>&&, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> = std::nullopt);

#if PLATFORM(COCOA)
    bool sendMessage(std::unique_ptr<MachMessage>);
#endif
    template<typename F>
    void dispatchToClient(F&& clientRunLoopTask) WTF_EXCLUDES_LOCK(m_incomingMessagesLock);

    template<typename F>
    void dispatchToClientWithIncomingMessagesLock(F&& clientRunLoopTask) WTF_REQUIRES_LOCK(m_incomingMessagesLock);

    size_t numberOfMessagesToProcess(size_t totalMessages);
    bool isThrottlingIncomingMessages() const { return *m_incomingMessagesThrottlingLevel > 0; }

    // Only valid between open() and invalidate().
    SerialFunctionDispatcher& dispatcher();

    class SyncMessageState;
    RefPtr<SyncMessageState> protectedSyncState() const;

    void addAsyncReplyHandler(AsyncReplyHandler&&);
    void addAsyncReplyHandlerWithDispatcher(AsyncReplyHandlerWithDispatcher&&);
    void cancelAsyncReplyHandlers();

    static constexpr size_t largeOutgoingMessageQueueCountThreshold { 128 };

    enum class MessageIdentifierType { };
    using MessageIdentifier = AtomicObjectIdentifier<MessageIdentifierType>;

    // Represents a sync request for which we're waiting on a reply.
    struct PendingSyncReply {
        PendingSyncReply();
        explicit PendingSyncReply(Connection::SyncRequestID);
        PendingSyncReply(PendingSyncReply&&);
        ~PendingSyncReply();

        // The request ID.
        Markable<Connection::SyncRequestID> syncRequestID;
        // The reply decoder, will be null if there was an error processing the sync
        // message on the other side.
        std::unique_ptr<Decoder> replyDecoder;
        // To make sure we maintain message ordering, we keep track of the last message (that returns true for shouldDispatchMessageWhenWaitingForSyncReply())
        // and that was received *before* the sync reply. This is to make sure that we dispatch messages up until this one, before dispatching the sync reply.
        std::optional<MessageIdentifier> identifierOfLastMessageToDispatchBeforeSyncReply;
    };

    CheckedPtr<Client> m_client;
    RefPtr<SyncMessageState> m_syncState;
    UniqueID m_uniqueID;
    bool m_isServer;
    std::atomic<bool> m_isValid { true };

    bool m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage { false };
    bool m_shouldExitOnSyncMessageSendFailure { false };
    DidCloseOnConnectionWorkQueueCallback m_didCloseOnConnectionWorkQueueCallback { nullptr };
    OutgoingMessageQueueIsGrowingLargeCallback m_outgoingMessageQueueIsGrowingLargeCallback;
    MonotonicTime m_lastOutgoingMessageQueueIsGrowingLargeCallbackCallTime WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);

    const Ref<WorkQueue> m_connectionQueue;
    bool m_isConnected { false };

    unsigned m_inSendSyncCount { 0 };
    unsigned m_inDispatchSyncMessageCount { 0 };
    unsigned m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount { 0 };
    unsigned m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting { 0 };
    bool m_fullySynchronousModeIsAllowedForTesting { false };
    bool m_ignoreTimeoutsForTesting { false };
    bool m_didReceiveInvalidMessage { false };
    std::optional<uint8_t> m_incomingMessagesThrottlingLevel;

#if ASSERT_ENABLED
    std::atomic<unsigned> m_inDispatchMessageCount { 0 };
#endif

    // Incoming messages.
#if ENABLE(UNFAIR_LOCK)
    mutable UnfairLock m_incomingMessagesLock;
#else
    mutable Lock m_incomingMessagesLock;
#endif
    Deque<UniqueRef<Decoder>> m_incomingMessages WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);
    MessageReceiveQueueMap m_receiveQueues WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);

    // Outgoing messages.
    Lock m_outgoingMessagesLock;
    Deque<UniqueRef<Encoder>> m_outgoingMessages WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);
    Condition m_outgoingMessagesEmptyCondition WTF_GUARDED_BY_LOCK(m_outgoingMessagesLock);

    Condition m_waitForMessageCondition;
    Lock m_waitForMessageLock;

    struct WaitForMessageState {
        WaitForMessageState(MessageName messageName, uint64_t destinationID, OptionSet<WaitForOption> waitForOptions)
            : messageName(messageName)
            , destinationID(destinationID)
            , waitForOptions(waitForOptions)
        {
        }

        MessageName messageName;
        uint64_t destinationID;
        OptionSet<WaitForOption> waitForOptions;
        bool messageWaitingInterrupted = false;
        std::unique_ptr<Decoder> decoder;
    };

    WaitForMessageState* m_waitingForMessage WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { nullptr }; // NOLINT

    Lock m_syncReplyStateLock;
    bool m_shouldWaitForSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock) { true };
    bool m_shouldWaitForMessages WTF_GUARDED_BY_LOCK(m_waitForMessageLock) { true };

    Vector<PendingSyncReply> m_pendingSyncReplies WTF_GUARDED_BY_LOCK(m_syncReplyStateLock);

    Lock m_incomingSyncMessageCallbackLock;
    HashMap<uint64_t, WTF::Function<void()>> m_incomingSyncMessageCallbacks WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    RefPtr<WorkQueue> m_incomingSyncMessageCallbackQueue WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock);
    uint64_t m_nextIncomingSyncMessageCallbackID WTF_GUARDED_BY_LOCK(m_incomingSyncMessageCallbackLock) { 0 };

    using AsyncReplyHandlerMap = HashMap<AsyncReplyID, CompletionHandler<void(Connection*, Decoder*)>>;
    AsyncReplyHandlerMap m_asyncReplyHandlers WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);
    using AsyncReplyHandlerWithDispatcherMap = HashMap<AsyncReplyID, CompletionHandler<void(Connection*, std::unique_ptr<Decoder>&&)>>;
    AsyncReplyHandlerWithDispatcherMap m_asyncReplyHandlerWithDispatchers WTF_GUARDED_BY_LOCK(m_incomingMessagesLock);

#if ENABLE(IPC_TESTING_API)
    Vector<WeakPtr<MessageObserver>> m_messageObservers;
    bool m_ignoreInvalidMessageForTesting { false };
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
    // Called on the connection queue.
    void readyReadHandler();
    bool sendOutputMessage(UnixMessage&&);

    Vector<uint8_t> m_readBuffer;

#if USE(GLIB)
    std::unique_ptr<Decoder> createMessageDecoder();

    GRefPtr<GSocket> m_socket;
    GRefPtr<GCancellable> m_cancellable;
    GSocketMonitor m_readSocketMonitor;
    GSocketMonitor m_writeSocketMonitor;
    Vector<UnixFileDescriptor> m_fileDescriptors;
    bool m_hasPendingOutputMessage { false };
#if OS(ANDROID)
    bool sendOutgoingHardwareBuffers();
    bool receiveIncomingHardwareBuffers();

    size_t m_pendingIncomingHardwareBufferCount { 0 };
    Vector<RefPtr<AHardwareBuffer>, 2> m_incomingHardwareBuffers;
    Vector<RefPtr<AHardwareBuffer>, 2> m_outgoingHardwareBuffers;
#endif
#else
    bool processMessage();
    int socketDescriptor() const;

    Vector<int> m_fileDescriptors;
    UnixFileDescriptor m_socketDescriptor;
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

    XPCObjectPtr<xpc_connection_t> m_xpcConnection;
    std::atomic<bool> m_didRequestProcessTermination { false };
    std::optional<audit_token_t> m_auditToken;
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

#if ENABLE(IPC_TESTING_API)
    ASCIILiteral m_errorString;
#endif

    friend class StreamClientConnection;
} SWIFT_SHARED_REFERENCE(refConnection, derefConnection);

template<typename T>
Error Connection::send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    static_assert(!T::isSync, "Async message expected");

    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    message.encode(encoder.get());

    return sendMessage(WTFMove(encoder), sendOptions, qos);
}

template<typename T>
Error Connection::send(UniqueID connectionID, T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    RefPtr connection = Connection::connection(connectionID);
    if (!connection)
        return Error::NoConnectionForIdentifier;
    return connection->send(std::forward<T>(message), destinationID, sendOptions, qos);
}

template<typename T, typename C>
std::optional<Connection::AsyncReplyID> Connection::sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");
    auto handler = makeAsyncReplyHandler<T>(std::forward<C>(completionHandler));
    auto replyID = handler.replyID;
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    message.encode(encoder.get());
    if (sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(handler), sendOptions) == Error::NoError)
        return replyID;
    // FIXME: Propagate the error back.
    return std::nullopt;
}

template<typename T, typename C>
std::optional<Connection::AsyncReplyID> Connection::sendWithAsyncReplyOnDispatcher(T&& message, GuaranteedSerialFunctionDispatcher& dispatcher, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");
    auto handler = makeAsyncReplyHandlerWithDispatcher<T>(std::forward<C>(completionHandler), dispatcher);
    auto replyID = handler.replyID;
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    message.encode(encoder.get());
    if (sendMessageWithAsyncReplyWithDispatcher(WTFMove(encoder), WTFMove(handler), sendOptions) == Error::NoError)
        return replyID;
    // FIXME: Propagate the error back.
    return std::nullopt;
}

template<typename PC, typename T, typename Promise>
Ref<Promise> Connection::sendWithPromisedReply(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions)
{
    static_assert(!T::isSync, "Async message expected");
    typename Promise::Producer producer;
    auto promise = producer.promise();
    auto handler = makeAsyncReplyHandlerWithDispatcher<PC, T, Promise>(WTFMove(producer));
    auto encoder = makeUniqueRef<Encoder>(T::name(), destinationID);
    message.encode(encoder.get());
    sendMessageWithAsyncReplyWithDispatcher(WTFMove(encoder), WTFMove(handler), sendOptions);
    // The promise will be rejected in the handler should an error occur.
    return promise;
}

template<typename T> Connection::SendSyncResult<T> Connection::sendSync(T&& message, uint64_t destinationID, Timeout timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    static_assert(T::isSync, "Sync message expected");
    auto [encoder, syncRequestID] = createSyncMessageEncoder(T::name(), destinationID);

    if (sendSyncOptions.contains(SendSyncOption::UseFullySynchronousModeForTesting)) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    message.encode(encoder.get());

    // Now send the message and wait for a reply.
    auto replyDecoderOrError = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, sendSyncOptions);
    if (!replyDecoderOrError.has_value()) {
        ASSERT(replyDecoderOrError.error() != Error::NoError);
        return { replyDecoderOrError.error() };
    }

    UniqueRef decoder = WTFMove(replyDecoderOrError.value());
    if (decoder->messageName() == MessageName::CancelSyncMessageReply)
        return { Error::SyncMessageCancelled };
    std::optional<typename T::ReplyArguments> replyArguments;
    decoder.get() >> replyArguments;
    if (!replyArguments)
        return { Error::FailedToDecodeReplyArguments };
    return SendSyncResult<T> { WTFMove(decoder), WTFMove(*replyArguments) };
}

template<typename T, typename... Arguments>
void Connection::sendAsyncReply(AsyncReplyID asyncReplyID, Arguments&&... arguments)
{
    auto encoder = makeUniqueRef<Encoder>(T::asyncMessageReplyName(), asyncReplyID.toUInt64());
    (encoder.get() << ... << std::forward<Arguments>(arguments));
    sendSyncReply(WTFMove(encoder));
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
    protectedClient()->didReceiveMessage(*this, decoderOrError.value());
    return Error::NoError;
}

template<typename T> Error Connection::waitForAsyncReplyAndDispatchImmediately(AsyncReplyID replyID, Timeout timeout)
{
    static_assert(T::replyCanDispatchOutOfOrder, "Can only use waitForAsyncReplyAndDispatchImmediately on messages declared with ReplyCanDispatchOutOfOrder");
    auto decoderOrError = waitForMessage(T::asyncMessageReplyName(), replyID.toUInt64(), timeout, { });
    if (!decoderOrError.has_value())
        return decoderOrError.error();

    ASSERT(decoderOrError.value()->isAsyncReplyMessage());
    ASSERT(decoderOrError.value()->destinationID() == replyID.toUInt64());
    ASSERT(!isAsyncReplyHandlerWithDispatcher(replyID), "Not supported with AsyncReplyHandlerWithDispatcher");
    auto handler = takeAsyncReplyHandler(AtomicObjectIdentifier<AsyncReplyIDType>(decoderOrError.value()->destinationID()));
    if (!handler) {
        ASSERT_NOT_REACHED();
        return Error::FailedToFindReplyHandler;
    }
    handler(this, &decoderOrError.value().get());
    return Error::NoError;
}

#if ENABLE(IPC_TESTING_API) && !defined(__swift__) // rdar://152496447
inline auto Connection::waitForMessageForTesting(MessageName messageName, uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> options) -> DecoderOrError
{
    return waitForMessage(messageName, destinationID, timeout, options);
}
#endif

template<typename T, typename C>
CompletionHandler<void(Connection*, Decoder*)> Connection::makeAsyncReplyCompletionHandler(C&& completionHandler, ThreadLikeAssertion callThread)
{
    return {
        [completionHandler = WTFMove(completionHandler)] (Connection* connection, Decoder* decoder) mutable {
            if (decoder && decoder->isValid()) {
                ASSERT(connection);
                callReply<T>(connection, *decoder, WTFMove(completionHandler));
            } else {
                ASSERT(!connection);
                cancelReply<T>(WTFMove(completionHandler));
            }
        }, callThread
    };
}

template<typename T, typename C>
Connection::AsyncReplyHandler Connection::makeAsyncReplyHandler(C&& completionHandler, ThreadLikeAssertion callThread)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=248947): callThread by default uses AnyThread because the
    // API contract on invalid sends does not make sense.
    return {
        makeAsyncReplyCompletionHandler<T, C>(std::forward<C>(completionHandler), callThread),
        AsyncReplyID::generate()
    };
}

template<typename T, typename C>
Connection::AsyncReplyHandlerWithDispatcher Connection::makeAsyncReplyHandlerWithDispatcher(C&& completionHandler, GuaranteedSerialFunctionDispatcher& dispatcher)
{
    // We use CompletionHandlerCallThread::AnyThread as it is up to the caller to determine the threading-model.
    // We can just guarantee that the CompletionHandler will be run on the dispatcher provided, we don't want to enforce
    // where it's been created.
    return {
        {
            [completionHandler = makeAsyncReplyCompletionHandler<T, C>(std::forward<C>(completionHandler), CompletionHandlerCallThread::AnyThread), dispatcher = Ref { dispatcher }](Connection* connection, std::unique_ptr<Decoder>&& decoder) mutable {
                dispatcher->dispatch([connection = RefPtr { connection }, completionHandler = WTFMove(completionHandler), decoder = WTFMove(decoder)]() mutable {
                    completionHandler(connection.get(), decoder.get());
                });
            }, CompletionHandlerCallThread::AnyThread
        },
        AsyncReplyID::generate()
    };
}

template<typename PC, typename T, typename Promise>
Connection::AsyncReplyHandlerWithDispatcher Connection::makeAsyncReplyHandlerWithDispatcher(typename Promise::Producer&& producer)
{
    return {
        {
            [producer = WTFMove(producer)](Connection*, std::unique_ptr<Decoder>&& decoder) mutable {
                producer.settleWithFunction([decoder = WTFMove(decoder)]() mutable -> typename Promise::Result {
                    if (!decoder)
                        return PC::convertError(Error::InvalidConnection);
                    if (!decoder->isValid())
                        return PC::convertError(Error::FailedToDecodeReplyArguments);
                    if constexpr (!std::tuple_size_v<typename T::ReplyArguments>)
                        return { };
                    else if (auto arguments = decoder->decode<typename T::ReplyArguments>()) {
                        if constexpr (std::tuple_size_v<typename T::ReplyArguments> == 1)
                            return std::get<0>(WTFMove(*arguments));
                        else
                            return WTFMove(*arguments);
                    }
                    ASSERT_NOT_REACHED();
                    return PC::convertError(Error::FailedToDecodeReplyArguments);
                });
            }, CompletionHandlerCallThread::AnyThread
        },
        AsyncReplyID::generate()
    };
}

template<typename, typename> struct CanApplyImpl;
template<typename C, typename... T> struct CanApplyImpl<C, std::tuple<T...>> {
    static constexpr bool value = std::is_invocable_v<C, T...>;
};
template<typename C, typename Tuple>
struct CanApply {
    static constexpr bool value = CanApplyImpl<C, Tuple>::value;
};

template<typename C, typename T>
void callWithConnectionAndArgsTuple(C&& completionHandler, Connection* connection, T&& tuple)
{
    return std::apply([&](auto&&... args) {
        // Use of object without protection is safe here since std::apply() runs synchronously.
        SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE completionHandler(connection, std::forward<decltype(args)>(args)...);
    }, std::forward<T>(tuple));
}

template<typename T, typename C>
void Connection::callReply(Connection* connection, Decoder& decoder, C&& completionHandler)
{
    if (auto arguments = decoder.decode<typename T::ReplyArguments>()) {
        if constexpr (CanApply<C, typename T::ReplyArguments>::value)
            return std::apply(std::forward<C>(completionHandler), WTFMove(*arguments));
        else
            return callWithConnectionAndArgsTuple(std::forward<C>(completionHandler), connection, WTFMove(*arguments));
    }
    cancelReply<T>(std::forward<C>(completionHandler));
}

template<typename T, typename C>
void Connection::cancelReply(C&& completionHandler)
{
    auto emptyReplyTuple = []<size_t... Indices>(std::index_sequence<Indices...>)
    {
        return std::make_tuple(AsyncReplyError<std::tuple_element_t<Indices, typename T::ReplyArguments>>::create()...);
    }(std::make_index_sequence<std::tuple_size_v<typename T::ReplyArguments>> { });

    if constexpr (CanApply<C, typename T::ReplyArguments>::value)
        std::apply(std::forward<C>(completionHandler), WTFMove(emptyReplyTuple));
    else
        callWithConnectionAndArgsTuple(std::forward<C>(completionHandler), nullptr, WTFMove(emptyReplyTuple));
}

inline void Connection::markCurrentlyDispatchedMessageAsInvalid(ASCIILiteral error)
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);
    m_didReceiveInvalidMessage = true;

#if ENABLE(IPC_TESTING_API)
    if (!error.isNull())
        setErrorString(error);
#endif
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

inline void markCurrentlyDispatchedMessageAsInvalid(Connection& connection, ASCIILiteral error)
{
    connection.markCurrentlyDispatchedMessageAsInvalid(error);
}

inline void markCurrentlyDispatchedMessageAsInvalid(const RefPtr<Connection>& connection, ASCIILiteral error)
{
    if (connection)
        connection->markCurrentlyDispatchedMessageAsInvalid(error);
}


} // namespace IPC

inline void refConnection(IPC::Connection* WTF_NONNULL obj)
{
    WTF::ref(obj);
}

inline void derefConnection(IPC::Connection* WTF_NONNULL obj)
{
    WTF::deref(obj);
}
