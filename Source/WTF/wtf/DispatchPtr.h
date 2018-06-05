/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// FIXME: Use OSObjectPtr instead in the three places this is used and then
// delete this. When this class template was originally created os_retain
// did not yet work with dispatch_data_t, but it now does on versions of
// Cocoa platforms that WebKit supports and WebKit already uses OSObjectPtr
// for dispatch_data_t in multiple places.

template<typename T> class DispatchPtr;
template<typename T> DispatchPtr<T> adoptDispatch(T dispatchObject);

template<typename T> class DispatchPtr {
public:
    DispatchPtr()
        : m_ptr(nullptr)
    {
    }
    explicit DispatchPtr(T ptr)
        : m_ptr(ptr)
    {
#if !defined(__OBJC__) || !__has_feature(objc_arc)
        if (m_ptr)
            dispatch_retain(m_ptr);
#endif
    }
    DispatchPtr(const DispatchPtr& other)
        : m_ptr(other.m_ptr)
    {
#if !defined(__OBJC__) || !__has_feature(objc_arc)
        if (m_ptr)
            dispatch_retain(m_ptr);

#endif
    }
    ~DispatchPtr()
    {
#if !defined(__OBJC__) || !__has_feature(objc_arc)
        if (m_ptr)
            dispatch_release(m_ptr);
#endif
    }

    DispatchPtr& operator=(const DispatchPtr& other)
    {
        auto copy = other;
        std::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    DispatchPtr& operator=(std::nullptr_t)
    {
#if defined(__OBJC__) && __has_feature(objc_arc)
        m_ptr = nullptr;
#else
        auto ptr = std::exchange(m_ptr, nullptr);
        if (LIKELY(ptr != nullptr))
            dispatch_release(ptr);
#endif
        return *this;
    }

    T get() const { return m_ptr; }
    explicit operator bool() const { return m_ptr; }

    friend DispatchPtr adoptDispatch<T>(T);

private:
    struct Adopt { };
    DispatchPtr(Adopt, T data)
        : m_ptr(data)
    {
    }

    T m_ptr;
};

template <typename T> DispatchPtr<T> adoptDispatch(T dispatchObject)
{
    return DispatchPtr<T>(typename DispatchPtr<T>::Adopt { }, dispatchObject);
}

} // namespace WTF

using WTF::DispatchPtr;
using WTF::adoptDispatch;
