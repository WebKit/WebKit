/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RenderPtr_h
#define RenderPtr_h

#include <algorithm>
#include <cstddef>
#include <memory>
#include <wtf/Assertions.h>
#include <wtf/HashTraits.h>

namespace WebCore {

template<typename T> class RenderPtr {
public:
    typedef T ValueType;
    typedef ValueType* PtrType;

    RenderPtr() : m_ptr(nullptr) { }
    RenderPtr(std::nullptr_t) : m_ptr(nullptr) { }
    explicit RenderPtr(T* ptr) : m_ptr(ptr) { }

    ~RenderPtr()
    {
        if (m_ptr)
            m_ptr->destroy();
    }

    PtrType get() const { return m_ptr; }

    void clear();
    PtrType leakPtr() WARN_UNUSED_RETURN;

    ValueType& operator*() const { ASSERT(m_ptr); return *m_ptr; }
    PtrType operator->() const { ASSERT(m_ptr); return m_ptr; }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef PtrType RenderPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &RenderPtr::m_ptr : nullptr; }

    RenderPtr& operator=(std::nullptr_t) { clear(); return *this; }

    RenderPtr(RenderPtr&&);
    template<typename U> RenderPtr(RenderPtr<U>&&);

    RenderPtr& operator=(RenderPtr&&);
    template<typename U> RenderPtr& operator=(RenderPtr<U>&&);

    void swap(RenderPtr& o) { std::swap(m_ptr, o.m_ptr); }

private:
    // We should never have two RenderPtrs for the same underlying object (otherwise we'll get
    // double-destruction), so these equality operators should never be needed.
    template<typename U> bool operator==(const RenderPtr<U>&) { COMPILE_ASSERT(!sizeof(U*), RenderPtrs_should_never_be_equal); return false; }
    template<typename U> bool operator!=(const RenderPtr<U>&) { COMPILE_ASSERT(!sizeof(U*), RenderPtrs_should_never_be_equal); return false; }

    PtrType m_ptr;
};

template<typename T> inline void RenderPtr<T>::clear()
{
    if (m_ptr)
        m_ptr->destroy();
    m_ptr = nullptr;
}

template<typename T> inline typename RenderPtr<T>::PtrType RenderPtr<T>::leakPtr()
{
    PtrType ptr = m_ptr;
    m_ptr = nullptr;
    return ptr;
}

template<typename T> inline RenderPtr<T>::RenderPtr(RenderPtr<T>&& o)
    : m_ptr(o.leakPtr())
{
}

template<typename T> template<typename U> inline RenderPtr<T>::RenderPtr(RenderPtr<U>&& o)
    : m_ptr(o.leakPtr())
{
}

template<typename T> inline auto RenderPtr<T>::operator=(RenderPtr&& o) -> RenderPtr&
{
    ASSERT(!o || o != m_ptr);
    RenderPtr ptr = WTF::move(o);
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline auto RenderPtr<T>::operator=(RenderPtr<U>&& o) -> RenderPtr&
{
    ASSERT(!o || o != m_ptr);
    RenderPtr ptr = WTF::move(o);
    swap(ptr);
    return *this;
}

template<typename T> inline void swap(RenderPtr<T>& a, RenderPtr<T>& b)
{
    a.swap(b);
}

template<typename T, typename U> inline bool operator==(const RenderPtr<T>& a, U* b)
{
    return a.get() == b;
}

template<typename T, typename U> inline bool operator==(T* a, const RenderPtr<U>& b) 
{
    return a == b.get();
}

template<typename T, typename U> inline bool operator!=(const RenderPtr<T>& a, U* b)
{
    return a.get() != b;
}

template<typename T, typename U> inline bool operator!=(T* a, const RenderPtr<U>& b)
{
    return a != b.get();
}

template<typename T> inline typename RenderPtr<T>::PtrType getPtr(const RenderPtr<T>& p)
{
    return p.get();
}

template<class T, class... Args> inline RenderPtr<T>
createRenderer(Args&&... args)
{
    return RenderPtr<T>(new T(std::forward<Args>(args)...));
}

template<typename T, typename U> inline RenderPtr<T> static_pointer_cast(RenderPtr<U>&& p)
{
    return RenderPtr<T>(static_cast<T*>(p.leakPtr()));
}

} // namespace WebCore

namespace WTF {

template<typename T> struct HashTraits<WebCore::RenderPtr<T>> : SimpleClassHashTraits<WebCore::RenderPtr<T>> {
    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    typedef T* PeekType;
    static T* peek(const WebCore::RenderPtr<T>& value) { return value.get(); }
    static T* peek(std::nullptr_t) { return nullptr; }
};

} // namespace WTF

#endif // RenderPtr_h
