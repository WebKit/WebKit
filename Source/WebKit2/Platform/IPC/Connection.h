/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef Connection_h
#define Connection_h

#include "Arguments.h"
#include "MessageDecoder.h"
#include "MessageEncoder.h"
#include "MessageReceiver.h"
#include "ProcessType.h"
#include <atomic>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/CString.h>

#if OS(DARWIN) && !USE(UNIX_DOMAIN_SOCKETS)
#include <mach/mach_port.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#if PLATFORM(GTK) || PLATFORM(EFL)
#include "PlatformProcessIdentifier.h"
#endif

#if PLATFORM(GTK)
#include "GSocketMonitor.h"
#endif

namespace IPC {

struct WaitForMessageState;

enum MessageSendFlags {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
};

enum SyncMessageSendFlags {
    // Use this to inform that this sync call will suspend this process until the user responds with input.
    InformPlatformProcessWillSuspend = 1 << 0,
    UseFullySynchronousModeForTesting = 1 << 1,
};

enum WaitForMessageFlags {
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

class Connection : public ThreadSafeRefCounted<Connection> {
public:
    class Client : public MessageReceiver {
    public:
        virtual void didClose(Connection&) = 0;
        virtual void didReceiveInvalidMessage(Connection&, StringReference messageReceiverName, StringReference messageName) = 0;
        virtual IPC::ProcessType localProcessType() = 0;
        virtual IPC::ProcessType remoteProcessType() = 0;

    protected:
        virtual ~Client() { }
    };

    class WorkQueueMessageReceiver : public MessageReceiver, public ThreadSafeRefCounted<WorkQueueMessageReceiver> {
    };

#if USE(UNIX_DOMAIN_SOCKETS)
    typedef int Identifier;
    static bool identifierIsNull(Identifier identifier) { return identifier == -1; }

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
            : port(MACH_PORT_NULL)
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

        mach_port_t port;
        OSObjectPtr<xpc_connection_t> xpcConnection;
    };
    static bool identifierIsNull(Identifier identifier) { return identifier.port == MACH_PORT_NULL; }
    xpc_connection_t xpcConnection() const { return m_xpcConnection.get(); }
    bool getAuditToken(audit_token_t&);
    pid_t remoteProcessID() const;
#endif

    static Ref<Connection> createServerConnection(Identifier, Client&);
    static Ref<Connection> createClientConnection(Identifier, Client&);
    ~Connection();

    Client* client() const { return m_client; }

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
    void setShouldCloseConnectionOnMachExceptions();
#endif

    void setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(bool);
    void setShouldExitOnSyncMessageSendFailure(bool shouldExitOnSyncMessageSendFailure);

    // The set callback will be called on the connection work queue when the connection is closed, 
    // before didCall is called on the client thread. Must be called before the connection is opened.
    // In the future we might want a more generic way to handle sync or async messages directly
    // on the work queue, for example if we want to handle them on some other thread we could avoid
    // handling the message on the client thread first.
    typedef void (*DidCloseOnConnectionWorkQueueCallback)(Connection*);
    void setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback callback);

    void addWorkQueueMessageReceiver(StringReference messageReceiverName, WorkQueue*, WorkQueueMessageReceiver*);
    void removeWorkQueueMessageReceiver(StringReference messageReceiverName);

    bool open();
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void postConnectionDidCloseOnConnectionWorkQueue();

    template<typename T> bool send(T&& message, uint64_t destinationID, unsigned messageSendFlags = 0);
    template<typename T> bool sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, std::chrono::milliseconds timeout = std::chrono::milliseconds::max(), unsigned syncSendFlags = 0);
    template<typename T> bool waitForAndDispatchImmediately(uint64_t destinationID, std::chrono::milliseconds timeout, unsigned waitForMessageFlags = 0);

    std::unique_ptr<MessageEncoder> createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID);
    bool sendMessage(std::unique_ptr<MessageEncoder>, unsigned messageSendFlags = 0, bool alreadyRecordedMessage = false);
    std::unique_ptr<MessageDecoder> sendSyncMessage(uint64_t syncRequestID, std::unique_ptr<MessageEncoder>, std::chrono::milliseconds timeout, unsigned syncSendFlags = 0);
    std::unique_ptr<MessageDecoder> sendSyncMessageFromSecondaryThread(uint64_t syncRequestID, std::unique_ptr<MessageEncoder>, std::chrono::milliseconds timeout);
    bool sendSyncReply(std::unique_ptr<MessageEncoder>);

    void wakeUpRunLoop();

    void incrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { ++m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }
    void decrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { --m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }

    bool inSendSync() const { return m_inSendSyncCount; }

    Identifier identifier() const;

#if PLATFORM(COCOA)
    bool kill();
    void terminateSoon(double intervalInSeconds);
#endif

    bool isValid() const { return m_client; }

#if HAVE(QOS_CLASSES)
    void setShouldBoostMainThreadOnSyncMessage(bool b) { m_shouldBoostMainThreadOnSyncMessage = b; }
#endif

    uint64_t installIncomingSyncMessageCallback(std::function<void ()>);
    void uninstallIncomingSyncMessageCallback(uint64_t);
    bool hasIncomingSyncMessage();

    void allowFullySynchronousModeForTesting() { m_fullySynchronousModeIsAllowedForTesting = true; }

private:
    Connection(Identifier, bool isServer, Client&);
    void platformInitialize(Identifier);
    void platformInvalidate();
    
    std::unique_ptr<MessageDecoder> waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, std::chrono::milliseconds timeout, unsigned waitForMessageFlags);
    
    std::unique_ptr<MessageDecoder> waitForSyncReply(uint64_t syncRequestID, std::chrono::milliseconds timeout, unsigned syncSendFlags);

    // Called on the connection work queue.
    void processIncomingMessage(std::unique_ptr<MessageDecoder>);
    void processIncomingSyncReply(std::unique_ptr<MessageDecoder>);

    void dispatchWorkQueueMessageReceiverMessage(WorkQueueMessageReceiver&, MessageDecoder&);

    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(std::unique_ptr<MessageEncoder>);
    void connectionDidClose();
    
    // Called on the listener thread.
    void dispatchOneMessage();
    void dispatchMessage(std::unique_ptr<MessageDecoder>);
    void dispatchMessage(MessageDecoder&);
    void dispatchSyncMessage(MessageDecoder&);
    void dispatchDidReceiveInvalidMessage(const CString& messageReceiverNameString, const CString& messageNameString);
    void didFailToSendSyncMessage();

    // Can be called on any thread.
    void enqueueIncomingMessage(std::unique_ptr<MessageDecoder>);

    void willSendSyncMessage(unsigned syncSendFlags);
    void didReceiveSyncReply(unsigned syncSendFlags);
    
    Client* m_client;
    bool m_isServer;
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
    bool m_didReceiveInvalidMessage;

    // Incoming messages.
    Lock m_incomingMessagesMutex;
    Deque<std::unique_ptr<MessageDecoder>> m_incomingMessages;

    // Outgoing messages.
    Lock m_outgoingMessagesMutex;
    Deque<std::unique_ptr<MessageEncoder>> m_outgoingMessages;
    
    Condition m_waitForMessageCondition;
    Lock m_waitForMessageMutex;

    WaitForMessageState* m_waitingForMessage;

    // Represents a sync request for which we're waiting on a reply.
    struct PendingSyncReply {
        // The request ID.
        uint64_t syncRequestID;

        // The reply decoder, will be null if there was an error processing the sync
        // message on the other side.
        std::unique_ptr<MessageDecoder> replyDecoder;

        // Will be set to true once a reply has been received.
        bool didReceiveReply;
    
        PendingSyncReply()
            : syncRequestID(0)
            , didReceiveReply(false)
        {
        }

        explicit PendingSyncReply(uint64_t syncRequestID)
            : syncRequestID(syncRequestID)
            , didReceiveReply(0)
        {
        }
    };

    class SyncMessageState;
    friend class SyncMessageState;

    Lock m_syncReplyStateMutex;
    bool m_shouldWaitForSyncReplies;
    Vector<PendingSyncReply> m_pendingSyncReplies;

    class SecondaryThreadPendingSyncReply;
    typedef HashMap<uint64_t, SecondaryThreadPendingSyncReply*> SecondaryThreadPendingSyncReplyMap;
    SecondaryThreadPendingSyncReplyMap m_secondaryThreadPendingSyncReplyMap;

    Lock m_incomingSyncMessageCallbackMutex;
    HashMap<uint64_t, std::function<void ()>> m_incomingSyncMessageCallbacks;
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

    Vector<uint8_t> m_readBuffer;
    size_t m_readBufferSize;
    Vector<int> m_fileDescriptors;
    size_t m_fileDescriptorsSize;
    int m_socketDescriptor;
#if PLATFORM(GTK)
    GSocketMonitor m_socketMonitor;
#endif
#elif OS(DARWIN)
    // Called on the connection queue.
    void receiveSourceEventHandler();
    void initializeDeadNameSource();

    mach_port_t m_sendPort;
    dispatch_source_t m_deadNameSource;

    mach_port_t m_receivePort;
    dispatch_source_t m_receivePortDataAvailableSource;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED <= 101000
    void exceptionSourceEventHandler();

    // If setShouldCloseConnectionOnMachExceptions has been called, this has
    // the exception port that exceptions from the other end will be sent on.
    mach_port_t m_exceptionPort;
    dispatch_source_t m_exceptionPortDataAvailableSource;
#endif

    OSObjectPtr<xpc_connection_t> m_xpcConnection;
#endif
};

template<typename T> bool Connection::send(T&& message, uint64_t destinationID, unsigned messageSendFlags)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = std::make_unique<MessageEncoder>(T::receiverName(), T::name(), destinationID);
    encoder->encode(message.arguments());
    
    return sendMessage(WTFMove(encoder), messageSendFlags);
}

template<typename T> bool Connection::sendSync(T&& message, typename T::Reply&& reply, uint64_t destinationID, std::chrono::milliseconds timeout, unsigned syncSendFlags)
{
    COMPILE_ASSERT(T::isSync, SyncMessageExpected);

    uint64_t syncRequestID = 0;
    std::unique_ptr<MessageEncoder> encoder = createSyncMessageEncoder(T::receiverName(), T::name(), destinationID, syncRequestID);

    if (syncSendFlags & SyncMessageSendFlags::UseFullySynchronousModeForTesting) {
        encoder->setFullySynchronousModeForTesting();
        m_fullySynchronousModeIsAllowedForTesting = true;
    }

    // Encode the rest of the input arguments.
    encoder->encode(message.arguments());

    // Now send the message and wait for a reply.
    std::unique_ptr<MessageDecoder> replyDecoder = sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, syncSendFlags);
    if (!replyDecoder)
        return false;

    // Decode the reply.
    return replyDecoder->decode(reply);
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, std::chrono::milliseconds timeout, unsigned waitForMessageFlags)
{
    std::unique_ptr<MessageDecoder> decoder = waitForMessage(T::receiverName(), T::name(), destinationID, timeout, waitForMessageFlags);
    if (!decoder)
        return false;

    ASSERT(decoder->destinationID() == destinationID);
    m_client->didReceiveMessage(*this, *decoder);
    return true;
}

} // namespace IPC

#endif // Connection_h
