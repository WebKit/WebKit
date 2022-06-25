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

#include "IPCTester.h"
#include "ObjectIdentifierReferenceTracker.h"
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadAssertions.h>

namespace WebKit {

// Container that holds identifier -> object mapping between two processes.
// Used in scenarios where the holder process processes messages with the object references in multiple
// threads. `ThreadSafeObjectHeap` tracks the read and write references to the content pointed
// by a particular identifier.
// Pending reads may block new writes. Alternatively new writes can skip ahead of the reads,
// for example in copy-on-write scenarios.
// Pending writes block reads.
// Currently implemented only for read-only (create-read-destroy) or copy-on-write types,
// e.g. no two-phase get() operations are not implemented yet.
template<typename Identifier, typename HeldType>
class ThreadSafeObjectHeap {
public:
    using ReadReference = typename ObjectIdentifierReferenceTracker<Identifier>::ReadReference;
    using WriteReference = typename ObjectIdentifierReferenceTracker<Identifier>::WriteReference;
    using Reference = typename ObjectIdentifierReferenceTracker<Identifier>::Reference;
    virtual ~ThreadSafeObjectHeap() = default;

    void add(Identifier, HeldType&&);

    // Waits until a write creates the reference and then retires the read or
    // times out.
    HeldType retire(ReadReference&&, IPC::Timeout);

    // Inserts a new version of the object or removes an old one and then retires the write or
    // times out.
    // If timeout is passed, waits until pending reads are done.
    // It is a caller error to not wait and mutate the old object.
    void retire(WriteReference&&, std::optional<HeldType>&& newObject, std::optional<IPC::Timeout>);

    void retireRemove(WriteReference&&);

    void clear();
private:
    Lock m_objectsLock;
    Condition m_objectsCondition;
    struct ReferenceState {
        uint64_t retiredReads { 0 };
        std::optional<uint64_t> finalRead; // Remove after `finalRead` reads.
        std::optional<HeldType> object; // Object or tombstone.

        ReferenceState() = default;
        explicit ReferenceState(HeldType&& object)
            : object(WTFMove(object))
        {
        }
        // Tombstone constructor.
        explicit ReferenceState(uint64_t finalRead)
            : finalRead(finalRead)
        {
        }
    };
    HashMap<Reference, ReferenceState> m_objects WTF_GUARDED_BY_LOCK(m_objectsLock);
};

template<typename Identifier, typename HeldType>
HeldType ThreadSafeObjectHeap<Identifier, HeldType>::retire(ReadReference&& read, IPC::Timeout timeout)
{
    Locker locker { m_objectsLock };
    do {
        auto it = m_objects.find(read.reference());
        if (it != m_objects.end()) {
            auto& state = it->value;
            if (state.object) {
                HeldType result = *state.object;
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
        if (!m_objectsCondition.waitUntil(m_objectsLock, timeout.deadline()))
            break;
    } while (true);
    return nullptr;
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::retire(WriteReference&& write, std::optional<HeldType>&& object, std::optional<IPC::Timeout> timeout)
{
    const bool waitForPendingReads = timeout.has_value();
    Locker locker { m_objectsLock };
    do {
        auto it = m_objects.find(write.reference());
        if (it != m_objects.end()) {
            auto& state = it->value;
            if (state.finalRead && state.object) {
                // Write replay: `finalRead` is assigned on write. Cannot have the same version with writes.
                ASSERT_IS_TESTING_IPC();
                return;
            }
            if (state.retiredReads > write.pendingReads()) {
                // Write underflow: more reads have been done than the sender claims is the maximum.
                ASSERT_IS_TESTING_IPC();
                return;
            }
            // It is likely a caller error to try to add a new reference with the same client object without
            // waiting for pending reads on the old reference.
            ASSERT(waitForPendingReads || !state.object || *state.object != object);

            if (state.retiredReads == write.pendingReads() || !waitForPendingReads) {
                if (state.retiredReads < write.pendingReads())
                    state.finalRead = write.pendingReads();
                else if (state.retiredReads == write.pendingReads())
                    m_objects.remove(it);
                if (object) {
                    auto addResult = m_objects.add(WTFMove(write).retiredReference(), ReferenceState { WTFMove(*object) });
                    if (!addResult.isNewEntry) {
                        auto& newState = addResult.iterator->value;
                        // Entry was a tombstone.
                        if (newState.finalRead && !*newState.finalRead) {
                            m_objects.remove(addResult.iterator);
                        } else if (!newState.object) {
                            newState.object = WTFMove(*object);
                        } else if (newState.object) {
                            // Write replay: the retired write already exists, the entry was not a tombstone.
                            ASSERT_IS_TESTING_IPC();
                            return;
                        }
                    }
                    m_objectsCondition.notifyAll();
                }
                return;
            }
        } else if (!waitForPendingReads || !write.pendingReads()) {
            // Write the tombstone.
            m_objects.add(write.reference(), ReferenceState { write.pendingReads() });
            if (object)
                m_objects.add(WTFMove(write).retiredReference(), ReferenceState { WTFMove(*object) });
            m_objectsCondition.notifyAll();
            return;
        }
        ASSERT(timeout);
        if (!m_objectsCondition.waitUntil(m_objectsLock, timeout->deadline()))
            break;
    } while (true);
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::retireRemove(WriteReference&& object)
{
    retire(WTFMove(object), std::nullopt, std::nullopt);
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::clear()
{
    Locker locker { m_objectsLock };
    m_objects.clear();
}

template<typename Identifier, typename HeldType>
void ThreadSafeObjectHeap<Identifier, HeldType>::add(Identifier identifier, HeldType&& object)
{
    Reference reference { identifier, 0 };
    Locker locker { m_objectsLock };
    ASSERT(!m_objects.contains(reference));
    m_objects.add(reference, ReferenceState { WTFMove(object) });
}

}
