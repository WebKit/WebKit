/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#include <wtf/ArgumentCoder.h>

#if USE(UNIX_DOMAIN_SOCKETS)
#include <wtf/unix/UnixFileDescriptor.h>
#elif OS(DARWIN)
#include <wtf/MachSendRight.h>
#elif OS(WINDOWS)
#include <wtf/win/Win32Handle.h>
#endif

namespace IPC {

class ConnectionHandle {
public:
    ConnectionHandle() = default;
    ConnectionHandle(ConnectionHandle&&) = default;
    ConnectionHandle& operator=(ConnectionHandle&&) = default;

    ConnectionHandle(const ConnectionHandle&) = delete;
    ConnectionHandle& operator=(const ConnectionHandle&) = delete;

#if USE(UNIX_DOMAIN_SOCKETS)
    ConnectionHandle(UnixFileDescriptor&& inHandle)
        : m_handle(WTFMove(inHandle))
    { }
    explicit operator bool() const { return !!m_handle; }
    int release() WARN_UNUSED_RETURN { return m_handle.release(); }
#elif OS(WINDOWS)
    ConnectionHandle(Win32Handle&& inHandle)
        : m_handle(WTFMove(inHandle))
    { }
    explicit operator bool() const { return !!m_handle; }
    HANDLE leak() WARN_UNUSED_RETURN { return m_handle.leak(); }
#elif OS(DARWIN)
    ConnectionHandle(MachSendRight&& sendRight)
        : m_handle(WTFMove(sendRight))
    { }
    explicit operator bool() const { return MACH_PORT_VALID(m_handle.sendRight()); }
    mach_port_t leakSendRight() WARN_UNUSED_RETURN { return m_handle.leakSendRight(); }
#endif

private:
    friend struct IPC::ArgumentCoder<ConnectionHandle, void>;

#if USE(UNIX_DOMAIN_SOCKETS)
    UnixFileDescriptor m_handle;
#elif OS(WINDOWS)
    Win32Handle m_handle;
#elif OS(DARWIN)
    MachSendRight m_handle;
#endif
};

} // namespace IPC
