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
#include "MessageReceiveQueues.h"
#include "WorkQueueMessageReceiver.h"
#include <memory>
#include <wtf/ArgumentCoder.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/text/WTFString.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#include "MachMessage.h"
#include "WKCrashReporter.h"
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

Lock Connection::s_connectionMapLock;

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

    bool wait(Timeout timeout)
    {
        return m_waitForSyncReplySemaphore.waitUntil(timeout.deadline());
    }

    // Returns true if this message will be handled on a client thread that is currently
    // waiting for a reply to a synchronous message.
    bool processIncomingMessage(Connection& connection, std::unique_ptr<Decoder>&) WTF_REQUIRES_LOCK(connection.m_incomingMessagesLock);

    // Dispatch pending sync messages.
    void dispatchMessages(Function<void(MessageName, uint64_t)>&& willDispatchMessage = { });

    // Add matching pending messages to the provided MessageReceiveQueue.
    void enqueueMatchingMessages(Connection&, MessageReceiveQueue&, const ReceiverMatcher&);

private:
    friend class LazyNeverDestroyed<Connection::SyncMessageState>;
    SyncMessageState() = default;

    // Dispatch pending sync messages for given connection.
    void dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(Connection&);

    BinarySemaphore m_waitForSyncReplySemaphore;

    // Protects m_didScheduleDispatchMessagesWorkSet and m_messagesToDispatchWhileWaitingForSyncReply.
    Lock m_lock;

    // The set of connections for which we've scheduled a call to dispatchMessageAndResetDidScheduleDispatchMessagesForConnection.
    HashSet<RefPtr<Connection>> m_didScheduleDispatchMessagesWorkSet WTF_GUARDED_BY_LOCK(m_lock);

    struct ConnectionAndIncomingMessage {
        Ref<Connection> connection;
        std::unique_ptr<Decoder> message;

        void dispatch()
        {
            connection->dispatchMessage(WTFMove(message));
        }
    };
    Deque<ConnectionAndIncomingMessage> m_messagesBeingDispatched; // Only used on the main thread.
    Deque<ConnectionAndIncomingMessage> m_messagesToDispatchWhileWaitingForSyncReply WTF_GUARDED_BY_LOCK(m_lock);
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

void Connection::SyncMessageState::enqueueMatchingMessages(Connection& connection, MessageReceiveQueue& receiveQueue, const ReceiverMatcher& receiverMatcher)
{
    ASSERT(isMainRunLoop());
    auto enqueueMatchingMessagesInContainer = [&](Deque<ConnectionAndIncomingMessage>& connectionAndMessages) {
        Deque<ConnectionAndIncomingMessage> rest;
        for (auto& connectionAndMessage : connectionAndMessages) {
            if (connectionAndMessage.connection.ptr() == &connection && connectionAndMessage.message->matches(receiverMatcher))
                receiveQueue.enqueueMessage(connection, WTFMove(connectionAndMessage.message));
            else
                rest.append(WTFMove(connectionAndMessage));
        }
        connectionAndMessages = WTFMove(rest);
    };
    Locker locker { m_lock };
    enqueueMatchingMessagesInContainer(m_messagesBeingDispatched);
    enqueueMatchingMessagesInContainer(m_messagesToDispatchWhileWaitingForSyncReply);
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
        Locker locker { m_lock };
        shouldDispatch = m_didScheduleDispatchMessagesWorkSet.add(&connection).isNewEntry;
        ASSERT(connection.m_incomingMessagesLock.isHeld());
        if (message->shouldMaintainOrderingWithAsyncMessages()) {
            // This sync message should maintain ordering with async messages so we need to process the pending async messages first.
            while (!connection.m_incomingMessages.isEmpty())
                m_messagesToDispatchWhileWaitingForSyncReply.append(ConnectionAndIncomingMessage { connection, connection.m_incomingMessages.takeFirst() });
        }
        m_messagesToDispatchWhileWaitingForSyncReply.append(ConnectionAndIncomingMessage { connection, WTFMove(message) });
    }

    if (shouldDispatch) {
        RunLoop::main().dispatch([this, protectedConnection = Ref { connection }]() mutable {
            dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(protectedConnection);
        });
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages(Function<void(MessageName, uint64_t)>&& willDispatchMessage)
{
    ASSERT(RunLoop::isMain());

    {
        Locker locker { m_lock };
        if (m_messagesBeingDispatched.isEmpty())
            m_messagesBeingDispatched = std::exchange(m_messagesToDispatchWhileWaitingForSyncReply, { });
        else {
            while (!m_messagesToDispatchWhileWaitingForSyncReply.isEmpty())
                m_messagesBeingDispatched.append(m_messagesToDispatchWhileWaitingForSyncReply.takeLast());
        }
    }

    while (!m_messagesBeingDispatched.isEmpty()) {
        auto messageToDispatch = m_messagesBeingDispatched.takeFirst();
        if (willDispatchMessage)
            willDispatchMessage(messageToDispatch.message->messageName(), messageToDispatch.message->destinationID());
        messageToDispatch.dispatch();
    }
}

void Connection::SyncMessageState::dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(Connection& connection)
{
    ASSERT(RunLoop::isMain());

    {
        Locker locker { m_lock };
        ASSERT(m_didScheduleDispatchMessagesWorkSet.contains(&connection));
        m_didScheduleDispatchMessagesWorkSet.remove(&connection);
        Deque<ConnectionAndIncomingMessage> messagesToPutBack;
        for (auto& connectionAndIncomingMessage : m_messagesToDispatchWhileWaitingForSyncReply) {
            if (&connection == connectionAndIncomingMessage.connection.ptr())
                m_messagesBeingDispatched.append(WTFMove(connectionAndIncomingMessage));
            else
                messagesToPutBack.append(WTFMove(connectionAndIncomingMessage));
        }
        m_messagesToDispatchWhileWaitingForSyncReply = WTFMove(messagesToPutBack);
    }

    while (!m_messagesBeingDispatched.isEmpty())
        m_messagesBeingDispatched.takeFirst().dispatch(); // This may cause the function to re-enter when there is a nested run loop.
}

// Represents a sync request for which we're waiting on a reply.
struct Connection::PendingSyncReply {
    // The request ID.
    Connection::SyncRequestID syncRequestID;

    // The reply decoder, will be null if there was an error processing the sync
    // message on the other side.
    std::unique_ptr<Decoder> replyDecoder;

    // Will be set to true once a reply has been received.
    bool didReceiveReply { false };

    PendingSyncReply() = default;

    explicit PendingSyncReply(Connection::SyncRequestID syncRequestID)
        : syncRequestID(syncRequestID)
    {
    }
};

Ref<Connection> Connection::createServerConnection(Identifier identifier)
{
    return adoptRef(*new Connection(identifier, true));
}

Ref<Connection> Connection::createClientConnection(Identifier identifier)
{
    return adoptRef(*new Connection(identifier, false));
}

HashMap<IPC::Connection::UniqueID, Connection*>& Connection::connectionMap()
{
    static NeverDestroyed<HashMap<IPC::Connection::UniqueID, Connection*>> map;
    return map;
}

static Lock asyncReplyHandlerMapLock;
static HashMap<uintptr_t, HashMap<uint64_t, CompletionHandler<void(Decoder*)>>>& asyncReplyHandlerMap() WTF_REQUIRES_LOCK(asyncReplyHandlerMapLock)
{
    ASSERT(asyncReplyHandlerMapLock.isHeld());
    static NeverDestroyed<HashMap<uintptr_t, HashMap<uint64_t, CompletionHandler<void(Decoder*)>>>> map;
    return map.get();
}

static void clearAsyncReplyHandlers(const Connection&);

Connection::Connection(Identifier identifier, bool isServer)
    : m_uniqueID(UniqueID::generate())
    , m_isServer(isServer)
    , m_connectionQueue(WorkQueue::create("com.apple.IPC.ReceiveQueue"))
{
    ASSERT(RunLoop::isMain());

    {
        Locker locker { s_connectionMapLock };
        connectionMap().add(m_uniqueID, this);
    }

    platformInitialize(identifier);
}

Connection::~Connection()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!isValid());

    {
        Locker locker { s_connectionMapLock };
        connectionMap().remove(m_uniqueID);
    }

    clearAsyncReplyHandlers(*this);
}

// WTF_IGNORES_THREAD_SAFETY_ANALYSIS because this function accesses connectionMap() without locking.
// It is safe because this function is only called on the main thread and Connection objects are only
// constructed / destroyed on the main thread.
Connection* Connection::connection(UniqueID uniqueID) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(RunLoop::isMain());
    return connectionMap().get(uniqueID);
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

// Enqueue any pending message to the MessageReceiveQueue that is meant to go on that queue. This is important to maintain the ordering of
// IPC messages as some messages may get received on the IPC thread before the message receiver registered itself on the main thread.
void Connection::enqueueMatchingMessagesToMessageReceiveQueue(MessageReceiveQueue& receiveQueue, const ReceiverMatcher& receiverMatcher)
{
    ASSERT(isMainRunLoop());

    SyncMessageState::singleton().enqueueMatchingMessages(*this, receiveQueue, receiverMatcher);

    Deque<std::unique_ptr<Decoder>> remainingIncomingMessages;
    for (auto& message : m_incomingMessages) {
        if (message->matches(receiverMatcher))
            receiveQueue.enqueueMessage(*this, WTFMove(message));
        else
            remainingIncomingMessages.append(WTFMove(message));
    }
    m_incomingMessages = WTFMove(remainingIncomingMessages);
}

void Connection::addMessageReceiveQueue(MessageReceiveQueue& receiveQueue, const ReceiverMatcher& receiverMatcher)
{
    Locker incomingMessagesLocker { m_incomingMessagesLock };
    enqueueMatchingMessagesToMessageReceiveQueue(receiveQueue, receiverMatcher);
    m_receiveQueues.add(receiveQueue, receiverMatcher);
}

void Connection::removeMessageReceiveQueue(const ReceiverMatcher& receiverMatcher)
{
    Locker locker { m_incomingMessagesLock };
    m_receiveQueues.remove(receiverMatcher);
}

void Connection::addWorkQueueMessageReceiver(ReceiverName receiverName, WorkQueue& workQueue, WorkQueueMessageReceiver& receiver, uint64_t destinationID)
{
    auto receiverMatcher = ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID);

    auto receiveQueue = makeUnique<WorkQueueMessageReceiverQueue>(workQueue, receiver);
    Locker incomingMessagesLocker { m_incomingMessagesLock };
    enqueueMatchingMessagesToMessageReceiveQueue(*receiveQueue, receiverMatcher);
    m_receiveQueues.add(WTFMove(receiveQueue), receiverMatcher);
}

void Connection::removeWorkQueueMessageReceiver(ReceiverName receiverName, uint64_t destinationID)
{
    removeMessageReceiveQueue(ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID));
}

void Connection::addMessageReceiver(FunctionDispatcher& dispatcher, MessageReceiver& receiver, ReceiverName receiverName, uint64_t destinationID)
{
    auto receiverMatcher = ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID);
    auto receiveQueue = makeUnique<FunctionDispatcherQueue>(dispatcher, receiver);
    Locker incomingMessagesLocker { m_incomingMessagesLock };
    enqueueMatchingMessagesToMessageReceiveQueue(*receiveQueue, receiverMatcher);
    m_receiveQueues.add(WTFMove(receiveQueue), receiverMatcher);
}

void Connection::removeMessageReceiver(ReceiverName receiverName, uint64_t destinationID)
{
    removeMessageReceiveQueue(ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID));
}

void Connection::dispatchMessageReceiverMessage(MessageReceiver& messageReceiver, std::unique_ptr<Decoder>&& decoder)
{
    if (!decoder->isSyncMessage()) {
        messageReceiver.didReceiveMessage(*this, *decoder);
        return;
    }

    SyncRequestID syncRequestID;
    if (UNLIKELY(!decoder->decode(syncRequestID))) {
        // We received an invalid sync message.
        // FIXME: Handle this.
        return;
    }

    auto replyEncoder = makeUniqueRef<Encoder>(MessageName::SyncMessageReply, syncRequestID.toUInt64());

    // Hand off both the decoder and encoder to the work queue message receiver.
    bool wasHandled = messageReceiver.didReceiveSyncMessage(*this, *decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(decoder->isValid());

    if (!wasHandled)
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
    m_isValid = false;
    if (!m_client)
        return;
    m_client = nullptr;

    clearAsyncReplyHandlers(*this);

    m_connectionQueue->dispatch([protectedThis = Ref { *this }]() mutable {
        protectedThis->platformInvalidate();
    });
}

void Connection::markCurrentlyDispatchedMessageAsInvalid()
{
    // This should only be called while processing a message.
    ASSERT(m_inDispatchMessageCount > 0);

    m_didReceiveInvalidMessage = true;
}

UniqueRef<Encoder> Connection::createSyncMessageEncoder(MessageName messageName, uint64_t destinationID, SyncRequestID& syncRequestID)
{
    auto encoder = makeUniqueRef<Encoder>(messageName, destinationID);

    // Encode the sync request ID.
    syncRequestID = makeSyncRequestID();
    encoder.get() << syncRequestID;

    return encoder;
}

bool Connection::sendMessage(UniqueRef<Encoder>&& encoder, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    if (!isValid())
        return false;

#if ENABLE(IPC_TESTING_API)
    if (isMainRunLoop()) {
        bool hasDeadObservers = false;
        for (auto& observerWeakPtr : m_messageObservers) {
            if (auto* observer = observerWeakPtr.get())
                observer->willSendMessage(encoder.get(), sendOptions);
            else
                hasDeadObservers = true;
        }
        if (hasDeadObservers)
            m_messageObservers.removeAllMatching([](auto& observer) { return !observer; });
    }
#endif

    if (isMainRunLoop() && m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting && !encoder->isSyncMessage() && !(encoder->messageReceiverName() == ReceiverName::IPC) && !sendOptions.contains(SendOption::IgnoreFullySynchronousMode)) {
        SyncRequestID syncRequestID;
        auto wrappedMessage = createSyncMessageEncoder(MessageName::WrappedAsyncMessageForTesting, encoder->destinationID(), syncRequestID);
        wrappedMessage->setFullySynchronousModeForTesting();
        wrappedMessage->wrapForTesting(WTFMove(encoder));
        return static_cast<bool>(sendSyncMessage(syncRequestID, WTFMove(wrappedMessage), Timeout::infinity(), { }));
    }

    if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForSyncReply)
        && (!m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage
            || m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply::Yes);
    else if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC);

    {
        Locker locker { m_outgoingMessagesLock };
        m_outgoingMessages.append(WTFMove(encoder));
    }
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    auto sendOutgoingMessages = [protectedThis = Ref { *this }]() mutable {
        protectedThis->sendOutgoingMessages();
    };

    if (qos)
        m_connectionQueue->dispatchWithQOS(WTFMove(sendOutgoingMessages), *qos);
    else
        m_connectionQueue->dispatch(WTFMove(sendOutgoingMessages));

    return true;
}

bool Connection::sendSyncReply(UniqueRef<Encoder>&& encoder)
{
    return sendMessage(WTFMove(encoder), { });
}

Timeout Connection::timeoutRespectingIgnoreTimeoutsForTesting(Timeout timeout) const
{
    return m_ignoreTimeoutsForTesting ? Timeout::infinity() : timeout;
}

std::unique_ptr<Decoder> Connection::waitForMessage(MessageName messageName, uint64_t destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions)
{
    ASSERT(RunLoop::isMain());
    Ref protectedThis { *this };

    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);

    WaitForMessageState waitingForMessage(messageName, destinationID, waitForOptions);

    {
        Locker locker { m_waitForMessageLock };

        // We don't support having multiple clients waiting for messages.
        ASSERT(!m_waitingForMessage);
        if (m_waitingForMessage)
            return nullptr;

        // If the connection is already invalidated, don't even start waiting.
        // Once m_waitingForMessage is set, messageWaitingInterrupted will cover this instead.
        if (!m_shouldWaitForMessages)
            return nullptr;

        bool hasIncomingSynchronousMessage = false;

        // First, check if this message is already in the incoming messages queue.
        {
            Locker locker { m_incomingMessagesLock };
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
            // We don't support having multiple clients waiting for messages.
            ASSERT(!m_waitingForMessage);
#endif
            return nullptr;
        }

        m_waitingForMessage = &waitingForMessage;
    }

    // Now wait for it to be set.
    while (true) {
        // Handle any messages that are blocked on a response from us.
        bool wasMessageToWaitForAlreadyDispatched = false;
        SyncMessageState::singleton().dispatchMessages([&](auto nameOfMessageToDispatch, uint64_t destinationOfMessageToDispatch) {
            wasMessageToWaitForAlreadyDispatched |= messageName == nameOfMessageToDispatch && destinationID == destinationOfMessageToDispatch;
        });

        Locker locker { m_waitForMessageLock };

        if (wasMessageToWaitForAlreadyDispatched) {
            m_waitingForMessage = nullptr;
            break;
        }

        if (UNLIKELY(m_inDispatchSyncMessageCount && !timeout.isInfinity())) {
            RELEASE_LOG_ERROR(IPC, "Connection::waitForMessage(%" PUBLIC_LOG_STRING "): Exiting immediately, since we're handling a sync message already", description(messageName));
            m_waitingForMessage = nullptr;
            break;
        }

        if (m_waitingForMessage->decoder) {
            auto decoder = WTFMove(m_waitingForMessage->decoder);
            m_waitingForMessage = nullptr;
            return decoder;
        }

        // Now we wait.
        bool didTimeout = !m_waitForMessageCondition.waitUntil(m_waitForMessageLock, timeout.deadline());
        // We timed out, lost our connection, or a sync message came in with InterruptWaitingIfSyncMessageArrives, so stop waiting.
        if (didTimeout || m_waitingForMessage->messageWaitingInterrupted) {
            m_waitingForMessage = nullptr;
            break;
        }
    }

    return nullptr;
}

bool Connection::pushPendingSyncRequestID(SyncRequestID syncRequestID)
{
    {
        Locker locker { m_syncReplyStateLock };
        if (!m_shouldWaitForSyncReplies)
            return false;
        m_pendingSyncReplies.append(PendingSyncReply(syncRequestID));
    }
    ++m_inSendSyncCount;
    return true;
}

void Connection::popPendingSyncRequestID(SyncRequestID syncRequestID)
{
    --m_inSendSyncCount;
    Locker locker { m_syncReplyStateLock };
    ASSERT_UNUSED(syncRequestID, m_pendingSyncReplies.last().syncRequestID == syncRequestID);
    m_pendingSyncReplies.removeLast();
}

std::unique_ptr<Decoder> Connection::sendSyncMessage(SyncRequestID syncRequestID, UniqueRef<Encoder>&& encoder, Timeout timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    ASSERT(syncRequestID);
    ASSERT(RunLoop::isMain());

    if (!isValid()) {
        didFailToSendSyncMessage();
        return nullptr;
    }
    if (!pushPendingSyncRequestID(syncRequestID)) {
        didFailToSendSyncMessage();
        return nullptr;
    }

    // First send the message.
    OptionSet<SendOption> sendOptions = IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply;
    if (sendSyncOptions.contains(SendSyncOption::ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply))
        sendOptions = sendOptions | IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply;

    if (sendSyncOptions.contains(IPC::SendSyncOption::MaintainOrderingWithAsyncMessages))
        encoder->setShouldMaintainOrderingWithAsyncMessages();

    auto messageName = encoder->messageName();

    // Since sync IPC is blocking the current thread, make sure we use the same priority for the IPC sending thread
    // as the current thread.
    sendMessage(WTFMove(encoder), sendOptions, Thread::currentThreadQOS());

    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    Ref<Connection> protect(*this);
    std::unique_ptr<Decoder> reply = waitForSyncReply(syncRequestID, messageName, timeout, sendSyncOptions);

    popPendingSyncRequestID(syncRequestID);

    if (!reply)
        didFailToSendSyncMessage();

    return reply;
}

std::unique_ptr<Decoder> Connection::waitForSyncReply(SyncRequestID syncRequestID, MessageName messageName, Timeout timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);

    willSendSyncMessage(sendSyncOptions);
    
    bool timedOut = false;
    while (!timedOut) {
        // First, check if we have any messages that we need to process.
        SyncMessageState::singleton().dispatchMessages();
        
        {
            Locker locker { m_syncReplyStateLock };

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
            RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Connection no longer valid, id=%" PRIu64, syncRequestID.toUInt64());
            didReceiveSyncReply(sendSyncOptions);
            return nullptr;
        }

        // We didn't find a sync reply yet, keep waiting.
        // This allows the WebProcess to still serve clients while waiting for the message to return.
        // Notably, it can continue to process accessibility requests, which are on the main thread.
        timedOut = !SyncMessageState::singleton().wait(timeout);
    }

#if OS(DARWIN)
    RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Timed-out while waiting for reply for %" PUBLIC_LOG_STRING " from process %d, id=%" PRIu64, description(messageName), remoteProcessID(), syncRequestID.toUInt64());
#else
    RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Timed-out while waiting for reply for %s, id=%" PRIu64, description(messageName), syncRequestID.toUInt64());
#endif

    didReceiveSyncReply(sendSyncOptions);

    return nullptr;
}

void Connection::processIncomingSyncReply(std::unique_ptr<Decoder> decoder)
{
    {
        Locker locker { m_syncReplyStateLock };

        // Go through the stack of sync requests that have pending replies and see which one
        // this reply is for.
        for (size_t i = m_pendingSyncReplies.size(); i > 0; --i) {
            PendingSyncReply& pendingSyncReply = m_pendingSyncReplies[i - 1];

            if (pendingSyncReply.syncRequestID.toUInt64() != decoder->destinationID())
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

static NEVER_INLINE NO_RETURN_DUE_TO_CRASH void terminateDueToIPCTerminateMessage()
{
#if PLATFORM(COCOA)
    WebKit::logAndSetCrashLogMessage("Receives Terminate message");
#else
    WTFLogAlways("Receives Terminate message");
#endif
    CRASH();
}

void Connection::processIncomingMessage(std::unique_ptr<Decoder> message)
{
    ASSERT(message->messageReceiverName() != ReceiverName::Invalid);

    if (message->messageName() == MessageName::SyncMessageReply) {
        processIncomingSyncReply(WTFMove(message));
        return;
    }

    if (message->messageName() == MessageName::Terminate)
        return terminateDueToIPCTerminateMessage();

    if (!MessageReceiveQueueMap::isValidMessage(*message)) {
        RunLoop::main().dispatch([protectedThis = Ref { *this }, messageName = message->messageName()]() mutable {
            protectedThis->dispatchDidReceiveInvalidMessage(messageName);
        });
        return;
    }

    // FIXME: These are practically the same mutex, so maybe they could be merged.
    Locker waitForMessagesLocker { m_waitForMessageLock };

    Locker incomingMessagesLocker { m_incomingMessagesLock };
    if (auto* receiveQueue = m_receiveQueues.get(*message)) {
        receiveQueue->enqueueMessage(*this, WTFMove(message));
        return;
    }

    if (message->isSyncMessage()) {
        Locker locker { m_incomingSyncMessageCallbackLock };

        for (auto& callback : m_incomingSyncMessageCallbacks.values())
            m_incomingSyncMessageCallbackQueue->dispatch(WTFMove(callback));

        m_incomingSyncMessageCallbacks.clear();
    }

    // Check if we're waiting for this message, or if we need to interrupt waiting due to an incoming sync message.
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

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (SyncMessageState::singleton().processIncomingMessage(*this, message))
        return;

    enqueueIncomingMessage(WTFMove(message));
}

uint64_t Connection::installIncomingSyncMessageCallback(WTF::Function<void ()>&& callback)
{
    Locker locker { m_incomingSyncMessageCallbackLock };

    m_nextIncomingSyncMessageCallbackID++;

    if (!m_incomingSyncMessageCallbackQueue)
        m_incomingSyncMessageCallbackQueue = WorkQueue::create("com.apple.WebKit.IPC.IncomingSyncMessageCallbackQueue");

    m_incomingSyncMessageCallbacks.add(m_nextIncomingSyncMessageCallbackID, WTFMove(callback));

    return m_nextIncomingSyncMessageCallbackID;
}

void Connection::uninstallIncomingSyncMessageCallback(uint64_t callbackID)
{
    Locker locker { m_incomingSyncMessageCallbackLock };
    m_incomingSyncMessageCallbacks.remove(callbackID);
}

bool Connection::hasIncomingSyncMessage()
{
    Locker locker { m_incomingMessagesLock };

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

#if ENABLE(IPC_TESTING_API)
void Connection::addMessageObserver(const MessageObserver& observer)
{
    m_messageObservers.append(observer);
}

void Connection::dispatchIncomingMessageForTesting(std::unique_ptr<Decoder>&& decoder)
{
    m_connectionQueue->dispatch([protectedThis = Ref { *this }, decoder = WTFMove(decoder)]() mutable {
        protectedThis->processIncomingMessage(WTFMove(decoder));
    });
}
#endif

void Connection::postConnectionDidCloseOnConnectionWorkQueue()
{
    m_connectionQueue->dispatch([protectedThis = Ref { *this }]() mutable {
        protectedThis->connectionDidClose();
    });
}

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    m_isValid = false;
    platformInvalidate();

    {
        Locker locker { m_syncReplyStateLock };

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        if (!m_pendingSyncReplies.isEmpty())
            SyncMessageState::singleton().wakeUpClientRunLoop();
    }

    {
        Locker locker { m_waitForMessageLock };

        ASSERT(m_shouldWaitForMessages);
        m_shouldWaitForMessages = false;

        if (m_waitingForMessage)
            m_waitingForMessage->messageWaitingInterrupted = true;
    }
    m_waitForMessageCondition.notifyAll();

    if (m_didCloseOnConnectionWorkQueueCallback)
        m_didCloseOnConnectionWorkQueueCallback(this);

    RunLoop::main().dispatch([protectedThis = Ref { *this }]() mutable {
        // If the connection has been explicitly invalidated before dispatchConnectionDidClose was called,
        // then the connection client will be nullptr here.
        if (!protectedThis->m_client)
            return;
        auto client = std::exchange(protectedThis->m_client, nullptr);
        client->didClose(protectedThis.get());
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
            Locker locker { m_outgoingMessagesLock };
            if (m_outgoingMessages.isEmpty())
                break;
            message = m_outgoingMessages.takeFirst().moveToUniquePtr();
        }
        ASSERT(message);

        if (!sendOutgoingMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(message))))
            break;
    }
}

void Connection::dispatchSyncMessage(Decoder& decoder)
{
    // FIXME: If the message is invalid, we should send back a SyncMessageError.
    // Currently we just wait for a timeout to happen, which will block the WebContent process.

    ASSERT(isMainRunLoop());
    ASSERT(decoder.isSyncMessage());

    SyncRequestID syncRequestID;
    if (UNLIKELY(!decoder.decode(syncRequestID))) {
        // We received an invalid sync message.
        return;
    }

    ++m_inDispatchSyncMessageCount;
    auto decrementSyncMessageCount = makeScopeExit([&] {
        ASSERT(m_inDispatchSyncMessageCount);
        --m_inDispatchSyncMessageCount;
    });

    auto replyEncoder = makeUniqueRef<Encoder>(MessageName::SyncMessageReply, syncRequestID.toUInt64());

    bool wasHandled = false;
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
        wasHandled = m_client->didReceiveSyncMessage(*this, decoder, replyEncoder);
    }

#if ENABLE(IPC_TESTING_API)
    ASSERT(decoder.isValid() || m_ignoreInvalidMessageForTesting);
#else
    ASSERT(decoder.isValid());
#endif

    if (!wasHandled)
        sendSyncReply(WTFMove(replyEncoder));
}

void Connection::dispatchDidReceiveInvalidMessage(MessageName messageName)
{
    ensureOnMainRunLoop([this, protectedThis = Ref { *this }, messageName]() mutable {
        if (!isValid())
            return;
        m_client->didReceiveInvalidMessage(*this, messageName);
    });
}

void Connection::didFailToSendSyncMessage()
{
    if (!m_shouldExitOnSyncMessageSendFailure)
        return;

    exit(0);
}

void Connection::enqueueIncomingMessage(std::unique_ptr<Decoder> incomingMessage)
{
    ASSERT(m_incomingMessagesLock.isHeld());
    {
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

    RunLoop::main().dispatch([protectedThis = Ref { *this }]() mutable {
        if (protectedThis->isIncomingMessagesThrottlingEnabled())
            protectedThis->dispatchIncomingMessages();
        else
            protectedThis->dispatchOneIncomingMessage();
    });
}

void Connection::dispatchMessage(Decoder& decoder)
{
    ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_client);
    if (decoder.messageReceiverName() == ReceiverName::AsyncReply) {
        auto handler = takeAsyncReplyHandler(*this, decoder.destinationID());
        if (!handler) {
            markCurrentlyDispatchedMessageAsInvalid();
#if ENABLE(IPC_TESTING_API)
            if (m_ignoreInvalidMessageForTesting)
                return;
#endif
            ASSERT_NOT_REACHED();
            return;
        }
        handler(&decoder);
        return;
    }

#if ENABLE(IPC_TESTING_API)
    if (isMainRunLoop()) {
        bool hasDeadObservers = false;
        for (auto& observerWeakPtr : m_messageObservers) {
            if (auto* observer = observerWeakPtr.get())
                observer->didReceiveMessage(decoder);
            else
                hasDeadObservers = true;
        }
        if (hasDeadObservers)
            m_messageObservers.removeAllMatching([](auto& observer) { return !observer; });
    }
#endif

    m_client->didReceiveMessage(*this, decoder);
}

void Connection::dispatchMessage(std::unique_ptr<Decoder> message)
{
    ASSERT(RunLoop::isMain());
    if (!isValid())
        return;

    {
        // FIXME: The matches here come from
        // m_messagesToDispatchWhileWaitingForSyncReply. This causes message
        // reordering, because some of the messages go to
        // SyncState::m_messagesToDispatchWhileWaitingForSyncReply while others
        // go to Connection::m_incomingMessages. Should be fixed by adding all
        // messages to one list.
        Locker incomingMessagesLocker { m_incomingMessagesLock };
        if (auto* receiveQueue = m_receiveQueues.get(*message)) {
            receiveQueue->enqueueMessage(*this, WTFMove(message));
            return;
        }
    }

    if (message->shouldUseFullySynchronousModeForTesting()) {
        if (!m_fullySynchronousModeIsAllowedForTesting) {
            m_client->didReceiveInvalidMessage(*this, message->messageName());
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
        m_client->didReceiveInvalidMessage(*this, message->messageName());

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
    RunLoop::main().dispatch([this, protectedConnection = RefPtr { &m_connection }]() mutable {
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
        Locker locker { m_incomingMessagesLock };
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
        Locker locker { m_incomingMessagesLock };
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
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: first IPC message in queue is %" PUBLIC_LOG_STRING, this, description(message->messageName()));
        }

        // Re-schedule ourselves *before* we dispatch the messages because we want to process follow-up messages if the client
        // spins a nested run loop while we're dispatching a message. Note that this means we can re-enter this method.
        if (!m_incomingMessages.isEmpty())
            m_incomingMessagesThrottler->scheduleMessagesDispatch();
    }

    dispatchMessage(WTFMove(message));

    for (size_t i = 1; i < messagesToProcess; ++i) {
        {
            Locker locker { m_incomingMessagesLock };
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
    Locker locker { asyncReplyHandlerMapLock };
    auto result = asyncReplyHandlerMap().ensure(reinterpret_cast<uintptr_t>(&connection), [] {
        return HashMap<uint64_t, CompletionHandler<void(Decoder*)>>();
    }).iterator->value.add(identifier, WTFMove(completionHandler));
    ASSERT_UNUSED(result, result.isNewEntry);
}

void clearAsyncReplyHandlers(const Connection& connection)
{
    HashMap<uint64_t, CompletionHandler<void(Decoder*)>> map;
    {
        Locker locker { asyncReplyHandlerMapLock };
        map = asyncReplyHandlerMap().take(reinterpret_cast<uintptr_t>(&connection));
    }

    for (auto& handler : map.values()) {
        if (handler)
            handler(nullptr);
    }
}

CompletionHandler<void(Decoder*)> takeAsyncReplyHandler(Connection& connection, uint64_t identifier)
{
    Locker locker { asyncReplyHandlerMapLock };
    auto& map = asyncReplyHandlerMap();
    auto iterator = map.find(reinterpret_cast<uintptr_t>(&connection));
    if (iterator == map.end())
        return nullptr;
    if (!iterator->value.isValidKey(identifier))
        return nullptr;
    return iterator->value.take(identifier);
}

void Connection::wakeUpRunLoop()
{
    RunLoop::main().wakeUp();
}

#if !USE(UNIX_DOMAIN_SOCKETS) && !OS(DARWIN) && !OS(WINDOWS)
std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    notImplemented();
    return std::nullopt;
}
#endif

} // namespace IPC
