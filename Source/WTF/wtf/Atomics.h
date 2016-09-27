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

    ALWAYS_INLINE T load(std::memory_order order = std::memory_order_seq_cst) const { return value.load(order); }

    ALWAYS_INLINE void store(T desired, std::memory_order order = std::memory_order_seq_cst) { value.store(desired, order); }

    ALWAYS_INLINE bool compareExchangeWeak(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_weak(expectedOrActual, desired, order);
    }

    ALWAYS_INLINE bool compareExchangeWeak(T expected, T desired, std::memory_order order_success, std::memory_order order_failure)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_weak(expectedOrActual, desired, order_success, order_failure);
    }

    ALWAYS_INLINE bool compareExchangeStrong(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_strong(expectedOrActual, desired, order);
    }

    ALWAYS_INLINE bool compareExchangeStrong(T expected, T desired, std::memory_order order_success, std::memory_order order_failure)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_strong(expectedOrActual, desired, order_success, order_failure);
    }

    template<typename U>
    ALWAYS_INLINE T exchangeAndAdd(U addend, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_add(addend, order); }
    
    ALWAYS_INLINE T exchange(T newValue, std::memory_order order = std::memory_order_seq_cst) { return value.exchange(newValue, order); }

    std::atomic<T> value;
};

// This is a weak CAS function that takes a direct pointer and has no portable fencing guarantees.
template<typename T>
inline bool weakCompareAndSwap(volatile T* location, T expected, T newValue)
{
    ASSERT(isPointerTypeAlignmentOkay(location) && "natural alignment required");
    ASSERT(bitwise_cast<std::atomic<T>*>(location)->is_lock_free() && "expected lock-free type");
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
inline void arm_dmb()
{
    asm volatile("dmb ish" ::: "memory");
}

// Like the above, but only affects stores.
inline void arm_dmb_st()
{
    asm volatile("dmb ishst" ::: "memory");
}

inline void loadLoadFence() { arm_dmb(); }
inline void loadStoreFence() { arm_dmb(); }
inline void storeLoadFence() { arm_dmb(); }
inline void storeStoreFence() { arm_dmb_st(); }
inline void memoryBarrierAfterLock() { arm_dmb(); }
inline void memoryBarrierBeforeUnlock() { arm_dmb(); }

#elif CPU(X86) || CPU(X86_64)

inline void x86_ortop()
{
#if OS(WINDOWS)
    // I think that this does the equivalent of a dummy interlocked instruction,
    // instead of using the 'mfence' instruction, at least according to MSDN. I
    // know that it is equivalent for our purposes, but it would be good to
    // investigate if that is actually better.
    MemoryBarrier();
#elif CPU(X86_64)
    // This has acqrel semantics and is much cheaper than mfence. For exampe, in the JSC GC, using
    // mfence as a store-load fence was a 9% slow-down on Octane/splay while using this was neutral.
    asm volatile("lock; orl $0, (%%rsp)" ::: "memory");
#else
    asm volatile("lock; orl $0, (%%esp)" ::: "memory");
#endif
}

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { x86_ortop(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }

#else

inline void loadLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void loadStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void memoryBarrierAfterLock() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void memoryBarrierBeforeUnlock() { std::atomic_thread_fence(std::memory_order_seq_cst); }

#endif

typedef size_t ConsumeDependency;

template <typename T, typename std::enable_if<sizeof(T) == 8>::type* = nullptr>
ALWAYS_INLINE ConsumeDependency zeroWithConsumeDependency(T value)
{
    uint64_t dependency;
    uint64_t copy = bitwise_cast<uint64_t>(value);
#if CPU(ARM64)
    // Create a magical zero value through inline assembly, whose computation
    // isn't visible to the optimizer. This zero is then usable as an offset in
    // further address computations: adding zero does nothing, but the compiler
    // doesn't know it. It's magical because it creates an address dependency
    // from the load of `location` to the uses of the dependency, which triggers
    // the ARM ISA's address dependency rule, a.k.a. the mythical C++ consume
    // ordering. This forces weak memory order CPUs to observe `location` and
    // dependent loads in their store order without the reader using a barrier
    // or an acquire load.
    asm volatile("eor %x[dependency], %x[in], %x[in]"
                 : [dependency] "=r"(dependency)
                 : [in] "r"(copy)
                 // Lie about touching memory. Not strictly needed, but is
                 // likely to avoid unwanted load/store motion.
                 : "memory");
#elif CPU(ARM)
    asm volatile("eor %[dependency], %[in], %[in]"
                 : [dependency] "=r"(dependency)
                 : [in] "r"(copy)
                 : "memory");
#else
    // No dependency is needed for this architecture.
    loadLoadFence();
    dependency = 0;
    (void)copy;
#endif
    return static_cast<ConsumeDependency>(dependency);
}

template <typename T, typename std::enable_if<sizeof(T) == 4>::type* = nullptr>
ALWAYS_INLINE ConsumeDependency zeroWithConsumeDependency(T value)
{
    uint32_t dependency;
    uint32_t copy = bitwise_cast<uint32_t>(value);
#if CPU(ARM64)
    asm volatile("eor %w[dependency], %w[in], %w[in]"
                 : [dependency] "=r"(dependency)
                 : [in] "r"(copy)
                 : "memory");
#elif CPU(ARM)
    asm volatile("eor %[dependency], %[in], %[in]"
                 : [dependency] "=r"(dependency)
                 : [in] "r"(copy)
                 : "memory");
#else
    loadLoadFence();
    dependency = 0;
    (void)copy;
#endif
    return static_cast<ConsumeDependency>(dependency);
}

template <typename T, typename std::enable_if<sizeof(T) == 2>::type* = nullptr>
ALWAYS_INLINE ConsumeDependency zeroWithConsumeDependency(T value)
{
    uint16_t copy = bitwise_cast<uint16_t>(value);
    return zeroWithConsumeDependency(static_cast<size_t>(copy));
}

template <typename T, typename std::enable_if<sizeof(T) == 1>::type* = nullptr>
ALWAYS_INLINE ConsumeDependency zeroWithConsumeDependency(T value)
{
    uint8_t copy = bitwise_cast<uint8_t>(value);
    return zeroWithConsumeDependency(static_cast<size_t>(copy));
}

template <typename T>
struct Consumed {
    T value;
    ConsumeDependency dependency;
};

// Consume load, returning the loaded `value` at `location` and a dependent-zero
// which creates an address dependency from the `location`.
//
// Usage notes:
//
//  * Regarding control dependencies: merely branching based on `value` or
//    `dependency` isn't sufficient to impose a dependency ordering: you must
//    use `dependency` in the address computation of subsequent loads which
//    should observe the store order w.r.t. `location`.
// * Regarding memory ordering: consume load orders the `location` load with
//   susequent dependent loads *only*. It says nothing about ordering of other
//   loads!
//
// Caveat emptor.
template <typename T>
ALWAYS_INLINE auto consumeLoad(const T* location)
{
    typedef typename std::remove_cv<T>::type Returned;
    Consumed<Returned> ret { };
    // Force the read of `location` to occur exactly once and without fusing or
    // forwarding using volatile. This is important because the compiler could
    // otherwise rematerialize or find equivalent loads, or simply forward from
    // a previous one, and lose the dependency we're trying so hard to
    // create. Prevent tearing by using an atomic, but let it move around by
    // using relaxed. We have at least a memory fence after this which prevents
    // the load from moving too much.
    ret.value = reinterpret_cast<const volatile std::atomic<Returned>*>(location)->load(std::memory_order_relaxed);
    ret.dependency = zeroWithConsumeDependency(ret.value);
    return ret;
}

} // namespace WTF

using WTF::Atomic;
using WTF::ConsumeDependency;
using WTF::consumeLoad;

#endif // Atomics_h
