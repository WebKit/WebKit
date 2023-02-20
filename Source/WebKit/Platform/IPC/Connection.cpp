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
    WaitForMessageState(MessageName messageName, UInt128 destinationID, OptionSet<WaitForOption> waitForOptions)
        : messageName(messageName)
        , destinationID(destinationID)
        , waitForOptions(waitForOptions)
    {
    }

    MessageName messageName;
    UInt128 destinationID;
    OptionSet<WaitForOption> waitForOptions;
    bool messageWaitingInterrupted = false;
    std::unique_ptr<Decoder> decoder;
};

class Connection::SyncMessageState {
public:
    static std::unique_ptr<SyncMessageState, SyncMessageStateRelease> get(SerialFunctionDispatcher&);
    SerialFunctionDispatcher& dispatcher() { return m_dispatcher; }

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
    void dispatchMessages(Function<void(MessageName, UInt128)>&& willDispatchMessage = { });

    // Add matching pending messages to the provided MessageReceiveQueue.
    void enqueueMatchingMessages(Connection&, MessageReceiveQueue&, const ReceiverMatcher&);

    // Dispatch pending sync messages for given connection.
    void dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(Connection&);

private:
    explicit SyncMessageState(SerialFunctionDispatcher& dispatcher)
        : m_dispatcher(dispatcher)
    {
    }
    static Lock syncMessageStateMapLock;
    static HashMap<SerialFunctionDispatcher*, SyncMessageState*>& syncMessageStateMap() WTF_REQUIRES_LOCK(syncMessageStateMapLock)
    {
        static NeverDestroyed<HashMap<SerialFunctionDispatcher*, SyncMessageState*>> map;
        return map;
    }

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

    SerialFunctionDispatcher& m_dispatcher;
    unsigned m_clients WTF_GUARDED_BY_LOCK(syncMessageStateMapLock) { 0 };
    friend struct Connection::SyncMessageStateRelease;
};

Lock Connection::SyncMessageState::syncMessageStateMapLock;

std::unique_ptr<Connection::SyncMessageState, Connection::SyncMessageStateRelease> Connection::SyncMessageState::get(SerialFunctionDispatcher& dispatcher)
{
    Locker locker { syncMessageStateMapLock };
    auto result = syncMessageStateMap().ensure(&dispatcher, [&dispatcher] { return new SyncMessageState { dispatcher }; }); // NOLINT.
    auto* state = result.iterator->value;
    state->m_clients++;
    return { state, Connection::SyncMessageStateRelease { } };
}

void Connection::SyncMessageStateRelease::operator()(SyncMessageState* instance) const
{
    if (!instance)
        return;
    {
        Locker locker { Connection::SyncMessageState::syncMessageStateMapLock };
        --instance->m_clients;
        if (instance->m_clients)
            return;
        Connection::SyncMessageState::syncMessageStateMap().remove(&instance->m_dispatcher);
    }
    delete instance;
}

void Connection::SyncMessageState::enqueueMatchingMessages(Connection& connection, MessageReceiveQueue& receiveQueue, const ReceiverMatcher& receiverMatcher)
{
    assertIsCurrent(m_dispatcher);
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
        m_dispatcher.dispatch([protectedConnection = Ref { connection }]() mutable {
            protectedConnection->dispatchSyncStateMessages();
        });
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages(Function<void(MessageName, UInt128)>&& willDispatchMessage)
{
    assertIsCurrent(m_dispatcher);
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
    assertIsCurrent(m_dispatcher);
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

Connection::Connection(Identifier identifier, bool isServer)
    : m_uniqueID(UniqueID::generate())
    , m_isServer(isServer)
    , m_connectionQueue(WorkQueue::create("com.apple.IPC.ReceiveQueue"))
{
    {
        Locker locker { s_connectionMapLock };
        connectionMap().add(m_uniqueID, this);
    }

    platformInitialize(identifier);
}

Connection::~Connection()
{
    ASSERT(!isValid());

    {
        Locker locker { s_connectionMapLock };
        connectionMap().remove(m_uniqueID);
    }

    cancelAsyncReplyHandlers();
}

RefPtr<Connection> Connection::connection(UniqueID uniqueID)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=238493): Removing with lock in destructor is not thread-safe.
    Locker locker { s_connectionMapLock };
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
    if (!isValid())
        return;
    // FIXME: m_isValid starts as true. It will be switched to start as false and toggled as true on
    // open. For the time being, check for m_syncState.
    if (m_syncState)
        m_syncState->enqueueMatchingMessages(*this, receiveQueue, receiverMatcher);

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

void Connection::addWorkQueueMessageReceiver(ReceiverName receiverName, WorkQueue& workQueue, WorkQueueMessageReceiver& receiver, UInt128 destinationID)
{
    auto receiverMatcher = ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID);

    auto receiveQueue = makeUnique<WorkQueueMessageReceiverQueue>(workQueue, receiver);
    Locker incomingMessagesLocker { m_incomingMessagesLock };
    enqueueMatchingMessagesToMessageReceiveQueue(*receiveQueue, receiverMatcher);
    m_receiveQueues.add(WTFMove(receiveQueue), receiverMatcher);
}

void Connection::removeWorkQueueMessageReceiver(ReceiverName receiverName, UInt128 destinationID)
{
    removeMessageReceiveQueue(ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID));
}

void Connection::addMessageReceiver(FunctionDispatcher& dispatcher, MessageReceiver& receiver, ReceiverName receiverName, UInt128 destinationID)
{
    auto receiverMatcher = ReceiverMatcher::createWithZeroAsAnyDestination(receiverName, destinationID);
    auto receiveQueue = makeUnique<FunctionDispatcherQueue>(dispatcher, receiver);
    Locker incomingMessagesLocker { m_incomingMessagesLock };
    enqueueMatchingMessagesToMessageReceiveQueue(*receiveQueue, receiverMatcher);
    m_receiveQueues.add(WTFMove(receiveQueue), receiverMatcher);
}

void Connection::removeMessageReceiver(ReceiverName receiverName, UInt128 destinationID)
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

bool Connection::open(Client& client, SerialFunctionDispatcher& dispatcher)
{
    ASSERT(!m_client);
    if (!platformPrepareForOpen())
        return false;
    m_client = &client;
    m_syncState = SyncMessageState::get(dispatcher);
    platformOpen();

    return true;
}

#if !USE(UNIX_DOMAIN_SOCKETS)
bool Connection::platformPrepareForOpen()
{
    return true;
}
#endif

void Connection::invalidate()
{
    m_isValid = false;
    if (!m_client)
        return;
    assertIsCurrent(dispatcher());
    m_client = nullptr;
    [this] {
        Locker locker { m_incomingMessagesLock };
        return WTFMove(m_syncState);
    }();

    cancelAsyncReplyHandlers();

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

UniqueRef<Encoder> Connection::createSyncMessageEncoder(MessageName messageName, UInt128 destinationID, SyncRequestID& syncRequestID)
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

#if ENABLE(IPC_TESTING_API)
    if (!sendOptions.contains(SendOption::IPCTestingMessage)) {
#endif
        if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForSyncReply))
            ASSERT(encoder->isAllowedWhenWaitingForSyncReply());
        else if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply))
            ASSERT(encoder->isAllowedWhenWaitingForUnboundedSyncReply());
        else
            ASSERT(!encoder->isAllowedWhenWaitingForSyncReply() && !encoder->isAllowedWhenWaitingForUnboundedSyncReply());
#if ENABLE(IPC_TESTING_API)
    }
#endif

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

bool Connection::sendMessageWithAsyncReply(UniqueRef<Encoder>&& encoder, AsyncReplyHandler replyHandler, OptionSet<SendOption> sendOptions, std::optional<Thread::QOS> qos)
{
    ASSERT(replyHandler.replyID);
    ASSERT(replyHandler.completionHandler);
    auto replyID = replyHandler.replyID;
    encoder.get() << replyID;
    addAsyncReplyHandler(WTFMove(replyHandler));
    if (sendMessage(WTFMove(encoder), sendOptions, qos))
        return true;
    // replyHandlerToCancel might be already cancelled if invalidate() happened in-between.
    if (auto replyHandlerToCancel = takeAsyncReplyHandler(replyID)) {
        // FIXME: Current contract is that completionHandler is called on the connection run loop.
        // This does not make sense. However, this needs a change that is done later.
        RunLoop::main().dispatch([completionHandler = WTFMove(replyHandlerToCancel)]() mutable {
            completionHandler(nullptr);
        });
    }
    return false;
}

bool Connection::sendSyncReply(UniqueRef<Encoder>&& encoder)
{
    return sendMessage(WTFMove(encoder), { });
}

Timeout Connection::timeoutRespectingIgnoreTimeoutsForTesting(Timeout timeout) const
{
    return m_ignoreTimeoutsForTesting ? Timeout::infinity() : timeout;
}

std::unique_ptr<Decoder> Connection::waitForMessage(MessageName messageName, UInt128 destinationID, Timeout timeout, OptionSet<WaitForOption> waitForOptions)
{
    if (!isValid())
        return nullptr;
    assertIsCurrent(dispatcher());
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
        m_syncState->dispatchMessages([&](auto nameOfMessageToDispatch, UInt128 destinationOfMessageToDispatch) {
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
    if (!isValid()) {
        didFailToSendSyncMessage();
        return nullptr;
    }
    assertIsCurrent(dispatcher());
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
        m_syncState->dispatchMessages();

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
        timedOut = !m_syncState->wait(timeout);
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
            if (i == m_pendingSyncReplies.size()) {
                Locker locker { m_incomingMessagesLock };
                if (m_syncState)
                    m_syncState->wakeUpClientRunLoop();
            }
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
        dispatchDidReceiveInvalidMessage(message->messageName());
        return;
    }

    // FIXME: These are practically the same mutex, so maybe they could be merged.
    Locker waitForMessagesLocker { m_waitForMessageLock };

    Locker incomingMessagesLocker { m_incomingMessagesLock };
    if (!m_syncState)
        return;

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

        if (m_waitingForMessage->waitForOptions.contains(WaitForOption::DispatchIncomingSyncMessagesWhileWaiting) && message->isSyncMessage() && m_syncState->processIncomingMessage(*this, message)) {
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

    if ((message->shouldDispatchMessageWhenWaitingForSyncReply() == ShouldDispatchWhenWaitingForSyncReply::YesDuringUnboundedIPC && !message->isAllowedWhenWaitingForUnboundedSyncReply()) || (message->shouldDispatchMessageWhenWaitingForSyncReply() == ShouldDispatchWhenWaitingForSyncReply::Yes && !message->isAllowedWhenWaitingForSyncReply())) {
        dispatchDidReceiveInvalidMessage(message->messageName());
        return;
    }

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (m_syncState->processIncomingMessage(*this, message))
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
    m_incomingMessagesThrottlingLevel = 0;
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

void Connection::connectionDidClose()
{
    // The connection is now invalid.
    m_isValid = false;
    platformInvalidate();

    bool hasPendingWaiters = false;
    {
        Locker locker { m_syncReplyStateLock };

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        hasPendingWaiters = !m_pendingSyncReplies.isEmpty();
    }

    if (hasPendingWaiters) {
        Locker locker { m_incomingMessagesLock };
        if (m_syncState)
            m_syncState->wakeUpClientRunLoop();
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

    dispatchDidCloseAndInvalidate();
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

    assertIsCurrent(dispatcher());
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
        m_syncState->dispatchMessages();
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
    dispatchToClient([protectedThis = Ref { *this }, messageName] {
        if (!protectedThis->isValid())
            return;
        protectedThis->m_client->didReceiveInvalidMessage(protectedThis, messageName);
    });
}

void Connection::dispatchDidCloseAndInvalidate()
{
    dispatchToClient([protectedThis = Ref { *this }] {
        // If the connection has been explicitly invalidated before dispatchConnectionDidClose was called,
        // then the connection client will be nullptr here.
        if (!protectedThis->m_client)
            return;
        protectedThis->m_client->didClose(protectedThis);
        protectedThis->invalidate();
    });
}

size_t Connection::pendingMessageCountForTesting() const
{
    // Note: current testing does not need to inspect the sync message state.
    Locker lock { m_incomingMessagesLock };
    return m_incomingMessages.size();
}

void Connection::dispatchOnReceiveQueueForTesting(Function<void()>&& completionHandler)
{
    m_connectionQueue->dispatch(WTFMove(completionHandler));
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

    if (!m_syncState)
        return;
    if (isIncomingMessagesThrottlingEnabled()) {
        dispatcher().dispatch([protectedThis = Ref { *this }] {
            protectedThis->dispatchIncomingMessages();
        });
    } else {
        dispatcher().dispatch([protectedThis = Ref { *this }] {
            protectedThis->dispatchOneIncomingMessage();
        });
    }
}

void Connection::dispatchMessage(Decoder& decoder)
{
    assertIsCurrent(dispatcher());
    RELEASE_ASSERT(m_client);
    if (decoder.messageReceiverName() == ReceiverName::AsyncReply) {
        auto handler = takeAsyncReplyHandler(makeObjectIdentifier<AsyncReplyIDType>(decoder.destinationID()));
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
    if (!isValid())
        return;
    assertIsCurrent(dispatcher());
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
#if ENABLE(IPC_TESTING_API)
            if (m_ignoreInvalidMessageForTesting)
                return;
#endif
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

    if (m_didReceiveInvalidMessage
#if ENABLE(IPC_TESTING_API)
        && !m_ignoreInvalidMessageForTesting
#endif
        && isValid())
        m_client->didReceiveInvalidMessage(*this, message->messageName());

    m_didReceiveInvalidMessage = oldDidReceiveInvalidMessage;
}

size_t Connection::numberOfMessagesToProcess(size_t totalMessages)
{
    // Never dispatch more than 600 messages without returning to the run loop, we can go as low as 60 with maximum throttling level.
    static const size_t maxIncomingMessagesDispatchingBatchSize { 600 };
    static const uint8_t maxThrottlingLevel = 9;

    size_t batchSize = maxIncomingMessagesDispatchingBatchSize / (*m_incomingMessagesThrottlingLevel + 1);

    if (totalMessages > maxIncomingMessagesDispatchingBatchSize)
        m_incomingMessagesThrottlingLevel = std::min<uint8_t>(*m_incomingMessagesThrottlingLevel + 1u, maxThrottlingLevel);
    else if (*m_incomingMessagesThrottlingLevel)
        --*m_incomingMessagesThrottlingLevel;

    return std::min(totalMessages, batchSize);
}

SerialFunctionDispatcher& Connection::dispatcher()
{
    // dispatcher can only be accessed while the connection is valid,
    // and must have the incoming message lock held if not being
    // called from the SerialFunctionDispatcher.
    RELEASE_ASSERT(m_syncState);
    if (!m_incomingMessagesLock.isLocked())
        assertIsCurrent(m_syncState->dispatcher());

    // Our syncState is specific to the SerialFunctionDispatcher we have been
    // bound to during open(), so we can retrieve the SerialFunctionDispatcher
    // from it (rather than storing another pointer on this class).
    return m_syncState->dispatcher();
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

void Connection::dispatchSyncStateMessages()
{
    if (m_syncState) {
        assertIsCurrent(dispatcher());
        m_syncState->dispatchMessagesAndResetDidScheduleDispatchMessagesForConnection(*this);
    }
}

void Connection::dispatchIncomingMessages()
{
    if (!isValid())
        return;

    std::unique_ptr<Decoder> message;

    size_t messagesToProcess = 0;
    {
        Locker locker { m_incomingMessagesLock };
        if (m_incomingMessages.isEmpty())
            return;

        message = m_incomingMessages.takeFirst();

        // Incoming messages may get adding to the queue by the IPC thread while we're dispatching the messages below.
        // To make sure dispatchIncomingMessages() yields, we only ever process messages that were in the queue when
        // dispatchIncomingMessages() was called. Additionally, the message throttling may further cap the number of
        // messages to process to make sure we give the main run loop a chance to process other events.
        messagesToProcess = numberOfMessagesToProcess(m_incomingMessages.size());
        if (messagesToProcess < m_incomingMessages.size()) {
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: IPC throttling was triggered (has %zu pending incoming messages, will only process %zu before yielding)", this, m_incomingMessages.size(), messagesToProcess);
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: first IPC message in queue is %" PUBLIC_LOG_STRING, this, description(message->messageName()));
        }

        // Re-schedule ourselves *before* we dispatch the messages because we want to process follow-up messages if the client
        // spins a nested run loop while we're dispatching a message. Note that this means we can re-enter this method.
        if (!m_incomingMessages.isEmpty()) {
            dispatcher().dispatch([protectedThis = Ref { *this }] {
                protectedThis->dispatchIncomingMessages();
            });
        }
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

void Connection::addAsyncReplyHandler(AsyncReplyHandler&& handler)
{
    Locker locker { m_incomingMessagesLock };
    auto result = m_asyncReplyHandlers.add(handler.replyID, WTFMove(handler.completionHandler));
    ASSERT_UNUSED(result, result.isNewEntry);
}

void Connection::cancelAsyncReplyHandlers()
{
    AsyncReplyHandlerMap map;
    {
        Locker locker { m_incomingMessagesLock };
        map.swap(m_asyncReplyHandlers);
    }

    for (auto& handler : map.values()) {
        if (handler)
            handler(nullptr);
    }
}

CompletionHandler<void(Decoder*)> Connection::takeAsyncReplyHandler(AsyncReplyID replyID)
{
    Locker locker { m_incomingMessagesLock };
    if (!m_asyncReplyHandlers.isValidKey(replyID))
        return nullptr;
    return m_asyncReplyHandlers.take(replyID);
}

void Connection::wakeUpRunLoop()
{
    if (!isValid())
        return;
    if (&dispatcher() == &RunLoop::main())
        RunLoop::main().wakeUp();
}

template<typename F>
void Connection::dispatchToClient(F&& clientRunLoopTask)
{
    Locker lock { m_incomingMessagesLock };
    if (!m_syncState)
        return;
    dispatcher().dispatch(WTFMove(clientRunLoopTask));
}

#if !USE(UNIX_DOMAIN_SOCKETS) && !OS(DARWIN) && !OS(WINDOWS)
std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    notImplemented();
    return std::nullopt;
}
#endif

} // namespace IPC
