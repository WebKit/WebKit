/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "IPCUtilities.h"
#include "ObjectIdentifierReferenceTracker.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include "Timeout.h"
#include <optional>
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {

// Container that holds identifier -> object mapping between two processes.
// Used in scenarios where the holder process processes messages with the object references in multiple
// threads. `ThreadSafeObjectHeap` tracks the read and write references to the content pointed
// by a particular identifier.
// Pending reads may block new writes. Alternatively new writes can skip ahead of the reads,
// for example in copy-on-write scenarios.
// Pending writes block reads.
// Stream message handlers do not block their work queue: they use the get() and read() variants that take the
// StreamServerConnection, and defer the message until the object is added.
// HeldType must be copyable because read() may or may not give out the last instance.
template<typename Identifier, typename HeldType>
class ThreadSafeObjectHeap {
    using MappedTraits = HashTraits<HeldType>;
public:
    using ReadReference = typename ObjectIdentifierReferenceTracker<Identifier>::ReadReference;
    using WriteReference = typename ObjectIdentifierReferenceTracker<Identifier>::WriteReference;
    using Reference = typename ObjectIdentifierReferenceTracker<Identifier>::Reference;
    using PeekType = typename MappedTraits::PeekType;
    using TakeType = typename MappedTraits::TakeType;
    virtual ~ThreadSafeObjectHeap() = default;

    // Establishes a new object in the heap with an initial reference.
    // Returns true on success.
    bool add(const Reference&, HeldType);

    // Waits until a write creates the reference or times out.
    PeekType get(const ReadReference&, Timeout) const;

    // Waits until a write creates the reference and retires the read or times out.
    // Return value should be used only if the object is immutable. Otherwise, callers should read
    // from the object returned by get() and then only retire the read reference via read().
    TakeType read(ReadReference&&, Timeout);

    // For stream message handlers. If the object has been added, returns it like get() and read(). Otherwise,
    // if the message can wait (StreamServerConnection::canCurrentlyDispatchedMessageWait()), arranges the
    // work queue of `connection` to be woken up when the object is added and returns std::nullopt. The
    // handler should then call StreamServerConnection::markCurrentlyDispatchedMessageAsDeferred() to have the
    // message dispatched again. If the message cannot wait, returns the empty value. read() retires the read
    // only if it returns the object.
    std::optional<PeekType> get(StreamServerConnection&, const ReadReference&);
    std::optional<TakeType> read(StreamServerConnection&, ReadReference&&);

    // Waits until write is possible and removes the object or times out.
    // To re-add after modification, the new reference can be obtained with
    // WriteReference::retiredReference().
    TakeType take(WriteReference&&, Timeout);

    // Marks the object for removal.
    bool remove(WriteReference&&);

    void clear();

    size_t sizeForTesting() const;

private:
    std::optional<TakeType> readIfAdded(const ReadReference&) WTF_REQUIRES_LOCK(m_objectsLock);
    bool waitForAdd(StreamServerConnection&, const Reference&) WTF_REQUIRES_LOCK(m_objectsLock);

    mutable Lock m_objectsLock;
    mutable Condition m_objectsCondition;
    struct ReferenceState {
        uint64_t retiredReads { 0 };
        std::optional<uint64_t> finalRead; // Remove after `finalRead` reads.
        std::optional<HeldType> object; // Object or final read marker.

        ReferenceState() = default;
        explicit ReferenceState(HeldType&& object)
            : object(WTF::move(object))
        {
        }
        explicit ReferenceState(uint64_t finalRead)
            : finalRead(finalRead)
        {
        }
    };
    HashMap<Reference, ReferenceState> m_objects WTF_GUARDED_BY_LOCK(m_objectsLock);
    // The work queues of the stream connections whose messages wait for the objects to be added. Weak, so that
    // the heap does not keep the work queue of a torn down connection alive.
    using StreamWaiters = Vector<ThreadSafeWeakPtr<StreamConnectionWorkQueue>, 1>;
    HashMap<Reference, StreamWaiters> m_streamWaiters WTF_GUARDED_BY_LOCK(m_objectsLock);
};

template<typename Identifier, typename HeldType>
bool ThreadSafeObjectHeap<Identifier, HeldType>::add(const Reference& ref, HeldType object)
{
    StreamWaiters streamWaiters;
    {
        Locker locker { m_objectsLock };
        auto addResult = m_objects.ensure(ref, [&] {
            return ReferenceState { WTF::move(object) };
        });
        if (!addResult.isNewEntry) {
            auto& state = addResult.iterator->value;
            if (state.finalRead && !*(state.finalRead)) {
                // `object` will be destroyed without the lock later.
                m_objects.remove(addResult.iterator);
                return true; // Reference was already removed, so nobody is waiting. No need to notify.
            }
            if (state.object)
                return false;
            state.object = WTF::move(object);
        }
        m_objectsCondition.notifyAll();
        streamWaiters = m_streamWaiters.take(ref);
    }
    for (auto& weakWorkQueue : streamWaiters) {
        if (RefPtr workQueue = weakWorkQueue)
            workQueue->wakeUp();
    }
    return true;
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::get(const ReadReference& read, IPC::Timeout timeout) const -> PeekType
{
    Locker locker { m_objectsLock };
    do {
        auto it = m_objects.find(read.reference());
        if (it != m_objects.end()) {
            auto& state = it->value;
            if (state.object)
                return MappedTraits::peek(*state.object);
            // Final read marker, write for this reference has not yet happened.
            return MappedTraits::peek(MappedTraits::emptyValue());
        }
    } while (m_objectsCondition.waitUntil(m_objectsLock, timeout.deadline()));
    return MappedTraits::peek(MappedTraits::emptyValue());
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::read(ReadReference&& read, Timeout timeout) -> TakeType
{
    Locker locker { m_objectsLock };
    do {
        if (auto result = readIfAdded(read))
            return WTF::move(*result);
    } while (m_objectsCondition.waitUntil(m_objectsLock, timeout.deadline()));
    return MappedTraits::peek(MappedTraits::emptyValue());
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::readIfAdded(const ReadReference& read) -> std::optional<TakeType>
{
    auto it = m_objects.find(read.reference());
    if (it == m_objects.end() || !it->value.object)
        return std::nullopt;
    auto& state = it->value;
    TakeType result = MappedTraits::peek(*state.object); // Assume PeekType is convertible to TakeType, e.g. the type is copyable.
    state.retiredReads++;
    if (state.finalRead) {
        ASSERT(state.finalRead >= state.retiredReads); // Trusted assert because this is ensured on `finalRead` assignment.
        if (state.finalRead == state.retiredReads)
            m_objects.remove(it);
    }
    m_objectsCondition.notifyAll();
    return result;
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::get(StreamServerConnection& connection, const ReadReference& read) -> std::optional<PeekType>
{
    Locker locker { m_objectsLock };
    auto it = m_objects.find(read.reference());
    if (it != m_objects.end() && it->value.object)
        return MappedTraits::peek(*it->value.object);
    if (waitForAdd(connection, read.reference()))
        return std::nullopt;
    return MappedTraits::peek(MappedTraits::emptyValue());
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::read(StreamServerConnection& connection, ReadReference&& read) -> std::optional<TakeType>
{
    Locker locker { m_objectsLock };
    if (auto result = readIfAdded(read))
        return result;
    if (waitForAdd(connection, read.reference()))
        return std::nullopt;
    return MappedTraits::peek(MappedTraits::emptyValue());
}

// Returns true if the currently dispatched message of `connection` waits for the object of `reference` to
// be added. The work queue of the connection is then woken up when the object is added.
template<typename Identifier, typename HeldType>
bool ThreadSafeObjectHeap<Identifier, HeldType>::waitForAdd(StreamServerConnection& connection, const Reference& reference)
{
    RefPtr workQueue = connection.workQueue();
    if (!workQueue)
        return false;
    if (!connection.canCurrentlyDispatchedMessageWait())
        return false;
    auto& waiters = m_streamWaiters.ensure(reference, [] {
        return StreamWaiters { };
    }).iterator->value;
    // One entry per work queue: one wake up runs all the connections of the queue. Drop the destroyed work queues.
    bool isWaiting = false;
    waiters.removeAllMatching([&](auto& waiter) {
        RefPtr waiterWorkQueue = waiter;
        isWaiting |= waiterWorkQueue == workQueue;
        return !waiterWorkQueue;
    });
    if (!isWaiting)
        waiters.append(*workQueue);
    return true;
}

template<typename Identifier, typename HeldType>
auto ThreadSafeObjectHeap<Identifier, HeldType>::take(WriteReference&& write, Timeout timeout) -> TakeType
{
    Locker locker { m_objectsLock };
    auto finalRead = write.pendingReads();
    do {
        auto it = m_objects.find(write.reference());
        if (it != m_objects.end()) {
            auto& state = it->value;
            if (state.finalRead || state.retiredReads > finalRead)
                return MappedTraits::take(MappedTraits::emptyValue());

            if (state.retiredReads == finalRead) {
                auto result = MappedTraits::take(WTF::move(*state.object));
                m_objects.remove(it);
                // Not notifying, as nothing can be waiting on a remove.
                return result;
            }
        }
    } while (m_objectsCondition.waitUntil(m_objectsLock, timeout.deadline()));
    return MappedTraits::take(MappedTraits::emptyValue());
}

template<typename Identifier, typename HeldType>
bool ThreadSafeObjectHeap<Identifier, HeldType>::remove(WriteReference&& write)
{
    TakeType object;

    Locker locker { m_objectsLock };
    auto finalRead = write.pendingReads();
    auto addResult = m_objects.ensure(write.reference(), [finalRead] {
        return ReferenceState { finalRead };
    });

    if (addResult.isNewEntry)
        return true;

    auto& state = addResult.iterator->value;
    if (state.finalRead || state.retiredReads > finalRead)
        return false;
    if (state.retiredReads == finalRead) {
        object = WTF::move(*state.object); // Destroy the object later without the lock held.
        m_objects.remove(addResult.iterator);
    } else
        state.finalRead = finalRead;
    // Not notifying, as nothing can be waiting on a remove.
    return true;
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::clear()
{
    HashMap<Reference, StreamWaiters> streamWaiters;
    {
        Locker locker { m_objectsLock };
        m_objects.clear();
        streamWaiters = std::exchange(m_streamWaiters, { });
    }
    // The waiting messages check again. They keep waiting, since the objects are not added any more.
    for (auto& waiters : streamWaiters.values()) {
        for (auto& weakWorkQueue : waiters) {
            if (RefPtr workQueue = weakWorkQueue)
                workQueue->wakeUp();
        }
    }
}

template<typename Identifier, typename HeldType>
size_t ThreadSafeObjectHeap<Identifier, HeldType>::sizeForTesting() const
{
    Locker locker { m_objectsLock };
    return m_objects.size();
}

}
