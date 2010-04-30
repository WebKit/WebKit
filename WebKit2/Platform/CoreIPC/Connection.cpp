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

bool Connection::sendMessage(MessageID messageID, auto_ptr<ArgumentEncoder> arguments)
{
    if (!isValid())
        return false;

    MutexLocker locker(m_outgoingMessagesLock);
    m_outgoingMessages.append(OutgoingMessage(messageID, arguments));
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::sendOutgoingMessages));
    return true;
}

std::auto_ptr<ArgumentDecoder> Connection::waitForMessage(MessageID messageID, uint64_t destinationID, double timeout)
{
    // First, check if this message is already in the incoming messages queue.
    {
        MutexLocker locker(m_incomingMessagesLock);

        for (size_t i = 0; i < m_incomingMessages.size(); ++i) {
            const IncomingMessage& message = m_incomingMessages[i];

            if (equalIgnoringFlags(message.messageID(), messageID) && message.arguments()->destinationID() == destinationID) {
                std::auto_ptr<ArgumentDecoder> arguments(message.arguments());
                
                // Erase the incoming message.
                m_incomingMessages.remove(i);
                return arguments;
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
    
    bool timedOut = false;
    
    // Now wait for it to be set.
    while (!timedOut) {
        MutexLocker locker(m_waitForMessageMutex);

        HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*>::iterator it = m_waitForMessageMap.find(messageAndDestination);
        if (it->second) {
            std::auto_ptr<ArgumentDecoder> arguments(it->second);
            m_waitForMessageMap.remove(it);
            
            return arguments;
        }
        
        // We didn't find it, keep waiting.
        timedOut = !m_waitForMessageCondition.timedWait(m_waitForMessageMutex, absoluteTime);
    }

    // We timed out, now remove the pending wait.
    {
        MutexLocker locker(m_waitForMessageMutex);
        m_waitForMessageMap.remove(messageAndDestination);
    }
    
    return std::auto_ptr<ArgumentDecoder>();
}

std::auto_ptr<ArgumentDecoder> Connection::sendSyncMessage(MessageID messageID, uint64_t syncRequestID, std::auto_ptr<ArgumentEncoder> encoder, double timeout)
{
    // First send the message.
    if (!sendMessage(messageID, encoder))
        return std::auto_ptr<ArgumentDecoder>();

    // Now wait for a reply and return it.
    return waitForMessage(MessageID(CoreIPCMessage::SyncMessageReply), syncRequestID, timeout);
}

void Connection::processIncomingMessage(MessageID messageID, std::auto_ptr<ArgumentDecoder> arguments)
{
    // First, check if we're waiting for this message.
    {
        MutexLocker locker(m_waitForMessageMutex);
        
        HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*>::iterator it = m_waitForMessageMap.find(std::make_pair(messageID.toInt(), arguments->destinationID()));
        if (it != m_waitForMessageMap.end()) {
            it->second = arguments.release();
        
            m_waitForMessageCondition.signal();
            return;
        }
    }

    if (messageID == MessageID(CoreIPCMessage::SyncMessageReply)) {
        // FIXME: We got a reply for another sync message someone sent, handle this.
        ASSERT_NOT_REACHED();
    }

    MutexLocker locker(m_incomingMessagesLock);
    m_incomingMessages.append(IncomingMessage(messageID, arguments));

    m_clientRunLoop->scheduleWork(WorkItem::create(this, &Connection::dispatchMessages));
}

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    platformInvalidate();
    
    m_clientRunLoop->scheduleWork(WorkItem::create(this, &Connection::dispatchConnectionDidClose));
}

void Connection::dispatchConnectionDidClose()
{
    m_client->didClose(this);
    
    // Reset the client.
    m_client = 0;
}

void Connection::sendOutgoingMessages()
{
    if (!m_isConnected)
        return;

    Vector<OutgoingMessage> outgoingMessages;

    {
        MutexLocker locker(m_outgoingMessagesLock);
        m_outgoingMessages.swap(outgoingMessages);
    }

    // Send messages.
    for (size_t i = 0; i < outgoingMessages.size(); ++i)
        sendOutgoingMessage(outgoingMessages[i].messageID(), std::auto_ptr<ArgumentEncoder>(outgoingMessages[i].arguments()));
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
        IncomingMessage& message = incomingMessages[i];
        ArgumentDecoder* arguments = message.arguments();

        if (message.messageID().isSync()) {
            // Decode the sync request ID.
            uint64_t syncRequestID = 0;

            if (!arguments->decodeUInt64(syncRequestID)) {
                // FIXME: Handle this case.
                ASSERT_NOT_REACHED();
            }

            // Create our reply encoder.
            std::auto_ptr<ArgumentEncoder> replyEncoder(new ArgumentEncoder(syncRequestID));
            
            // Hand off both the decoder and encoder to the client..
            m_client->didReceiveSyncMessage(this, message.messageID(), arguments, replyEncoder.get());
            
            // Send the reply.
            sendMessage(MessageID(CoreIPCMessage::SyncMessageReply), replyEncoder);
        } else
            m_client->didReceiveMessage(this, message.messageID(), arguments);

        message.destroy();
    }
}

} // namespace CoreIPC
