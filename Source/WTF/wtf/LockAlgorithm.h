/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Atomics.h>
#include <wtf/Compiler.h>

namespace WTF {

// This is the algorithm used by WTF::Lock. You can use it to project one lock onto any atomic
// field. The limit of one lock is due to the use of the field's address as a key to find the lock's
// queue.

template<typename LockType>
struct EmptyLockHooks {
    static LockType lockHook(LockType value) { return value; }
    static LockType unlockHook(LockType value) { return value; }
    static LockType parkHook(LockType value) { return value; }
    static LockType handoffHook(LockType value) { return value; }
};

template<typename LockType, LockType isHeldBit, LockType hasParkedBit, typename Hooks = EmptyLockHooks<LockType>>
class LockAlgorithm {
    static constexpr bool verbose = false;
    static constexpr LockType mask = isHeldBit | hasParkedBit;

public:
    static bool lockFastAssumingZero(Atomic<LockType>& lock)
    {
        return lock.compareExchangeWeak(0, Hooks::lockHook(isHeldBit), std::memory_order_acquire);
    }
    
    static bool lockFast(Atomic<LockType>& lock)
    {
        return lock.transaction(
            [&] (LockType& value) -> bool {
                if (value & isHeldBit)
                    return false;
                value |= isHeldBit;
                value = Hooks::lockHook(value);
                return true;
            },
            std::memory_order_acquire);
    }
    
    static void lock(Atomic<LockType>& lock)
    {
        if (UNLIKELY(!lockFast(lock)))
            lockSlow(lock);
    }
    
    static bool tryLock(Atomic<LockType>& lock)
    {
        for (;;) {
            LockType currentValue = lock.load(std::memory_order_relaxed);
            if (currentValue & isHeldBit)
                return false;
            if (lock.compareExchangeWeak(currentValue, Hooks::lockHook(currentValue | isHeldBit), std::memory_order_acquire))
                return true;
        }
    }

    static bool unlockFastAssumingZero(Atomic<LockType>& lock)
    {
        return lock.compareExchangeWeak(isHeldBit, Hooks::unlockHook(0), std::memory_order_release);
    }
    
    static bool unlockFast(Atomic<LockType>& lock)
    {
        return lock.transaction(
            [&] (LockType& value) -> bool {
                if ((value & mask) != isHeldBit)
                    return false;
                value &= ~isHeldBit;
                value = Hooks::unlockHook(value);
                return true;
            },
            std::memory_order_relaxed);
    }
    
    static void unlock(Atomic<LockType>& lock)
    {
        if (UNLIKELY(!unlockFast(lock)))
            unlockSlow(lock, Unfair);
    }
    
    static void unlockFairly(Atomic<LockType>& lock)
    {
        if (UNLIKELY(!unlockFast(lock)))
            unlockSlow(lock, Fair);
    }
    
    static bool safepointFast(const Atomic<LockType>& lock)
    {
        WTF::compilerFence();
        return !(lock.load(std::memory_order_relaxed) & hasParkedBit);
    }
    
    static void safepoint(Atomic<LockType>& lock)
    {
        if (UNLIKELY(!safepointFast(lock)))
            safepointSlow(lock);
    }
    
    static bool isLocked(const Atomic<LockType>& lock)
    {
        return lock.load(std::memory_order_acquire) & isHeldBit;
    }
    
    NEVER_INLINE static void lockSlow(Atomic<LockType>& lock);
    
    enum Fairness {
        Unfair,
        Fair
    };
    NEVER_INLINE static void unlockSlow(Atomic<LockType>& lock, Fairness fairness = Unfair);
    
    NEVER_INLINE static void safepointSlow(Atomic<LockType>& lockWord)
    {
        unlockFairly(lockWord);
        lock(lockWord);
    }
    
private:
    enum Token {
        BargingOpportunity,
        DirectHandoff
    };
};

} // namespace WTF

using WTF::LockAlgorithm;
