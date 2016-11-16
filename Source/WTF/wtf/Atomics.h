/*
 * Copyright (C) 2007-2008, 2010, 2012-2016 Apple Inc. All rights reserved.
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
#include <intrin.h>
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
    
    ALWAYS_INLINE T loadRelaxed() const { return load(std::memory_order_relaxed); }

    ALWAYS_INLINE void store(T desired, std::memory_order order = std::memory_order_seq_cst) { value.store(desired, order); }

    ALWAYS_INLINE bool compareExchangeWeak(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_weak(expectedOrActual, desired, order);
    }

    ALWAYS_INLINE bool compareExchangeWeakRelaxed(T expected, T desired)
    {
        return compareExchangeWeak(expected, desired, std::memory_order_relaxed);
    }

    ALWAYS_INLINE bool compareExchangeWeak(T expected, T desired, std::memory_order order_success, std::memory_order order_failure)
    {
        T expectedOrActual = expected;
        return value.compare_exchange_weak(expectedOrActual, desired, order_success, order_failure);
    }

    ALWAYS_INLINE T compareExchangeStrong(T expected, T desired, std::memory_order order = std::memory_order_seq_cst)
    {
        T expectedOrActual = expected;
        value.compare_exchange_strong(expectedOrActual, desired, order);
        return expectedOrActual;
    }

    ALWAYS_INLINE T compareExchangeStrong(T expected, T desired, std::memory_order order_success, std::memory_order order_failure)
    {
        T expectedOrActual = expected;
        value.compare_exchange_strong(expectedOrActual, desired, order_success, order_failure);
        return expectedOrActual;
    }

    template<typename U>
    ALWAYS_INLINE T exchangeAdd(U operand, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_add(operand, order); }
    
    template<typename U>
    ALWAYS_INLINE T exchangeAnd(U operand, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_and(operand, order); }
    
    template<typename U>
    ALWAYS_INLINE T exchangeOr(U operand, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_or(operand, order); }
    
    template<typename U>
    ALWAYS_INLINE T exchangeSub(U operand, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_sub(operand, order); }
    
    template<typename U>
    ALWAYS_INLINE T exchangeXor(U operand, std::memory_order order = std::memory_order_seq_cst) { return value.fetch_xor(operand, order); }
    
    ALWAYS_INLINE T exchange(T newValue, std::memory_order order = std::memory_order_seq_cst) { return value.exchange(newValue, order); }
    
    template<typename Func>
    ALWAYS_INLINE bool tryTransactionRelaxed(const Func& func)
    {
        T oldValue = load(std::memory_order_relaxed);
        T newValue = oldValue;
        func(newValue);
        return compareExchangeWeakRelaxed(oldValue, newValue);
    }

    template<typename Func>
    ALWAYS_INLINE void transactionRelaxed(const Func& func)
    {
        while (!tryTransationRelaxed(func)) { }
    }

    template<typename Func>
    ALWAYS_INLINE bool tryTransaction(const Func& func)
    {
        T oldValue = load(std::memory_order_relaxed);
        T newValue = oldValue;
        func(newValue);
        return compareExchangeWeak(oldValue, newValue);
    }

    template<typename Func>
    ALWAYS_INLINE void transaction(const Func& func)
    {
        while (!tryTransaction(func)) { }
    }

    std::atomic<T> value;
};

template<typename T>
inline T atomicLoad(T* location, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->load(order);
}

template<typename T>
inline void atomicStore(T* location, T newValue, std::memory_order order = std::memory_order_seq_cst)
{
    bitwise_cast<Atomic<T>*>(location)->store(newValue, order);
}

template<typename T>
inline bool atomicCompareExchangeWeak(T* location, T expected, T newValue, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->compareExchangeWeak(expected, newValue, order);
}

template<typename T>
inline bool atomicCompareExchangeWeakRelaxed(T* location, T expected, T newValue)
{
    return bitwise_cast<Atomic<T>*>(location)->compareExchangeWeakRelaxed(expected, newValue);
}

template<typename T>
inline T atomicCompareExchangeStrong(T* location, T expected, T newValue, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->compareExchangeStrong(expected, newValue, order);
}

template<typename T, typename U>
inline T atomicExchangeAdd(T* location, U operand, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchangeAdd(operand, order);
}

template<typename T, typename U>
inline T atomicExchangeAnd(T* location, U operand, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchangeAnd(operand, order);
}

template<typename T, typename U>
inline T atomicExchangeOr(T* location, U operand, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchangeOr(operand, order);
}

template<typename T, typename U>
inline T atomicExchangeSub(T* location, U operand, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchangeSub(operand, order);
}

template<typename T, typename U>
inline T atomicExchangeXor(T* location, U operand, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchangeXor(operand, order);
}

template<typename T>
inline T atomicExchange(T* location, T newValue, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->exchange(newValue, order);
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

inline void arm_isb()
{
    asm volatile("isb" ::: "memory");
}

inline void loadLoadFence() { arm_dmb(); }
inline void loadStoreFence() { arm_dmb(); }
inline void storeLoadFence() { arm_dmb(); }
inline void storeStoreFence() { arm_dmb_st(); }
inline void memoryBarrierAfterLock() { arm_dmb(); }
inline void memoryBarrierBeforeUnlock() { arm_dmb(); }
inline void crossModifyingCodeFence() { arm_isb(); }

#elif CPU(X86) || CPU(X86_64)

inline void x86_ortop()
{
#if OS(WINDOWS)
    MemoryBarrier();
#elif CPU(X86_64)
    // This has acqrel semantics and is much cheaper than mfence. For exampe, in the JSC GC, using
    // mfence as a store-load fence was a 9% slow-down on Octane/splay while using this was neutral.
    asm volatile("lock; orl $0, (%%rsp)" ::: "memory");
#else
    asm volatile("lock; orl $0, (%%esp)" ::: "memory");
#endif
}

inline void x86_cpuid()
{
#if OS(WINDOWS)
    int info[4];
    __cpuid(info, 0);
#elif CPU(X86)
    // GCC 4.9 on x86 in PIC mode can't use %ebx, so we have to save and restore it manually.
    // But since we don't care about what cpuid returns (we use it as a serializing instruction),
    // we can simply throw away what cpuid put in %ebx.
    intptr_t a = 0, c, d;
    asm volatile(
        "pushl %%ebx\n\t"
        "cpuid\n\t"
        "popl %%ebx\n\t"
        : "+a"(a), "=c"(c), "=d"(d)
        :
        : "memory");
#else
    intptr_t a = 0, b, c, d;
    asm volatile(
        "cpuid"
        : "+a"(a), "=b"(b), "=c"(c), "=d"(d)
        :
        : "memory");
#endif
}

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { x86_ortop(); }
inline void storeStoreFence() { compilerFence(); }
inline void memoryBarrierAfterLock() { compilerFence(); }
inline void memoryBarrierBeforeUnlock() { compilerFence(); }
inline void crossModifyingCodeFence() { x86_cpuid(); }

#else

inline void loadLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void loadStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void memoryBarrierAfterLock() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void memoryBarrierBeforeUnlock() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void crossModifyingCodeFence() { std::atomic_thread_fence(std::memory_order_seq_cst); } // Probably not strong enough.

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
