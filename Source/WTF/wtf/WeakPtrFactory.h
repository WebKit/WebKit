/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#include <wtf/CompactRefPtrTuple.h>
#include <wtf/Packed.h>
#include <wtf/WeakRef.h>

namespace WTF {

// Note: you probably want to inherit from CanMakeWeakPtr rather than use this directly.
template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakPtrFactory {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ObjectType = T;
    using WeakPtrImplType = WeakPtrImpl;

    WeakPtrFactory()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    void prepareForUseOnlyOnNonMainThread()
    {
#if ASSERT_ENABLED
        ASSERT(m_wasConstructedOnMainThread);
        m_wasConstructedOnMainThread = false;
#endif
    }

    ~WeakPtrFactory()
    {
        if (m_impl)
            m_impl->clear();
    }

    WeakPtrImpl* impl() const
    {
        return m_impl.get();
    }

    void initializeIfNeeded(const T& object) const
    {
        if (m_impl)
            return;

        ASSERT(m_wasConstructedOnMainThread == isMainThread());

        static_assert(std::is_final_v<WeakPtrImpl>);
        m_impl = adoptRef(*new WeakPtrImpl(const_cast<T*>(&object)));
    }

    template<typename U> WeakPtr<U, WeakPtrImpl> createWeakPtr(U& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl->template get<T>());
        return WeakPtr<U, WeakPtrImpl>(*m_impl, enableWeakPtrThreadingAssertions);
    }

    void revokeAll()
    {
        if (RefPtr impl = std::exchange(m_impl, nullptr))
            impl->clear();
    }

    unsigned weakPtrCount() const
    {
        return m_impl ? m_impl->refCount() - 1 : 0u;
    }

#if ASSERT_ENABLED
    bool isInitialized() const { return m_impl; }
#endif

private:
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakRef;

    mutable RefPtr<WeakPtrImpl> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// Note: you probably want to inherit from CanMakeWeakPtrWithBitField rather than use this directly.
template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakPtrFactoryWithBitField {
    WTF_MAKE_NONCOPYABLE(WeakPtrFactoryWithBitField);
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ObjectType = T;
    using WeakPtrImplType = WeakPtrImpl;

    WeakPtrFactoryWithBitField()
#if ASSERT_ENABLED
        : m_wasConstructedOnMainThread(isMainThread())
#endif
    {
    }

    ~WeakPtrFactoryWithBitField()
    {
        if (auto* pointer = m_impl.pointer())
            pointer->clear();
    }

    WeakPtrImpl* impl() const
    {
        return m_impl.pointer();
    }

    void initializeIfNeeded(const T& object) const
    {
        if (m_impl.pointer())
            return;

        ASSERT(m_wasConstructedOnMainThread == isMainThread());

        static_assert(std::is_final_v<WeakPtrImpl>);
        m_impl.setPointer(adoptRef(*new WeakPtrImpl(const_cast<T*>(&object))));
    }

    template<typename U> WeakPtr<U, WeakPtrImpl> createWeakPtr(U& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) const
    {
        initializeIfNeeded(object);

        ASSERT(&object == m_impl.pointer()->template get<T>());
        return WeakPtr<U, WeakPtrImpl>(*m_impl.pointer(), enableWeakPtrThreadingAssertions);
    }

    void revokeAll()
    {
        if (auto* pointer = m_impl.pointer()) {
            pointer->clear();
            m_impl.setPointer(nullptr);
        }
    }

    unsigned weakPtrCount() const
    {
        if (auto* pointer = m_impl.pointer())
            return pointer->refCount() - 1;
        return 0;
    }

#if ASSERT_ENABLED
    bool isInitialized() const { return m_impl.pointer(); }
#endif

    uint16_t bitfield() const { return m_impl.type(); }
    void setBitfield(uint16_t value) const { return m_impl.setType(value); }

private:
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;
    template<typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakListHashSet;
    template<typename, typename, typename> friend class WeakHashMap;
    template<typename, typename, typename> friend class WeakPtr;
    template<typename, typename> friend class WeakRef;

    mutable CompactRefPtrTuple<WeakPtrImpl, uint16_t> m_impl;
#if ASSERT_ENABLED
    bool m_wasConstructedOnMainThread;
#endif
};

// We use lazy initialization of the WeakPtrFactory by default to avoid unnecessary initialization. Eager
// initialization is however useful if you plan to call construct WeakPtrs from other threads.
enum class WeakPtrFactoryInitialization { Lazy, Eager };

} // namespace WTF

using WTF::WeakPtrFactory;
using WTF::WeakPtrFactoryInitialization;
