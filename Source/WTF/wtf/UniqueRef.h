/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <memory>
#include <wtf/Assertions.h>
#include <wtf/GetPtr.h>
#include <wtf/TypeCasts.h>

namespace WTF {

template<typename T> class UniqueRef;

template<typename T, class... Args>
UniqueRef<T> makeUniqueRefWithoutFastMallocCheck(Args&&... args)
{
    return UniqueRef<T>(*new T(std::forward<Args>(args)...));
}

template<typename T, class... Args>
UniqueRef<T> makeUniqueRef(Args&&... args)
{
    static_assert(std::is_same<typename T::webkitFastMalloced, int>::value, "T is FastMalloced");
    return makeUniqueRefWithoutFastMallocCheck<T>(std::forward<Args>(args)...);
}

template<typename T>
UniqueRef<T> makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<T>&& ptr)
{
    return UniqueRef<T>(WTFMove(ptr));
}

template<typename T>
class UniqueRef {
public:
    template <typename U>
    UniqueRef(UniqueRef<U>&& other)
        : m_ref(other.m_ref.release())
    {
        ASSERT(m_ref);
    }

    explicit UniqueRef(T& other)
        : m_ref(&other)
    {
        ASSERT(m_ref);
    }

    T* ptr() RETURNS_NONNULL { ASSERT(m_ref); return m_ref.get(); }
    T* ptr() const RETURNS_NONNULL { ASSERT(m_ref); return m_ref.get(); }

    T& get() { ASSERT(m_ref); return *m_ref; }
    const T& get() const { ASSERT(m_ref); return *m_ref; }

    T* operator&() { ASSERT(m_ref); return m_ref.get(); }
    const T* operator&() const { ASSERT(m_ref); return m_ref.get(); }

    T* operator->() { ASSERT(m_ref); return m_ref.get(); }
    const T* operator->() const { ASSERT(m_ref); return m_ref.get(); }
    
    operator T&() { ASSERT(m_ref); return *m_ref; }
    operator const T&() const { ASSERT(m_ref); return *m_ref; }

    std::unique_ptr<T> moveToUniquePtr() { return WTFMove(m_ref); }

    explicit UniqueRef(HashTableEmptyValueType) { }

private:
    template<class U, class... Args> friend UniqueRef<U> makeUniqueRefWithoutFastMallocCheck(Args&&...);
    template<class U> friend UniqueRef<U> makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<U>&&);
    template<class U> friend class UniqueRef;

    explicit UniqueRef(std::unique_ptr<T>&& ptr)
        : m_ref(WTFMove(ptr))
    {
        ASSERT(m_ref);
    }

    std::unique_ptr<T> m_ref;
};

template <typename T>
struct GetPtrHelper<UniqueRef<T>> {
    using PtrType = T*;
    static T* getPtr(const UniqueRef<T>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T>
struct IsSmartPtr<UniqueRef<T>> {
    static constexpr bool value = true;
};

template<typename ExpectedType, typename ArgType>
inline bool is(UniqueRef<ArgType>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType>
inline bool is(const UniqueRef<ArgType>& source)
{
    return is<ExpectedType>(source.get());
}

} // namespace WTF

using WTF::UniqueRef;
using WTF::makeUniqueRef;
using WTF::makeUniqueRefWithoutFastMallocCheck;
using WTF::makeUniqueRefFromNonNullUniquePtr;
