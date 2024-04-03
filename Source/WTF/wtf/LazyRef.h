/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename OwnerType, typename T>
class LazyRef {
    WTF_MAKE_NONCOPYABLE(LazyRef);
    WTF_MAKE_NONMOVABLE(LazyRef);
public:
    LazyRef() = default;

    template<typename Func>
    LazyRef(const Func& func)
    {
        initLater(func);
    }

    typedef T* (*FuncType)(OwnerType&, LazyRef&);

    ~LazyRef()
    {
        ASSERT(!(m_pointer & initializingTag));
        if (m_pointer & lazyTag)
            return;
        uintptr_t pointer = std::exchange(m_pointer, 0);
        if (pointer)
            bitwise_cast<T*>(pointer)->deref();
    }

    bool isInitialized() const { return !(m_pointer & lazyTag); }

    const T& get(const OwnerType& owner) const
    {
        return const_cast<LazyRef&>(*this).get(const_cast<OwnerType&>(owner));
    }

    T& get(OwnerType& owner)
    {
        ASSERT(m_pointer);
        ASSERT(!(m_pointer & initializingTag));
        if (UNLIKELY(m_pointer & lazyTag)) {
            FuncType func = *bitwise_cast<FuncType*>(m_pointer & ~(lazyTag | initializingTag));
            return *func(owner, *this);
        }
        return *bitwise_cast<T*>(m_pointer);
    }

    const T* getIfExists() const
    {
        return const_cast<LazyRef&>(*this).getIfExists();
    }

    T* getIfExists()
    {
        ASSERT(m_pointer);
        if (m_pointer & lazyTag)
            return nullptr;
        return bitwise_cast<T*>(m_pointer);
    }

    T* ptr(OwnerType& owner) RETURNS_NONNULL { &get(owner); }
    T* ptr(const OwnerType& owner) const RETURNS_NONNULL { return &get(owner); }

    template<typename Func>
    void initLater(const Func&)
    {
        static_assert(alignof(T) >= 4);
        static_assert(isStatelessLambda<Func>());
        // Logically we just want to stuff the function pointer into m_pointer, but then we'd be sad
        // because a function pointer is not guaranteed to be a multiple of anything. The tag bits
        // may be used for things. We address this problem by indirecting through a global const
        // variable. The "theFunc" variable is guaranteed to be native-aligned, i.e. at least a
        // multiple of 4.
        static constexpr FuncType theFunc = &callFunc<Func>;
        m_pointer = lazyTag | bitwise_cast<uintptr_t>(&theFunc);
    }

    void set(Ref<T>&& ref)
    {
        Ref<T> local = WTFMove(ref);
        m_pointer = bitwise_cast<uintptr_t>(&local.leakRef());
    }

private:
    static const uintptr_t lazyTag = 1;
    static const uintptr_t initializingTag = 2;

    template<typename Func>
    static T* callFunc(OwnerType& owner, LazyRef& ref)
    {
        ref.m_pointer |= initializingTag;
        callStatelessLambda<void, Func>(owner, ref);
        RELEASE_ASSERT(!(ref.m_pointer & lazyTag));
        RELEASE_ASSERT(!(ref.m_pointer & initializingTag));
        return bitwise_cast<T*>(ref.m_pointer);
    }

    uintptr_t m_pointer { 0 };
};

} // namespace WTF

using WTF::LazyRef;
