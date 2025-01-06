/*
 * Copyright (C) 2011 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2023 Sony Interactive Entertainment Inc. All rights reserved.
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

#include <windows.h>
#include <wtf/FastMalloc.h>

namespace WTF {

class Win32Handle {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE static Win32Handle adopt(HANDLE);

    Win32Handle() = default;
    WTF_EXPORT_PRIVATE explicit Win32Handle(const Win32Handle&);
    WTF_EXPORT_PRIVATE Win32Handle(Win32Handle&&);
    WTF_EXPORT_PRIVATE ~Win32Handle();

    WTF_EXPORT_PRIVATE Win32Handle& operator=(Win32Handle&&);

    explicit operator bool() const { return m_handle != INVALID_HANDLE_VALUE; }

    HANDLE get() const { return m_handle; }

    WTF_EXPORT_PRIVATE HANDLE leak() WARN_UNUSED_RETURN;

    struct IPCData {
        uintptr_t handle;
        DWORD processID;
    };

    WTF_EXPORT_PRIVATE IPCData toIPCData();
    WTF_EXPORT_PRIVATE static Win32Handle createFromIPCData(IPCData&&);

private:
    explicit Win32Handle(HANDLE);

    HANDLE m_handle { INVALID_HANDLE_VALUE };
};

} // namespace WTF

using WTF::Win32Handle;
