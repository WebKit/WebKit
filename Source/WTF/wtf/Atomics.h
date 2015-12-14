/*
 * Copyright (C) 2007-2008, 2010, 2012-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Atomics_h
#define Atomics_h

#include <atomic>
#include <wtf/StdLibExtras.h>

#if OS(WINDOWS)
#if !COMPILER(GCC_OR_CLANG)
extern "C" void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#endif
#include <windows.h>
#endif

namespace WTF {

// Atomic wraps around std::atomic with the sole purpose of making the compare_exchange
// operations not alter the expected value. This is more in line with how we typically
// use CAS in our code.
//
// Atomic is a struct without explicitly defined constructors so that it can be
// initialized at compile time.

template<typename T>
struct Atomic {
    // Don't pass a non-default value for the order parameter unless you really know
    // what you are doing and have thought about it very hard. The cost of seq_cst
    // is usually not high enough to justify the risk.

    T load(std::memory_order order = std::memory_order_seq_cst) const { return value.load(order); }

    void store(T desired, std::memory_order order = std::memory_order_seq_cst) { value.store(desired, order); }

    bool compareExchangeWeak(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
#if OS(WINDOWS)
        // Windows makes strange assertions about the argument to compare_exchange_weak, and anyway,
        // Windows is X86 so seq_cst is cheap.
        order = std::memory_order_seq_cst;
#endif
        T expectedOrActual = expected;
        return value.compare_exchange_weak(expectedOrActual, desired, order);
    }

    bool compareExchangeStrong(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
#if OS(WINDOWS)
        // See above.
        order = std::memory_order_seq_cst;
#endif
        T expectedOrActual = expected;
        return value.compare_exchange_strong(expectedOrActual, desired, order);
    }

    std::atomic<T> value;
};

// This is a weak CAS function that takes a direct pointer and has no portable fencing guarantees.
template<typename T>
inline bool weakCompareAndSwap(volatile T* location, T expected, T newValue)
{
    return bitwise_cast<Atomic<T>*>(location)->compareExchangeWeak(expected, newValue, std::memory_order_relaxed);
}

// Just a compiler fence. Has no effect on the hardware, but tells the compiler
// not to move things around this call. Should not affect the compiler's ability
// to do things like register allocation and code motion over pure operations.
inline void compilerFence()
{
#if OS(WINDOWS) && !COMPILER(GCC_OR_CLANG)
    _ReadWriteBarrier();
#else
    asm volatile("" ::: "memory");
#endif
}

#if CPU(ARM_THUMB2) || CPU(ARM64)

// Full memory fence. No accesses will float above this, and no accesses will sink
// below it.
inline void armV7_dmb()
{
    asm volatile("dmb sy" ::: "memory");
}

// Like the above, but only affects stores.
inline void armV7_dmb_st()
{
    asm volatile("dmb st" ::: "memory");
}

inline void loadLoadFence() { armV7_dmb(); }
inline void loadStoreFence() { armV7_dmb(); }
inline void storeLoadFence() { armV7_dmb(); }
inline void storeStoreFence() { armV7_dmb_st(); }
inline void memoryBarrierAfterLock() { armV7_dmb(); }
inline void memoryBarrierBeforeUnlock() { armV7_dmb(); }

#elif CPU(X86) || CPU(X86_64)

inline void x86_mfence()
{
#if OS(WINDOWS)
    // I think that this does the equivalent of a dummy interlocked instruction,
    // instead of using the 'mfence' instruction, at least according to MSDN. I
    // know that it is equivalent for our purposes, but it would be good to
    // investigate if that is actually better.
    MemoryBarrier();
#else
    asm volatile("mfence" ::: "memory");
#endif
}

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { x86_mfence(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }

#else

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { compilerFence(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }

#endif

} // namespace WTF

using WTF::Atomic;

#endif // Atomics_h
