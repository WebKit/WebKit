/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <wtf/CheckedRef.h>

namespace WTF {

template<typename T, typename PtrTraits>
class CheckedPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:

    constexpr CheckedPtr()
        : m_ptr(nullptr)
    { }

    constexpr CheckedPtr(std::nullptr_t)
        : m_ptr(nullptr)
    { }

    ALWAYS_INLINE CheckedPtr(T* ptr)
        : m_ptr { ptr }
    {
        refIfNotNull();
    }

    ALWAYS_INLINE CheckedPtr(const CheckedPtr& other)
        : m_ptr { other.m_ptr }
    {
        refIfNotNull();
    }

    ALWAYS_INLINE CheckedPtr(CheckedPtr&& other)
        : m_ptr { PtrTraits::exchange(other.m_ptr, nullptr) }
    { }

    ALWAYS_INLINE ~CheckedPtr()
    {
        derefIfNotNull();
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(const CheckedPtr<OtherType, OtherPtrTraits>& other)
        : CheckedPtr(OtherPtrTraits::unwrap(other.m_ptr))
    { }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(CheckedPtr<OtherType, OtherPtrTraits>&& other)
        : m_ptr { PtrTraits::exchange(other.m_ptr, nullptr) }
    { }

    CheckedPtr(CheckedRef<T, PtrTraits>& other)
        : CheckedPtr(PtrTraits::unwrap(other.m_ptr))
    { }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(const CheckedRef<OtherType, OtherPtrTraits>& other)
        : CheckedPtr(OtherPtrTraits::unwrap(other.m_ptr))
    { }

    CheckedPtr(CheckedRef<T, PtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(get());
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr(CheckedRef<OtherType, OtherPtrTraits>&& other)
        : m_ptr { other.releasePtr() }
    {
        ASSERT(get());
    }

    CheckedPtr(HashTableDeletedValueType)
        : m_ptr(PtrTraits::hashTableDeletedValue())
    { }

    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    using UnspecifiedBoolType = void (CheckedPtr::*)() const;
    operator UnspecifiedBoolType() const { return m_ptr ? &CheckedPtr::unspecifiedBoolTypeInstance : nullptr; }

    ALWAYS_INLINE bool operator!() const { return !PtrTraits::unwrap(m_ptr); }

    ALWAYS_INLINE const T* get() const { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T* get() { return PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE const T& operator*() const { ASSERT(this); return *get(); }
    ALWAYS_INLINE T& operator*() { ASSERT(this); return *get(); }
    ALWAYS_INLINE const T* operator->() const { return get(); }
    ALWAYS_INLINE T* operator->() { return get(); }

    bool operator==(const T* other) const { return m_ptr == other; }
    template<typename U> bool operator==(U* other) const { return m_ptr == other; }

    bool operator==(const CheckedPtr& other) const { return m_ptr == other.m_ptr; }

    template<typename OtherType, typename OtherPtrTraits>
    bool operator==(const CheckedPtr<OtherType, OtherPtrTraits>& other) const { return m_ptr == other.m_ptr; }

    CheckedPtr& operator=(std::nullptr_t)
    {
        derefIfNotNull();
        m_ptr = nullptr;
        return *this;
    }

    CheckedPtr& operator=(T* ptr)
    {
        CheckedPtr copy { ptr };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedPtr& operator=(const CheckedPtr& other)
    {
        CheckedPtr copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr& operator=(const CheckedPtr<OtherType, OtherPtrTraits>& other)
    {
        CheckedPtr copy { other };
        PtrTraits::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    CheckedPtr& operator=(CheckedPtr&& other)
    {
        CheckedPtr moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

    template<typename OtherType, typename OtherPtrTraits> CheckedPtr& operator=(CheckedPtr<OtherType, OtherPtrTraits>&& other)
    {
        CheckedPtr moved { WTFMove(other) };
        PtrTraits::swap(m_ptr, moved.m_ptr);
        return *this;
    }

private:
    template<typename OtherType, typename OtherPtrTraits> friend class CheckedPtr;

    void unspecifiedBoolTypeInstance() const { }

    ALWAYS_INLINE void refIfNotNull()
    {
        if (T* ptr = PtrTraits::unwrap(m_ptr); LIKELY(ptr))
            ptr->incrementPtrCount();
    }

    ALWAYS_INLINE void derefIfNotNull()
    {
        if (T* ptr = PtrTraits::unwrap(m_ptr); LIKELY(ptr))
            ptr->decrementPtrCount();
    }

    typename PtrTraits::StorageType m_ptr;
};

template <typename T, typename PtrTraits>
struct GetPtrHelper<CheckedPtr<T, PtrTraits>> {
    typedef T* PtrType;
    static T* getPtr(const CheckedPtr<T, PtrTraits>& p) { return const_cast<T*>(p.get()); }
};

template <typename T, typename U>
struct IsSmartPtr<CheckedPtr<T, U>> {
    static constexpr bool value = true;
};

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename ArgPtrTraits>
inline bool is(const CheckedPtr<ArgType, ArgPtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename P> struct HashTraits<CheckedPtr<P>> : SimpleClassHashTraits<CheckedPtr<P>> {
    static P* emptyValue() { return nullptr; }

    typedef P* PeekType;
    static PeekType peek(const CheckedPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }

    static void customDeleteBucket(CheckedPtr<P>& value)
    {
        // See unique_ptr's customDeleteBucket() for an explanation.
        ASSERT(!SimpleClassHashTraits<CheckedPtr<P>>::isDeletedValue(value));
        auto valueToBeDestroyed = WTFMove(value);
        SimpleClassHashTraits<CheckedPtr<P>>::constructDeletedValue(value);
    }
};

template<typename P> struct DefaultHash<CheckedPtr<P>> : PtrHash<CheckedPtr<P>> { };

} // namespace WTF

using WTF::CheckedPtr;

