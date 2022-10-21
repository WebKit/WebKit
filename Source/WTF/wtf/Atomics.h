/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>

#if OS(WINDOWS)
#if !COMPILER(GCC_COMPATIBLE)
extern "C" void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#endif
#include <windows.h>
#include <intrin.h>
#endif

namespace WTF {

ALWAYS_INLINE bool hasFence(std::memory_order order)
{
    return order != std::memory_order_relaxed;
}
    
// Atomic wraps around std::atomic with the sole purpose of making the compare_exchange
// operations not alter the expected value. This is more in line with how we typically
// use CAS in our code.
//
// Atomic is a struct without explicitly defined constructors so that it can be
// initialized at compile time.

template<typename T>
struct Atomic {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    // Don't pass a non-default value for the order parameter unless you really know
    // what you are doing and have thought about it very hard. The cost of seq_cst
    // is usually not high enough to justify the risk.

    ALWAYS_INLINE T load(std::memory_order order = std::memory_order_seq_cst) const { return value.load(order); }
    
    ALWAYS_INLINE T loadRelaxed() const { return load(std::memory_order_relaxed); }
    
    // This is a load that simultaneously does a full fence - neither loads nor stores will move
    // above or below it.
    ALWAYS_INLINE T loadFullyFenced() const
    {
        Atomic<T>* ptr = const_cast<Atomic<T>*>(this);
        return ptr->exchangeAdd(T());
    }
    
    ALWAYS_INLINE void store(T desired, std::memory_order order = std::memory_order_seq_cst) { value.store(desired, order); }
    
    ALWAYS_INLINE void storeRelaxed(T desired) { store(desired, std::memory_order_relaxed); }

    // This is a store that simultaneously does a full fence - neither loads nor stores will move
    // above or below it.
    ALWAYS_INLINE void storeFullyFenced(T desired)
    {
        exchange(desired);
    }

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

    // WARNING: This does not have strong fencing guarantees when it fails. For example, stores could
    // sink below it in that case.
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
    ALWAYS_INLINE bool transaction(const Func& func, std::memory_order order = std::memory_order_seq_cst)
    {
        for (;;) {
            T oldValue = load(std::memory_order_relaxed);
            T newValue = oldValue;
            if (!func(newValue))
                return false;
            if (compareExchangeWeak(oldValue, newValue, order))
                return true;
        }
    }

    template<typename Func>
    ALWAYS_INLINE bool transactionRelaxed(const Func& func)
    {
        return transaction(func, std::memory_order_relaxed);
    }

    Atomic() = default;
    constexpr Atomic(T initial)
        : value(std::forward<T>(initial))
    {
    }

    std::atomic<T> value;
};

template<typename T>
inline T atomicLoad(T* location, std::memory_order order = std::memory_order_seq_cst)
{
    return bitwise_cast<Atomic<T>*>(location)->load(order);
}

template<typename T>
inline T atomicLoadFullyFenced(T* location)
{
    return bitwise_cast<Atomic<T>*>(location)->loadFullyFenced();
}

template<typename T>
inline void atomicStore(T* location, T newValue, std::memory_order order = std::memory_order_seq_cst)
{
    bitwise_cast<Atomic<T>*>(location)->store(newValue, order);
}

template<typename T>
inline void atomicStoreFullyFenced(T* location, T newValue)
{
    bitwise_cast<Atomic<T>*>(location)->storeFullyFenced(newValue);
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
#if OS(WINDOWS) && !COMPILER(GCC_COMPATIBLE)
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

template<typename T>
T opaque(T pointer)
{
#if !OS(WINDOWS)
    asm volatile("" : "+r"(pointer) ::);
#endif
    return pointer;
}

typedef unsigned InternalDependencyType;

inline InternalDependencyType opaqueMixture()
{
    return 0;
}

template<typename... Arguments, typename T>
inline InternalDependencyType opaqueMixture(T value, Arguments... arguments)
{
    union {
        InternalDependencyType copy;
        T value;
    } u;
    u.copy = 0;
    u.value = value;
    return opaqueMixture(arguments...) + u.copy;
}

class Dependency {
    WTF_MAKE_FAST_ALLOCATED;
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
        asm("eor %w[out], %w[in], %w[in]"
            : [out] "=r"(output)
            : [in] "r"(input));
#elif CPU(ARM)
        asm("eor %[out], %[in], %[in]"
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
        return bitwise_cast<T*>(bitwise_cast<char*>(pointer) + m_value);
#else
        UNUSED_PARAM(m_value);
        return pointer;
#endif
    }
    
private:
    InternalDependencyType m_value;
};

template<typename InputType, typename ValueType>
struct InputAndValue {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

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

template<typename T, typename Func>
ALWAYS_INLINE T& ensurePointer(Atomic<T*>& pointer, const Func& func)
{
    for (;;) {
        T* oldValue = pointer.load(std::memory_order_relaxed);
        if (oldValue) {
            // On all sensible CPUs, we get an implicit dependency-based load-load barrier when
            // loading this.
            return *oldValue;
        }
        T* newValue = func();
        if (pointer.compareExchangeWeak(oldValue, newValue))
            return *newValue;
        delete newValue;
    }
}

} // namespace WTF

using WTF::Atomic;
using WTF::Dependency;
using WTF::InputAndValue;
using WTF::inputAndValue;
using WTF::ensurePointer;
using WTF::opaqueMixture;
