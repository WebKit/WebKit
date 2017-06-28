/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template <typename T> class DispatchPtr;
template <typename T> DispatchPtr<T> adoptDispatch(T dispatchObject);

// FIXME: Use OSObjectPtr instead when it works with dispatch_data_t on all platforms.
template<typename T> class DispatchPtr {
public:
    DispatchPtr()
        : m_ptr(nullptr)
    {
    }
    explicit DispatchPtr(T ptr)
        : m_ptr(ptr)
    {
        if (m_ptr)
            dispatch_retain(m_ptr);
    }
    DispatchPtr(const DispatchPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            dispatch_retain(m_ptr);
    }
    ~DispatchPtr()
    {
        if (m_ptr)
            dispatch_release(m_ptr);
    }

    DispatchPtr& operator=(const DispatchPtr& other)
    {
        auto copy = other;
        std::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    DispatchPtr& operator=(std::nullptr_t)
    {
        auto ptr = std::exchange(m_ptr, nullptr);
        if (LIKELY(ptr != nullptr))
            dispatch_release(ptr);
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

#endif
