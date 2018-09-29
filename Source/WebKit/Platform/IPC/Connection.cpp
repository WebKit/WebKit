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
#include <memory>
#include <wtf/HashSet.h>
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

struct Connection::ReplyHandler {
    RefPtr<FunctionDispatcher> dispatcher;
    Function<void (std::unique_ptr<Decoder>)> handler;
};

struct Connection::WaitForMessageState {
    WaitForMessageState(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, OptionSet<WaitForOption> waitForOptions)
        : messageReceiverName(messageReceiverName)
        , messageName(messageName)
        , destinationID(destinationID)
        , waitForOptions(waitForOptions)
    {
    }

    StringReference messageReceiverName;
    StringReference messageName;
    uint64_t destinationID;

    OptionSet<WaitForOption> waitForOptions;
    bool messageWaitingInterrupted = false;

    std::unique_ptr<Decoder> decoder;
};

class Connection::SyncMessageState {
public:
    static SyncMessageState& singleton();

    SyncMessageState();
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

    // Dispatch pending sync messages. if allowedConnection is not null, will only dispatch messages
    // from that connection and put the other messages back in the queue.
    void dispatchMessages(Connection* allowedConnection);

private:
    void dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection&);

    BinarySemaphore m_waitForSyncReplySemaphore;

    // Protects m_didScheduleDispatchMessagesWorkSet and m_messagesToDispatchWhileWaitingForSyncReply.
    Lock m_mutex;

    // The set of connections for which we've scheduled a call to dispatchMessageAndResetDidScheduleDispatchMessagesForConnection.
    HashSet<RefPtr<Connection>> m_didScheduleDispatchMessagesWorkSet;

    struct ConnectionAndIncomingMessage {
        Ref<Connection> connection;
        std::unique_ptr<Decoder> message;
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

Connection::SyncMessageState::SyncMessageState()
{
}

bool Connection::SyncMessageState::processIncomingMessage(Connection& connection, std::unique_ptr<Decoder>& message)
{
    if (!message->shouldDispatchMessageWhenWaitingForSyncReply())
        return false;

    ConnectionAndIncomingMessage connectionAndIncomingMessage { connection, WTFMove(message) };

    {
        std::lock_guard<Lock> lock(m_mutex);
        
        if (m_didScheduleDispatchMessagesWorkSet.add(&connection).isNewEntry) {
            RunLoop::main().dispatch([this, protectedConnection = Ref<Connection>(connection)]() mutable {
                dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(protectedConnection);
            });
        }

        m_messagesToDispatchWhileWaitingForSyncReply.append(WTFMove(connectionAndIncomingMessage));
    }

    wakeUpClientRunLoop();

    return true;
}

void Connection::SyncMessageState::dispatchMessages(Connection* allowedConnection)
{
    ASSERT(RunLoop::isMain());

    Vector<ConnectionAndIncomingMessage> messagesToDispatchWhileWaitingForSyncReply;

    {
        std::lock_guard<Lock> lock(m_mutex);
        m_messagesToDispatchWhileWaitingForSyncReply.swap(messagesToDispatchWhileWaitingForSyncReply);
    }

    Vector<ConnectionAndIncomingMessage> messagesToPutBack;

    for (size_t i = 0; i < messagesToDispatchWhileWaitingForSyncReply.size(); ++i) {
        ConnectionAndIncomingMessage& connectionAndIncomingMessage = messagesToDispatchWhileWaitingForSyncReply[i];

        if (allowedConnection && allowedConnection != connectionAndIncomingMessage.connection.ptr()) {
            // This incoming message belongs to another connection and we don't want to dispatch it now
            // so mark it to be put back in the message queue.
            messagesToPutBack.append(WTFMove(connectionAndIncomingMessage));
            continue;
        }

        connectionAndIncomingMessage.connection->dispatchMessage(WTFMove(connectionAndIncomingMessage.message));
    }

    if (!messagesToPutBack.isEmpty()) {
        std::lock_guard<Lock> lock(m_mutex);

        for (auto& message : messagesToPutBack)
            m_messagesToDispatchWhileWaitingForSyncReply.append(WTFMove(message));
    }
}

void Connection::SyncMessageState::dispatchMessageAndResetDidScheduleDispatchMessagesForConnection(Connection& connection)
{
    {
        std::lock_guard<Lock> lock(m_mutex);
        ASSERT(m_didScheduleDispatchMessagesWorkSet.contains(&connection));
        m_didScheduleDispatchMessagesWorkSet.remove(&connection);
    }

    dispatchMessages(&connection);
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

Connection::Connection(Identifier identifier, bool isServer, Client& client)
    : m_client(client)
    , m_uniqueID(generateObjectIdentifier<UniqueIDType>())
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
    , m_waitingForMessage(nullptr)
    , m_shouldWaitForSyncReplies(true)
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

void Connection::addWorkQueueMessageReceiver(StringReference messageReceiverName, WorkQueue& workQueue, WorkQueueMessageReceiver* workQueueMessageReceiver)
{
    ASSERT(RunLoop::isMain());

    m_connectionQueue->dispatch([protectedThis = makeRef(*this), messageReceiverName = WTFMove(messageReceiverName), workQueue = &workQueue, workQueueMessageReceiver]() mutable {
        ASSERT(!protectedThis->m_workQueueMessageReceivers.contains(messageReceiverName));

        protectedThis->m_workQueueMessageReceivers.add(messageReceiverName, std::make_pair(workQueue, workQueueMessageReceiver));
    });
}

void Connection::removeWorkQueueMessageReceiver(StringReference messageReceiverName)
{
    ASSERT(RunLoop::isMain());

    m_connectionQueue->dispatch([protectedThis = makeRef(*this), messageReceiverName = WTFMove(messageReceiverName)]() mutable {
        ASSERT(protectedThis->m_workQueueMessageReceivers.contains(messageReceiverName));
        protectedThis->m_workQueueMessageReceivers.remove(messageReceiverName);
    });
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

    auto replyEncoder = std::make_unique<Encoder>("IPC", "SyncMessageReply", syncRequestID);

    // Hand off both the decoder and encoder to the work queue message receiver.
    workQueueMessageReceiver.didReceiveSyncMessage(*this, decoder, replyEncoder);

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder.isInvalid());

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

    {
        std::lock_guard<Lock> lock(m_replyHandlersLock);
        for (auto& replyHandler : m_replyHandlers.values()) {
            replyHandler.dispatcher->dispatch([handler = WTFMove(replyHandler.handler)] {
                handler(nullptr);
            });
        }

        m_replyHandlers.clear();
    }

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

std::unique_ptr<Encoder> Connection::createSyncMessageEncoder(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, uint64_t& syncRequestID)
{
    auto encoder = std::make_unique<Encoder>(messageReceiverName, messageName, destinationID);
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

    if (isMainThread() && m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting && !encoder->isSyncMessage() && !(encoder->messageReceiverName() == "IPC") && !sendOptions.contains(SendOption::IgnoreFullySynchronousMode)) {
        uint64_t syncRequestID;
        auto wrappedMessage = createSyncMessageEncoder("IPC", "WrappedAsyncMessageForTesting", encoder->destinationID(), syncRequestID);
        wrappedMessage->setFullySynchronousModeForTesting();
        wrappedMessage->wrapForTesting(WTFMove(encoder));
        return static_cast<bool>(sendSyncMessage(syncRequestID, WTFMove(wrappedMessage), Seconds::infinity(), { }));
    }

    if (sendOptions.contains(SendOption::DispatchMessageEvenWhenWaitingForSyncReply)
        && (!m_onlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage
            || m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount))
        encoder->setShouldDispatchMessageWhenWaitingForSyncReply(true);

    {
        std::lock_guard<Lock> lock(m_outgoingMessagesMutex);
        m_outgoingMessages.append(WTFMove(encoder));
    }
    
    // FIXME: We should add a boolean flag so we don't call this when work has already been scheduled.
    m_connectionQueue->dispatch([protectedThis = makeRef(*this)]() mutable {
        protectedThis->sendOutgoingMessages();
    });
    return true;
}

void Connection::sendMessageWithReply(uint64_t requestID, std::unique_ptr<Encoder> encoder, FunctionDispatcher& replyDispatcher, Function<void (std::unique_ptr<Decoder>)>&& replyHandler)
{
    {
        std::lock_guard<Lock> lock(m_replyHandlersLock);

        if (!isValid()) {
            replyDispatcher.dispatch([replyHandler = WTFMove(replyHandler)] {
                replyHandler(nullptr);
            });
            return;
        }

        ASSERT(!m_replyHandlers.contains(requestID));
        m_replyHandlers.set(requestID, ReplyHandler { &replyDispatcher, WTFMove(replyHandler) });
    }

    sendMessage(WTFMove(encoder), { });
}

bool Connection::sendSyncReply(std::unique_ptr<Encoder> encoder)
{
    return sendMessage(WTFMove(encoder), { });
}

Seconds Connection::timeoutRespectingIgnoreTimeoutsForTesting(Seconds timeout) const
{
    return m_ignoreTimeoutsForTesting ? Seconds::infinity() : timeout;
}

std::unique_ptr<Decoder> Connection::waitForMessage(StringReference messageReceiverName, StringReference messageName, uint64_t destinationID, Seconds timeout, OptionSet<WaitForOption> waitForOptions)
{
    ASSERT(RunLoop::isMain());

    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);

    bool hasIncomingSynchronousMessage = false;

    // First, check if this message is already in the incoming messages queue.
    {
        std::lock_guard<Lock> lock(m_incomingMessagesMutex);

        for (auto it = m_incomingMessages.begin(), end = m_incomingMessages.end(); it != end; ++it) {
            std::unique_ptr<Decoder>& message = *it;

            if (message->messageReceiverName() == messageReceiverName && message->messageName() == messageName && message->destinationID() == destinationID) {
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
        m_waitingForMessage = nullptr;
        return nullptr;
    }

    WaitForMessageState waitingForMessage(messageReceiverName, messageName, destinationID, waitForOptions);

    {
        std::lock_guard<Lock> lock(m_waitForMessageMutex);

        // We don't support having multiple clients waiting for messages.
        ASSERT(!m_waitingForMessage);

        m_waitingForMessage = &waitingForMessage;
    }

    MonotonicTime absoluteTimeout = MonotonicTime::now() + timeout;

    // Now wait for it to be set.
    while (true) {
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
    sendMessage(WTFMove(encoder), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);

    // Then wait for a reply. Waiting for a reply could involve dispatching incoming sync messages, so
    // keep an extra reference to the connection here in case it's invalidated.
    Ref<Connection> protect(*this);
    std::unique_ptr<Decoder> reply = waitForSyncReply(syncRequestID, timeout, sendSyncOptions);

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

std::unique_ptr<Decoder> Connection::waitForSyncReply(uint64_t syncRequestID, Seconds timeout, OptionSet<SendSyncOption> sendSyncOptions)
{
    timeout = timeoutRespectingIgnoreTimeoutsForTesting(timeout);
    WallTime absoluteTime = WallTime::now() + timeout;

    willSendSyncMessage(sendSyncOptions);
    
    bool timedOut = false;
    while (!timedOut) {
        // First, check if we have any messages that we need to process.
        SyncMessageState::singleton().dispatchMessages(nullptr);
        
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

    RELEASE_LOG_ERROR(IPC, "Connection::waitForSyncReply: Timed-out while waiting for reply, id = %" PRIu64, syncRequestID);
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

    {
        LockHolder locker(m_replyHandlersLock);

        auto replyHandler = m_replyHandlers.take(decoder->destinationID());
        if (replyHandler.dispatcher) {
            replyHandler.dispatcher->dispatch([protectedThis = makeRef(*this), handler = WTFMove(replyHandler.handler), decoder = WTFMove(decoder)] () mutable {
                if (!protectedThis->isValid()) {
                    handler(nullptr);
                    return;
                }

                handler(WTFMove(decoder));
            });
        }
    }

    // If we get here, it means we got a reply for a message that wasn't in the sync request stack or map.
    // This can happen if the send timed out, so it's fine to ignore.
}

void Connection::processIncomingMessage(std::unique_ptr<Decoder> message)
{
    ASSERT(!message->messageReceiverName().isEmpty());
    ASSERT(!message->messageName().isEmpty());

    if (message->messageReceiverName() == "IPC" && message->messageName() == "SyncMessageReply") {
        processIncomingSyncReply(WTFMove(message));
        return;
    }

    if (!m_workQueueMessageReceivers.isValidKey(message->messageReceiverName())) {
        RefPtr<Connection> protectedThis(this);
        StringReference messageReceiverNameReference = message->messageReceiverName();
        String messageReceiverName(messageReceiverNameReference.isEmpty() ? "<unknown message receiver>" : String(messageReceiverNameReference.data(), messageReceiverNameReference.size()));
        StringReference messageNameReference = message->messageName();
        String messageName(messageNameReference.isEmpty() ? "<unknown message>" : String(messageNameReference.data(), messageNameReference.size()));

        RunLoop::main().dispatch([protectedThis = makeRef(*this), messageReceiverName = WTFMove(messageReceiverName), messageName = WTFMove(messageName)]() mutable {
            protectedThis->dispatchDidReceiveInvalidMessage(messageReceiverName.utf8(), messageName.utf8());
        });
        return;
    }

    auto it = m_workQueueMessageReceivers.find(message->messageReceiverName());
    if (it != m_workQueueMessageReceivers.end()) {
        it->value.first->dispatch([protectedThis = makeRef(*this), workQueueMessageReceiver = it->value.second, decoder = WTFMove(message)]() mutable {
            protectedThis->dispatchWorkQueueMessageReceiverMessage(*workQueueMessageReceiver, *decoder);
        });
        return;
    }

#if HAVE(QOS_CLASSES)
    if (message->isSyncMessage() && m_shouldBoostMainThreadOnSyncMessage) {
        pthread_override_t override = pthread_override_qos_class_start_np(m_mainThread, Thread::adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE), 0);
        message->setQOSClassOverride(override);
    }
#endif

    if (message->isSyncMessage()) {
        std::lock_guard<Lock> lock(m_incomingSyncMessageCallbackMutex);

        for (auto& callback : m_incomingSyncMessageCallbacks.values())
            m_incomingSyncMessageCallbackQueue->dispatch(WTFMove(callback));

        m_incomingSyncMessageCallbacks.clear();
    }

    // Check if this is a sync message or if it's a message that should be dispatched even when waiting for
    // a sync reply. If it is, and we're waiting for a sync reply this message needs to be dispatched.
    // If we don't we'll end up with a deadlock where both sync message senders are stuck waiting for a reply.
    if (SyncMessageState::singleton().processIncomingMessage(*this, message))
        return;

    // Check if we're waiting for this message.
    {
        std::lock_guard<Lock> lock(m_waitForMessageMutex);

        if (m_waitingForMessage && !m_waitingForMessage->decoder) {
            if (m_waitingForMessage->messageReceiverName == message->messageReceiverName() && m_waitingForMessage->messageName == message->messageName() && m_waitingForMessage->destinationID == message->destinationID()) {
                m_waitingForMessage->decoder = WTFMove(message);
                ASSERT(m_waitingForMessage->decoder);
                m_waitForMessageCondition.notifyOne();
                return;
            }

            if (m_waitingForMessage->waitForOptions.contains(WaitForOption::InterruptWaitingIfSyncMessageArrives) && message->isSyncMessage()) {
                m_waitingForMessage->messageWaitingInterrupted = true;
                m_waitForMessageCondition.notifyOne();
            }
        }
    }

    enqueueIncomingMessage(WTFMove(message));
}

uint64_t Connection::installIncomingSyncMessageCallback(WTF::Function<void ()>&& callback)
{
    std::lock_guard<Lock> lock(m_incomingSyncMessageCallbackMutex);

    m_nextIncomingSyncMessageCallbackID++;

    if (!m_incomingSyncMessageCallbackQueue)
        m_incomingSyncMessageCallbackQueue = WorkQueue::create("com.apple.WebKit.IPC.IncomingSyncMessageCallbackQueue");

    m_incomingSyncMessageCallbacks.add(m_nextIncomingSyncMessageCallbackID, WTFMove(callback));

    return m_nextIncomingSyncMessageCallbackID;
}

void Connection::uninstallIncomingSyncMessageCallback(uint64_t callbackID)
{
    std::lock_guard<Lock> lock(m_incomingSyncMessageCallbackMutex);
    m_incomingSyncMessageCallbacks.remove(callbackID);
}

bool Connection::hasIncomingSyncMessage()
{
    std::lock_guard<Lock> lock(m_incomingMessagesMutex);

    for (auto& message : m_incomingMessages) {
        if (message->isSyncMessage())
            return true;
    }
    
    return false;
}

void Connection::enableIncomingMessagesThrottling()
{
    if (m_incomingMessagesThrottler)
        return;

    m_incomingMessagesThrottler = std::make_unique<MessagesThrottler>(*this, &Connection::dispatchIncomingMessages);
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
        LockHolder locker(m_replyHandlersLock);
        for (auto& replyHandler : m_replyHandlers.values()) {
            replyHandler.dispatcher->dispatch([handler = WTFMove(replyHandler.handler)] {
                handler(nullptr);
            });
        }

        m_replyHandlers.clear();
    }

    {
        LockHolder locker(m_syncReplyStateMutex);

        ASSERT(m_shouldWaitForSyncReplies);
        m_shouldWaitForSyncReplies = false;

        if (!m_pendingSyncReplies.isEmpty())
            SyncMessageState::singleton().wakeUpClientRunLoop();
    }

    {
        std::lock_guard<Lock> lock(m_waitForMessageMutex);
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
            std::lock_guard<Lock> lock(m_outgoingMessagesMutex);
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

    auto replyEncoder = std::make_unique<Encoder>("IPC", "SyncMessageReply", syncRequestID);

    if (decoder.messageReceiverName() == "IPC" && decoder.messageName() == "WrappedAsyncMessageForTesting") {
        if (!m_fullySynchronousModeIsAllowedForTesting) {
            decoder.markInvalid();
            return;
        }
        std::unique_ptr<Decoder> unwrappedDecoder = Decoder::unwrapForTesting(decoder);
        RELEASE_ASSERT(unwrappedDecoder);
        processIncomingMessage(WTFMove(unwrappedDecoder));

        SyncMessageState::singleton().dispatchMessages(nullptr);
    } else {
        // Hand off both the decoder and encoder to the client.
        m_client.didReceiveSyncMessage(*this, decoder, replyEncoder);
    }

    // FIXME: If the message was invalid, we should send back a SyncMessageError.
    ASSERT(!decoder.isInvalid());

    if (replyEncoder)
        sendSyncReply(WTFMove(replyEncoder));
}

void Connection::dispatchDidReceiveInvalidMessage(const CString& messageReceiverNameString, const CString& messageNameString)
{
    ASSERT(RunLoop::isMain());

    if (!isValid())
        return;

    m_client.didReceiveInvalidMessage(*this, StringReference(messageReceiverNameString.data(), messageReceiverNameString.length()), StringReference(messageNameString.data(), messageNameString.length()));
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
        std::lock_guard<Lock> lock(m_incomingMessagesMutex);

#if PLATFORM(COCOA)
        if (m_wasKilled)
            return;

        if (m_incomingMessages.size() >= maxPendingIncomingMessagesKillingThreshold) {
            if (kill()) {
                RELEASE_LOG_ERROR(IPC, "%p - Connection::enqueueIncomingMessage: Over %zu incoming messages have been queued without the main thread processing them, killing the connection as the remote process seems to be misbehaving", this, maxPendingIncomingMessagesKillingThreshold);
                m_incomingMessages.clear();
            }
            return;
        }
#endif

        m_incomingMessages.append(WTFMove(incomingMessage));

        if (m_incomingMessagesThrottler && m_incomingMessages.size() != 1)
            return;
    }

    RunLoop::main().dispatch([protectedThis = makeRef(*this)]() mutable {
        if (protectedThis->m_incomingMessagesThrottler)
            protectedThis->dispatchIncomingMessages();
        else
            protectedThis->dispatchOneIncomingMessage();
    });
}

void Connection::dispatchMessage(Decoder& decoder)
{
    RELEASE_ASSERT(isValid());
    m_client.didReceiveMessage(*this, decoder);
}

void Connection::dispatchMessage(std::unique_ptr<Decoder> message)
{
    if (!isValid())
        return;

    if (message->shouldUseFullySynchronousModeForTesting()) {
        if (!m_fullySynchronousModeIsAllowedForTesting) {
            m_client.didReceiveInvalidMessage(*this, message->messageReceiverName(), message->messageName());
            return;
        }
        m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting++;
    }

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

    // FIXME: For Delayed synchronous messages, we should not decrement the counter until we send a response.
    // Otherwise, we would deadlock if processing the message results in a sync message back after we exit this function.
    if (message->shouldDispatchMessageWhenWaitingForSyncReply())
        m_inDispatchMessageMarkedDispatchWhenWaitingForSyncReplyCount--;

    if (message->shouldUseFullySynchronousModeForTesting())
        m_inDispatchMessageMarkedToUseFullySynchronousModeForTesting--;

    if (m_didReceiveInvalidMessage && isValid())
        m_client.didReceiveInvalidMessage(*this, message->messageReceiverName(), message->messageName());

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
        std::lock_guard<Lock> lock(m_incomingMessagesMutex);
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
        std::lock_guard<Lock> lock(m_incomingMessagesMutex);
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
            RELEASE_LOG_ERROR(IPC, "%p - Connection::dispatchIncomingMessages: first IPC message in queue is %{public}s::%{public}s", this, message->messageReceiverName().toString().data(), message->messageName().toString().data());
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
            std::lock_guard<Lock> lock(m_incomingMessagesMutex);
            if (m_incomingMessages.isEmpty())
                return;

            message = m_incomingMessages.takeFirst();
        }
        dispatchMessage(WTFMove(message));
    }
}

void Connection::wakeUpRunLoop()
{
    RunLoop::main().wakeUp();
}

} // namespace IPC
