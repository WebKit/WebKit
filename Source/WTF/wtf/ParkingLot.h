/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef WTF_ParkingLot_h
#define WTF_ParkingLot_h

#include <chrono>
#include <functional>
#include <wtf/Atomics.h>
#include <wtf/ScopedLambda.h>
#include <wtf/Threading.h>

namespace WTF {

class ParkingLot {
    ParkingLot() = delete;
    ParkingLot(const ParkingLot&) = delete;

public:
    typedef std::chrono::steady_clock Clock;
    
    // Parks the thread in a queue associated with the given address, which cannot be null. The
    // parking only succeeds if the validation function returns true while the queue lock is held.
    // If validation returns false, it will unlock the internal parking queue and then it will
    // return without doing anything else. If validation returns true, it will enqueue the thread,
    // unlock the parking queue lock, call the beforeSleep function, and then it will sleep so long
    // as the thread continues to be on the queue and the timeout hasn't fired. Finally, this
    // returns true if we actually got unparked or false if the timeout was hit. Note that
    // beforeSleep is called with no locks held, so it's OK to do pretty much anything so long as
    // you don't recursively call parkConditionally(). You can call unparkOne()/unparkAll() though.
    // It's useful to use beforeSleep() to unlock some mutex in the implementation of
    // Condition::wait().
    template<typename ValidationFunctor, typename BeforeSleepFunctor>
    static bool parkConditionally(
        const void* address,
        ValidationFunctor&& validation,
        BeforeSleepFunctor&& beforeSleep,
        Clock::time_point timeout)
    {
        return parkConditionallyImpl(
            address,
            scopedLambda<bool()>(std::forward<ValidationFunctor>(validation)),
            scopedLambda<void()>(std::forward<BeforeSleepFunctor>(beforeSleep)),
            timeout);
    }

    // Simple version of parkConditionally() that covers the most common case: you want to park
    // indefinitely so long as the value at the given address hasn't changed.
    template<typename T, typename U>
    static bool compareAndPark(const Atomic<T>* address, U expected)
    {
        return parkConditionally(
            address,
            [address, expected] () -> bool {
                U value = address->load();
                return value == expected;
            },
            [] () { },
            Clock::time_point::max());
    }

    // Unparks one thread from the queue associated with the given address, which cannot be null.
    // Returns true if there may still be other threads on that queue, or false if there definitely
    // are no more threads on the queue.
    struct UnparkResult {
        bool didUnparkThread { false };
        bool mayHaveMoreThreads { false };
    };
    WTF_EXPORT_PRIVATE static UnparkResult unparkOne(const void* address);

    // Unparks one thread from the queue associated with the given address, and calls the given
    // functor while the address is locked. Reports to the callback whether any thread got unparked
    // and whether there may be any other threads still on the queue. This is an expert-mode version
    // of unparkOne() that allows for really good thundering herd avoidance in adaptive mutexes.
    // Without this, a lock implementation that uses unparkOne() has to have some trick for knowing
    // if there are still threads parked on the queue, so that it can set some bit in its lock word
    // to indicate that the next unlock() also needs to unparkOne(). But there is a race between
    // manipulating that bit and some other thread acquiring the lock. It's possible to work around
    // that race - see Rusty Russel's well-known usersem library - but it's not pretty. This form
    // allows that race to be completely avoided, since there is no way that a thread can be parked
    // while the callback is running.
    template<typename CallbackFunctor>
    static void unparkOne(const void* address, CallbackFunctor&& callback)
    {
        unparkOneImpl(address, scopedLambda<void(UnparkResult)>(std::forward<CallbackFunctor>(callback)));
    }

    // Unparks every thread from the queue associated with the given address, which cannot be null.
    WTF_EXPORT_PRIVATE static void unparkAll(const void* address);

    // Locks the parking lot and walks all of the parked threads and the addresses they are waiting
    // on. Threads that are on the same queue are guaranteed to be walked from first to last, but the
    // queues may be randomly interleaved. For example, if the queue for address A1 has T1 and T2 and
    // the queue for address A2 has T3 and T4, then you might see iteration orders like:
    //
    // A1,T1 A1,T2 A2,T3 A2,T4
    // A2,T3 A2,T4 A1,T1 A1,T2
    // A1,T1 A2,T3 A1,T2 A2,T4
    // A1,T1 A2,T3 A2,T4 A1,T2
    //
    // As well as many other possible interleavings that all have T1 before T2 and T3 before T4 but are
    // otherwise unconstrained. This method is useful primarily for debugging. It's also used by unit
    // tests.
    template<typename CallbackFunctor>
    static void forEach(CallbackFunctor&& callback)
    {
        forEachImpl(scopedLambda<void(ThreadIdentifier, const void*)>(std::forward<CallbackFunctor>(callback)));
    }

private:
    WTF_EXPORT_PRIVATE static bool parkConditionallyImpl(
        const void* address,
        const ScopedLambda<bool()>& validation,
        const ScopedLambda<void()>& beforeSleep,
        Clock::time_point timeout);
    
    WTF_EXPORT_PRIVATE static void unparkOneImpl(
        const void* address, const ScopedLambda<void(UnparkResult)>& callback);

    WTF_EXPORT_PRIVATE static void forEachImpl(const ScopedLambda<void(ThreadIdentifier, const void*)>&);
};

} // namespace WTF

using WTF::ParkingLot;

#endif // WTF_ParkingLot_h

