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

#ifndef WTF_ParkingLot_h
#define WTF_ParkingLot_h

#include <functional>
#include <wtf/Atomics.h>
#include <wtf/Threading.h>

namespace WTF {

class ParkingLot {
    ParkingLot() = delete;
    ParkingLot(const ParkingLot&) = delete;

public:
    // Parks the thread in a queue associated with the given address, which cannot be null. The
    // parking only succeeds if the validation function returns true while the queue lock is held.
    // Returns true if the thread actually slept, or false if it returned quickly because of
    // validation failure.
    WTF_EXPORT_PRIVATE static bool parkConditionally(const void* address, std::function<bool()> validation);

    template<typename T, typename U>
    static bool compareAndPark(const Atomic<T>* address, U expected)
    {
        return parkConditionally(
            address,
            [address, expected] () -> bool {
                U value = address->load();
                return value == expected;
            });
    }

    // Unparks one thread from the queue associated with the given address, which cannot be null.
    // Returns true if there may still be other threads on that queue, or false if there definitely
    // are no more threads on the queue.
    WTF_EXPORT_PRIVATE static bool unparkOne(const void* address);

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
    // As well as many other possible interleaves that all have T1 before T2 and T3 before T4 but are
    // otherwise unconstrained. This method is useful primarily for debugging. It's also used by unit
    // tests.
    WTF_EXPORT_PRIVATE static void forEach(std::function<void(ThreadIdentifier, const void*)>);
};

} // namespace WTF

using WTF::ParkingLot;

#endif // WTF_ParkingLot_h

