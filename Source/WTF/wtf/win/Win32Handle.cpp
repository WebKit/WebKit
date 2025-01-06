/*
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

#include "config.h"
#include <wtf/win/Win32Handle.h>

namespace WTF {

static void closeHandle(HANDLE handle)
{
    if (handle == INVALID_HANDLE_VALUE)
        return;

    ::CloseHandle(handle);
}

static HANDLE duplicateHandle(HANDLE handle)
{
    if (handle == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;

    auto processHandle = ::GetCurrentProcess();
    HANDLE duplicate;
    if (!::DuplicateHandle(processHandle, handle, processHandle, &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return INVALID_HANDLE_VALUE;

    return duplicate;
}

Win32Handle Win32Handle::adopt(HANDLE handle)
{
    return Win32Handle(handle);
}

Win32Handle::Win32Handle(HANDLE handle)
    : m_handle(handle)
{
}

Win32Handle::Win32Handle(const Win32Handle& other)
    : m_handle(duplicateHandle(other.get()))
{
}

Win32Handle::Win32Handle(Win32Handle&& other)
    : m_handle(other.leak())
{
}

Win32Handle::~Win32Handle()
{
    closeHandle(m_handle);
}

Win32Handle& Win32Handle::operator=(Win32Handle&& other)
{
    if (this != &other) {
        closeHandle(m_handle);
        m_handle = other.leak();
    }

    return *this;
}

HANDLE Win32Handle::leak()
{
    return std::exchange(m_handle, INVALID_HANDLE_VALUE);
}

Win32Handle::IPCData Win32Handle::toIPCData()
{
    return { reinterpret_cast<uintptr_t>(leak()), GetCurrentProcessId() };
}

Win32Handle Win32Handle::createFromIPCData(IPCData&& data)
{
    auto handle = reinterpret_cast<HANDLE>(data.handle);
    if (handle == INVALID_HANDLE_VALUE)
        return { };
    auto sourceProcess = Win32Handle::adopt(OpenProcess(PROCESS_DUP_HANDLE, FALSE, data.processID));
    if (!sourceProcess)
        return { };
    HANDLE duplicatedHandle;
    // Copy the handle into our process and close the handle that the sending process created for us.
    if (!DuplicateHandle(sourceProcess.get(), handle, GetCurrentProcess(), &duplicatedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        // The source process may exit after the above OpenProcess calling.
        // DuplicateHandle fails with ERROR_INVALID_HANDLE in such a case.
        return { };
    }
    return Win32Handle::adopt(duplicatedHandle);
}

} // namespace WTF
