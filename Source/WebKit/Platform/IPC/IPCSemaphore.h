/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Timeout.h"
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include <mach/semaphore.h>
#include <wtf/MachSendRight.h>
#elif OS(WINDOWS)
#include <wtf/win/Win32Handle.h>
#elif USE(UNIX_DOMAIN_SOCKETS)
#include <wtf/unix/UnixFileDescriptor.h>
#endif

namespace IPC {

class Decoder;
class Encoder;
struct EventSignalPair;

std::optional<EventSignalPair> createEventSignalPair();

// A semaphore implementation that can be duplicated across IPC.
// The cocoa implementation of this only interrupts wait calls upon the
// remote process terminating if the Semaphore was created by the remote
// process.
// It is generally preferred to start using IPC::Event/Signal instead
// to avoid this.
class Semaphore {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Semaphore);
    WTF_MAKE_NONCOPYABLE(Semaphore);
public:
    Semaphore();
    Semaphore(Semaphore&&);
    ~Semaphore();
    Semaphore& operator=(Semaphore&&);

    void signal();
    bool wait();
    bool waitFor(Timeout);

#if PLATFORM(COCOA)
    explicit Semaphore(MachSendRight&&);

    MachSendRight createSendRight() const;
    explicit operator bool() const { return m_sendRight || m_semaphore != SEMAPHORE_NULL; }
#elif OS(WINDOWS)
    explicit Semaphore(Win32Handle&&);
    Win32Handle win32Handle() const { return Win32Handle { m_semaphoreHandle }; }

#elif USE(UNIX_DOMAIN_SOCKETS)
    explicit Semaphore(UnixFileDescriptor&&);
    UnixFileDescriptor duplicateDescriptor() const;
    explicit operator bool() const { return !!m_fd; }
#else
    explicit operator bool() const { return true; }
#endif

private:
    friend std::optional<EventSignalPair> createEventSignalPair();
    void destroy();
#if PLATFORM(COCOA)
    MachSendRight m_sendRight;
    semaphore_t m_semaphore { SEMAPHORE_NULL };
#elif OS(WINDOWS)
    Win32Handle m_semaphoreHandle;
#elif USE(UNIX_DOMAIN_SOCKETS)
    UnixFileDescriptor m_fd;
#endif
};

} // namespace IPC
