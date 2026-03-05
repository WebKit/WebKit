/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TypeTraits.h>

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define adoptOSObject adoptOSObjectArc
#endif

namespace WTF {

template<typename T, typename arcEnabled = ARCEnabled> struct DefaultOSObjectRetainTraits {
    static ALWAYS_INLINE void retain(T ptr)
    {
#if __has_feature(objc_arc)
        UNUSED_PARAM(ptr);
#else
        os_retain(ptr);
#endif
    }
    static ALWAYS_INLINE void release(T ptr)
    {
#if __has_feature(objc_arc)
        UNUSED_PARAM(ptr);
#else
        os_release(ptr);
#endif
    }
};

template<typename T, typename RetainTraits = DefaultOSObjectRetainTraits<T, ARCEnabled>> [[nodiscard]] OSObjectPtr<T, RetainTraits> adoptOSObject(T);

/**
 * @brief OSObjectPtr is a reference-counting smart pointer for Darwin OS object types.
 *
 * It extends the lifetime of the referenced object by retaining it on construction and releasing it on
 * destruction.
 *
 * OSObjectPtr is used for libdispatch types (dispatch_queue_t, dispatch_source_t, dispatch_data_t,
 * dispatch_group_t, dispatch_semaphore_t, etc.), XPC types (xpc_connection_t, xpc_object_t,
 * xpc_endpoint_t, etc.), and Network framework types (nw_endpoint_t, nw_path_t, etc.). Each type family
 * uses its own retain/release functions (dispatch_retain/dispatch_release, xpc_retain/xpc_release,
 * nw_retain/nw_release, or os_retain/os_release for other types).
 *
 * To create an OSObjectPtr, use one of the following:
 * @code
 * OSObjectPtr ptr = value;            // Retains the value (increments the ref count)
 * OSObjectPtr ptr = adoptOSObject(x); // Takes ownership without retaining
 * @endcode
 *
 * Use adoptOSObject() when you receive an object that you already own (i.e., the object was returned to
 * you with a +1 retain count). This includes objects from creation functions like dispatch_queue_create()
 * or xpc_*_create(). Using the regular OSObjectPtr constructor instead of adoptOSObject() would add an
 * extra retain, causing a leak when the OSObjectPtr is destroyed. Use the regular constructor when you
 * want to add a reference to an object you don't already own.
 *
 * @note For Objective-C types and Core Foundation types, use RetainPtr instead of OSObjectPtr.
 *
 * @note OSObjectPtr is compatible with ARC (Automatic Reference Counting) and will automatically use the
 * appropriate retain/release semantics based on the compilation mode.
 */
template<typename T, typename RetainTraits> class OSObjectPtr {
public:
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

    OSObjectPtr()
        : m_ptr(nullptr)
    {
    }

    ~OSObjectPtr()
    {
        if (m_ptr)
            RetainTraits::release(m_ptr);
    }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    constexpr OSObjectPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    constexpr bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    constexpr operator PtrType() const LIFETIME_BOUND { return m_ptr; }
    T get() const LIFETIME_BOUND { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    OSObjectPtr(const OSObjectPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            RetainTraits::retain(m_ptr);
    }

    OSObjectPtr(OSObjectPtr&& other)
        : m_ptr(WTF::move(other.m_ptr))
    {
        other.m_ptr = nullptr;
    }

    OSObjectPtr(T ptr)
        : m_ptr(WTF::move(ptr))
    {
        if (m_ptr)
            RetainTraits::retain(m_ptr);
    }

    OSObjectPtr& operator=(const OSObjectPtr& other)
    {
        OSObjectPtr ptr = other;
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(OSObjectPtr&& other)
    {
        OSObjectPtr ptr = WTF::move(other);
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            RetainTraits::release(m_ptr);
        m_ptr = nullptr;
        return *this;
    }

    OSObjectPtr& operator=(T other)
    {
        OSObjectPtr ptr = WTF::move(other);
        swap(ptr);
        return *this;
    }

    void swap(OSObjectPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    [[nodiscard]] T leakRef()
    {
        return std::exchange(m_ptr, nullptr);
    }

    friend OSObjectPtr adoptOSObject<T, RetainTraits>(T);

private:
    struct AdoptOSObject { };
    OSObjectPtr(AdoptOSObject, T ptr)
        : m_ptr(WTF::move(ptr))
    {
    }

    static constexpr T hashTableDeletedValue() { return reinterpret_cast<T>(-1); }

    T m_ptr;
};

template<typename T, typename U, typename V> constexpr bool operator==(const OSObjectPtr<T, V>& a, const OSObjectPtr<U, V>& b)
{
    return a.get() == b.get();
}

template<typename T, typename RetainTraits> inline OSObjectPtr<T, RetainTraits> adoptOSObject(T ptr)
{
    return OSObjectPtr<T, RetainTraits> { typename OSObjectPtr<T, RetainTraits>::AdoptOSObject { }, WTF::move(ptr) };
}

template<typename T, typename U, typename RetainTraits>
ALWAYS_INLINE void lazyInitialize(const OSObjectPtr<T, RetainTraits>& ptr, OSObjectPtr<U, RetainTraits>&& obj)
{
    RELEASE_ASSERT(!ptr);
    const_cast<OSObjectPtr<T, RetainTraits>&>(ptr) = std::move(obj); // NOLINT
}

template<typename T, typename RetainTraits> struct IsSmartPtr<OSObjectPtr<T, RetainTraits>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename T, typename RetainTraits> struct HashTraits<OSObjectPtr<T, RetainTraits>> : SimpleClassHashTraits<OSObjectPtr<T, RetainTraits>> { };
template<typename T, typename RetainTraits> struct DefaultHash<OSObjectPtr<T, RetainTraits>> : PtrHash<OSObjectPtr<T, RetainTraits>> { };

} // namespace WTF

using WTF::OSObjectPtr;
using WTF::adoptOSObject;
using WTF::lazyInitialize;
