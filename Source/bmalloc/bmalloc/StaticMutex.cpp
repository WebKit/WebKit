/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "StaticMutex.h"

#include "PerThread.h"
#include "ScopeExit.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace bmalloc {

// FIXME: This duplicates code from WTF::WordLock.
// https://bugs.webkit.org/show_bug.cgi?id=177719

namespace {

// This data structure serves three purposes:
//
// 1) A parking mechanism for threads that go to sleep. That involves just a system mutex and
//    condition variable.
//
// 2) A queue node for when a thread is on some lock's queue.
//
// 3) The queue head. This is kind of funky. When a thread is the head of a queue, it also serves as
//    the basic queue bookkeeping data structure. When a thread is dequeued, the next thread in the
//    queue takes on the queue head duties.
struct ThreadData {
    // The parking mechanism.
    bool shouldPark { false };
    std::mutex parkingLock;
    std::condition_variable parkingCondition;

    // The queue node.
    ThreadData* nextInQueue { nullptr };

    // The queue itself.
    ThreadData* queueTail { nullptr };
};

ThreadData* myThreadData()
{
    return PerThread<ThreadData>::get();
}

} // anonymous namespace

// NOTE: It's a bug to use any memory order other than seq_cst in this code. The cost of seq_cst
// fencing is negligible on slow paths, so any use of a more relaxed memory model is all risk and no
// reward.

BNO_INLINE void StaticMutex::lockSlow()
{
    unsigned spinCount = 0;

    // This magic number turns out to be optimal based on past JikesRVM experiments.
    const unsigned spinLimit = 40;
    
    for (;;) {
        uintptr_t currentWordValue = m_word.load();
        
        if (!(currentWordValue & isLockedBit)) {
            // It's not possible for someone to hold the queue lock while the lock itself is no longer
            // held, since we will only attempt to acquire the queue lock when the lock is held and
            // the queue lock prevents unlock.
            BASSERT(!(currentWordValue & isQueueLockedBit));
            if (compareExchangeWeak(m_word, currentWordValue, currentWordValue | isLockedBit)) {
                // Success! We acquired the lock.
                return;
            }
        }

        // If there is no queue and we haven't spun too much, we can just try to spin around again.
        if (!(currentWordValue & ~queueHeadMask) && spinCount < spinLimit) {
            spinCount++;
            sched_yield();
            continue;
        }

        // Need to put ourselves on the queue. Create the queue if one does not exist. This requries
        // owning the queue for a little bit. The lock that controls the queue is itself a spinlock.
        // But before we acquire the queue spinlock, we make sure that we have a ThreadData for this
        // thread.
        ThreadData* me = myThreadData();
        BASSERT(!me->shouldPark);
        BASSERT(!me->nextInQueue);
        BASSERT(!me->queueTail);

        // Reload the current word value, since some time may have passed.
        currentWordValue = m_word.load();

        // We proceed only if the queue lock is not held, the lock is held, and we succeed in
        // acquiring the queue lock.
        if ((currentWordValue & isQueueLockedBit)
            || !(currentWordValue & isLockedBit)
            || !compareExchangeWeak(m_word, currentWordValue, currentWordValue | isQueueLockedBit)) {
            sched_yield();
            continue;
        }
        
        me->shouldPark = true;

        // We own the queue. Nobody can enqueue or dequeue until we're done. Also, it's not possible
        // to release the lock while we hold the queue lock.
        ThreadData* queueHead = reinterpret_cast<ThreadData*>(currentWordValue & ~queueHeadMask);
        if (queueHead) {
            // Put this thread at the end of the queue.
            queueHead->queueTail->nextInQueue = me;
            queueHead->queueTail = me;

            // Release the queue lock.
            currentWordValue = m_word.load();
            BASSERT(currentWordValue & ~queueHeadMask);
            BASSERT(currentWordValue & isQueueLockedBit);
            BASSERT(currentWordValue & isLockedBit);
            m_word.store(currentWordValue & ~isQueueLockedBit);
        } else {
            // Make this thread be the queue-head.
            queueHead = me;
            me->queueTail = me;

            // Release the queue lock and install ourselves as the head. No need for a CAS loop, since
            // we own the queue lock.
            currentWordValue = m_word.load();
            BASSERT(~(currentWordValue & ~queueHeadMask));
            BASSERT(currentWordValue & isQueueLockedBit);
            BASSERT(currentWordValue & isLockedBit);
            uintptr_t newWordValue = currentWordValue;
            newWordValue |= reinterpret_cast<uintptr_t>(queueHead);
            newWordValue &= ~isQueueLockedBit;
            m_word.store(newWordValue);
        }

        // At this point everyone who acquires the queue lock will see me on the queue, and anyone who
        // acquires me's lock will see that me wants to park. Note that shouldPark may have been
        // cleared as soon as the queue lock was released above, but it will happen while the
        // releasing thread holds me's parkingLock.

        {
            std::unique_lock<std::mutex> locker(me->parkingLock);
            while (me->shouldPark)
                me->parkingCondition.wait(locker);
        }

        BASSERT(!me->shouldPark);
        BASSERT(!me->nextInQueue);
        BASSERT(!me->queueTail);
        
        // Now we can loop around and try to acquire the lock again.
    }
}

BNO_INLINE void StaticMutex::unlockSlow()
{
    // The fast path can fail either because of spurious weak CAS failure, or because someone put a
    // thread on the queue, or the queue lock is held. If the queue lock is held, it can only be
    // because someone *will* enqueue a thread onto the queue.

    // Acquire the queue lock, or release the lock. This loop handles both lock release in case the
    // fast path's weak CAS spuriously failed and it handles queue lock acquisition if there is
    // actually something interesting on the queue.
    for (;;) {
        uintptr_t currentWordValue = m_word.load();

        BASSERT(currentWordValue & isLockedBit);
        
        if (currentWordValue == isLockedBit) {
            if (compareExchangeWeak(m_word, isLockedBit, clear)) {
                // The fast path's weak CAS had spuriously failed, and now we succeeded. The lock is
                // unlocked and we're done!
                return;
            }
            // Loop around and try again.
            sched_yield();
            continue;
        }
        
        if (currentWordValue & isQueueLockedBit) {
            sched_yield();
            continue;
        }

        // If it wasn't just a spurious weak CAS failure and if the queue lock is not held, then there
        // must be an entry on the queue.
        BASSERT(currentWordValue & ~queueHeadMask);

        if (compareExchangeWeak(m_word, currentWordValue, currentWordValue | isQueueLockedBit))
            break;
    }

    uintptr_t currentWordValue = m_word.load();
        
    // After we acquire the queue lock, the lock must still be held and the queue must be
    // non-empty. The queue must be non-empty since only the lockSlow() method could have held the
    // queue lock and if it did then it only releases it after putting something on the queue.
    BASSERT(currentWordValue & isLockedBit);
    BASSERT(currentWordValue & isQueueLockedBit);
    ThreadData* queueHead = reinterpret_cast<ThreadData*>(currentWordValue & ~queueHeadMask);
    RELEASE_BASSERT(queueHead);
    RELEASE_BASSERT(queueHead->shouldPark);

    ThreadData* newQueueHead = queueHead->nextInQueue;
    // Either this was the only thread on the queue, in which case we delete the queue, or there
    // are still more threads on the queue, in which case we create a new queue head.
    if (newQueueHead)
        newQueueHead->queueTail = queueHead->queueTail;

    // Change the queue head, possibly removing it if newQueueHead is null. No need for a CAS loop,
    // since we hold the queue lock and the lock itself so nothing about the lock can change right
    // now.
    currentWordValue = m_word.load();
    BASSERT(currentWordValue & isLockedBit);
    BASSERT(currentWordValue & isQueueLockedBit);
    BASSERT((currentWordValue & ~queueHeadMask) == reinterpret_cast<uintptr_t>(queueHead));
    uintptr_t newWordValue = currentWordValue;
    newWordValue &= ~isLockedBit; // Release the lock.
    newWordValue &= ~isQueueLockedBit; // Release the queue lock.
    newWordValue &= queueHeadMask; // Clear out the old queue head.
    newWordValue |= reinterpret_cast<uintptr_t>(newQueueHead); // Install new queue head.
    m_word.store(newWordValue);

    // Now the lock is available for acquisition. But we just have to wake up the old queue head.
    // After that, we're done!

    queueHead->nextInQueue = nullptr;
    queueHead->queueTail = nullptr;

    // We do this carefully because this may run either before or during the parkingLock critical
    // section in lockSlow().
    {
        std::lock_guard<std::mutex> locker(queueHead->parkingLock);
        RELEASE_BASSERT(queueHead->shouldPark);
        queueHead->shouldPark = false;
        // We have to do the notify while still holding the lock, since otherwise, we could try to
        // do it after the queueHead has destructed. It's impossible for queueHead to destruct
        // while we hold the lock, since it is either currently in the wait loop or it's before it
        // so it has to grab the lock before destructing.
        queueHead->parkingCondition.notify_one();
    }
    
    // The old queue head can now contend for the lock again. We're done!
}

} // namespace bmalloc
