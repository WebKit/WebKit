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

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define adoptGCDObject adoptGCDObjectArc
#define retainGCDObject retainGCDObjectArc
#define releaseGCDObject releaseGCDObject
#endif

namespace WTF {

template<typename> class GCDPtr;
template<typename T> GCDPtr<T> adoptGCDObject(T) WARN_UNUSED_RETURN;

template<typename T> static inline void retainGCDObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    dispatch_retain(ptr);
#endif
}

template<typename T> static inline void releaseGCDObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    dispatch_release(ptr);
#endif
}

template<typename T> class GCDPtr {
public:
    GCDPtr()
        : m_ptr(nullptr)
    {
    }

    ~GCDPtr()
    {
        if (m_ptr)
            releaseGCDObject(m_ptr);
    }

    T get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    GCDPtr(const GCDPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            retainGCDObject(m_ptr);
    }

    GCDPtr(GCDPtr&& other)
        : m_ptr(WTFMove(other.m_ptr))
    {
        other.m_ptr = nullptr;
    }

    GCDPtr(T ptr)
        : m_ptr(WTFMove(ptr))
    {
        if (m_ptr)
            retainGCDObject(m_ptr);
    }

    GCDPtr& operator=(const GCDPtr& other)
    {
        GCDPtr ptr = other;
        swap(ptr);
        return *this;
    }

    GCDPtr& operator=(GCDPtr&& other)
    {
        GCDPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    GCDPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            releaseGCDObject(m_ptr);
        m_ptr = nullptr;
        return *this;
    }

    GCDPtr& operator=(T other)
    {
        GCDPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    void swap(GCDPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    T leakRef() WARN_UNUSED_RETURN
    {
        return std::exchange(m_ptr, nullptr);
    }

    friend GCDPtr adoptGCDObject<T>(T) WARN_UNUSED_RETURN;

private:
    struct AdoptGCDObject { };
    GCDPtr(AdoptGCDObject, T ptr)
        : m_ptr(WTFMove(ptr))
    {
    }

    T m_ptr;
};

template<typename T> inline GCDPtr<T> adoptGCDObject(T ptr)
{
    return GCDPtr<T> { typename GCDPtr<T>::AdoptGCDObject { }, WTFMove(ptr) };
}

} // namespace WTF

using WTF::adoptGCDObject;
using WTF::GCDPtr;
