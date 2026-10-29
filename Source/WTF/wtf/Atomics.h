/*
 * Copyright (C) 2007-2017, 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <atomic>
#include <wtf/FastMalloc.h>
#include <wtf/StdIntExtras.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// Unlike C++, code using WTF's atomics may rely on multi-copy atomicity: a store, atomic or not, becomes
// visible to all other threads at the same time (the storing thread may see it earlier). ARM64 and x86_64
// provide this in hardware, so code that relies on it must only race on those CPUs. ARMv7, for example,
// doesn't guarantee it.
enum class MemoryOrder : uint8_t {
    // No ordering relative to other memory accesses.
    Relaxed,
    // Loads and read-modify-writes only. Later loads and stores can't move above it, even without a paired
    // Release.
    Acquire,
    // Stores and read-modify-writes only. Earlier loads and stores can't move below it, even without a paired
    // Acquire. Release and Acquire are RCpc: a Release store followed by an Acquire load of a different
    // location may be reordered, including the reads and writes of read-modify-writes.
    Release,
    // Read-modify-writes only. Acquire on its read and Release on its write, so an earlier access and a later
    // access can both move between them (as in a load/store-exclusive loop), and then be reordered with each
    // other.
    AcquireRelease,
    // Acquire for loads, Release for stores, and AcquireRelease for read-modify-writes. All SeqCst operations
    // also have a single total order (RCsc: a SeqCst store followed by a SeqCst load is never reordered, but
    // if either is only Release or Acquire, the pair is RCpc). Non-SeqCst accesses are ordered only by the
    // Acquire/Release part: an earlier store can still move below a later SeqCst load, and accesses around a
    // SeqCst read-modify-write can be reordered as with AcquireRelease. Use FullyFenced when nothing may move
    // across an access.
    SeqCst,
    // Read-modify-writes only. No load or store, atomic or not, can move across it in either direction.
    // Unlike SeqCst, this also keeps its own write and any earlier store above a later load, which
    // Dekker-style protocols need (e.g. two threads that each set their own flag, then check the other's).
    // A compare-exchange that fails stores nothing, so it's only as strong as a load with its failure order.
    FullyFenced,
};

ALWAYS_INLINE constexpr std::memory_order toStdMemoryOrder(MemoryOrder order)
{
    switch (order) {
    case MemoryOrder::Relaxed:
        return std::memory_order_relaxed;
    case MemoryOrder::Acquire:
        return std::memory_order_acquire;
    case MemoryOrder::Release:
        return std::memory_order_release;
    case MemoryOrder::AcquireRelease:
        return std::memory_order_acq_rel;
    case MemoryOrder::SeqCst:
    case MemoryOrder::FullyFenced:
        return std::memory_order_seq_cst;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr bool isValidLoadOrder(MemoryOrder order)
{
    return order == MemoryOrder::Relaxed || order == MemoryOrder::Acquire || order == MemoryOrder::SeqCst;
}

constexpr bool isValidStoreOrder(MemoryOrder order)
{
    return order == MemoryOrder::Relaxed || order == MemoryOrder::Release || order == MemoryOrder::SeqCst;
}

constexpr MemoryOrder defaultCompareExchangeFailureOrder(MemoryOrder orderSuccess)
{
    switch (orderSuccess) {
    case MemoryOrder::Relaxed:
    case MemoryOrder::Release:
        return MemoryOrder::Relaxed;
    case MemoryOrder::Acquire:
    case MemoryOrder::AcquireRelease:
        return MemoryOrder::Acquire;
    case MemoryOrder::SeqCst:
    case MemoryOrder::FullyFenced:
        return MemoryOrder::SeqCst;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// A lock-free SeqCst read-modify-write of an integer or pointer is already a full fence on x86 (any lock-prefixed
// instruction) and on ARM64 with LSE (the acquire-release forms). Other types, like floating point, can still
// compile to a load/store-exclusive loop, which isn't.
// FIXME: 16 byte integers compile to a caspal loop on ARM64 with LSE, which is also a full fence, so they could skip
// the fence too if anything needs it.
template<MemoryOrder order, typename T>
ALWAYS_INLINE void fenceAfterReadModifyWriteIfNecessary()
{
    if constexpr (order == MemoryOrder::FullyFenced) {
#if CPU(X86) || CPU(X86_64) || (CPU(ARM64) && defined(__ARM_FEATURE_ATOMICS))
        if constexpr ((std::is_integral_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>) && sizeof(T) <= sizeof(uint64_t))
            static_assert(std::atomic_ref<T>::is_always_lock_free);
        else
            std::atomic_thread_fence(std::memory_order_seq_cst);
#else
        std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
    }
}

constexpr bool hasAcquire(MemoryOrder order)
{
    return order != MemoryOrder::Relaxed && order != MemoryOrder::Release;
}

constexpr bool hasRelease(MemoryOrder order)
{
    return order != MemoryOrder::Relaxed && order != MemoryOrder::Acquire;
}

// Just a compiler fence. Has no effect on the hardware, but tells the compiler
// not to move things around this call. Should not affect the compiler's ability
// to do things like register allocation and code motion over pure operations.
inline void compilerFence()
{
    asm volatile("" ::: "memory");
}

// ISO C++ only orders an Acquire after a Release it reads from, so the compiler could otherwise move a later
// access above an Acquire, or an earlier access below a Release, when it can prove no pairing happens.
// FullyFenced uses compilerFence() because a signal fence doesn't formally stop an earlier store from moving
// below a later load. Every CPU we support already orders the underlying atomic instructions as MemoryOrder
// documents.
template<MemoryOrder order>
SUPPRESS_NODELETE ALWAYS_INLINE void LEGACY_NODELETE compilerFenceBefore()
{
    if constexpr (order == MemoryOrder::FullyFenced)
        compilerFence();
    else if constexpr (hasRelease(order))
        std::atomic_signal_fence(std::memory_order_release);
}

template<MemoryOrder order>
SUPPRESS_NODELETE ALWAYS_INLINE void LEGACY_NODELETE compilerFenceAfter()
{
    if constexpr (order == MemoryOrder::FullyFenced)
        compilerFence();
    else if constexpr (hasAcquire(order))
        std::atomic_signal_fence(std::memory_order_acquire);
}

// Atomic wraps around std::atomic and makes the compare_exchange operations not alter
// the expected value. This is more in line with how we typically use CAS in our code.
//
// Atomic is a struct without explicitly defined constructors so that it can be
// initialized at compile time.

class Dependency;

template<typename T>
struct Atomic {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Atomic);

    // Don't pass a non-default value for the order template argument unless you really know
    // what you are doing and have thought about it very hard. The cost of seq_cst
    // is usually not high enough to justify the risk.

    template<MemoryOrder order = MemoryOrder::SeqCst>
    ALWAYS_INLINE T load() const
    {
        static_assert(isValidLoadOrder(order));
        T result = value.load(toStdMemoryOrder(order));
        compilerFenceAfter<order>();
        return result;
    }

    // FIXME: Decide whether the named orders (loadRelaxed(), storeRelaxed(), compareExchangeWeakRelaxed(),
    // transactionRelaxed(), loadFullyFenced(), storeFullyFenced()) should stay alongside the MemoryOrder
    // template parameter or be replaced by it.
    ALWAYS_INLINE T loadRelaxed() const { return load<MemoryOrder::Relaxed>(); }

    Dependency loadAndFence(T& output) const;

    // This is a load that simultaneously does a full fence - neither loads nor stores will move
    // above or below it.
    ALWAYS_INLINE T loadFullyFenced() const
    {
        return const_cast<Atomic*>(this)->template exchangeAdd<MemoryOrder::FullyFenced>(T());
    }

    template<MemoryOrder order = MemoryOrder::SeqCst>
    ALWAYS_INLINE void store(T desired)
    {
        static_assert(isValidStoreOrder(order));
        compilerFenceBefore<order>();
        value.store(desired, toStdMemoryOrder(order));
    }

    ALWAYS_INLINE void storeRelaxed(T desired) { store<MemoryOrder::Relaxed>(desired); }

    // This is a store that simultaneously does a full fence - neither loads nor stores will move
    // above or below it.
    ALWAYS_INLINE void storeFullyFenced(T desired)
    {
        exchange<MemoryOrder::FullyFenced>(desired);
    }

    template<MemoryOrder orderSuccess = MemoryOrder::SeqCst, MemoryOrder orderFailure = defaultCompareExchangeFailureOrder(orderSuccess)>
    ALWAYS_INLINE bool compareExchangeWeak(T expected, T desired)
    {
        static_assert(isValidLoadOrder(orderFailure));
        T expectedOrActual = expected;
        compilerFenceBefore<orderSuccess>();
        bool result = value.compare_exchange_weak(expectedOrActual, desired, toStdMemoryOrder(orderSuccess), toStdMemoryOrder(orderFailure));
        fenceAfterReadModifyWriteIfNecessary<orderSuccess, T>();
        compilerFenceAfter<orderSuccess>();
        compilerFenceAfter<orderFailure>();
        return result;
    }

    ALWAYS_INLINE bool compareExchangeWeakRelaxed(T expected, T desired)
    {
        return compareExchangeWeak<MemoryOrder::Relaxed>(expected, desired);
    }

    // WARNING: This does not have strong fencing guarantees when it fails. For example, stores could
    // sink below it in that case.
    template<MemoryOrder orderSuccess = MemoryOrder::SeqCst, MemoryOrder orderFailure = defaultCompareExchangeFailureOrder(orderSuccess)>
    ALWAYS_INLINE T compareExchangeStrong(T expected, T desired)
    {
        static_assert(isValidLoadOrder(orderFailure));
        T expectedOrActual = expected;
        compilerFenceBefore<orderSuccess>();
        value.compare_exchange_strong(expectedOrActual, desired, toStdMemoryOrder(orderSuccess), toStdMemoryOrder(orderFailure));
        fenceAfterReadModifyWriteIfNecessary<orderSuccess, T>();
        compilerFenceAfter<orderSuccess>();
        compilerFenceAfter<orderFailure>();
        return expectedOrActual;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst, typename U>
    ALWAYS_INLINE T exchangeAdd(U operand)
    {
        compilerFenceBefore<order>();
        T result = value.fetch_add(operand, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst, typename U>
    ALWAYS_INLINE T exchangeAnd(U operand)
    {
        compilerFenceBefore<order>();
        T result = value.fetch_and(operand, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst, typename U>
    ALWAYS_INLINE T exchangeOr(U operand)
    {
        compilerFenceBefore<order>();
        T result = value.fetch_or(operand, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst, typename U>
    ALWAYS_INLINE T exchangeSub(U operand)
    {
        compilerFenceBefore<order>();
        T result = value.fetch_sub(operand, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst, typename U>
    ALWAYS_INLINE T exchangeXor(U operand)
    {
        compilerFenceBefore<order>();
        T result = value.fetch_xor(operand, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst>
    ALWAYS_INLINE T exchange(T newValue)
    {
        compilerFenceBefore<order>();
        T result = value.exchange(newValue, toStdMemoryOrder(order));
        fenceAfterReadModifyWriteIfNecessary<order, T>();
        compilerFenceAfter<order>();
        return result;
    }

    // func is supposed to return false if the value is already in the desired state.
    // Returns true if the value was changed. Else returns false.
    template<MemoryOrder order = MemoryOrder::SeqCst>
    ALWAYS_INLINE bool transaction(NOESCAPE const Invocable<bool(T&)> auto& func)
    {
        // FIXME: Support FullyFenced once transaction can take separate success and failure orders. When func
        // returns false we've only done a Relaxed load.
        static_assert(order != MemoryOrder::FullyFenced);
        for (;;) {
            T oldValue = load<MemoryOrder::Relaxed>();
            T newValue = oldValue;
            if (!func(newValue))
                return false;
            if (compareExchangeWeak<order>(oldValue, newValue))
                return true;
        }
    }

    // func is supposed to return false if the value is already in the desired state.
    // Returns true if the value was changed. Else returns false.
    template<typename Func>
    ALWAYS_INLINE bool transactionRelaxed(NOESCAPE const Func& func)
    {
        return transaction<MemoryOrder::Relaxed>(func);
    }

    Atomic() = default;
    constexpr Atomic(T initial)
        : value(std::forward<T>(initial))
    {
    }

    std::atomic<T> value;
};

template<MemoryOrder order = MemoryOrder::SeqCst, typename T>
inline T atomicLoad(T* location)
{
    static_assert(isValidLoadOrder(order));
    T result = std::atomic_ref<T>(*location).load(toStdMemoryOrder(order));
    compilerFenceAfter<order>();
    return result;
}

template<typename T>
inline T atomicLoadFullyFenced(T* location)
{
    compilerFenceBefore<MemoryOrder::FullyFenced>();
    T result = std::atomic_ref<T>(*location).fetch_add(T(), toStdMemoryOrder(MemoryOrder::FullyFenced));
    fenceAfterReadModifyWriteIfNecessary<MemoryOrder::FullyFenced, T>();
    compilerFenceAfter<MemoryOrder::FullyFenced>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T>
inline void atomicStore(T* location, T newValue)
{
    static_assert(isValidStoreOrder(order));
    compilerFenceBefore<order>();
    std::atomic_ref<T>(*location).store(newValue, toStdMemoryOrder(order));
}

template<typename T>
inline void atomicStoreFullyFenced(T* location, T newValue)
{
    compilerFenceBefore<MemoryOrder::FullyFenced>();
    std::atomic_ref<T>(*location).exchange(newValue, toStdMemoryOrder(MemoryOrder::FullyFenced));
    fenceAfterReadModifyWriteIfNecessary<MemoryOrder::FullyFenced, T>();
    compilerFenceAfter<MemoryOrder::FullyFenced>();
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T>
inline bool atomicCompareExchangeWeak(T* location, T expected, T newValue)
{
    T expectedOrActual = expected;
    compilerFenceBefore<order>();
    bool result = std::atomic_ref<T>(*location).compare_exchange_weak(expectedOrActual, newValue, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<typename T>
inline bool atomicCompareExchangeWeakRelaxed(T* location, T expected, T newValue)
{
    return atomicCompareExchangeWeak<MemoryOrder::Relaxed>(location, expected, newValue);
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T>
inline T atomicCompareExchangeStrong(T* location, T expected, T newValue)
{
    T expectedOrActual = expected;
    compilerFenceBefore<order>();
    std::atomic_ref<T>(*location).compare_exchange_strong(expectedOrActual, newValue, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return expectedOrActual;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T, typename U>
inline T atomicExchangeAdd(T* location, U operand)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).fetch_add(operand, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T, typename U>
inline T atomicExchangeAnd(T* location, U operand)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).fetch_and(operand, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T, typename U>
inline T atomicExchangeOr(T* location, U operand)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).fetch_or(operand, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T, typename U>
inline T atomicExchangeSub(T* location, U operand)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).fetch_sub(operand, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T, typename U>
inline T atomicExchangeXor(T* location, U operand)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).fetch_xor(operand, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
}

template<MemoryOrder order = MemoryOrder::SeqCst, typename T>
inline T atomicExchange(T* location, T newValue)
{
    compilerFenceBefore<order>();
    T result = std::atomic_ref<T>(*location).exchange(newValue, toStdMemoryOrder(order));
    fenceAfterReadModifyWriteIfNecessary<order, T>();
    compilerFenceAfter<order>();
    return result;
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

// Like the above, but only loads before the barrier are constrained. Accesses after the
// barrier are constrained whether they are loads or stores.
inline void arm_dmb_ld()
{
#if CPU(ARM64)
    asm volatile("dmb ishld" ::: "memory");
#else
    // ARMv7 doesn't have the load only variant of dmb
    arm_dmb();
#endif
}

inline void arm_isb()
{
    asm volatile("isb" ::: "memory");
}

inline void loadLoadFence() { arm_dmb_ld(); }
inline void loadStoreFence() { arm_dmb_ld(); }
inline void storeLoadFence() { arm_dmb(); }
inline void storeStoreFence() { arm_dmb_st(); }
inline void crossModifyingCodeFence() { arm_isb(); }

#elif CPU(X86) || CPU(X86_64)

inline void x86_ortop()
{
#if CPU(X86_64)
    // This has acqrel semantics and is much cheaper than mfence. For exampe, in the JSC GC, using
    // mfence as a store-load fence was a 9% slow-down on Octane/splay while using this was neutral.
    asm volatile("lock; orl $0, (%%rsp)" ::: "memory");
#else
    asm volatile("lock; orl $0, (%%esp)" ::: "memory");
#endif
}

inline void x86_cpuid()
{
    intptr_t a = 0, b, c, d;
    asm volatile(
        "cpuid"
        : "+a"(a), "=b"(b), "=c"(c), "=d"(d)
        :
        : "memory");
}

inline void loadLoadFence() { compilerFence(); }
inline void loadStoreFence() { compilerFence(); }
inline void storeLoadFence() { x86_ortop(); }
inline void storeStoreFence() { compilerFence(); }
inline void crossModifyingCodeFence() { x86_cpuid(); }

#else

inline void loadLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void loadStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeLoadFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void storeStoreFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }
inline void crossModifyingCodeFence() { std::atomic_thread_fence(std::memory_order_seq_cst); } // Probably not strong enough.

#endif

#if CPU(ARM64) || CPU(X86) || CPU(X86_64)
// Use this fence if you want a fence between loads that are already depdendent.
inline void dependentLoadLoadFence() { compilerFence(); }
#else
inline void dependentLoadLoadFence() { loadLoadFence(); }
#endif

// We use this primitive to hide an atomic variable from the optimizer.
template<typename T>
inline T opaque(T value)
{
    asm volatile("" : "+r"(value) ::);
    return value;
}

using InternalDependencyType = UCPURegister;

// We use this primitive on ARM to express memory ordering efficiently.
template<typename T>
inline T* addOpaqueZero(T* pointer, InternalDependencyType opaqueZero)
{
    ASSERT(!opaque(opaqueZero));

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // @safe
    // Safety: This is bounds-safe because we're not actually adjusting the pointer, only pretending to.
    return std::bit_cast<T*>(std::bit_cast<char*>(pointer) + opaqueZero);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename T>
inline InternalDependencyType toInternalDependencyType(T value)
{
    if constexpr (std::is_pointer_v<T>)
        return static_cast<InternalDependencyType>(reinterpret_cast<uintptr_t>(value));
    else
        return static_cast<InternalDependencyType>(value);
}

inline InternalDependencyType opaqueMixture()
{
    return 0;
}

template<typename... Arguments, typename T>
inline InternalDependencyType opaqueMixture(T value, Arguments... arguments)
{
    return opaqueMixture(arguments...) + toInternalDependencyType(opaque(value));
}

class Dependency {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Dependency);
public:
    constexpr Dependency()
        : m_value(0)
    {
    }
    
    // On TSO architectures, this is a load-load fence and the value it returns is not meaningful (it's
    // zero). The load-load fence is usually just a compiler fence. On ARM, this is a self-xor that
    // produces zero, but it's concealed from the compiler. The CPU understands this dummy op to be a
    // phantom dependency.
    template<typename... Arguments>
    static Dependency fence(Arguments... arguments)
    {
        InternalDependencyType input = opaqueMixture(arguments...);
        InternalDependencyType output;
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
        __asm__("eor %w[out], %w[in], %w[in]"
            : [out] "=r"(output)
            : [in] "r"(input));
#elif CPU(ARM)
        __asm__("eor %[out], %[in], %[in]"
            : [out] "=r"(output)
            : [in] "r"(input));
#else
        // No dependency is needed for this architecture.
        loadLoadFence();
        output = 0;
        UNUSED_PARAM(input);
#endif
        Dependency result;
        result.m_value = output;
        return result;
    }

    // This function exists as a helper to aid in not making mistakes when doing a load
    // and fencing on the result of the load. A couple examples of where things can go
    // wrong, and how this function helps:
    // 
    // Consider this program:
    // ```
    // a = load(p1)
    // b = load(p2)
    // if (a != b) return;
    // d = Dependency::fence(b)
    // ```
    // When consuming the d dependency, the compiler can prove that a and b are the same
    // value, and end up replacing the dependency on whatever register is allocated for `a`
    // instead of being over `b`, leading to the dependency being on load(p1) instead of
    // load(p2). We fix this by splitting the value feeding into the fence and the value
    // being used:
    // b' = load(p2)
    // Dependency::fence(b')
    // b = opaque(b')
    // b' feeds into the fence, and b will be the value compared. Crucially, the compiler can't
    // prove that b == b'.
    //
    // Let's consider another use case. Imagine you end up with a program like this (perhaps
    // after some inlining or various optimizations):
    // a = load(p1)
    // b = load(p2)
    // if (a != b) return;
    // c = load(p2)
    // d = Dependency::fence(c)
    // Similar to the first test, the compiler can prove a and b are the same, allowing it to
    // prove that c == a == b, allowing it to potentially have the dependency be on the wrong
    // value, similar to above. The fix here is to obscure the pointer we're loading from from
    // the compiler.
    // Keep this in sync with Atomic<T>::loadAndFence().
    template<typename T>
    static Dependency loadAndFence(const T* pointer, T& output)
    {
#if CPU(ARM64) || CPU(ARM)
        T value = *opaque(pointer);
        Dependency dependency = Dependency::fence(value);
        output = opaque(value);
        return dependency;
#else
        T value = *pointer;
        Dependency dependency = Dependency::fence(value);
        output = value;
        return dependency;
#endif
    }
    
    // On TSO architectures, this just returns the pointer you pass it. On ARM, this produces a new
    // pointer that is dependent on this dependency and the input pointer.
    template<typename T>
    T* consume(T* pointer)
    {
#if CPU(ARM64) || CPU(ARM)
        return addOpaqueZero(pointer, m_value);
#else
        UNUSED_PARAM(m_value);
        return pointer;
#endif
    }
    
private:
    InternalDependencyType m_value;
};

// Despite the name, this isn't a fence: it only orders later accesses made through dependency.consume().
// Keep this in sync with Dependency::loadAndFence().
template<typename T>
ALWAYS_INLINE Dependency Atomic<T>::loadAndFence(T& output) const
{
#if TSAN_ENABLED
    // TSan doesn't model dependency ordering, so use an Acquire to avoid false races on data published
    // with a Release.
    output = load<MemoryOrder::Acquire>();
    return Dependency();
#elif CPU(ARM64) || CPU(ARM)
    T value = opaque(this)->loadRelaxed();
    Dependency dependency = Dependency::fence(value);
    output = opaque(value);
    return dependency;
#else
    T value = loadRelaxed();
    Dependency dependency = Dependency::fence(value);
    output = value;
    return dependency;
#endif
}

template<typename InputType, typename ValueType>
struct InputAndValue {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(InputAndValue);

    InputAndValue() { }
    
    InputAndValue(InputType input, ValueType value)
        : input(input)
        , value(value)
    {
    }
    
    InputType input;
    ValueType value;
};

template<typename InputType, typename ValueType>
InputAndValue<InputType, ValueType> inputAndValue(InputType input, ValueType value)
{
    return InputAndValue<InputType, ValueType>(input, value);
}

template<typename T>
ALWAYS_INLINE T& ensurePointer(Atomic<T*>& pointer, NOESCAPE const Invocable<T*()> auto& func)
{
    T* oldValue;
    Dependency dependency = pointer.loadAndFence(oldValue);
    if (oldValue)
        return *dependency.consume(oldValue);
    T* newValue = func();
    if (T* actual = pointer.template compareExchangeStrong<MemoryOrder::Release, MemoryOrder::Acquire>(nullptr, newValue)) {
        delete newValue;
        return *actual;
    }
    return *newValue;
}

} // namespace WTF

using WTF::Atomic;
using WTF::Dependency;
using WTF::InputAndValue;
using WTF::inputAndValue;
using WTF::MemoryOrder;
using WTF::ensurePointer;
using WTF::opaqueMixture;
