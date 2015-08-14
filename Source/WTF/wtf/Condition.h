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
#include <mutex>
#include <wtf/ParkingLot.h>

namespace WTF {

class Condition {
public:
    Condition() { }

    // Wait on a parking queue while releasing the given lock. It will unlock the lock just before
    // parking, and relock it upon wakeup. Returns true if we woke up due to some call to
    // notifyOne() or notifyAll(). Returns false if we woke up due to a timeout.
    template<typename LockType>
    bool waitUntil(
        LockType& lock, std::chrono::steady_clock::time_point timeout)
    {
        bool result = ParkingLot::parkConditionally(
            &m_dummy,
            [] () -> bool { return true; },
            [&lock] () { lock.unlock(); },
            timeout);
        lock.lock();
        return result;
    }

    // Wait until the given predicate is satisfied. Returns true if it is satisfied in the end.
    // May return early due to timeout.
    template<typename LockType, typename Functor>
    bool waitUntil(
        LockType& lock, std::chrono::steady_clock::time_point timeout, const Functor& predicate)
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
        std::chrono::steady_clock::time_point absoluteTimeout =
            std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(relativeTimeout);

        return waitUntil(lock, absoluteTimeout, predicate);
    }

    template<typename LockType>
    void wait(LockType& lock)
    {
        waitUntil(lock, std::chrono::steady_clock::time_point::max());
    }

    template<typename LockType, typename Functor>
    void wait(LockType& lock, const Functor& predicate)
    {
        while (!predicate())
            wait(lock);
    }

    void notifyOne()
    {
        ParkingLot::unparkOne(&m_dummy);
    }
    
    void notifyAll()
    {
        ParkingLot::unparkAll(&m_dummy);
    }
    
private:
    
    uint8_t m_dummy;
};

} // namespace WTF

using WTF::Condition;

#endif // WTF_Condition_h

