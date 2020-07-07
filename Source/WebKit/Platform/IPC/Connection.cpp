/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "MessageFlags.h"
#include <memory>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#include "MachMessage.h"
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
#include "UnixMessage.h"
#endif

namespace IPC {

#if PLATFORM(COCOA)
// The IPC connection gets killed if the incoming message queue reaches 50000 messages before the main thread has a chance to dispatch them.
const size_t maxPendingIncomingMessagesKillingThreshold { 50000 };
#endif

std::atomic<unsigned> UnboundedSynchronousIPCScope::unboundedSynchronousIPCCount = 0;

struct Connection::WaitForMessageState {
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

class Connection::SyncMessageState {
public:
    static SyncMessageState& singleton();

    ~SyncMessageState() = delete;

    void wakeUpClientRunLoop()
    {
        m_waitForSyncReplySemaphore.signal();
    }

    bool wait(TimeWithDynamicClockType absoluteTime)
    {
        return m_waitForSyncReplySemaphore.waitUntil(absoluteTime);
    }

    // Returns true if this message will be handled on a client thread that is currently
    // waiting for a reply to a synchronous message.
    bool processIncomingMessage(Connection&, std::unique_ptr<Decoder>&);

    // Dispatch pending sync messages.
    void dispatchMessages();

private:
    friend class LazyNeverDestroyed<Connection::SyncMessageState>;
    SyncMessageState() = default;

    // Dispatch pending sync messages for given connection.
    void dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(Connection&);

    BinarySemaphore m_waitForSyncReplySemaphore;

    // Protects m_didScheduleDispatchMessagesWorkSet and m_messagesToDispatchWhileWaitingForSyncReply.
    Lock m_mutex;

    // The set of connections for which we've scheduled a call to dispatchMessageAndResetDidScheduleDispatchMessagesForConnection.
    HashSet<RefPtr<Connection>> m_didScheduleDispatchMessagesWorkSet;

    struct ConnectionAndIncomingMessage {
        Ref<Connection> connection;
        std::unique_ptr<Decoder> message;

        void dispatch()
        {
            connection->dispatchMessage(WTFMove(message));
        }
    };
    Vector<ConnectionAndIncomingMessage> m_messagesToDispatchWhileWaitingForSyncReply;
};

Connection::SyncMessageState& Connection::SyncMessageState::singleton()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<SyncMessageState> syncMessageState;

    std::call_once(onceFlag, [] {
        syncMessageState.construct();
    });

    return syncMessageState;
}

bool Connection::SyncMessageState::processIncomingMessage(Connection& connection, std::unique_ptr<Decoder>& message)
{
    switch (message->shouldDispatchMessageWhenWaitingForSyncReply()) {
    case ShouldDispatchWhenWaitingForSyncReply::No:
        return false;
    case ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC:
        if (!UnboundedSynchronousIPCScope::hasOngoingUnboundedSyncIPC())
            return false;
        break;
    case ShouldDispatchWhenWaitingForSyncReply::Yes:
        break;
    }

    bool shouldDispatch;
    {
        auto locker = holdLock(m_mutex);
        shouldDispatch = m_didScheduleDispatchMessagesWorkSet.add(&connection).isNewEntry;
        m_messagesToDispatchWhileWaitingForSyncReply.append(ConnectionAndIncomingMessage { connection, WTFMove(message) });
    }

    if (shouldDispatch) {
        RunLoop::main().dispatch([this, protectedConnection = makeRef(connection)]() mutable {
            dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(protectedConnection);
        });
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages()
{
    ASSERT(RunLoop::isMain());

    Vector<ConnectionAndIncomingMessage> messagesToDispatchWhileWaitingForSyncReply;
    {
        auto locker = holdLock(m_mutex);
        m_messagesToDispatchWhileWaitingForSyncReply.swap(messagesToDispatchWhileWaitingForSyncReply);
    }

    for (auto& connectionAndIncomingMessage : messagesToDispatchWhileWaitingForSyncReply)
        connectionAndIncomingMessage.dispatch();
}

void Connection::SyncMessageState::dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(Connection& connection)
{
    ASSERT(RunLoop::isMain());

    Vector<ConnectionAndIncomingMessage> messagesToDispatchWhileWaitingForSyncReply;
    {
        auto locker = holdLock(m_mutex);
        ASSERT(m_didScheduleDispatchMessagesWorkSet.contains(&connection));
        m_didScheduleDispatchMessagesWorkSet.remove(&connection);
        m_messagesToDispatchWhileWaitingForSyncReply.swap(messagesToDispatchWhileWaitingForSyncReply);
    }

    Vector<ConnectionAndIncomingMessage> messagesToPutBack;
    for (auto& connectionAndIncomingMessage : messagesToDispatchWhileWaitingForSyncReply) {
        if (&connection == connectionAndIncomingMessage.connection.ptr())
            connectionAndIncomingMessage.dispatch();
        else
            messagesToPutBack.append(WTFMove(connectionAndIncomingMessage));
    }

    if (!messagesToPutBack.isEmpty()) {
        auto locker = holdLock(m_mutex);
        messagesToPutBack.appendVector(WTFMove(m_messagesToDispatchWhileWaitingForSyncReply));
        m_messagesToDispatchWhileWaitingForSyncReply = WTFMove(messagesToPutBack);
    }
}

// Represents a sync request for which we're waiting on a reply.
struct Connection::PendingSyncReply {
    // The request ID.
    uint64_t syncRequestID { 0 };

    // The reply decoder, will be null if there was an error processing the sync
    // message on the other side.
    std::unique_ptr<Decoder> replyDecoder;

    // Will be set to true once a reply has been received.
    bool didReceiveReply { false };

    PendingSyncReply() = default;

    explicit PendingSyncReply(uint64_t syncRequestID)
        : syncRequestID(syncRequestID)
    {
    }
};

Ref<Connection> Connection::createServerConnection(Identifier identifier, Client& client)
{
    return adoptRef(*new Connection(identifier, true, client));
}

Ref<Connection> Connection::createClientConnection(Identifier identifier, Client& client)
{
    return adoptRef(*new Connection(identifier, false, client));
}

static HashMap<IPC::Connection::UniqueID, Connection*>& allConnections()
{
    static NeverDestroyed<HashMap<IPC::Connection::UniqueID, Connection*>> map;
    return map;
}

static Lock& asyncReplyHandlerMapLock()
{
    static Lock lock;
    return lock;
}

static HashMap<uintptr_t, HashMap<uint64_t, CompletionHandler<void(Decoder*)>>>& asyncReplyHandlerMap(const LockHolder&)
{
    ASSERT(asyncReplyHandlerMapLock().isHeld());
    static NeverDestroyed<HashMap<uintptr_t, HashMap<uint64_t, CompletionHandler<void(Decoder*)>>>> map;
    return map.get();
}

static void clearAsyncReplyHandlers(const Connection&);

Connection::Connection(Identifier identifier, bool isServer, Client& client)
    : m_client(client)
    , m_uniqueID(UniqueID::generate())
    , m_isServer(isServer)
    , m_syncRequestID(0)
    , m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(false)
    , m_shouldExitOnSyncMessageSendFailure(false)
    , m_didCloseOnConnectionWorkQueueCallback(0)
    , m_isConnected(false)
    , m_connectionQueue(WorkQueue::create("com.apple.IPC.ReceiveQueue"))
    , m_inSendSyncCount(0)
    , m_inDispatchMessageCount(0)
    , m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount(0)
    , m_didReceiveInvalidMessage(false)
    , m_shouldWaitForSyncReplies(true)
    , m_shouldWaitForMessages(true)
{
    ASSERT(RunLoop::isMain());
    allConnections().add(m_uniqueID, this);

    platformInitialize(identifier);

#if HAVE(QOS_CLASSES)
    ASSERT(pthread_main_np());
    m_mainThread = pthread_self();
#endif
}

Connection::~Connection()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!isValid());

    allConnections().remove(m_uniqueID);

    clearAsyncReplyHandlers(*this);
}

Connection* Connection::connection(UniqueID uniqueID)
{
    ASSERT(RunLoop::isMain());
    return allConnections().get(uniqueID);
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

void Connection::addWorkQueueMessageReceiver(ReceiverName messageReceiverName, WorkQueue& workQueue, WorkQueueMessageReceiver* workQueueMessageReceiver)
{
    ASSERT(RunLoop::isMain());

    auto locker = holdLock(m_workQueueMessageReceiversMutex);
    ASSERT(!m_workQueueMessageReceivers.contains(messageReceiverName));

    m_workQueueMessageReceivers.add(messageReceiverName, std::make_pair(&workQueue, workQueueMessageReceiver));
}

void Connection::removeWorkQueueMessageReceiver(ReceiverName messageReceiverName)
{
    ASSERT(RunLoop::isMain());

    auto locker = holdLock(m_workQueueMessageReceiversMutex);
    ASSERT(m_workQueueMessageReceivers.contains(messageReceiverName));
    m_workQueueMessageReceivers.remove(messageReceiverName);
}

void Connection::dispatchWorkQueueMessageReceiverMessage(WorkQueueMessageReceiver& workQueueMessageReceiver, Decoder& decoder)
{
    if (!decoder.isSyncMessage()) {
        workQueueMessageReceiver.didReceiveMessage(*this, decoder);
        return;
    }

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        // FIXME: Handle this.
        decoder.markInvalid();
        return;
    }

    auto replyEncoder = makeUnique<Encoder>(MessageName::SyncMessageReply, syncRequestID);

    // Hand off both the decoder and encoder to the work queue message receiver.
    workQueueMessageReceiver.didReceiveSyncMessage(*this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(decoder.isValid());

    if (replyEncoder)
        sendSyncReply(WTFMove(replyEncoder));
}

void Connection::addThreadMessageReceiver(ReceiverName messageReceiverName, ThreadMessageReceiver* threadMessageReceiver)
{
    ASSERT(RunLoop::isMain());

    auto locker = holdLock(m_threadMessageReceiversLock);
    ASSERT(!m_threadMessageReceivers.contains(messageReceiverName));

    m_threadMessageReceivers.add(messageReceiverName, threadMessageReceiver);
}

void Connection::removeThreadMessageReceiver(ReceiverName messageReceiverName)
{
    ASSERT(RunLoop::isMain());

    auto locker = holdLock(m_threadMessageReceiversLock);
    ASSERT(m_threadMessageReceivers.contains(messageReceiverName));

    m_threadMessageReceivers.remove(messageReceiverName);
}

void Connection::dispatchThreadMessageReceiverMessage(ThreadMessageReceiver& threadMessageReceiver, Decoder& decoder)
{
    if (!decoder.isSyncMessage()) {
        threadMessageReceiver.didReceiveMessage(*this, decoder);
        return;
    }

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // FIXME: Handle invalid sync message.
        decoder.markInvalid();
        return;
    }

    auto replyEncoder = makeUnique<Encoder>(MessageName::SyncMessageReply, syncRequestID);
    threadMessageReceiver.didReceiveSyncMessage(*this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(decoder.isValid());

    if (replyEncoder)
        sendSyncReply(WTFMove(replyEncoder));
}

void Connection::setDidCloseOnConnectionWorkQueueCallback(DidCloseOnConnectionWorkQueueCallback callback)
{
    ASSERT(!m_isConnected);

    m_didCloseOnConnectionWorkQueueCallback = callback;    
}

void Connection::invalidate()
{
    ASSERT(RunLoop::isMain());

    if (!isValid()) {
        // Someone already called invalidate().
        return;
    }
    
    m_isValid = false;

    m_connectionQueue->dispatch([protectedThis = makeRef(*this)]() mutable {
        protectedThis->platformInvalidate();
    });
}

void Connection::markCurrentlyDispatchedMessageAsInvalid()
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);

    m_didReceiveInvalidMessage = true;
}

std::unique_ptr<Encoder> Connection::createSyncMessageEncoder(MessageName messageName, uint64_t destinationID, uint64_t& syncRequestID)
{
    auto encoder = makeUnique<Encoder>(messageName, destinationID);
    encoder->setIsSyncMessage(true);

    // Encode the sync request ID.
    syncRequestID = ++m_syncRequestID;
    *encoder << syncRequestID;

    return encoder;
}

bool Connection::sendMessage(std::unique_ptr<Encoder> encoder, OptionSet<SendOption> sendOptions)
{
    if (!isValid())
        return false;

    if (isMainThread() && m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting && !encoder->isSyncMessage() && !(encoder->messageReceiverName() == ReceiverName::IPC) && !sendOptions.contains(SendOption::IgnoreFullySynchronousMode)) {
        uint64_t syncRequestID;
        auto wrappedMessage = createSyncMessageEncoder(MessageName::WrappedAsyncMessageForTesting, encoder->destinationID(), syncRequestID);
        wrappedMessage->setFullySynchronousModeForTesting();
        wrappedMessage->wrapForTesting(WTFMove(encoder));
        return static_cast<bool>(sendSyncMessage(syncRequestID, WTFMove(wrappedMessage), Seconds::infinity(), { }));
    }

    if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForSyncReply)
        && (!m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage
            || m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply::Yes);
    else if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC);

    {
        auto locker = holdLock(m_outgoingMessagesMutex);
        m_outgoingMessages.append(WTFMove(encoder));
    }
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    m_connectionQueue->dispatch([protectedThis = makeRef(*this)]() mutable {
        protectedThis->sendOutgoingMessages();
    });
    return true;
}

bool Connection::sendSyncReply(std::unique_ptr<Encoder> encoder)
{
    return sendMessage(WTFMove(encoder), { });
}

Seconds Connection::timeoutRespectingIgnoreTimeoutsForTesting(Seconds timeout) const
{
    return m_ignoreTimeoutsForTesting ? Seconds::infinity() : timeout;
}

std::unique_ptr<Decoder> Connection::waitForMessage(MessageName messageName, uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions)
{
    ASSERT(RunLoop::isMain());
    auto protectedThis = makeRef(*this);

    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);

    bool hasIncomingSynchronousMessage = false;

    // First, check if this message is already in the incoming messages queue.
    {
        auto locker = holdLock(m_incomingMessagesMutex);

        for (auto it = m_incomingMessages.begin(), end = m_incomingMessages.end(); it != end; ++it) {
            std::unique_ptr<Decoder>& message = *it;

            if (message->messageName() == messageName && message->destinationID() == destinationID) {
                std::unique_ptr<Decoder> returnedMessage = WTFMove(message);

                m_incomingMessages.remove(it);
                return returnedMessage;
            }

            if (message->isSyncMessage())
                hasIncomingSynchronousMessage = true;
        }
    }

    // Don't even start waiting if we have InterruptWaitingIfSyncMessageArrives and there's a sync message already in the queue.
    if (hasIncomingSynchronousMessage && waitForOptions.contains(WaitForOption::InterruptWaitingIfSyncMessageArrives)) {
#if ASSERT_ENABLED
        auto locker = holdLock(m_waitForMessageMutex);
        // We don't support having multiple clients waiting for messages.
        ASSERT(!m_waitingForMessage);
#endif
        return nullptr;
    }

    WaitForMessageState waitingForMessage(messageName, destinationID, waitForOptions);

    {
        auto locker = holdLock(m_waitForMessageMutex);

        // We don't support having multiple clients waiting for messages.
        ASSERT(!m_waitingForMessage);
        if (m_waitingForMessage)
            return nullptr;

        // If the connection is already invalidated, don't even start waiting.
        // Once m_waitingForMessage is set, messageWaitingInterrupted will cover this instead.
        if (!m_shouldWaitForMessages)
            return nullptr;

        m_waitingForMessage = &waitingForMessage;
    }

    MonotonicTime absoluteTimeout = MonotonicTime::now() + timeout;

    // Now wait for it to be set.
    while (true) {
        // Handle any messages that are blocked on a response from us.
        SyncMessageState::singleton().dispatchMessages();

        std::unique_lock<Lock> lock(m_waitForMessageMutex);

        if (m_waitingForMessage->decoder) {
            auto decoder = WTFMove(m_waitingForMessage->decoder);
            m_waitingForMessage = nullptr;
            return decoder;
        }

        // Now we wait.
        bool didTimeout = !m_waitForMessageCondition.waitUntil(lock, absoluteTimeout);
        // We timed out, lost our connection, or a sync message came in with InterruptWaitingIfSyncMessageArrives, so stop waiting.
        if (didTimeout || m_waitingForMessage->messageWaitingInterrupted) {
            m_waitingForMessage = nullptr;
            break;
        }
    }

    return nullptr;
}

std::unique_ptr<Decoder> Connection::sendSyncMessage(uint64_t syncRequestID, std::unique_ptr<Encoder> encoder, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    ASSERT(RunLoop::isMain());

    if (!isValid()) {
        didFailToSendSyncMessage();
        return nullptr;
    }

    // Push the pending sync reply information on our stack.
    {
        LockHolder locker(m_syncReplyStateMutex);
        if (!m_shouldWaitForSyncReplies) {
            didFailToSendSyncMessage();
            return nullptr;
        }

        m_pendingSyncReplies.append(PendingSyncReply(syncRequestID));
    }

    ++m_inSendSyncCount;

    // First send the message.
    OptionSet<SendOption> sendOptions = IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply;
    if (sendSyncOptions.contains(SendSyncOption::ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply))
        sendOptions = sendOptions | IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply;

    auto messageName = encoder->messageName();
    sendMessage(WTFMove(encoder), sendOptions);

    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    Ref<Connection> protect(*this);
    std::unique_ptr<Decoder> reply = waitForSyncReply(syncRequestID, messageName, timeout, sendSyncOptions);

    --m_inSendSyncCount;

    // Finally, pop the pending sync reply information.
    {
        LockHolder locker(m_syncReplyStateMutex);
        ASSERT(m_pendingSyncReplies.last().syncRequestID == syncRequestID);
        m_pendingSyncReplies.removeLast();
    }

    if (!reply)
        didFailToSendSyncMessage();

    return reply;
}

std::unique_ptr<Decoder> Connection::waitForSyncReply(uint64_t syncRequestID, MessageName messageName, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);
    MonotonicTime absoluteTime = MonotonicTime::now() + timeout;

    willSendSyncMessage(sendSyncOptions);
    
    bool timedOut = false;
    while (!timedOut) {
        // First, check if we have any messages that we need to process.
        SyncMessageState::singleton().dispatchMessages();
        
        {
            LockHolder locker(m_syncReplyStateMutex);

            // Second, check if there is a sync reply at the top of the stack.
            ASSERT(!m_pendingSyncReplies.isEmpty());
            
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies.last();
            ASSERT_UNUSED(syncRequestID, pendingSyncReply.syncRequestID == syncRequestID);
            
            // We found the sync reply, or the connection was closed.
            if (pendingSyncReply.didReceiveReply || !m_shouldWaitForSyncReplies) {
                didReceiveSyncReply(sendSyncOptions);
                return WTFMove(pendingSyncReply.replyDecoder);
            }
        }

        // Processing a sync message could cause the connection to be invalidated.
        // (If the handler ends up calling Connection::invalidate).
        // If that happens, we need to stop waiting, or we'll hang since we won't get
        // any more incoming messages.
        if (!isValid()) {
            RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Connection no longer valid, id = %" PRIu64, syncRequestID);
            didReceiveSyncReply(sendSyncOptions);
            return nullptr;
        }

        // We didn't find a sync reply yet, keep waiting.
        // This allows the WebProcess to still serve clients while waiting for the message to return.
        // Notably, it can continue to process accessibility requests, which are on the main thread.
        timedOut = !SyncMessageState::singleton().wait(absoluteTime);
    }

#if OS(DARWIN)
    RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Timed-out while waiting for reply for %{public}s from process %d, id = %" PRIu64, description(messageName), remoteProcessID(), syncRequestID);
#else
    RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Timed-out while waiting for reply for %s, id = %" PRIu64, description(messageName), syncRequestID);
#endif

    didReceiveSyncReply(sendSyncOptions);

    return nullptr;
}

void Connection::processIncomingSyncReply(std::unique_ptr<Decoder> decoder)
{
    {
        LockHolder locker(m_syncReplyStateMutex);

        // Go through the stack of sync requests that have pending replies and see which one
        // this reply is for.
        for (size_t i = m_pendingSyncReplies.size(); i > 0; --i) {
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies[i - 1];

            if (pendingSyncReply.syncRequestID != decoder->destinationID())
                continue;

            ASSERT(!pendingSyncReply.replyDecoder);

            pendingSyncReply.replyDecoder = WTFMove(decoder);
            pendingSyncReply.didReceiveReply = true;

            // We got a reply to the last send message, wake up the client run loop so it can be processed.
            if (i == m_pendingSyncReplies.size())
                SyncMessageState::singleton().wakeUpClientRunLoop();

            return;
        }
    }

    // If we get here, it means we got a reply for a message that wasn't in the sync request stack or map.
    // This can happen if the send timed out, so it's fine to ignore.
}

void Connection::processIncomingMessage(std::unique_ptr<Decoder> message)
{
    ASSERT(message->messageReceiverName() != ReceiverName::Invalid);

    if (message->messageName() == MessageName::SyncMessageReply) {
        processIncomingSyncReply(WTFMove(message));
        return;
    }

    if (!WorkQueueMessageReceiverMap::isValidKey(message->messageReceiverName()) || !ThreadMessageReceiverMap::isValidKey(message->messageReceiverName())) {
        RunLoop::main().dispatch([protectedThis = makeRef(*this), messageName = message->messageName()]() mutable {
            protectedThis->dispatchDidReceiveInvalidMessage(messageName);
        });
        return;
    }

    if (dispatchMessageToWorkQueueReceiver(message))
        return;

    if (dispatchMessageToThreadReceiver(message))
        return;

#if HAVE(QOS_CLASSES)
    if (message->isSyncMessage() && m_shouldBoostMainThreadOnSyncMessage) {
        pthread_override_t override = pthread_override_qos_class_start_np(m_mainThread, Thread::adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE), 0);
        message->setQOSClassOverride(override);
    }
#endif

    if (message->isSyncMessage()) {
        auto locker = holdLock(m_incomingSyncMessageCallbackMutex);

        for (auto& callback : m_incomingSyncMessageCallbacks.values())
            m_incomingSyncMessageCallbackQueue->dispatch(WTFMove(callback));

        m_incomingSyncMessageCallbacks.clear();
    }

    // Check if we're waiting for this message, or if we need to interrupt waiting due to an incoming sync message.
    {
        auto locker = holdLock(m_waitForMessageMutex);

        if (m_waitingForMessage && !m_waitingForMessage->decoder) {
            if (m_waitingForMessage->messageName == message->messageName() && m_waitingForMessage->destinationID == message->destinationID()) {
                m_waitingForMessage->decoder = WTFMove(message);
                ASSERT(m_waitingForMessage->decoder);
                m_waitForMessageCondition.notifyOne();
                return;
            }

            if (m_waitingForMessage->waitForOptions.contains(WaitForOption::DispatchIncomingSyncMessagesWhileWaiting) && message->isSyncMessage() && SyncMessageState::singleton().processIncomingMessage(*this, message)) {
                m_waitForMessageCondition.notifyOne();
                return;
            }

            if (m_waitingForMessage->waitForOptions.contains(WaitForOption::InterruptWaitingIfSyncMessageArrives) && message->isSyncMessage()) {
                m_waitingForMessage->messageWaitingInterrupted = true;
                m_waitForMessageCondition.notifyOne();
                enqueueIncomingMessage(WTFMove(message));
                return;
            }
        }
    }

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (SyncMessageState::singleton().processIncomingMessage(*this, message))
        return;

    enqueueIncomingMessage(WTFMove(message));
}

uint64_t Connection::installIncomingSyncMessageCallback(WTF::Function<void ()>&& callback)
{
    auto locker = holdLock(m_incomingSyncMessageCallbackMutex);

    m_nextIncomingSyncMessageCallbackID++;

    if (!m_incomingSyncMessageCallbackQueue)
        m_incomingSyncMessageCallbackQueue = WorkQueue::create("com.apple.WebKit.IPC.IncomingSyncMessageCallbackQueue");

    m_incomingSyncMessageCallbacks.add(m_nextIncomingSyncMessageCallbackID, WTFMove(callback));

    return m_nextIncomingSyncMessageCallbackID;
}

void Connection::uninstallIncomingSyncMessageCallback(uint64_t callbackID)
{
    auto locker = holdLock(m_incomingSyncMessageCallbackMutex);
    m_incomingSyncMessageCallbacks.remove(callbackID);
}

bool Connection::hasIncomingSyncMessage()
{
    auto locker = holdLock(m_incomingMessagesMutex);

    for (auto& message : m_incomingMessages) {
        if (message->isSyncMessage())
            return true;
    }
    
    return false;
}

void Connection::enableIncomingMessagesThrottling()
{
    if (isIncomingMessagesThrottlingEnabled())
        return;

    m_incomingMessagesThrottler = makeUnique<MessagesThrottler>(*this, &Connection::dispatchIncomingMessages);
}

void Connection::postConnectionDidCloseOnConnectionWorkQueue()
{
    m_connectionQueue->dispatch([protectedThis = makeRef(*this)]() mutable {
        protectedThis->connectionDidClose();
    });
}

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    platformInvalidate();

    {
        LockHolder locker(m_syncReplyStateMutex);

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        if (!m_pendingSyncReplies.isEmpty())
            SyncMessageState::singleton().wakeUpClientRunLoop();
    }

    {
        auto locker = holdLock(m_waitForMessageMutex);

        ASSERT(m_shouldWaitForMessages);
        m_shouldWaitForMessages = false;

        if (m_waitingForMessage)
            m_waitingForMessage->messageWaitingInterrupted = true;
    }
    m_waitForMessageCondition.notifyAll();

    if (m_didCloseOnConnectionWorkQueueCallback)
        m_didCloseOnConnectionWorkQueueCallback(this);

    RunLoop::main().dispatch([protectedThis = makeRef(*this)]() mutable {
        // If the connection has been explicitly invalidated before dispatchConnectionDidClose was called,
        // then the connection will be invalid here.
        if (!protectedThis->isValid())
            return;

        // Set m_isValid to false before calling didClose, otherwise, sendSync will try to send a message
        // to the connection and will then wait indefinitely for a reply.
        protectedThis->m_isValid = false;

        protectedThis->m_client.didClose(protectedThis.get());

        clearAsyncReplyHandlers(protectedThis.get());
    });
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
        std::unique_ptr<Encoder> message;

        {
            auto locker = holdLock(m_outgoingMessagesMutex);
            if (m_outgoingMessages.isEmpty())
                break;
            message = m_outgoingMessages.takeFirst();
        }

        if (!sendOutgoingMessage(WTFMove(message)))
            break;
    }
}

void Connection::dispatchSyncMessage(Decoder& decoder)
{
    ASSERT(decoder.isSyncMessage());

    uint64_t syncRequestID = 0;
    if (!decoder.decode(syncRequestID) || !syncRequestID) {
        // We received an invalid sync message.
        decoder.markInvalid();
        return;
    }

    auto replyEncoder = makeUnique<Encoder>(MessageName::SyncMessageReply, syncRequestID);

    if (decoder.messageName() == MessageName::WrappedAsyncMessageForTesting) {
        if (!m_fullySynchronousModeIsAllowedForTesting) {
            decoder.markInvalid();
            return;
        }
        std::unique_ptr<Decoder> unwrappedDecoder = Decoder::unwrapForTesting(decoder);
        RELEASE_ASSERT(unwrappedDecoder);
        processIncomingMessage(WTFMove(unwrappedDecoder));

        SyncMessageState::singleton().dispatchMessages();
    } else {
        // Hand off both the decoder and encoder to the client.
        m_client.didReceiveSyncMessage(*this, decoder, replyEncoder);
    }

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(decoder.isValid());

    if (replyEncoder)
        sendSyncReply(WTFMove(replyEncoder));
}

void Connection::dispatchDidReceiveInvalidMessage(MessageName messageName)
{
    ASSERT(RunLoop::isMain());

    if (!isValid())
        return;

    m_client.didReceiveInvalidMessage(*this, messageName);
}

void Connection::didFailToSendSyncMessage()
{
    if (!m_shouldExitOnSyncMessageSendFailure)
        return;

    exit(0);
}

void Connection::enqueueIncomingMessage(std::unique_ptr<Decoder> incomingMessage)
{
    {
        auto locker = holdLock(m_incomingMessagesMutex);

#if PLATFORM(COCOA)
        if (m_wasKilled)
            return;

        if (isIncomingMessagesThrottlingEnabled() && m_incomingMessages.size() >= maxPendingIncomingMessagesKillingThreshold) {
            if (kill()) {
                RELEASE_LOG_FAULT(IPC, "%p - Connection::enqueueIncomingMessage: Over %zu incoming messages have been queued without the main thread processing them, killing the connection as the remote process seems to be misbehaving", this, maxPendingIncomingMessagesKillingThreshold);
                m_incomingMessages.clear();
            }
            return;
        }
#endif

        m_incomingMessages.append(WTFMove(incomingMessage));

        if (isIncomingMessagesThrottlingEnabled() && m_incomingMessages.size() != 1)
            return;
    }

    RunLoop::main().dispatch([protectedThis = makeRef(*this)]() mutable {
        if (protectedThis->isIncomingMessagesThrottlingEnabled())
            protectedThis->dispatchIncomingMessages();
        else
            protectedThis->dispatchOneIncomingMessage();
    });
}

void Connection::dispatchMessage(Decoder& decoder)
{
    RELEASE_ASSERT(isValid());
    if (decoder.messageReceiverName() == ReceiverName::AsyncReply) {
        Optional<uint64_t> listenerID;
        decoder >> listenerID;
        if (!listenerID) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto handler = takeAsyncReplyHandler(*this, *listenerID);
        if (!handler) {
            ASSERT_NOT_REACHED();
            return;
        }
        handler(&decoder);
        return;
    }

    m_client.didReceiveMessage(*this, decoder);
}

bool Connection::dispatchMessageToWorkQueueReceiver(std::unique_ptr<Decoder>& message)
{
    auto locker = holdLock(m_workQueueMessageReceiversMutex);
    auto it = m_workQueueMessageReceivers.find(message->messageReceiverName());
    if (it != m_workQueueMessageReceivers.end()) {
        it->value.first->dispatch([protectedThis = makeRef(*this), workQueueMessageReceiver = it->value.second, decoder = WTFMove(message)]() mutable {
            protectedThis->dispatchWorkQueueMessageReceiverMessage(*workQueueMessageReceiver, *decoder);
        });
        return true;
    }
    return false;
}

bool Connection::dispatchMessageToThreadReceiver(std::unique_ptr<Decoder>& message)
{
    RefPtr<ThreadMessageReceiver> protectedThreadMessageReceiver;
    {
        auto locker = holdLock(m_threadMessageReceiversLock);
        protectedThreadMessageReceiver = m_threadMessageReceivers.get(message->messageReceiverName());
    }

    if (protectedThreadMessageReceiver) {
        protectedThreadMessageReceiver->dispatchToThread([protectedThis = makeRef(*this), threadMessageReceiver = WTFMove(protectedThreadMessageReceiver), decoder = WTFMove(message)]() mutable {
            protectedThis->dispatchThreadMessageReceiverMessage(*threadMessageReceiver, *decoder);
        });
        return true;
    }
    return false;
}

void Connection::dispatchMessage(std::unique_ptr<Decoder> message)
{
    ASSERT(RunLoop::isMain());
    if (!isValid())
        return;

    // Messages to WorkQueueMessageReceivers are normally dispatched from the IPC WorkQueue. However, there is a race if
    // a client adds itself as a WorkQueueMessageReceiver as a result of receiving an IPC message on the main thread.
    // The message might have already been dispatched from the IPC WorkQueue to the main thread by the time the
    // client registers itself as a WorkQueueMessageReceiver. To address this, we check again for messages receivers
    // once the message arrives on the main thread.
    if (dispatchMessageToWorkQueueReceiver(message))
        return;

    if (message->shouldUseFullySynchronousModeForTesting()) {
        if (!m_fullySynchronousModeIsAllowedForTesting) {
            m_client.didReceiveInvalidMessage(*this, message->messageName());
            return;
        }
        m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting++;
    }

    m_inDispatchMessageCount++;
    
    bool isDispatchingMessageWhileWaitingForSyncReply = (message->shouldDispatchMessageWhenWaitingForSyncReply() == ShouldDispatchWhenWaitingForSyncReply::Yes)
        || (message->shouldDispatchMessageWhenWaitingForSyncReply() == ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC && UnboundedSynchronousIPCScope::hasOngoingUnboundedSyncIPC());

    if (isDispatchingMessageWhileWaitingForSyncReply)
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount++;

    bool oldDidReceiveInvalidMessage = m_didReceiveInvalidMessage;
    m_didReceiveInvalidMessage = false;

    if (message->isSyncMessage())
        dispatchSyncMessage(*message);
    else
        dispatchMessage(*message);

    m_didReceiveInvalidMessage |= !message->isValid();
    m_inDispatchMessageCount--;

    // FIXME: For synchronous messages, we should not decrement the counter until we send a response.
    // Otherwise, we would deadlock if processing the message results in a sync message back after we exit this function.
    if (isDispatchingMessageWhileWaitingForSyncReply)
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount--;

    if (message->shouldUseFullySynchronousModeForTesting())
        m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting--;

    if (m_didReceiveInvalidMessage && isValid())
        m_client.didReceiveInvalidMessage(*this, message->messageName());

    m_didReceiveInvalidMessage = oldDidReceiveInvalidMessage;
}

Connection::MessagesThrottler::MessagesThrottler(Connection& connection, DispatchMessagesFunction dispatchMessages)
    : m_dispatchMessagesTimer(RunLoop::main(), &connection, dispatchMessages)
    , m_connection(connection)
    , m_dispatchMessages(dispatchMessages)
{
    ASSERT(RunLoop::isMain());
}

void Connection::MessagesThrottler::scheduleMessagesDispatch()
{
    ASSERT(RunLoop::isMain());

    if (m_throttlingLevel) {
        m_dispatchMessagesTimer.startOneShot(0_s);
        return;
    }
    RunLoop::main().dispatch([this, protectedConnection = makeRefPtr(&m_connection)]() mutable {
        (protectedConnection.get()->*m_dispatchMessages)();
    });
}

size_t Connection::MessagesThrottler::numberOfMessagesToProcess(size_t totalMessages)
{
    ASSERT(RunLoop::isMain());

    // Never dispatch more than 600 messages without returning to the run loop, we can go as low as 60 with maximum throttling level.
    static const size_t maxIncomingMessagesDispatchingBatchSize { 600 };
    static const unsigned maxThrottlingLevel = 9;

    size_t batchSize = maxIncomingMessagesDispatchingBatchSize / (m_throttlingLevel + 1);

    if (totalMessages > maxIncomingMessagesDispatchingBatchSize)
        m_throttlingLevel = std::min(m_throttlingLevel + 1, maxThrottlingLevel);
    else if (m_throttlingLevel)
        --m_throttlingLevel;

    return std::min(totalMessages, batchSize);
}

void Connection::dispatchOneIncomingMessage()
{
    std::unique_ptr<Decoder> message;
    {
        auto locker = holdLock(m_incomingMessagesMutex);
        if (m_incomingMessages.isEmpty())
            return;

        message = m_incomingMessages.takeFirst();
    }

    dispatchMessage(WTFMove(message));
}

void Connection::dispatchIncomingMessages()
{
    ASSERT(RunLoop::isMain());

    std::unique_ptr<Decoder> message;

    size_t messagesToProcess = 0;
    {
        auto locker = holdLock(m_incomingMessagesMutex);
        if (m_incomingMessages.isEmpty())
            return;

        message = m_incomingMessages.takeFirst();

        // Incoming messages may get adding to the queue by the IPC thread while we're dispatching the messages below.
        // To make sure dispatchIncomingMessages() yields, we only ever process messages that were in the queue when
        // dispatchIncomingMessages() was called. Additionally, the MessageThrottler may further cap the number of
        // messages to process to make sure we give the main run loop a chance to process other events.
        messagesToProcess = m_incomingMessagesThrottler->numberOfMessagesToProcess(m_incomingMessages.size());
        if (messagesToProcess < m_incomingMessages.size()) {
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: IPC throttling was triggered (has %zu pending incoming messages, will only process %zu before yielding)", this, m_incomingMessages.size(), messagesToProcess);
#if PLATFORM(COCOA)
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: first IPC message in queue is %{public}s", this, description(message->messageName()));
#endif
        }

        // Re-schedule ourselves *before* we dispatch the messages because we want to process follow-up messages if the client
        // spins a nested run loop while we're dispatching a message. Note that this means we can re-enter this method.
        if (!m_incomingMessages.isEmpty())
            m_incomingMessagesThrottler->scheduleMessagesDispatch();
    }

    dispatchMessage(WTFMove(message));

    for (size_t i = 1; i < messagesToProcess; ++i) {
        {
            auto locker = holdLock(m_incomingMessagesMutex);
            if (m_incomingMessages.isEmpty())
                return;

            message = m_incomingMessages.takeFirst();
        }
        dispatchMessage(WTFMove(message));
    }
}

uint64_t nextAsyncReplyHandlerID()
{
    static std::atomic<uint64_t> identifier { 0 };
    return ++identifier;
}

void addAsyncReplyHandler(Connection& connection, uint64_t identifier, CompletionHandler<void(Decoder*)>&& completionHandler)
{
    LockHolder locker(asyncReplyHandlerMapLock());
    auto result = asyncReplyHandlerMap(locker).ensure(reinterpret_cast<uintptr_t>(&connection), [] {
        return HashMap<uint64_t, CompletionHandler<void(Decoder*)>>();
    }).iterator->value.add(identifier, WTFMove(completionHandler));
    ASSERT_UNUSED(result, result.isNewEntry);
}

void clearAsyncReplyHandlers(const Connection& connection)
{
    HashMap<uint64_t, CompletionHandler<void(Decoder*)>> map;
    {
        LockHolder locker(asyncReplyHandlerMapLock());
        map = asyncReplyHandlerMap(locker).take(reinterpret_cast<uintptr_t>(&connection));
    }

    for (auto& handler : map.values()) {
        if (handler)
            handler(nullptr);
    }
}

CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(Connection& connection, uint64_t identifier)
{
    LockHolder locker(asyncReplyHandlerMapLock());
    auto& map = asyncReplyHandlerMap(locker);
    auto iterator = map.find(reinterpret_cast<uintptr_t>(&connection));
    if (iterator != map.end()) {
        if (!iterator->value.isValidKey(identifier)) {
            ASSERT_NOT_REACHED();
            connection.markCurrentlyDispatchedMessageAsInvalid();
            return nullptr;
        }
        ASSERT(iterator->value.contains(identifier));
        return iterator->value.take(identifier);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void Connection::wakeUpRunLoop()
{
    RunLoop::main().wakeUp();
}

} // namespace IPC
