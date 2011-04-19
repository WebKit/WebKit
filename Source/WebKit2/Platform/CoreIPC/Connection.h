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

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "Arguments.h"
#include "MessageID.h"
#include "WorkQueue.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>

#if PLATFORM(MAC)
#include <mach/mach_port.h>
#elif PLATFORM(WIN)
#include <string>
#elif PLATFORM(QT)
class QSocketNotifier;
#endif

#if PLATFORM(QT) || PLATFORM(GTK)
#include "PlatformProcessIdentifier.h"
#endif

class RunLoop;

namespace CoreIPC {

class MessageID;
    
enum SyncReplyMode {
    AutomaticReply,
    ManualReply
};

enum MessageSendFlags {
    // Whether this message should be dispatched when waiting for a sync reply.
    // This is the default for synchronous messages.
    DispatchMessageEvenWhenWaitingForSyncReply = 1 << 0,
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
    class MessageReceiver {
    protected:
        virtual ~MessageReceiver() { }

    public:
        virtual void didReceiveMessage(Connection*, MessageID, ArgumentDecoder*) = 0;
        virtual SyncReplyMode didReceiveSyncMessage(Connection*, MessageID, ArgumentDecoder*, ArgumentEncoder*) { ASSERT_NOT_REACHED(); return AutomaticReply; }
    };
    
    class Client : public MessageReceiver {
    protected:
        virtual ~Client() { }

    public:
        virtual void didClose(Connection*) = 0;
        virtual void didReceiveInvalidMessage(Connection*, MessageID) = 0;
        virtual void syncMessageSendTimedOut(Connection*) = 0;

#if PLATFORM(WIN)
        virtual Vector<HWND> windowsToReceiveSentMessagesWhileWaitingForSyncReply() = 0;
#endif
    };

#if PLATFORM(MAC)
    typedef mach_port_t Identifier;
#elif PLATFORM(WIN)
    typedef HANDLE Identifier;
    static bool createServerAndClientIdentifiers(Identifier& serverIdentifier, Identifier& clientIdentifier);
#elif USE(UNIX_DOMAIN_SOCKETS)
    typedef int Identifier;
#endif

    static PassRefPtr<Connection> createServerConnection(Identifier, Client*, RunLoop* clientRunLoop);
    static PassRefPtr<Connection> createClientConnection(Identifier, Client*, RunLoop* clientRunLoop);
    ~Connection();

#if PLATFORM(MAC)
    void setShouldCloseConnectionOnMachExceptions();
#elif PLATFORM(QT) || PLATFORM(GTK)
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
                                                
    bool open();
    void invalidate();
    void markCurrentlyDispatchedMessageAsInvalid();

    void setDefaultSyncMessageTimeout(double);

    static const int DefaultTimeout = 0;
    static const int NoTimeout = -1;

    template<typename T> bool send(const T& message, uint64_t destinationID, unsigned messageSendFlags = 0);
    template<typename T> bool sendSync(const T& message, const typename T::Reply& reply, uint64_t destinationID, double timeout = DefaultTimeout);
    template<typename T> bool waitForAndDispatchImmediately(uint64_t destinationID, double timeout);

    PassOwnPtr<ArgumentEncoder> createSyncMessageArgumentEncoder(uint64_t destinationID, uint64_t& syncRequestID);
    bool sendMessage(MessageID, PassOwnPtr<ArgumentEncoder>, unsigned messageSendFlags = 0);
    bool sendSyncReply(PassOwnPtr<ArgumentEncoder>);

    // FIXME: These variants of send, sendSync and waitFor are all deprecated.
    // All clients should move to the overloads that take a message type.
    template<typename E, typename T> bool deprecatedSend(E messageID, uint64_t destinationID, const T& arguments);
    template<typename E, typename T, typename U> bool deprecatedSendSync(E messageID, uint64_t destinationID, const T& arguments, const U& reply, double timeout = NoTimeout);
    template<typename E> PassOwnPtr<ArgumentDecoder> deprecatedWaitFor(E messageID, uint64_t destinationID, double timeout);
    
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
            T* arguments = m_arguments;
            m_arguments = 0;

            return arguments;
        }
        
    private:
        MessageID m_messageID;
        T* m_arguments;
    };

public:
    typedef Message<ArgumentEncoder> OutgoingMessage;

private:
    Connection(Identifier, bool isServer, Client*, RunLoop* clientRunLoop);
    void platformInitialize(Identifier);
    void platformInvalidate();
    
    bool isValid() const { return m_client; }
    
    PassOwnPtr<ArgumentDecoder> waitForMessage(MessageID, uint64_t destinationID, double timeout);
    
    PassOwnPtr<ArgumentDecoder> sendSyncMessage(MessageID, uint64_t syncRequestID, PassOwnPtr<ArgumentEncoder>, double timeout);
    PassOwnPtr<ArgumentDecoder> waitForSyncReply(uint64_t syncRequestID, double timeout);

    // Called on the connection work queue.
    void processIncomingMessage(MessageID, PassOwnPtr<ArgumentDecoder>);
    void processIncomingSyncReply(PassOwnPtr<ArgumentDecoder>);

    bool canSendOutgoingMessages() const;
    bool platformCanSendOutgoingMessages() const;
    void sendOutgoingMessages();
    bool sendOutgoingMessage(MessageID, PassOwnPtr<ArgumentEncoder>);
    void connectionDidClose();
    
    typedef Message<ArgumentDecoder> IncomingMessage;

    // Called on the listener thread.
    void dispatchConnectionDidClose();
    void dispatchMessage(IncomingMessage&);
    void dispatchMessages();
    void dispatchSyncMessage(MessageID, ArgumentDecoder*);
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
    RunLoop* m_clientRunLoop;

    unsigned m_inDispatchMessageCount;
    unsigned m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount;
    bool m_didReceiveInvalidMessage;

    double m_defaultSyncMessageTimeout;

    // Incoming messages.
    Mutex m_incomingMessagesLock;
    Vector<IncomingMessage> m_incomingMessages;

    // Outgoing messages.
    Mutex m_outgoingMessagesLock;
    Deque<OutgoingMessage> m_outgoingMessages;
    
    ThreadCondition m_waitForMessageCondition;
    Mutex m_waitForMessageMutex;
    HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*> m_waitForMessageMap;

    // Represents a sync request for which we're waiting on a reply.
    struct PendingSyncReply {
        // The request ID.
        uint64_t syncRequestID;

        // The reply decoder, will be null if there was an error processing the sync
        // message on the other side.
        ArgumentDecoder* replyDecoder;

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

        PassOwnPtr<ArgumentDecoder> releaseReplyDecoder()
        {
            OwnPtr<ArgumentDecoder> reply = adoptPtr(replyDecoder);
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

#if PLATFORM(MAC)
    // Called on the connection queue.
    void receiveSourceEventHandler();
    void initializeDeadNameSource();
    void exceptionSourceEventHandler();

    mach_port_t m_sendPort;
    mach_port_t m_receivePort;

    // If setShouldCloseConnectionOnMachExceptions has been called, this has
    // the exception port that exceptions from the other end will be sent on.
    mach_port_t m_exceptionPort;

#elif PLATFORM(WIN)
    // Called on the connection queue.
    void readEventHandler();
    void writeEventHandler();

    Vector<uint8_t> m_readBuffer;
    OVERLAPPED m_readState;
    OwnPtr<ArgumentEncoder> m_pendingWriteArguments;
    OVERLAPPED m_writeState;
    HANDLE m_connectionPipe;
#elif USE(UNIX_DOMAIN_SOCKETS)
    // Called on the connection queue.
    void readyReadHandler();

    Vector<uint8_t> m_readBuffer;
    size_t m_currentMessageSize;
    int m_socketDescriptor;

#if PLATFORM(QT)
    QSocketNotifier* m_socketNotifier;
#endif
#endif
};

template<typename T> bool Connection::send(const T& message, uint64_t destinationID, unsigned messageSendFlags)
{
    OwnPtr<ArgumentEncoder> argumentEncoder = ArgumentEncoder::create(destinationID);
    argumentEncoder->encode(message);
    
    return sendMessage(MessageID(T::messageID), argumentEncoder.release(), messageSendFlags);
}

template<typename T> bool Connection::sendSync(const T& message, const typename T::Reply& reply, uint64_t destinationID, double timeout)
{
    uint64_t syncRequestID = 0;
    OwnPtr<ArgumentEncoder> argumentEncoder = createSyncMessageArgumentEncoder(destinationID, syncRequestID);
    
    // Encode the rest of the input arguments.
    argumentEncoder->encode(message);

    // Now send the message and wait for a reply.
    OwnPtr<ArgumentDecoder> replyDecoder = sendSyncMessage(MessageID(T::messageID), syncRequestID, argumentEncoder.release(), timeout);
    if (!replyDecoder)
        return false;

    // Decode the reply.
    return replyDecoder->decode(const_cast<typename T::Reply&>(reply));
}

template<typename T> bool Connection::waitForAndDispatchImmediately(uint64_t destinationID, double timeout)
{
    OwnPtr<ArgumentDecoder> decoder = waitForMessage(MessageID(T::messageID), destinationID, timeout);
    if (!decoder)
        return false;

    ASSERT(decoder->destinationID() == destinationID);
    m_client->didReceiveMessage(this, MessageID(T::messageID), decoder.get());
    return true;
}

// These three member functions are all deprecated.

template<typename E, typename T, typename U>
inline bool Connection::deprecatedSendSync(E messageID, uint64_t destinationID, const T& arguments, const U& reply, double timeout)
{
    uint64_t syncRequestID = 0;
    OwnPtr<ArgumentEncoder> argumentEncoder = createSyncMessageArgumentEncoder(destinationID, syncRequestID);

    // Encode the input arguments.
    argumentEncoder->encode(arguments);
    
    // Now send the message and wait for a reply.
    OwnPtr<ArgumentDecoder> replyDecoder = sendSyncMessage(MessageID(messageID), syncRequestID, argumentEncoder.release(), timeout);
    if (!replyDecoder)
        return false;
    
    // Decode the reply.
    return replyDecoder->decode(const_cast<U&>(reply));
}

template<typename E, typename T>
bool Connection::deprecatedSend(E messageID, uint64_t destinationID, const T& arguments)
{
    OwnPtr<ArgumentEncoder> argumentEncoder = ArgumentEncoder::create(destinationID);
    argumentEncoder->encode(arguments);

    return sendMessage(MessageID(messageID), argumentEncoder.release());
}

template<typename E> inline PassOwnPtr<ArgumentDecoder> Connection::deprecatedWaitFor(E messageID, uint64_t destinationID, double timeout)
{
    return waitForMessage(MessageID(messageID), destinationID, timeout);
}

} // namespace CoreIPC

#endif // Connection_h
