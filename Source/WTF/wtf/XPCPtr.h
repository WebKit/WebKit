/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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

#include <os/object.h>
#include <wtf/StdLibExtras.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define adoptXPCObject adoptXPCObjectArc
#define retainXPCObject retainXPCObjectArc
#define releaseXPCObject releaseXPCObject
#endif

namespace WTF {

template<typename> class XPCPtr;
template<typename T> XPCPtr<T> adoptXPCObject(T) WARN_UNUSED_RETURN;

template<typename T> static inline void retainXPCObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    xpc_retain(ptr);
#endif
}

template<typename T> static inline void releaseXPCObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    xpc_release(ptr);
#endif
}

template<typename T> class XPCPtr {
public:
    XPCPtr()
        : m_ptr(nullptr)
    {
    }

    ~XPCPtr()
    {
        if (m_ptr)
            releaseXPCObject(m_ptr);
    }

    T get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    XPCPtr(const XPCPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            retainXPCObject(m_ptr);
    }

    XPCPtr(XPCPtr&& other)
        : m_ptr(WTFMove(other.m_ptr))
    {
        other.m_ptr = nullptr;
    }

    XPCPtr(T ptr)
        : m_ptr(WTFMove(ptr))
    {
        if (m_ptr)
            retainXPCObject(m_ptr);
    }

    XPCPtr& operator=(const XPCPtr& other)
    {
        XPCPtr ptr = other;
        swap(ptr);
        return *this;
    }

    XPCPtr& operator=(XPCPtr&& other)
    {
        XPCPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    XPCPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            releaseXPCObject(m_ptr);
        m_ptr = nullptr;
        return *this;
    }

    XPCPtr& operator=(T other)
    {
        XPCPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    void swap(XPCPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    T leakRef() WARN_UNUSED_RETURN
    {
        return std::exchange(m_ptr, nullptr);
    }

    friend XPCPtr adoptXPCObject<T>(T) WARN_UNUSED_RETURN;

private:
    struct AdoptXPCObject { };
    XPCPtr(AdoptXPCObject, T ptr)
        : m_ptr(WTFMove(ptr))
    {
    }

    T m_ptr;
};

template<typename T> inline void swap(RetainPtr<T>& a, RetainPtr<T>& b)
{
    a.swap(b);
}

template<typename T, typename U> constexpr bool operator==(const RetainPtr<T>& a, const RetainPtr<U>& b)
{ 
    return a.get() == b.get(); 
}

template<typename T, typename U> constexpr bool operator==(const RetainPtr<T>& a, U* b)
{
    return a.get() == b; 
}

template<typename T, typename U> constexpr bool operator==(T* a, const RetainPtr<U>& b)
{
    return a == b.get(); 
}

template<typename T> struct IsSmartPtr<XPCPtr<T>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename P> struct HashTraits<XPCPtr<P>> : SimpleClassHashTraits<XPCPtr<P>> {
};

template<typename P> struct DefaultHash<XPCPtr<P>> : PtrHash<XPCPtr<P>> { };

template<typename P> struct XPCPtrObjectHashTraits : SimpleClassHashTraits<XPCPtr<P>> {
    static const XPCPtr<P>& emptyValue()
    {
        static NeverDestroyed<XPCPtr<P>> null;
        return null;
    }

    static bool isEmptyValue(const XPCPtr<P>& value) { return !value; }
};

template<typename P> struct XPCPtrObjectHash {
    static size_t hash(const XPCPtr<P>& o)
    {
        ASSERT_WITH_MESSAGE(o.get(), "attempt to use null XPCPtr in HashTable");
        return xpc_hash(o.get());
    }
    static bool equal(const XPCPtr<P>& a, const XPCPtr<P>& b)
    {
        return xpc_equal(a.get(), b.get());
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

inline bool safeXPCEqual(xpc_object_t a, xpc_object_t b)
{
    return (!a && !b) || (a && b && xpc_equal(a, b));
}

inline size_t safeXPCHash(xpc_object_t a)
{
    return a ? xpc_hash(a) : 0;
}

} // namespace WTF

using WTF::adoptXPCObject;
using WTF::XPCPtr;
using WTF::safeXPCEqual;
using WTF::safeXPCHash;
