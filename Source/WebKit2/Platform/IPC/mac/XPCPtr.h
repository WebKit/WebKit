/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef XPCPtr_h
#define XPCPtr_h

#include <xpc/xpc.h>

namespace IPC {

struct AdoptXPC { };

template<typename T> class XPCPtr {
public:
    XPCPtr()
        : m_ptr(nullptr)
    {
    }

    XPCPtr(AdoptXPC, T ptr)
        : m_ptr(ptr)
    {
    }

    ~XPCPtr()
    {
        if (m_ptr)
            xpc_release(m_ptr);
    }

    T get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    XPCPtr(const XPCPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            xpc_retain(m_ptr);
    }

    XPCPtr(XPCPtr&& other)
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    XPCPtr& operator=(const XPCPtr& other)
    {
        T optr = other.get();
        if (optr)
            xpc_retain(optr);

        T ptr = m_ptr;
        m_ptr = optr;

        if (ptr)
            xpc_release(ptr);

        return *this;
    }

    XPCPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            xpc_release(m_ptr);
        m_ptr = nullptr;

        return *this;
    }
private:
    T m_ptr;
};

template<typename T> inline XPCPtr<T> adoptXPC(T ptr)
{
    return XPCPtr<T>(AdoptXPC { }, ptr);
}

} // namespace IPC

#endif // XPCPtr_h
