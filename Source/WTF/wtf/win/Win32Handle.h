/*
 * Copyright (C) 2011 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <memory>
#include <windows.h>
#include <wtf/Noncopyable.h>

namespace WTF {

class Win32Handle {
    WTF_MAKE_FAST_ALLOCATED;

public:
    Win32Handle() = default;
    explicit Win32Handle(HANDLE handle) : m_handle(handle) { }
    Win32Handle(const Win32Handle& other)
    {
        *this = other;
    }

    Win32Handle(Win32Handle&& other)
    {
        *this = WTFMove(other);
    }

    ~Win32Handle() { clear(); }

    Win32Handle& operator=(const Win32Handle& other)
    {
        if (this != &other) {
            clear();
            if (other.isValid()) {
                auto processHandle = ::GetCurrentProcess();
                ::DuplicateHandle(processHandle, other.m_handle, processHandle, &m_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
            }
        }
        return *this;
    }

    Win32Handle& operator=(Win32Handle&& other)
    {
        if (this != &other) {
            clear();
            m_handle = other.release();
        }
        return *this;
    }

    void clear()
    {
        if (!isValid())
            return;
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }

    bool isValid() const { return m_handle != INVALID_HANDLE_VALUE; }
    explicit operator bool() const { return isValid(); }

    HANDLE get() const { return m_handle; }
    HANDLE release() { HANDLE ret = m_handle; m_handle = INVALID_HANDLE_VALUE; return ret; }

    Win32Handle& operator=(HANDLE handle)
    {
        clear();
        m_handle = handle;
        return *this;
    }

private:
    HANDLE m_handle { INVALID_HANDLE_VALUE };
};

} // namespace WTF

using WTF::Win32Handle;
