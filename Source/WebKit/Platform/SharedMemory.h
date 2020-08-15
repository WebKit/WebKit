/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

#if USE(UNIX_DOMAIN_SOCKETS)
#include "Attachment.h"
#include <wtf/Optional.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class SharedBuffer;
}

#if OS(DARWIN)
namespace WTF {
class MachSendRight;
}
#endif

namespace WebKit {

class SharedMemory : public RefCounted<SharedMemory> {
public:
    enum class Protection {
        ReadOnly,
        ReadWrite
    };

    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();
        ~Handle();
        Handle(Handle&&);
        Handle& operator=(Handle&&);

        bool isNull() const;

#if OS(DARWIN) || OS(WINDOWS)
        size_t size() const { return m_size; }
#endif

        void clear();

#if USE(UNIX_DOMAIN_SOCKETS)
        IPC::Attachment releaseAttachment() const;
        void adoptAttachment(IPC::Attachment&&);
#endif
#if OS(WINDOWS)
        static void encodeHandle(IPC::Encoder&, HANDLE);
        static Optional<HANDLE> decodeHandle(IPC::Decoder&);
#endif
    private:
        friend class SharedMemory;
#if USE(UNIX_DOMAIN_SOCKETS)
        mutable IPC::Attachment m_attachment;
#elif OS(DARWIN)
        mutable mach_port_t m_port { MACH_PORT_NULL };
        size_t m_size;
#elif OS(WINDOWS)
        mutable HANDLE m_handle;
        size_t m_size;
#endif
    };

    struct IPCHandle {
        IPCHandle() = default;
        IPCHandle(Handle&& handle, uint64_t dataSize)
            : handle(WTFMove(handle))
            , dataSize(dataSize)
        {
        }
        void encode(IPC::Encoder&) const;
        static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, IPCHandle&);

        Handle handle;
        uint64_t dataSize { 0 };
    };

    static RefPtr<SharedMemory> allocate(size_t);
    static RefPtr<SharedMemory> create(void*, size_t, Protection);
    static RefPtr<SharedMemory> copyBuffer(const WebCore::SharedBuffer&);
    static RefPtr<SharedMemory> map(const Handle&, Protection);
#if USE(UNIX_DOMAIN_SOCKETS)
    static RefPtr<SharedMemory> wrapMap(void*, size_t, int fileDescriptor);
#elif OS(DARWIN)
    static RefPtr<SharedMemory> wrapMap(void*, size_t, Protection);
#endif
#if OS(WINDOWS)
    static RefPtr<SharedMemory> adopt(HANDLE, size_t, Protection);
#endif

    ~SharedMemory();

    bool createHandle(Handle&, Protection);

    size_t size() const { return m_size; }
    void* data() const
    {
        ASSERT(m_data);
        return m_data;
    }

#if OS(WINDOWS)
    HANDLE handle() const { return m_handle; }
#endif

    // Return the system page size in bytes.
    static unsigned systemPageSize();

private:
#if OS(DARWIN)
    WTF::MachSendRight createSendRight(Protection) const;
#endif

    size_t m_size;
    void* m_data;
#if PLATFORM(COCOA)
    Protection m_protection;
#endif

#if USE(UNIX_DOMAIN_SOCKETS)
    Optional<int> m_fileDescriptor;
    bool m_isWrappingMap { false };
#elif OS(DARWIN)
    mach_port_t m_port { MACH_PORT_NULL };
#elif OS(WINDOWS)
    HANDLE m_handle;
#endif
};

};
