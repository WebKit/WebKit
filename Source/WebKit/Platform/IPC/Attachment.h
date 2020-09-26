/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#if OS(DARWIN) && !USE(UNIX_DOMAIN_SOCKETS)
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

namespace IPC {

class Decoder;
class Encoder;

class Attachment {
public:
    Attachment();

    enum Type {
        Uninitialized,
#if USE(UNIX_DOMAIN_SOCKETS)
        SocketType,
        MappedMemoryType,
#elif OS(DARWIN)
        MachPortType
#endif
    };

#if USE(UNIX_DOMAIN_SOCKETS)
    Attachment(Attachment&&);
    Attachment& operator=(Attachment&&);
    Attachment(int fileDescriptor, size_t);
    Attachment(int fileDescriptor);
    ~Attachment();
#elif OS(DARWIN)
    Attachment(mach_port_name_t, mach_msg_type_name_t disposition);
#elif OS(WINDOWS)
    Attachment(HANDLE handle)
        : m_handle(handle)
    { }
#endif

    Type type() const { return m_type; }

#if USE(UNIX_DOMAIN_SOCKETS)
    size_t size() const { return m_size; }

    int releaseFileDescriptor() { int temp = m_fileDescriptor; m_fileDescriptor = -1; return temp; }
    int fileDescriptor() const { return m_fileDescriptor; }
#elif OS(DARWIN)
    void release();

    // MachPortType
    mach_port_name_t port() const { return m_port; }
    mach_msg_type_name_t disposition() const { return m_disposition; }
#elif OS(WINDOWS)
    HANDLE handle() const { return m_handle; }
#endif

    void encode(Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(Decoder&, Attachment&);
    
private:
    Type m_type;

#if USE(UNIX_DOMAIN_SOCKETS)
    int m_fileDescriptor { -1 };
    size_t m_size;
#elif OS(DARWIN)
    mach_port_name_t m_port;
    mach_msg_type_name_t m_disposition;
#elif OS(WINDOWS)
    HANDLE m_handle { INVALID_HANDLE_VALUE };
#endif
};

} // namespace IPC
