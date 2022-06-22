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

#include <optional>

#if OS(DARWIN) && !USE(UNIX_DOMAIN_SOCKETS)
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <wtf/MachSendRight.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
#include <variant>
#include <wtf/Function.h>
#include <wtf/unix/UnixFileDescriptor.h>
#endif

namespace IPC {

class Decoder;
class Encoder;

#if OS(DARWIN)
class Attachment : public MachSendRight {
#else
class Attachment {
#endif
public:
    Attachment();

    enum Type {
        Uninitialized,
#if USE(UNIX_DOMAIN_SOCKETS)
        SocketType,
        MappedMemoryType,
        CustomWriterType,
#elif OS(DARWIN)
        MachPortType
#endif
    };

#if USE(UNIX_DOMAIN_SOCKETS)
    explicit Attachment(UnixFileDescriptor&&, size_t);
    explicit Attachment(UnixFileDescriptor&&);

    Attachment(Attachment&&);
    Attachment& operator=(Attachment&&);
    ~Attachment();

    using SocketDescriptor = int;
    using CustomWriterFunc = WTF::Function<void(SocketDescriptor)>;
    using CustomWriter = std::variant<CustomWriterFunc, SocketDescriptor>;
    Attachment(CustomWriter&&);
#elif OS(DARWIN)
    Attachment(MachSendRight&&);
    Attachment(const MachSendRight&);
#elif OS(WINDOWS)
    Attachment(HANDLE handle)
        : m_handle(handle)
    { }
#endif

    Type type() const { return m_type; }

#if USE(UNIX_DOMAIN_SOCKETS)
    bool isNull() const { return !m_fd; }
    size_t size() const { return m_size; }

    const UnixFileDescriptor& fd() const { return m_fd; }
    UnixFileDescriptor release() { return std::exchange(m_fd, UnixFileDescriptor { }); }

    const CustomWriter& customWriter() const { return m_customWriter; }
#elif OS(WINDOWS)
    HANDLE handle() const { return m_handle; }
#endif

    void encode(Encoder&) const;
    static std::optional<Attachment> decode(Decoder&);
    
private:
    Type m_type;

#if USE(UNIX_DOMAIN_SOCKETS)
    UnixFileDescriptor m_fd;
    size_t m_size;
    CustomWriter m_customWriter;
#elif OS(WINDOWS)
    HANDLE m_handle { INVALID_HANDLE_VALUE };
#endif
};

} // namespace IPC
