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

#include "config.h"
#include "Connection.h"

#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>
#include <wtf/threads/BinarySemaphore.h>

namespace IPC {

class Connection::SyncMessageState : public ThreadSafeRefCounted<Connection::SyncMessageState> {
public:
    static PassRefPtr<SyncMessageState> getOrCreate(RunLoop*);
    ~SyncMessageState();

    void wakeUpClientRunLoop()
    {
        m_waitForSyncReplySemaphore.signal();
    }

    bool wait(double absoluteTime)
    {
        return m_waitForSyncReplySemaphore.wait(absoluteTime);
    }

    // Returns true if this message will be handled on a client thread that is currently
    // waiting for a reply to a synchronous message.
    bool processIncomingMessage(Connection*, std::unique_ptr<MessageDecoder>&);

    // Dispatch pending sync messages. if allowedConnection is not null, will only dispatch messages
    // from that connection and put the other messages back in the queue.
    void dispatchMessages(Connection* allowedConnection);

private:
    explicit SyncMessageState(RunLoop*);

    typedef HashMap<RunLoop*, SyncMessageState*> SyncMessageStateMap;
    static SyncMessageStateMap& syncMessageStateMap()
    {
        static NeverDestroyed<SyncMessageStateMap> syncMessageStateMap;
        return syncMessageStateMap;
    }

    static Mutex& syncMessageStateMapMutex()
    {
        static NeverDestroyed<Mutex> syncMessageStateMapMutex;
        return syncMessageStateMapMutex;
    }

    void dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection*);

    RunLoop* m_runLoop;
    BinarySemaphore m_waitForSyncReplySemaphore;

    // Protects m_didScheduleDispatchMessagesWorkSet and m_messagesToDispatchWhileWaitingForSyncReply.
    Mutex m_mutex;

    // The set of connections for which we've scheduled a call to dispatchMessageAndResetDidScheduleDispatchMessagesForConnection.
    HashSet<RefPtr<Connection>> m_didScheduleDispatchMessagesWorkSet;

    struct ConnectionAndIncomingMessage {
        RefPtr<Connection> connection;
        std::unique_ptr<MessageDecoder> message;
    };
    Vector<ConnectionAndIncomingMessage> m_messagesToDispatchWhileWaitingForSyncReply;
};

class Connection::SecondaryThreadPendingSyncReply {
public:
    // The reply decoder, will be null if there was an error processing the sync message on the other side.
    std::unique_ptr<MessageDecoder> replyDecoder;

    BinarySemaphore semaphore;
};


PassRefPtr<Connection::SyncMessageState> Connection::SyncMessageState::getOrCreate(RunLoop* runLoop)
{
    MutexLocker locker(syncMessageStateMapMutex());
    SyncMessageStateMap::AddResult result = syncMessageStateMap().add(runLoop, nullptr);

    if (!result.isNewEntry) {
        ASSERT(result.iterator->value);
        return result.iterator->value;
    }

    RefPtr<SyncMessageState> syncMessageState = adoptRef(new SyncMessageState(runLoop));
    result.iterator->value = syncMessageState.get();

    return syncMessageState.release();
}

Connection::SyncMessageState::SyncMessageState(RunLoop* runLoop)
    : m_runLoop(runLoop)
{
}

Connection::SyncMessageState::~SyncMessageState()
{
    MutexLocker locker(syncMessageStateMapMutex());
    
    ASSERT(syncMessageStateMap().contains(m_runLoop));
    syncMessageStateMap().remove(m_runLoop);

    ASSERT(m_messagesToDispatchWhileWaitingForSyncReply.isEmpty());
}

bool Connection::SyncMessageState::processIncomingMessage(Connection* connection, std::unique_ptr<MessageDecoder>& message)
{
    if (!message->shouldDispatchMessageWhenWaitingForSyncReply())
        return false;

    ConnectionAndIncomingMessage connectionAndIncomingMessage;
    connectionAndIncomingMessage.connection = connection;
    connectionAndIncomingMessage.message = std::move(message);

    {
        MutexLocker locker(m_mutex);
        
        if (m_didScheduleDispatchMessagesWorkSet.add(connection).isNewEntry)
            m_runLoop->dispatch(bind(&SyncMessageState::dispatchMessageAndResetDidScheduleDispatchMessagesForConnection, this, RefPtr<Connection>(connection)));

        m_messagesToDispatchWhileWaitingForSyncReply.append(std::move(connectionAndIncomingMessage));
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages(Connection* allowedConnection)
{
    ASSERT(m_runLoop == RunLoop::current());

    Vector<ConnectionAndIncomingMessage> messagesToDispatchWhileWaitingForSyncReply;

    {
        MutexLocker locker(m_mutex);
        m_messagesToDispatchWhileWaitingForSyncReply.swap(messagesToDispatchWhileWaitingForSyncReply);
    }

    Vector<ConnectionAndIncomingMessage> messagesToPutBack;

    for (size_t i = 0; i < messagesToDispatchWhileWaitingForSyncReply.size(); ++i) {
        ConnectionAndIncomingMessage& connectionAndIncomingMessage = messagesToDispatchWhileWaitingForSyncReply[i];

        if (allowedConnection && allowedConnection != connectionAndIncomingMessage.connection) {
            // This incoming message belongs to another connection and we don't want to dispatch it now
            // so mark it to be put back in the message queue.
            messagesToPutBack.append(std::move(connectionAndIncomingMessage));
            continue;
        }

        connectionAndIncomingMessage.connection->dispatchMessage(std::move(connectionAndIncomingMessage.message));
    }

    if (!messagesToPutBack.isEmpty()) {
        MutexLocker locker(m_mutex);

        for (auto& message : messagesToPutBack)
            m_messagesToDispatchWhileWaitingForSyncReply.append(std::move(message));
    }
}

void Connection::SyncMessageState::dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection* connection)
{
    {
        MutexLocker locker(m_mutex);
        ASSERT(m_didScheduleDispatchMessagesWorkSet.contains(connection));
        m_didScheduleDispatchMessagesWorkSet.remove(connection);
    }

    dispatchMessages(connection);
}

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
    , m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(false)
    , m_shouldExitOnSyncMessageSendFailure(false)
    , m_didCloseOnConnectionWorkQueueCallback(0)
    , m_isConnected(false)
    , m_connectionQueue(WorkQueue::create("com.apple.IPC.ReceiveQueue"))
    , m_clientRunLoop(clientRunLoop)
    , m_inSendSyncCount(0)
    , m_inDispatchMessageCount(0)
    , m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount(0)
    , m_didReceiveInvalidMessage(false)
    , m_syncMessageState(SyncMessageState::getOrCreate(clientRunLoop))
    , m_shouldWaitForSyncReplies(true)
{
    ASSERT(m_client);

    platformInitialize(identifier);
}

Connection::~Connection()
{
    ASSERT(!isValid());
}

void Connection::setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(bool flag)
{
    ASSERT(!m_isConnected);

    m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage = flag;
}

void Connection::setShouldExitOnSyncMessageSendFailure(bool shouldExitOnSyncMessageSendFailure)
{
    ASSERT(!m_isConnected);

    m_shouldExitOnSyncMessageSendFailure = shouldExitOnSyncMessageSendFailure;
}

void Connection::addWorkQueueMessageReceiver(StringReference messageReceiverName, WorkQueue* workQueue, WorkQueueMessageReceiver* workQueueMessageReceiver)
{
    ASSERT(RunLoop::current() == m_clientRunLoop);
    ASSERT(!m_isConnected);

    m_connectionQueue->dispatch(bind(&Connection::addWorkQueueMessageReceiverOnConnectionWorkQueue, this, messageReceiverName, RefPtr<WorkQueue>(workQueue), RefPtr<WorkQueueMessageReceiver>(workQueueMessageReceiver)));
}

void Connection::removeWorkQueueMessageReceiver(StringReference messageReceiverName)
{
    ASSERT(RunLoop::current() == m_clientRunLoop);

    m_connectionQueue->dispatch(bind(&Connection::removeWorkQueueMessageReceiverOnConnectionWorkQueue, this, messageReceiverName));
}

void Connection::addWorkQueueMessageReceiverOnConnectionWorkQueue(StringReference messageReceiverName, WorkQueue* workQueue, WorkQueueMessageReceiver* workQueueMessageReceiver)
{
    ASSERT(workQueue);
    ASSERT(workQueueMessageReceiver);
    ASSERT(!m_workQueueMessageReceivers.contains(messageReceiverName));

    m_workQueueMessageReceivers.add(messageReceiverName, std::make_pair(workQueue, workQueueMessageReceiver));
}

void Connection::removeWorkQueueMessageReceiverOnConnectionWorkQueue(StringReference messageReceiverName)
{
    ASSERT(m_workQueueMessageReceivers.contains(messageReceiverName));
    m_workQueueMessageReceivers.remove(messageReceiverName);
}

void Connection::dispatchWorkQueueMessageReceiverMessage(WorkQueueMessageReceiver* workQueueMessageReceiver, MessageDecoder* incomingMessageDecoder)
{
    OwnPtr<MessageDecoder> decoder = adoptPtr(incomingMessageDecoder);

    if (!decoder->isSyncMessage()) {
        workQueueMessageReceiver->didReceiveMessage(this, *decoder);
        return;
    }

    uint64_t syncRequestID = 0;
    if (!decoder->decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        // FIXME: Handle this.
        decoder->markInvalid();
        return;
    }

    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID);

    // Hand off both the decoder and encoder to the work queue message receiver.
    workQueueMessageReceiver->didReceiveSyncMessage(this, *decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder->isInvalid());

    if (replyEncoder)
        sendSyncReply(std::move(replyEncoder));
}

void Connection::setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback callback)
{
    ASSERT(!m_isConnected);

    m_didCloseOnConnectionWorkQueueCallback = callback;    
}

void Connection::invalidate()
{
    if (!isValid()) {
        // Someone already called invalidate().
        return;
    }
    
    // Reset the client.
    m_client = 0;

    m_connectionQueue->dispatch(WTF::bind(&Connection::platformInvalidate, this));
}

void Connection::markCurrentlyDispatchedMessageAsInvalid()
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);

    m_didReceiveInvalidMessage = true;
}

std::unique_ptr<MessageEncoder> Connection::createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID)
{
    auto encoder = std::make_unique<MessageEncoder>(messageReceiverName, messageName, destinationID);
    encoder->setIsSyncMessage(true);

    // Encode the sync request ID.
    syncRequestID = ++m_syncRequestID;
    *encoder << syncRequestID;

    return encoder;
}

bool Connection::sendMessage(std::unique_ptr<MessageEncoder> encoder, unsigned messageSendFlags)
{
    if (!isValid())
        return false;

    if (messageSendFlags & DispatchMessageEvenWhenWaitingForSyncReply
        && (!m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage
            || m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(true);

    {
        MutexLocker locker(m_outgoingMessagesLock);
        m_outgoingMessages.append(std::move(encoder));
    }
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    m_connectionQueue->dispatch(WTF::bind(&Connection::sendOutgoingMessages, this));
    return true;
}

bool Connection::sendSyncReply(std::unique_ptr<MessageEncoder> encoder)
{
    return sendMessage(std::move(encoder));
}

std::unique_ptr<MessageDecoder> Connection::waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, std::chrono::milliseconds timeout)
{
    // First, check if this message is already in the incoming messages queue.
    {
        MutexLocker locker(m_incomingMessagesLock);

        for (auto it = m_incomingMessages.begin(), end = m_incomingMessages.end(); it != end; ++it) {
            std::unique_ptr<MessageDecoder>& message = *it;

            if (message->messageReceiverName() == messageReceiverName && message->messageName() == messageName && message->destinationID() == destinationID) {
                std::unique_ptr<MessageDecoder> returnedMessage = std::move(message);

                m_incomingMessages.remove(it);
                return returnedMessage;
            }
        }
    }

    std::pair<std::pair<StringReference, StringReference>, uint64_t> messageAndDestination(std::make_pair(std::make_pair(messageReceiverName, messageName), destinationID));
    
    {
        std::lock_guard<std::mutex> lock(m_waitForMessageMutex);

        // We don't support having multiple clients wait for the same message.
        ASSERT(!m_waitForMessageMap.contains(messageAndDestination));
    
        // Insert our pending wait.
        m_waitForMessageMap.set(messageAndDestination, nullptr);
    }
    
    // Now wait for it to be set.
    while (true) {
        std::unique_lock<std::mutex> lock(m_waitForMessageMutex);

        auto it = m_waitForMessageMap.find(messageAndDestination);
        if (it->value) {
            std::unique_ptr<MessageDecoder> decoder = std::move(it->value);
            m_waitForMessageMap.remove(it);

            return decoder;
        }

        // Now we wait.
        if (m_waitForMessageCondition.wait_for(lock, timeout) == std::cv_status::timeout) {
            // We timed out, now remove the pending wait.
            m_waitForMessageMap.remove(messageAndDestination);

            break;
        }
    }

    return nullptr;
}

std::unique_ptr<MessageDecoder> Connection::sendSyncMessage(uint64_t syncRequestID, std::unique_ptr<MessageEncoder> encoder, std::chrono::milliseconds timeout, unsigned syncSendFlags)
{
    if (RunLoop::current() != m_clientRunLoop) {
        // No flags are supported for synchronous messages sent from secondary threads.
        ASSERT(!syncSendFlags);
        return sendSyncMessageFromSecondaryThread(syncRequestID, std::move(encoder), timeout);
    }

    if (!isValid()) {
        didFailToSendSyncMessage();
        return nullptr;
    }

    // Push the pending sync reply information on our stack.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies) {
            didFailToSendSyncMessage();
            return nullptr;
        }

        m_pendingSyncReplies.append(PendingSyncReply(syncRequestID));
    }

    ++m_inSendSyncCount;

    // First send the message.
    sendMessage(std::move(encoder), DispatchMessageEvenWhenWaitingForSyncReply);

    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    Ref<Connection> protect(*this);
    std::unique_ptr<MessageDecoder> reply = waitForSyncReply(syncRequestID, timeout, syncSendFlags);

    --m_inSendSyncCount;

    // Finally, pop the pending sync reply information.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(m_pendingSyncReplies.last().syncRequestID == syncRequestID);
        m_pendingSyncReplies.removeLast();
    }

    if (!reply)
        didFailToSendSyncMessage();

    return reply;
}

std::unique_ptr<MessageDecoder> Connection::sendSyncMessageFromSecondaryThread(uint64_t syncRequestID, std::unique_ptr<MessageEncoder> encoder, std::chrono::milliseconds timeout)
{
    ASSERT(RunLoop::current() != m_clientRunLoop);

    if (!isValid())
        return nullptr;

    SecondaryThreadPendingSyncReply pendingReply;

    // Push the pending sync reply information on our stack.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies)
            return nullptr;

        ASSERT(!m_secondaryThreadPendingSyncReplyMap.contains(syncRequestID));
        m_secondaryThreadPendingSyncReplyMap.add(syncRequestID, &pendingReply);
    }

    sendMessage(std::move(encoder), 0);

    pendingReply.semaphore.wait(currentTime() + (timeout.count() / 1000.0));

    // Finally, pop the pending sync reply information.
    {
        MutexLocker locker(m_syncReplyStateMutex);
        ASSERT(m_secondaryThreadPendingSyncReplyMap.contains(syncRequestID));
        m_secondaryThreadPendingSyncReplyMap.remove(syncRequestID);
    }

    return std::move(pendingReply.replyDecoder);
}

std::unique_ptr<MessageDecoder> Connection::waitForSyncReply(uint64_t syncRequestID, std::chrono::milliseconds timeout, unsigned syncSendFlags)
{
    double absoluteTime = currentTime() + (timeout.count() / 1000.0);

    bool timedOut = false;
    while (!timedOut) {
        // First, check if we have any messages that we need to process.
        m_syncMessageState->dispatchMessages(0);
        
        {
            MutexLocker locker(m_syncReplyStateMutex);

            // Second, check if there is a sync reply at the top of the stack.
            ASSERT(!m_pendingSyncReplies.isEmpty());
            
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies.last();
            ASSERT_UNUSED(syncRequestID, pendingSyncReply.syncRequestID == syncRequestID);
            
            // We found the sync reply, or the connection was closed.
            if (pendingSyncReply.didReceiveReply || !m_shouldWaitForSyncReplies)
                return std::move(pendingSyncReply.replyDecoder);
        }

        // Processing a sync message could cause the connection to be invalidated.
        // (If the handler ends up calling Connection::invalidate).
        // If that happens, we need to stop waiting, or we'll hang since we won't get
        // any more incoming messages.
        if (!isValid())
            return nullptr;

        // We didn't find a sync reply yet, keep waiting.
        // This allows the WebProcess to still serve clients while waiting for the message to return.
        // Notably, it can continue to process accessibility requests, which are on the main thread.
        if (syncSendFlags & SpinRunLoopWhileWaitingForReply) {
#if PLATFORM(MAC)
            // FIXME: Although we run forever, any events incoming will cause us to drop out and exit out. This however doesn't
            // account for a timeout value passed in. Timeout is always NoTimeout in these cases, but that could change.
            RunLoop::current()->runForDuration(1e10);
            timedOut = currentTime() >= absoluteTime;
#endif
        } else
            timedOut = !m_syncMessageState->wait(absoluteTime);
        
    }

    return nullptr;
}

void Connection::processIncomingSyncReply(std::unique_ptr<MessageDecoder> decoder)
{
    MutexLocker locker(m_syncReplyStateMutex);

    // Go through the stack of sync requests that have pending replies and see which one
    // this reply is for.
    for (size_t i = m_pendingSyncReplies.size(); i > 0; --i) {
        PendingSyncReply& pendingSyncReply = m_pendingSyncReplies[i - 1];

        if (pendingSyncReply.syncRequestID != decoder->destinationID())
            continue;

        ASSERT(!pendingSyncReply.replyDecoder);

        pendingSyncReply.replyDecoder = std::move(decoder);
        pendingSyncReply.didReceiveReply = true;

        // We got a reply to the last send message, wake up the client run loop so it can be processed.
        if (i == m_pendingSyncReplies.size())
            m_syncMessageState->wakeUpClientRunLoop();

        return;
    }

    // If it's not a reply to any primary thread message, check if it is a reply to a secondary thread one.
    SecondaryThreadPendingSyncReplyMap::iterator secondaryThreadReplyMapItem = m_secondaryThreadPendingSyncReplyMap.find(decoder->destinationID());
    if (secondaryThreadReplyMapItem != m_secondaryThreadPendingSyncReplyMap.end()) {
        SecondaryThreadPendingSyncReply* reply = secondaryThreadReplyMapItem->value;
        ASSERT(!reply->replyDecoder);
        reply->replyDecoder = std::move(decoder);
        reply->semaphore.signal();
    }

    // If we get here, it means we got a reply for a message that wasn't in the sync request stack or map.
    // This can happen if the send timed out, so it's fine to ignore.
}

void Connection::processIncomingMessage(std::unique_ptr<MessageDecoder> message)
{
    ASSERT(!message->messageReceiverName().isEmpty());
    ASSERT(!message->messageName().isEmpty());

    if (message->messageReceiverName() == "IPC" && message->messageName() == "SyncMessageReply") {
        processIncomingSyncReply(std::move(message));
        return;
    }

    if (!m_workQueueMessageReceivers.isValidKey(message->messageReceiverName())) {
        if (message->messageReceiverName().isEmpty() && message->messageName().isEmpty()) {
            // Something went wrong when decoding the message. Encode the message length so we can figure out if this
            // happens for certain message lengths.
            CString messageReceiverName = "<unknown message>";
            CString messageName = String::format("<message length: %zu bytes>", message->length()).utf8();

            m_clientRunLoop->dispatch(bind(&Connection::dispatchDidReceiveInvalidMessage, this, messageReceiverName, messageName));
            return;
        }

        m_clientRunLoop->dispatch(bind(&Connection::dispatchDidReceiveInvalidMessage, this, message->messageReceiverName().toString(), message->messageName().toString()));
        return;
    }

    auto it = m_workQueueMessageReceivers.find(message->messageReceiverName());
    if (it != m_workQueueMessageReceivers.end()) {
        it->value.first->dispatch(bind(&Connection::dispatchWorkQueueMessageReceiverMessage, this, it->value.second, message.release()));
        return;
    }

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (m_syncMessageState->processIncomingMessage(this, message))
        return;

    // Check if we're waiting for this message.
    {
        std::lock_guard<std::mutex> lock(m_waitForMessageMutex);

        auto it = m_waitForMessageMap.find(std::make_pair(std::make_pair(message->messageReceiverName(), message->messageName()), message->destinationID()));
        if (it != m_waitForMessageMap.end()) {
            it->value = std::move(message);
            ASSERT(it->value);
        
            m_waitForMessageCondition.notify_one();
            return;
        }
    }

    enqueueIncomingMessage(std::move(message));
}

void Connection::postConnectionDidCloseOnConnectionWorkQueue()
{
    m_connectionQueue->dispatch(WTF::bind(&Connection::connectionDidClose, this));
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
            m_syncMessageState->wakeUpClientRunLoop();

        for (SecondaryThreadPendingSyncReplyMap::iterator iter = m_secondaryThreadPendingSyncReplyMap.begin(); iter != m_secondaryThreadPendingSyncReplyMap.end(); ++iter)
            iter->value->semaphore.signal();
    }

    if (m_didCloseOnConnectionWorkQueueCallback)
        m_didCloseOnConnectionWorkQueueCallback(this);

    m_clientRunLoop->dispatch(WTF::bind(&Connection::dispatchConnectionDidClose, this));
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
        std::unique_ptr<MessageEncoder> message;

        {
            MutexLocker locker(m_outgoingMessagesLock);
            if (m_outgoingMessages.isEmpty())
                break;
            message = m_outgoingMessages.takeFirst();
        }

        if (!sendOutgoingMessage(std::move(message)))
            break;
    }
}

void Connection::dispatchSyncMessage(MessageDecoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        decoder.markInvalid();
        return;
    }

    auto replyEncoder = std::make_unique<MessageEncoder>("IPC", "SyncMessageReply", syncRequestID);

    // Hand off both the decoder and encoder to the client.
    m_client->didReceiveSyncMessage(this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder.isInvalid());

    if (replyEncoder)
        sendSyncReply(std::move(replyEncoder));
}

void Connection::dispatchDidReceiveInvalidMessage(const CString& messageReceiverNameString, const CString& messageNameString)
{
    ASSERT(RunLoop::current() == m_clientRunLoop);

    if (!m_client)
        return;

    m_client->didReceiveInvalidMessage(this, StringReference(messageReceiverNameString.data(), messageReceiverNameString.length()), StringReference(messageNameString.data(), messageNameString.length()));
}

void Connection::didFailToSendSyncMessage()
{
    if (!m_shouldExitOnSyncMessageSendFailure)
        return;

    exit(0);
}

void Connection::enqueueIncomingMessage(std::unique_ptr<MessageDecoder> incomingMessage)
{
    {
        MutexLocker locker(m_incomingMessagesLock);
        m_incomingMessages.append(std::move(incomingMessage));
    }

    m_clientRunLoop->dispatch(WTF::bind(&Connection::dispatchOneMessage, this));
}

void Connection::dispatchMessage(MessageDecoder& decoder)
{
    m_client->didReceiveMessage(this, decoder);
}

void Connection::dispatchMessage(std::unique_ptr<MessageDecoder> message)
{
    // If there's no client, return. We do this after calling releaseArguments so that
    // the ArgumentDecoder message will be freed.
    if (!m_client)
        return;

    m_inDispatchMessageCount++;

    if (message->shouldDispatchMessageWhenWaitingForSyncReply())
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount++;

    bool oldDidReceiveInvalidMessage = m_didReceiveInvalidMessage;
    m_didReceiveInvalidMessage = false;

    if (message->isSyncMessage())
        dispatchSyncMessage(*message);
    else
        dispatchMessage(*message);

    m_didReceiveInvalidMessage |= message->isInvalid();
    m_inDispatchMessageCount--;

    if (message->shouldDispatchMessageWhenWaitingForSyncReply())
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount--;

    if (m_didReceiveInvalidMessage && m_client)
        m_client->didReceiveInvalidMessage(this, message->messageReceiverName(), message->messageName());

    m_didReceiveInvalidMessage = oldDidReceiveInvalidMessage;
}

void Connection::dispatchOneMessage()
{
    std::unique_ptr<MessageDecoder> message;

    {
        MutexLocker locker(m_incomingMessagesLock);
        if (m_incomingMessages.isEmpty())
            return;

        message = m_incomingMessages.takeFirst();
    }

    dispatchMessage(std::move(message));
}

void Connection::wakeUpRunLoop()
{
    m_clientRunLoop->wakeUp();
}

} // namespace IPC
