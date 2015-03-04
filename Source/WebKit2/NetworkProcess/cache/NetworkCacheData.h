/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheData_h
#define NetworkCacheData_h

#if ENABLE(NETWORK_CACHE)

#include <functional>
#include <wtf/FunctionDispatcher.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

#if PLATFORM(COCOA)
template <typename T> class DispatchPtr;
template <typename T> DispatchPtr<T> adoptDispatch(T dispatchObject);

// FIXME: Use OSObjectPtr instead when it works with dispatch_data_t on all platforms.
template<typename T> class DispatchPtr {
public:
    DispatchPtr()
        : m_ptr(nullptr)
    {
    }
    DispatchPtr(T ptr)
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
#endif

class NetworkCacheData {
public:
    NetworkCacheData() { }
    NetworkCacheData(const uint8_t*, size_t);

    enum class Backing { Buffer, Map };
#if PLATFORM(COCOA)
    NetworkCacheData(DispatchPtr<dispatch_data_t>, Backing = Backing::Buffer);
#endif
    bool isNull() const;

    const uint8_t* data() const;
    size_t size() const { return m_size; }
    bool isMap() const { return m_isMap; }

#if PLATFORM(COCOA)
    dispatch_data_t dispatchData() const { return m_dispatchData.get(); }
#endif
private:
#if PLATFORM(COCOA)
    mutable DispatchPtr<dispatch_data_t> m_dispatchData;
#endif
    mutable const uint8_t* m_data { nullptr };
    size_t m_size { 0 };
    bool m_isMap { false };
};

}

#endif
#endif
