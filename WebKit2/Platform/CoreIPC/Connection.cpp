/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "Connection.h"

#include "CoreIPCMessageKinds.h"
#include "RunLoop.h"
#include "WorkItem.h"
#include <wtf/CurrentTime.h>

using namespace std;

namespace CoreIPC {

PassRefPtr<Connection> Connection::createServerConnection(Identifier identifier, Client* client, RunLoop* clientRunLoop)
{
    return adoptRef(new Connection(identifier, true, client, clientRunLoop));
}

PassRefPtr<Connection> Connection::createClientConnection(Identifier identifier, Client* client, RunLoop* clientRunLoop)
{
    return adoptRef(new Connection(identifier, false, client, clientRunLoop));
}

Connection::Connection(Identifier identifier, bool isServer, Client* client, RunLoop* clientRunLoop)
    : m_client(client)
    , m_isServer(isServer)
    , m_syncRequestID(0)
    , m_isConnected(false)
    , m_connectionQueue("com.apple.CoreIPC.ReceiveQueue")
    , m_clientRunLoop(clientRunLoop)
    , m_inDispatchMessageCount(0)
    , m_didReceiveInvalidMessage(false)
    , m_shouldWaitForSyncReplies(true)
{
    ASSERT(m_client);

    platformInitialize(identifier);
}

Connection::~Connection()
{
    ASSERT(!isValid());

    m_connectionQueue.invalidate();
}

void Connection::invalidate()
{
    if (!isValid()) {
        // Someone already called invalidate().
        return;
    }
    
    // Reset the client.
    m_client = 0;

    m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::platformInvalidate));
}

void Connection::markCurrentlyDispatchedMessageAsInvalid()
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);

    m_didReceiveInvalidMessage = true;
}

PassOwnPtr<ArgumentEncoder> Connection::createSyncMessageArgumentEncoder(uint64_t destinationID, uint64_t& syncRequestID)
{
    OwnPtr<ArgumentEncoder> argumentEncoder = ArgumentEncoder::create(destinationID);

    // Encode the sync request ID.
    syncRequestID = ++m_syncRequestID;
    argumentEncoder->encode(syncRequestID);

    return argumentEncoder.release();
}

bool Connection::sendMessage(MessageID messageID, PassOwnPtr<ArgumentEncoder> arguments)
{
    if (!isValid())
        return false;

    MutexLocker locker(m_outgoingMessagesLock);
    m_outgoingMessages.append(OutgoingMessage(messageID, arguments));
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::sendOutgoingMessages));
    return true;
}

bool Connection::sendSyncReply(PassOwnPtr<ArgumentEncoder> arguments)
{
    return sendMessage(MessageID(CoreIPCMessage::SyncMessageReply), arguments);
}

PassOwnPtr<ArgumentDecoder> Connection::waitForMessage(MessageID messageID, uint64_t destinationID, double timeout)
{
    // First, check if this message is already in the incoming messages queue.
    {
        MutexLocker locker(m_incomingMessagesLock);

        for (size_t i = 0; i < m_incomingMessages.size(); ++i) {
            const IncomingMessage& message = m_incomingMessages[i];

            if (message.messageID() == messageID && message.arguments()->destinationID() == destinationID) {
                OwnPtr<ArgumentDecoder> arguments(message.arguments());
                
                // Erase the incoming message.
                m_incomingMessages.remove(i);
                return arguments.release();
            }
        }
    }
    
    double absoluteTime = currentTime() + timeout;
    
    std::pair<unsigned, uint64_t> messageAndDestination(std::make_pair(messageID.toInt(), destinationID));
    
    {
        MutexLocker locker(m_waitForMessageMutex);

        // We don't support having multiple clients wait for the same message.
        ASSERT(!m_waitForMessageMap.contains(messageAndDestination));
    
        // Insert our pending wait.
        m_waitForMessageMap.set(messageAndDestination, 0);
    }
    
    // Now wait for it to be set.
    while (true) {
        MutexLocker locker(m_waitForMessageMutex);

        HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*>::iterator it = m_waitForMessageMap.find(messageAndDestination);
        if (it->second) {
            OwnPtr<ArgumentDecoder> arguments(it->second);
            m_waitForMessageMap.remove(it);
            
            return arguments.release();
        }
        
        // Now we wait.
        if (!m_waitForMessageCondition.timedWait(m_waitForMessageMutex, absoluteTime)) {
            // We timed out, now remove the pending wait.
            m_waitForMessageMap.remove(messageAndDestination);

            break;
        }
    }
    
    return PassOwnPtr<ArgumentDecoder>();
}

PassOwnPtr<ArgumentDecoder> Connection::sendSyncMessage(MessageID messageID, uint64_t syncRequestID, PassOwnPtr<ArgumentEncoder> encoder, double timeout)
{
    // We only allow sending sync messages from the client run loop.
    ASSERT(RunLoop::current() == m_clientRunLoop);

    if (!isValid())
        return 0;
    
    // Push the pending sync reply information on our stack.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies)
            return 0;

        m_pendingSyncReplies.append(PendingSyncReply(syncRequestID));
    }
    
    // First send the message.
    sendMessage(messageID, encoder);
    
    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    RefPtr<Connection> protect(this);
    OwnPtr<ArgumentDecoder> reply = waitForSyncReply(syncRequestID, timeout);

    // Finally, pop the pending sync reply information.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(m_pendingSyncReplies.last().syncRequestID == syncRequestID);
        m_pendingSyncReplies.removeLast();

        if (m_pendingSyncReplies.isEmpty()) {
            // This was the bottom-most sendSyncMessage call in the stack. If we have any pending incoming
            // sync messages, they need to be dispatched.
            if (!m_syncMessagesReceivedWhileWaitingForSyncReply.isEmpty()) {
                // Add the messages.
                MutexLocker locker(m_incomingMessagesLock);
                m_incomingMessages.append(m_syncMessagesReceivedWhileWaitingForSyncReply);
                m_syncMessagesReceivedWhileWaitingForSyncReply.clear();

                // Schedule for the messages to be sent.
                m_clientRunLoop->scheduleWork(WorkItem::create(this, &Connection::dispatchMessages));
            }
        }
    }
    
    return reply.release();
}

PassOwnPtr<ArgumentDecoder> Connection::waitForSyncReply(uint64_t syncRequestID, double timeout)
{
    double absoluteTime = currentTime() + timeout;

    bool timedOut = false;
    while (!timedOut) {
        {
            MutexLocker locker(m_syncReplyStateMutex);

            // First, check if we have any incoming sync messages that we need to process.
            Vector<IncomingMessage> syncMessagesReceivedWhileWaitingForSyncReply;
            m_syncMessagesReceivedWhileWaitingForSyncReply.swap(syncMessagesReceivedWhileWaitingForSyncReply);

            if (!syncMessagesReceivedWhileWaitingForSyncReply.isEmpty()) {
                // Make sure to unlock the mutex here because we're calling out to client code which could in turn send
                // another sync message and we don't want that to deadlock.
                m_syncReplyStateMutex.unlock();
                
                for (size_t i = 0; i < syncMessagesReceivedWhileWaitingForSyncReply.size(); ++i) {
                    IncomingMessage& message = syncMessagesReceivedWhileWaitingForSyncReply[i];
                    OwnPtr<ArgumentDecoder> arguments = message.releaseArguments();

                    dispatchSyncMessage(message.messageID(), arguments.get());
                }
                m_syncReplyStateMutex.lock();
            }

            // Second, check if there is a sync reply at the top of the stack.
            ASSERT(!m_pendingSyncReplies.isEmpty());
            
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies.last();
            ASSERT(pendingSyncReply.syncRequestID == syncRequestID);
            
            // We found the sync reply, or the connection was closed.
            if (pendingSyncReply.didReceiveReply || !m_shouldWaitForSyncReplies)
                return pendingSyncReply.releaseReplyDecoder();
        }

        // We didn't find a sync reply yet, keep waiting.
        timedOut = !m_waitForSyncReplySemaphore.wait(absoluteTime);
    }

    // We timed out.
    return 0;
}

void Connection::processIncomingMessage(MessageID messageID, PassOwnPtr<ArgumentDecoder> arguments)
{
    // Check if this is a sync reply.
    if (messageID == MessageID(CoreIPCMessage::SyncMessageReply)) {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(!m_pendingSyncReplies.isEmpty());

        PendingSyncReply& pendingSyncReply = m_pendingSyncReplies.last();
        ASSERT(pendingSyncReply.syncRequestID == arguments->destinationID());

        pendingSyncReply.replyDecoder = arguments.leakPtr();
        pendingSyncReply.didReceiveReply = true;

        m_waitForSyncReplySemaphore.signal();
        return;
    }

    // Check if this is a sync message. If it is, and we're waiting for a sync reply this message
    // needs to be dispatched. If we don't we'll end up with a deadlock where both sync message senders are
    // stuck waiting for a reply.
    if (messageID.isSync()) {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_pendingSyncReplies.isEmpty()) {
            m_syncMessagesReceivedWhileWaitingForSyncReply.append(IncomingMessage(messageID, arguments));

            // The message has been added, now wake up the client thread.
            m_waitForSyncReplySemaphore.signal();
            return;
        }
    }
        
    // Check if we're waiting for this message.
    {
        MutexLocker locker(m_waitForMessageMutex);
        
        HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*>::iterator it = m_waitForMessageMap.find(std::make_pair(messageID.toInt(), arguments->destinationID()));
        if (it != m_waitForMessageMap.end()) {
            it->second = arguments.leakPtr();
        
            m_waitForMessageCondition.signal();
            return;
        }
    }

    MutexLocker locker(m_incomingMessagesLock);
    m_incomingMessages.append(IncomingMessage(messageID, arguments));

    m_clientRunLoop->scheduleWork(WorkItem::create(this, &Connection::dispatchMessages));
}

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    platformInvalidate();

    {
        MutexLocker locker(m_syncReplyStateMutex);

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        if (!m_pendingSyncReplies.isEmpty())
            m_waitForSyncReplySemaphore.signal();
    }

    m_client->didCloseOnConnectionWorkQueue(&m_connectionQueue, this);

    m_clientRunLoop->scheduleWork(WorkItem::create(this, &Connection::dispatchConnectionDidClose));
}

void Connection::dispatchConnectionDidClose()
{
    // If the connection has been explicitly invalidated before dispatchConnectionDidClose was called,
    // then the client will be null here.
    if (!m_client)
        return;


    // Because we define a connection as being "valid" based on wheter it has a null client, we null out
    // the client before calling didClose here. Otherwise, sendSync will try to send a message to the connection and
    // will then wait indefinitely for a reply.
    Client* client = m_client;
    m_client = 0;
    
    client->didClose(this);
}

bool Connection::canSendOutgoingMessages() const
{
    return m_isConnected && platformCanSendOutgoingMessages();
}

void Connection::sendOutgoingMessages()
{
    if (!canSendOutgoingMessages())
        return;

    while (true) {
        OutgoingMessage message;
        {
            MutexLocker locker(m_outgoingMessagesLock);
            if (m_outgoingMessages.isEmpty())
                break;
            message = m_outgoingMessages.takeFirst();
        }

        if (!sendOutgoingMessage(message.messageID(), adoptPtr(message.arguments())))
            break;
    }
}

void Connection::dispatchSyncMessage(MessageID messageID, ArgumentDecoder* arguments)
{
    ASSERT(messageID.isSync());

    // Decode the sync request ID.
    uint64_t syncRequestID = 0;

    if (!arguments->decodeUInt64(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        arguments->markInvalid();
        return;
    }

    // Create our reply encoder.
    ArgumentEncoder* replyEncoder = ArgumentEncoder::create(syncRequestID).leakPtr();
    
    // Hand off both the decoder and encoder to the client..
    SyncReplyMode syncReplyMode = m_client->didReceiveSyncMessage(this, messageID, arguments, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!arguments->isInvalid());

    if (syncReplyMode == ManualReply) {
        // The client will take ownership of the reply encoder and send it at some point in the future.
        // We won't do anything here.
        return;
    }

    // Send the reply.
    sendSyncReply(replyEncoder);
}

void Connection::dispatchMessages()
{
    Vector<IncomingMessage> incomingMessages;
    
    {
        MutexLocker locker(m_incomingMessagesLock);
        m_incomingMessages.swap(incomingMessages);
    }

    // Dispatch messages.
    for (size_t i = 0; i < incomingMessages.size(); ++i) {
        // If someone calls invalidate while we're invalidating messages, we should stop.
        if (!m_client)
            return;
        
        IncomingMessage& message = incomingMessages[i];
        OwnPtr<ArgumentDecoder> arguments = message.releaseArguments();

        m_inDispatchMessageCount++;

        bool oldDidReceiveInvalidMessage = m_didReceiveInvalidMessage;
        m_didReceiveInvalidMessage = false;

        if (message.messageID().isSync())
            dispatchSyncMessage(message.messageID(), arguments.get());
        else
            m_client->didReceiveMessage(this, message.messageID(), arguments.get());

        m_didReceiveInvalidMessage |= arguments->isInvalid();
        m_inDispatchMessageCount--;

        if (m_didReceiveInvalidMessage)
            m_client->didReceiveInvalidMessage(this, message.messageID());

        m_didReceiveInvalidMessage = oldDidReceiveInvalidMessage;
    }
}

} // namespace CoreIPC
