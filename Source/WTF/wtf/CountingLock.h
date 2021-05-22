/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/DataLog.h>
#include <wtf/LockAlgorithm.h>

namespace WTF {

// This is mostly just a word-sized WTF::Lock. It supports basically everything that lock supports. But as
// a bonus, it atomically counts lock() calls and allows you to perform an optimistic read transaction by
// comparing the count before and after the transaction. If at the start of the transaction the lock is
// not held and the count remains the same throughout the transaction, then you know that nobody could
// have modified your data structure while you ran. You can even use this to optimistically read pointers
// that could become dangling under concurrent writes, if you just revalidate the count every time you're
// about to do something dangerous.
//
// This is largely inspired by StampedLock from Java:
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/CountingLock.html
//
// This is simplified a lot compared to StampedLock. Unlike StampedLock, it uses an exclusive lock as a
// fallback. There is no way to acquire a CountingLock for read. The only read access is via optimistic
// read transactions.
//
// CountingLock provides two ways of doing optimistic reads:
//
// - The easy way, where CountingLock does all of the fencing for you. That fencing is free on x86 but
//   somewhat expensive on ARM.
// - The hard way, where you do fencing yourself using Dependency. This allows you to be fenceless on both
//   x86 and ARM.
//
// The latter is important for us because some GC paths are known to be sensitive to fences on ARM.

class CountingLock final {
    WTF_MAKE_NONCOPYABLE(CountingLock);
    WTF_MAKE_FAST_ALLOCATED;

    typedef unsigned LockType;
    
    static constexpr LockType isHeldBit = 1;
    static constexpr LockType hasParkedBit = 2;
    static constexpr LockType mask = isHeldBit | hasParkedBit;
    static constexpr LockType shift = 2;
    static constexpr LockType countUnit = 4;
    
    struct LockHooks {
        static LockType lockHook(LockType value)
        {
            return value + countUnit;
        }
        
        static LockType unlockHook(LockType value) { return value; }
        static LockType parkHook(LockType value) { return value; }
        static LockType handoffHook(LockType value) { return value; }
    };
    
    typedef LockAlgorithm<LockType, isHeldBit, hasParkedBit, LockHooks> ExclusiveAlgorithm;
    
public:
    CountingLock() = default;
    
    bool tryLock()
    {
        return ExclusiveAlgorithm::tryLock(m_word);
    }

    void lock()
    {
        if (UNLIKELY(!ExclusiveAlgorithm::lockFast(m_word)))
            lockSlow();
    }
    
    void unlock()
    {
        if (UNLIKELY(!ExclusiveAlgorithm::unlockFast(m_word)))
            unlockSlow();
    }
    
    bool isHeld() const
    {
        return ExclusiveAlgorithm::isLocked(m_word);
    }
    
    bool isLocked() const
    {
        return isHeld();
    }
    
    // The only thing you're allowed to infer from this value is that if it's zero, then you didn't get
    // a real count.
    class Count {
    public:
        explicit operator bool() const { return !!m_value; }
        
        bool operator==(const Count& other) const { return m_value == other.m_value; }
        bool operator!=(const Count& other) const { return m_value != other.m_value; }
        
    private:
        friend class CountingLock;
        
        LockType m_value { 0 };
    };
    
    // Example of how to use this:
    //
    //     int read()
    //     {
    //         if (CountingLock::Count count = m_lock.tryOptimisticRead()) {
    //             int value = m_things;
    //             if (m_lock.validate(count))
    //                 return value; // success!
    //         }
    //         Locker locker { m_lock };
    //         int value = m_things;
    //         return value;
    //     }
    //
    // If tryOptimisitcRead() runs when the lock is not held, this thread will run a critical section
    // without ever writing to memory. However, on ARM, this requires fencing. We use a load-acquire for
    // tryOptimisticRead(). We have no choice but to use the more expensive `dmb ish` in validate(). If
    // you want to avoid that, you could try to use tryOptimisticFencelessRead().
    Count tryOptimisticRead()
    {
        LockType currentValue = m_word.load();
        // FIXME: We could eliminate this check, if we think it's OK to proceed with the optimistic read
        // path even after knowing that it must fail. That's probably good for perf since we expect
        // failure to be super rare. We would get rid of this check and instead of calling getCount below,
        // we would return currentValue ^ mask. If the lock state was empty to begin with, the result
        // would be a properly blessed count (both low bits set). If the lock state was anything else, we
        // would get an improperly blessed count that would not possibly succeed in validate. We could
        // actually do something like "return (currentValue | hasParkedBit) ^ isHeldBit", which would mean
        // that we allow parked-but-not-held-locks through.
        // https://bugs.webkit.org/show_bug.cgi?id=180394
        if (currentValue & isHeldBit)
            return Count();
        return getCount(currentValue);
    }
    
    bool validate(Count count)
    {
        WTF::loadLoadFence();
        LockType currentValue = m_word.loadRelaxed();
        return getCount(currentValue) == count;
    }
    
    // Example of how to use this:
    //
    //     int read()
    //     {
    //         return m_lock.doOptimizedRead(
    //             [&] () -> int {
    //                 int value = m_things;
    //                 return value;
    //             });
    //     }
    template<typename Func>
    auto doOptimizedRead(const Func& func)
    {
        Count count = tryOptimisticRead();
        if (count) {
            auto result = func();
            if (validate(count))
                return result;
        }
        lock();
        auto result = func();
        unlock();
        return result;
    }
    
    // Example of how to use this:
    //
    //     int read()
    //     {
    //         auto result = m_lock.tryOptimisticFencelessRead();
    //         if (CountingLock::Count count = result.value) {
    //             Dependency fenceBefore = Dependency::fence(result.input);
    //             auto* fencedThis = fenceBefore.consume(this);
    //             int value = fencedThis->m_things;
    //             if (m_lock.fencelessValidate(count, Dependency::fence(value)))
    //                 return value; // success!
    //         }
    //         Locker locker { m_lock };
    //         int value = m_things;
    //         return value;
    //     }
    //
    // Use this to create a read transaction using dependency chains only. You have to be careful to
    // thread the dependency input (the `input` field that the returns) through a Dependency, and then
    // thread that Dependency into every load (except for loads that are chasing pointers loaded from
    // loads that already uses that dependency). Then, to validate the read transaction, you have to pass
    // both the count and another Dependency that is based on whatever loads you used to produce the
    // output.
    //
    // On non-ARM platforms, the Dependency objects don't do anything except for Dependency::fence, which
    // is a load-load fence. The idiom above does the right thing on both ARM and TSO.
    //
    // WARNING: This can be hard to get right. Please only use this for very short critical sections that
    // are known to be sufficiently perf-critical to justify the risk.
    InputAndValue<LockType, Count> tryOptimisticFencelessRead()
    {
        LockType currentValue = m_word.loadRelaxed();
        if (currentValue & isHeldBit)
            return inputAndValue(currentValue, Count());
        return inputAndValue(currentValue, getCount(currentValue));
    }
    
    bool fencelessValidate(Count count, Dependency dependency)
    {
        LockType currentValue = dependency.consume(this)->m_word.loadRelaxed();
        return getCount(currentValue) == count;
    }
    
    template<typename OptimisticFunc, typename Func>
    auto doOptimizedFencelessRead(const OptimisticFunc& optimisticFunc, const Func& func)
    {
        auto count = tryOptimisticFencelessRead();
        if (count.value) {
            Dependency dependency = Dependency::fence(count.input);
            auto result = optimisticFunc(dependency, count.value);
            if (fencelessValidate(count.value, dependency))
                return result;
        }
        lock();
        auto result = func();
        unlock();
        return result;
    }
    
private:
    WTF_EXPORT_PRIVATE void lockSlow();
    WTF_EXPORT_PRIVATE void unlockSlow();
    
    Count getCount(LockType value)
    {
        Count result;
        result.m_value = value | mask;
        return result;
    }
    
    Atomic<LockType> m_word { 0 };
};

} // namespace WTF

using WTF::CountingLock;

