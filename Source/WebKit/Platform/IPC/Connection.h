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
#include "HandleMessage.h"
#include "MessageReceiver.h"
#include <WebCore/ScriptDisallowedScope.h>
#include <atomic>
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
#include "GSocketMonitor.h"
#endif

namespace IPC {

enum class SendOption {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
    IgnoreFullySynchronousMode = 1 << 1,
};

enum class SendSyncOption {
    // Use this to inform that this sync call will suspend this process until the user responds with input.
    InformPlatformProcessWillSuspend = 1 << 0,
    UseFullySynchronousModeForTesting = 1 << 1,
};

enum class WaitForOption {
    // Use this to make waitForMessage be interrupted immediately by any incoming sync messages.
    InterruptWaitingIfSyncMessageArrives = 1 << 0,
};

#define MESSAGE_CHECK_BASE(assertion, connection) do \
    if (!(assertion)) { \
        ASSERT(assertion); \
        (connection)->markCurrentlyDispatchedMessageAsInvalid(); \
        return; \
    } \
while (0)

class MachMessage;
class UnixMessage;

class Connection : public ThreadSafeRefCounted<Connection, WTF::DestructionThread::Main> {
public:
    class Client : public MessageReceiver {
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, StringReference messageReceiverName, StringReference messageName) = 0;

    protected:
        virtual ~Client() { }
    };

    class WorkQueueMessageReceiver : public MessageReceiver, public ThreadSafeRefCounted<WorkQueueMessageReceiver> {
    };

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
    bool getAuditToken(audit_token_t&);
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

    void addWorkQueueMessageReceiver(StringReference messageReceiverName, WorkQueue&, WorkQueueMessageReceiver*);
    void removeWorkQueueMessageReceiver(StringReference messageReceiverName);

    bool open();
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void postConnectionDidCloseOnConnectionWorkQueue();

    template<typename T, typename... Args> void sendWithAsyncReply(T&& message, CompletionHandler<void(Args...)>&& args, uint64_t destinationID = 0);
    template<typename T> bool send(T&& message, uint64_t destinationID, OptionSet<SendOption> sendOptions = { });
    template<typename T> void sendWithReply(T&& message, uint64_t destinationID, FunctionDispatcher& replyDispatcher, Function<void(Optional<typename CodingType<typename T::Reply>::Type>)>&& replyHandler);
    template<typename T> bool sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, Seconds timeout = Seconds::infinity(), OptionSet<SendSyncOption> sendSyncOptions = { });
    template<typename T> bool waitForAndDispatchImmediately(uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions = { });

    bool sendMessage(std::unique_ptr<Encoder>, OptionSet<SendOption> sendOptions);
    void sendMessageWithReply(uint64_t requestID, std::unique_ptr<Encoder>, FunctionDispatcher& replyDispatcher, Function<void(std::unique_ptr<Decoder>)>&& replyHandler);
    std::unique_ptr<Encoder> createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID);
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

#if HAVE(QOS_CLASSES)
    void setShouldBoostMainThreadOnSyncMessage(bool b) { m_shouldBoostMainThreadOnSyncMessage = b; }
#endif

    uint64_t installIncomingSyncMessageCallback(WTF::Function<void()>&&);
    void uninstallIncomingSyncMessageCallback(uint64_t);
    bool hasIncomingSyncMessage();

    void allowFullySynchronousModeForTesting() { m_fullySynchronousModeIsAllowedForTesting = true; }

    void ignoreTimeoutsForTesting() { m_ignoreTimeoutsForTesting = true; }

    void enableIncomingMessagesThrottling();

private:
    Connection(Identifier, bool isServer, Client&);
    void platformInitialize(Identifier);
    void platformInvalidate();
    
    std::unique_ptr<Decoder> waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption>);
    
    std::unique_ptr<Decoder> waitForSyncReply(uint64_t syncRequestID, Seconds timeout, OptionSet<SendSyncOption>);

    // Called on the connection work queue.
    void processIncomingMessage(std::unique_ptr<Decoder>);
    void processIncomingSyncReply(std::unique_ptr<Decoder>);

    void dispatchWorkQueueMessageReceiverMessage(WorkQueueMessageReceiver&, Decoder&);

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
    void dispatchDidReceiveInvalidMessage(const CString& messageReceiverNameString, const CString& messageNameString);
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

    HashMap<StringReference, std::pair<RefPtr<WorkQueue>, RefPtr<WorkQueueMessageReceiver>>> m_workQueueMessageReceivers;

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

    // Outgoing messages.
    Lock m_outgoingMessagesMutex;
    Deque<std::unique_ptr<Encoder>> m_outgoingMessages;
    
    Condition m_waitForMessageCondition;
    Lock m_waitForMessageMutex;

    struct ReplyHandler;

    Lock m_replyHandlersLock;
    HashMap<uint64_t, ReplyHandler> m_replyHandlers;

    struct WaitForMessageState;
    WaitForMessageState* m_waitingForMessage;

    class SyncMessageState;

    Lock m_syncReplyStateMutex;
    bool m_shouldWaitForSyncReplies;
    struct PendingSyncReply;
    Vector<PendingSyncReply> m_pendingSyncReplies;

    Lock m_incomingSyncMessageCallbackMutex;
    HashMap<uint64_t, WTF::Function<void()>> m_incomingSyncMessageCallbacks;
    RefPtr<WorkQueue> m_incomingSyncMessageCallbackQueue;
    uint64_t m_nextIncomingSyncMessageCallbackID { 0 };

#if HAVE(QOS_CLASSES)
    pthread_t m_mainThread { 0 };
    bool m_shouldBoostMainThreadOnSyncMessage { false };
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
        static void callback(void*, BOOLEAN);

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

    auto encoder = std::make_unique<Encoder>(T::receiverName(), T::name(), destinationID);
    encoder->encode(message.arguments());
    
    return sendMessage(WTFMove(encoder), sendOptions);
}

uint64_t nextAsyncReplyHandlerID();
void addAsyncReplyHandler(Connection&, uint64_t, CompletionHandler<void(Decoder*)>&&);
CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(Connection&, uint64_t);

template<typename T, typename... Args>
void Connection::sendWithAsyncReply(T&& message, CompletionHandler<void(Args...)>&& completionHandler, uint64_t destinationID)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = std::make_unique<Encoder>(T::receiverName(), T::name(), destinationID);
    uint64_t listenerID = nextAsyncReplyHandlerID();
    encoder->encode(listenerID);
    encoder->encode(message.arguments());
    sendMessage(WTFMove(encoder), { });
    addAsyncReplyHandler(*this, listenerID, [completionHandler = WTFMove(completionHandler)] (Decoder* decoder) mutable {
        if (decoder && !decoder->isInvalid())
            T::callReply(*decoder, WTFMove(completionHandler));
        else
            T::cancelReply(WTFMove(completionHandler));
    });
}

template<typename T>
void Connection::sendWithReply(T&& message, uint64_t destinationID, FunctionDispatcher& replyDispatcher, Function<void(Optional<typename CodingType<typename T::Reply>::Type>)>&& replyHandler)
{
    uint64_t requestID = 0;
    std::unique_ptr<Encoder> encoder = createSyncMessageEncoder(T::receiverName(), T::name(), destinationID, requestID);

    encoder->encode(message.arguments());

    sendMessageWithReply(requestID, WTFMove(encoder), replyDispatcher, [replyHandler = WTFMove(replyHandler)](std::unique_ptr<Decoder> decoder) {
        if (decoder) {
            typename CodingType<typename T::Reply>::Type reply;
            if (decoder->decode(reply)) {
                replyHandler(WTFMove(reply));
                return;
            }
        }

        replyHandler(WTF::nullopt);
    });
}

template<typename T> bool Connection::sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    COMPILE_ASSERT(T::isSync, SyncMessageExpected);

    uint64_t syncRequestID = 0;
    std::unique_ptr<Encoder> encoder = createSyncMessageEncoder(T::receiverName(), T::name(), destinationID, syncRequestID);

    if (sendSyncOptions.contains(SendSyncOption::UseFullySynchronousModeForTesting)) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    encoder->encode(message.arguments());

    // Now send the message and wait for a reply.
    std::unique_ptr<Decoder> replyDecoder = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, sendSyncOptions);
    if (!replyDecoder)
        return false;

    // Decode the reply.
    return replyDecoder->decode(reply);
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions)
{
    std::unique_ptr<Decoder> decoder = waitForMessage(T::receiverName(), T::name(), destinationID, timeout, waitForOptions);
    if (!decoder)
        return false;

    ASSERT(decoder->destinationID() == destinationID);
    m_client.didReceiveMessage(*this, *decoder);
    return true;
}

} // namespace IPC
