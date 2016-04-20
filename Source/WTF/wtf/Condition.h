/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WTF_Condition_h
#define WTF_Condition_h

#include <chrono>
#include <functional>
#include <wtf/CurrentTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/ParkingLot.h>

namespace WTF {

// This is a condition variable that is suitable for use with any lock-like object, including
// our own WTF::Lock. It features standard wait()/notifyOne()/notifyAll() methods in addition to
// a variety of wait-with-timeout methods. This includes methods that use WTF's own notion of
// time, like wall-clock time (i.e. currentTime()) and monotonic time (i.e.
// monotonicallyIncreasingTime()). This is a very efficient condition variable. It only requires
// one byte of memory. notifyOne() and notifyAll() require just a load and branch for the fast
// case where no thread is waiting. This condition variable, when used with WTF::Lock, can
// outperform a system condition variable and lock by up to 58x.

// This is a struct without a constructor or destructor so that it can be statically initialized.
// Use Lock in instance variables.
struct ConditionBase {
    typedef ParkingLot::Clock Clock;
    
    // Wait on a parking queue while releasing the given lock. It will unlock the lock just before
    // parking, and relock it upon wakeup. Returns true if we woke up due to some call to
    // notifyOne() or notifyAll(). Returns false if we woke up due to a timeout. Note that this form
    // of waitUntil() has some quirks:
    //
    // No spurious wake-up: in order for this to return before the timeout, some notifyOne() or
    // notifyAll() call must have happened. No scenario other than timeout or notify can lead to this
    // method returning. This means, for example, that you can't use pthread cancelation or signals to
    // cause early return.
    //
    // Past timeout: it's possible for waitUntil() to be called with a timeout in the past. In that
    // case, waitUntil() will still release the lock and reacquire it. waitUntil() will always return
    // false in that case. This is subtly different from some pthread_cond_timedwait() implementations,
    // which may not release the lock for past timeout. But, this behavior is consistent with OpenGroup
    // documentation for timedwait().
    template<typename LockType>
    bool waitUntil(LockType& lock, Clock::time_point timeout)
    {
        bool result;
        if (timeout < Clock::now()) {
            lock.unlock();
            result = false;
        } else {
            result = ParkingLot::parkConditionally(
                &m_hasWaiters,
                [this] () -> bool {
                    // Let everyone know that we will be waiting. Do this while we hold the queue lock,
                    // to prevent races with notifyOne().
                    m_hasWaiters.store(true);
                    return true;
                },
                [&lock] () { lock.unlock(); },
                timeout);
        }
        lock.lock();
        return result;
    }

    // Wait until the given predicate is satisfied. Returns true if it is satisfied in the end.
    // May return early due to timeout.
    template<typename LockType, typename Functor>
    bool waitUntil(LockType& lock, Clock::time_point timeout, const Functor& predicate)
    {
        while (!predicate()) {
            if (!waitUntil(lock, timeout))
                return predicate();
        }
        return true;
    }

    // Wait until the given predicate is satisfied. Returns true if it is satisfied in the end.
    // May return early due to timeout.
    template<typename LockType, typename DurationType, typename Functor>
    bool waitFor(
        LockType& lock, const DurationType& relativeTimeout, const Functor& predicate)
    {
        return waitUntil(lock, absoluteFromRelative(relativeTimeout), predicate);
    }

    template<typename LockType>
    void wait(LockType& lock)
    {
        waitUntil(lock, Clock::time_point::max());
    }

    template<typename LockType, typename Functor>
    void wait(LockType& lock, const Functor& predicate)
    {
        while (!predicate())
            wait(lock);
    }

    template<typename LockType, typename TimeType>
    bool waitUntil(LockType& lock, const TimeType& timeout)
    {
        if (timeout == TimeType::max()) {
            wait(lock);
            return true;
        }
        return waitForImpl(lock, timeout - TimeType::clock::now());
    }

    template<typename LockType>
    bool waitUntilWallClockSeconds(LockType& lock, double absoluteTimeoutSeconds)
    {
        return waitForSecondsImpl(lock, absoluteTimeoutSeconds - currentTime());
    }

    template<typename LockType>
    bool waitUntilMonotonicClockSeconds(LockType& lock, double absoluteTimeoutSeconds)
    {
        return waitForSecondsImpl(lock, absoluteTimeoutSeconds - monotonicallyIncreasingTime());
    }
    
    template<typename LockType, typename Functor>
    bool waitForSeconds(LockType& lock, double relativeTimeoutSeconds, const Functor& predicate)
    {
        double relativeTimeoutNanoseconds = relativeTimeoutSeconds * (1000.0 * 1000.0 * 1000.0);
        
        if (!(relativeTimeoutNanoseconds > 0)) {
            // This handles insta-timeouts as well as NaN.
            lock.unlock();
            lock.lock();
            return false;
        }

        if (relativeTimeoutNanoseconds > static_cast<double>(std::numeric_limits<int64_t>::max())) {
            // If the timeout in nanoseconds cannot be expressed using a 64-bit integer, then we
            // might as well wait forever.
            wait(lock, predicate);
            return true;
        }
        
        auto relativeTimeout =
            std::chrono::nanoseconds(static_cast<int64_t>(relativeTimeoutNanoseconds));

        return waitFor(lock, relativeTimeout, predicate);
    }

    // Note that this method is extremely fast when nobody is waiting. It is not necessary to try to
    // avoid calling this method.
    void notifyOne()
    {
        if (!m_hasWaiters.load()) {
            // At this exact instant, there is nobody waiting on this condition. The way to visualize
            // this is that if unparkOne() ran to completion without obstructions at this moment, it
            // wouldn't wake anyone up. Hence, we have nothing to do!
            return;
        }
        
        ParkingLot::unparkOne(
            &m_hasWaiters,
            [this] (ParkingLot::UnparkResult result) {
                if (!result.mayHaveMoreThreads)
                    m_hasWaiters.store(false);
            });
    }
    
    void notifyAll()
    {
        if (!m_hasWaiters.load()) {
            // See above.
            return;
        }

        // It's totally safe for us to set this to false without any locking, because this thread is
        // guaranteed to then unparkAll() anyway. So, if there is a race with some thread calling
        // wait() just before this store happens, that thread is guaranteed to be awoken by the call to
        // unparkAll(), below.
        m_hasWaiters.store(false);
        
        ParkingLot::unparkAll(&m_hasWaiters);
    }
    
protected:
    template<typename LockType>
    bool waitForSecondsImpl(LockType& lock, double relativeTimeoutSeconds)
    {
        double relativeTimeoutNanoseconds = relativeTimeoutSeconds * (1000.0 * 1000.0 * 1000.0);
        
        if (!(relativeTimeoutNanoseconds > 0)) {
            // This handles insta-timeouts as well as NaN.
            lock.unlock();
            lock.lock();
            return false;
        }

        if (relativeTimeoutNanoseconds > static_cast<double>(std::numeric_limits<int64_t>::max())) {
            // If the timeout in nanoseconds cannot be expressed using a 64-bit integer, then we
            // might as well wait forever.
            wait(lock);
            return true;
        }
        
        auto relativeTimeout =
            std::chrono::nanoseconds(static_cast<int64_t>(relativeTimeoutNanoseconds));

        return waitForImpl(lock, relativeTimeout);
    }
    
    template<typename LockType, typename DurationType>
    bool waitForImpl(LockType& lock, const DurationType& relativeTimeout)
    {
        return waitUntil(lock, absoluteFromRelative(relativeTimeout));
    }

    template<typename DurationType>
    Clock::time_point absoluteFromRelative(const DurationType& relativeTimeout)
    {
        if (relativeTimeout < DurationType::zero())
            return Clock::time_point::min();

        if (relativeTimeout > Clock::duration::max()) {
            // This is highly unlikely. But if it happens, we want to not do anything dumb. Sleeping
            // without a timeout seems sensible when the timeout duration is greater than what can be
            // expressed using steady_clock.
            return Clock::time_point::max();
        }
        
        Clock::duration myRelativeTimeout =
            std::chrono::duration_cast<Clock::duration>(relativeTimeout);

        return Clock::now() + myRelativeTimeout;
    }

    Atomic<bool> m_hasWaiters;
};    

class Condition : public ConditionBase {
    WTF_MAKE_NONCOPYABLE(Condition);
public:
    Condition()
    {
        m_hasWaiters.store(false);
    }
};

typedef ConditionBase StaticCondition;

} // namespace WTF

using WTF::Condition;
using WTF::StaticCondition;

#endif // WTF_Condition_h

