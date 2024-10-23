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
#include "Timeout.h"
#include <optional>
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadAssertions.h>

namespace IPC {

// Container that holds identifier -> object mapping between two processes.
// Used in scenarios where the holder process processes messages with the object references in multiple
// threads. `ThreadSafeObjectHeap` tracks the read and write references to the content pointed
// by a particular identifier.
// Pending reads may block new writes. Alternatively new writes can skip ahead of the reads,
// for example in copy-on-write scenarios.
// Pending writes block reads.
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

    // Waits until write is possible and removes the object or times out.
    // To re-add after modification, the new reference can be obtained with
    // WriteReference::retiredReference().
    TakeType take(WriteReference&&, Timeout);

    // Marks the object for removal.
    bool remove(WriteReference&&);

    void clear();

    size_t sizeForTesting() const;

private:
    mutable Lock m_objectsLock;
    mutable Condition m_objectsCondition;
    struct ReferenceState {
        uint64_t retiredReads { 0 };
        std::optional<uint64_t> finalRead; // Remove after `finalRead` reads.
        std::optional<HeldType> object; // Object or final read marker.

        ReferenceState() = default;
        explicit ReferenceState(HeldType&& object)
            : object(WTFMove(object))
        {
        }
        explicit ReferenceState(uint64_t finalRead)
            : finalRead(finalRead)
        {
        }
    };
    HashMap<Reference, ReferenceState> m_objects WTF_GUARDED_BY_LOCK(m_objectsLock);
};

template<typename Identifier, typename HeldType>
bool ThreadSafeObjectHeap<Identifier, HeldType>::add(const Reference& ref, HeldType object)
{
    Locker locker { m_objectsLock };
    auto addResult = m_objects.ensure(ref, [&] {
        return ReferenceState { WTFMove(object) };
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
        state.object = WTFMove(object);
    }
    m_objectsCondition.notifyAll();
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
        auto it = m_objects.find(read.reference());
        if (it != m_objects.end()) {
            auto& state = it->value;
            if (state.object) {
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
        }
    } while (m_objectsCondition.waitUntil(m_objectsLock, timeout.deadline()));
    return MappedTraits::peek(MappedTraits::emptyValue());
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
                auto result = MappedTraits::take(WTFMove(*state.object));
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
        object = WTFMove(*state.object); // Destroy the object later without the lock held.
        m_objects.remove(addResult.iterator);
    } else
        state.finalRead = finalRead;
    // Not notifying, as nothing can be waiting on a remove.
    return true;
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::clear()
{
    Locker locker { m_objectsLock };
    m_objects.clear();
}

template<typename Identifier, typename HeldType>
size_t ThreadSafeObjectHeap<Identifier, HeldType>::sizeForTesting() const
{
    Locker locker { m_objectsLock };
    return m_objects.size();
}

}
