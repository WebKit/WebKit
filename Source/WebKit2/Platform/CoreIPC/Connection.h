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
#include "WorkQueue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>

#if OS(DARWIN)
#include <mach/mach_port.h>
#if HAVE(XPC)
#include <xpc/xpc.h>
#endif
#elif PLATFORM(WIN)
#include <string>
#elif PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QSocketNotifier;
QT_END_NAMESPACE
#endif

#if PLATFORM(QT) || PLATFORM(GTK) || PLATFORM(EFL)
#include "PlatformProcessIdentifier.h"
#endif

namespace WebCore {
class RunLoop;
}

namespace CoreIPC {

class BinarySemaphore;
class MessageID;
    
enum MessageSendFlags {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
};

enum SyncMessageSendFlags {
    // Will allow events to continue being handled while waiting for the synch reply.
    SpinRunLoopWhileWaitingForReply = 1 << 0,
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
        virtual void didClose(Connection*) = 0;
        virtual void didReceiveInvalidMessage(Connection*, StringReference messageReceiverName, StringReference messageName) = 0;

#if PLATFORM(WIN)
        virtual Vector<HWND> windowsToReceiveSentMessagesWhileWaitingForSyncReply() = 0;
#endif

    protected:
        virtual ~Client() { }
    };

    class QueueClient {
    public:
        virtual void didReceiveMessageOnConnectionWorkQueue(Connection*, MessageID, MessageDecoder&, bool& didHandleMessage) = 0;

    protected:
        virtual ~QueueClient() { }
    };

#if OS(DARWIN)
    struct Identifier {
        Identifier()
            : port(MACH_PORT_NULL)
#if HAVE(XPC)
            , xpcConnection(0)
#endif
        {
        }

        Identifier(mach_port_t port)
            : port(port)
#if HAVE(XPC)
            , xpcConnection(0)
#endif
        {
        }

#if HAVE(XPC)
        Identifier(mach_port_t port, xpc_connection_t xpcConnection)
            : port(port)
            , xpcConnection(xpcConnection)
        {
        }
#endif

        mach_port_t port;
#if HAVE(XPC)
        xpc_connection_t xpcConnection;
#endif
    };
    static bool identifierIsNull(Identifier identifier) { return identifier.port == MACH_PORT_NULL; }
#elif OS(WINDOWS)
    typedef HANDLE Identifier;
    static bool createServerAndClientIdentifiers(Identifier& serverIdentifier, Identifier& clientIdentifier);
    static bool identifierIsNull(Identifier identifier) { return !identifier; }
#elif USE(UNIX_DOMAIN_SOCKETS)
    typedef int Identifier;
    static bool identifierIsNull(Identifier identifier) { return !identifier; }
#endif

    static PassRefPtr<Connection> createServerConnection(Identifier, Client*, WebCore::RunLoop* clientRunLoop);
    static PassRefPtr<Connection> createClientConnection(Identifier, Client*, WebCore::RunLoop* clientRunLoop);
    ~Connection();

    Client* client() const { return m_client; }

#if OS(DARWIN)
    void setShouldCloseConnectionOnMachExceptions();
#elif PLATFORM(QT)
    void setShouldCloseConnectionOnProcessTermination(WebKit::PlatformProcessIdentifier);
#endif

    void setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(bool);
    void setShouldExitOnSyncMessageSendFailure(bool shouldExitOnSyncMessageSendFailure);

    // The set callback will be called on the connection work queue when the connection is closed, 
    // before didCall is called on the client thread. Must be called before the connection is opened.
    // In the future we might want a more generic way to handle sync or async messages directly
    // on the work queue, for example if we want to handle them on some other thread we could avoid
    // handling the message on the client thread first.
    typedef void (*DidCloseOnConnectionWorkQueueCallback)(WorkQueue&, Connection*);
    void setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback callback);

    void addQueueClient(QueueClient*);
    void removeQueueClient(QueueClient*);

    bool open();
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void postConnectionDidCloseOnConnectionWorkQueue();

    static const int NoTimeout = -1;

    template<typename T> bool send(const T& message, uint64_t destinationID, unsigned messageSendFlags = 0);
    template<typename T> bool sendSync(const T& message, const typename T::Reply& reply, uint64_t destinationID, double timeout = NoTimeout, unsigned syncSendFlags = 0);
    template<typename T> bool waitForAndDispatchImmediately(uint64_t destinationID, double timeout);

    PassOwnPtr<MessageEncoder> createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID);
    bool sendMessage(MessageID, PassOwnPtr<MessageEncoder>, unsigned messageSendFlags = 0);
    PassOwnPtr<MessageDecoder> sendSyncMessage(MessageID, uint64_t syncRequestID, PassOwnPtr<MessageEncoder>, double timeout, unsigned syncSendFlags = 0);
    bool sendSyncReply(PassOwnPtr<MessageEncoder>);

    void wakeUpRunLoop();

    void incrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { ++m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }
    void decrementDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount() { --m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount; }

private:
    template<typename T> class Message {
    public:
        Message()
            : m_arguments(0)
        {
        }

        Message(MessageID messageID, PassOwnPtr<T> arguments)
            : m_messageID(messageID)
            , m_arguments(arguments.leakPtr())
        {
        }
        
        MessageID messageID() const { return m_messageID; }
        uint64_t destinationID() const { return m_arguments->destinationID(); }

        T* arguments() const { return m_arguments; }
        
        PassOwnPtr<T> releaseArguments()
        {
            OwnPtr<T> arguments = adoptPtr(m_arguments);
            m_arguments = 0;

            return arguments.release();
        }
        
    private:
        MessageID m_messageID;
        // The memory management of this class is very unusual. The class acts
        // as if it has an owning reference to m_arguments (e.g., accepting a
        // PassOwnPtr in its constructor) in all ways except that it does not
        // deallocate m_arguments on destruction.
        // FIXME: Does this leak m_arguments on destruction?
        T* m_arguments;
    };

public:
    typedef Message<MessageEncoder> OutgoingMessage;

private:
    Connection(Identifier, bool isServer, Client*, WebCore::RunLoop* clientRunLoop);
    void platformInitialize(Identifier);
    void platformInvalidate();
    
    bool isValid() const { return m_client; }
    
    PassOwnPtr<MessageDecoder> waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, double timeout);
    
    PassOwnPtr<MessageDecoder> waitForSyncReply(uint64_t syncRequestID, double timeout, unsigned syncSendFlags);

    // Called on the connection work queue.
    void processIncomingMessage(MessageID, PassOwnPtr<MessageDecoder>);
    void processIncomingSyncReply(PassOwnPtr<MessageDecoder>);

    void addQueueClientOnWorkQueue(QueueClient*);
    void removeQueueClientOnWorkQueue(QueueClient*);
    
    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(MessageID, PassOwnPtr<MessageEncoder>);
    void connectionDidClose();
    
    typedef Message<MessageDecoder> IncomingMessage;

    // Called on the listener thread.
    void dispatchConnectionDidClose();
    void dispatchOneMessage();
    void dispatchMessage(IncomingMessage&);
    void dispatchMessage(MessageID, MessageDecoder&);
    void dispatchSyncMessage(MessageID, MessageDecoder&);
    void didFailToSendSyncMessage();

    // Can be called on any thread.
    void enqueueIncomingMessage(IncomingMessage&);

    Client* m_client;
    bool m_isServer;
    uint64_t m_syncRequestID;

    bool m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage;
    bool m_shouldExitOnSyncMessageSendFailure;
    DidCloseOnConnectionWorkQueueCallback m_didCloseOnConnectionWorkQueueCallback;

    bool m_isConnected;
    WorkQueue m_connectionQueue;
    WebCore::RunLoop* m_clientRunLoop;

    Vector<QueueClient*> m_connectionQueueClients;

    unsigned m_inDispatchMessageCount;
    unsigned m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount;
    bool m_didReceiveInvalidMessage;

    // Incoming messages.
    Mutex m_incomingMessagesLock;
    Deque<IncomingMessage> m_incomingMessages;

    // Outgoing messages.
    Mutex m_outgoingMessagesLock;
    Deque<OutgoingMessage> m_outgoingMessages;
    
    ThreadCondition m_waitForMessageCondition;
    Mutex m_waitForMessageMutex;
    HashMap<std::pair<std::pair<StringReference, StringReference>, uint64_t>, OwnPtr<MessageDecoder> > m_waitForMessageMap;

    // Represents a sync request for which we're waiting on a reply.
    struct PendingSyncReply {
        // The request ID.
        uint64_t syncRequestID;

        // The reply decoder, will be null if there was an error processing the sync
        // message on the other side.
        MessageDecoder* replyDecoder;

        // Will be set to true once a reply has been received or an error occurred.
        bool didReceiveReply;
    
        PendingSyncReply()
            : syncRequestID(0)
            , replyDecoder(0)
            , didReceiveReply(false)
        {
        }

        explicit PendingSyncReply(uint64_t syncRequestID)
            : syncRequestID(syncRequestID)
            , replyDecoder(0)
            , didReceiveReply(0)
        {
        }

        PassOwnPtr<MessageDecoder> releaseReplyDecoder()
        {
            OwnPtr<MessageDecoder> reply = adoptPtr(replyDecoder);
            replyDecoder = 0;
            
            return reply.release();
        }
    };
    
    class SyncMessageState;
    friend class SyncMessageState;
    RefPtr<SyncMessageState> m_syncMessageState;

    Mutex m_syncReplyStateMutex;
    bool m_shouldWaitForSyncReplies;
    Vector<PendingSyncReply> m_pendingSyncReplies;

#if OS(DARWIN)
    // Called on the connection queue.
    void receiveSourceEventHandler();
    void initializeDeadNameSource();
    void exceptionSourceEventHandler();

    mach_port_t m_sendPort;
    mach_port_t m_receivePort;

    // If setShouldCloseConnectionOnMachExceptions has been called, this has
    // the exception port that exceptions from the other end will be sent on.
    mach_port_t m_exceptionPort;

#if HAVE(XPC)
    xpc_connection_t m_xpcConnection;
#endif

#elif OS(WINDOWS)
    // Called on the connection queue.
    void readEventHandler();
    void writeEventHandler();

    // Called by Connection::SyncMessageState::waitWhileDispatchingSentWin32Messages.
    // The absoluteTime is in seconds, starting on January 1, 1970. The time is assumed to use the
    // same time zone as WTF::currentTime(). Dispatches sent (not posted) messages to the passed-in
    // set of HWNDs until the semaphore is signaled or absoluteTime is reached. Returns true if the
    // semaphore is signaled, false otherwise.
    static bool dispatchSentMessagesUntil(const Vector<HWND>& windows, CoreIPC::BinarySemaphore& semaphore, double absoluteTime);

    Vector<uint8_t> m_readBuffer;
    OVERLAPPED m_readState;
    OwnPtr<MessageEncoder> m_pendingWriteEncoder;
    OVERLAPPED m_writeState;
    HANDLE m_connectionPipe;
#elif USE(UNIX_DOMAIN_SOCKETS)
    // Called on the connection queue.
    void readyReadHandler();
    bool processMessage();

    Vector<uint8_t> m_readBuffer;
    size_t m_readBufferSize;
    Vector<int> m_fileDescriptors;
    size_t m_fileDescriptorsSize;
    int m_socketDescriptor;
#if PLATFORM(QT)
    QSocketNotifier* m_socketNotifier;
#endif
#endif
};

template<typename T> bool Connection::send(const T& message, uint64_t destinationID, unsigned messageSendFlags)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    OwnPtr<MessageEncoder> encoder = MessageEncoder::create(T::receiverName(), T::name(), destinationID);
    encoder->encode(message);
    
    return sendMessage(MessageID(T::messageID), encoder.release(), messageSendFlags);
}

template<typename T> bool Connection::sendSync(const T& message, const typename T::Reply& reply, uint64_t destinationID, double timeout, unsigned syncSendFlags)
{
    COMPILE_ASSERT(T::isSync, SyncMessageExpected);

    uint64_t syncRequestID = 0;
    OwnPtr<MessageEncoder> encoder = createSyncMessageEncoder(T::receiverName(), T::name(), destinationID, syncRequestID);
    
    // Encode the rest of the input arguments.
    encoder->encode(message);

    // Now send the message and wait for a reply.
    OwnPtr<MessageDecoder> replyDecoder = sendSyncMessage(MessageID(T::messageID), syncRequestID, encoder.release(), timeout, syncSendFlags);
    if (!replyDecoder)
        return false;

    // Decode the reply.
    return replyDecoder->decode(const_cast<typename T::Reply&>(reply));
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, double timeout)
{
    OwnPtr<MessageDecoder> decoder = waitForMessage(T::receiverName(), T::name(), destinationID, timeout);
    if (!decoder)
        return false;

    ASSERT(decoder->destinationID() == destinationID);
    m_client->didReceiveMessage(this, MessageID(T::messageID), *decoder);
    return true;
}

} // namespace CoreIPC

#endif // Connection_h
