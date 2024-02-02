/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "Encoder.h"
#include "IPCSemaphore.h"

namespace IPC {

struct EventSignalPair;
std::optional<EventSignalPair> createEventSignalPair();

// A waitable Event object (and corresponding Signal object), where the
// Signal object can be serialized across IPC.
// The wait operation will be interrupted if the Signal instance is
// destroyed (including if the remote process that owns it crashes or is
// killed).
// FIXME: Write proper interruptible implementations for non-COCOA platforms.
class Signal {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Signal);
public:
    Signal(Signal&& other) = default;
    Signal& operator=(Signal&& other) = default;

#if PLATFORM(COCOA)
    void signal();

    MachSendRight takeSendRight() { return WTFMove(m_sendRight); }
#else
    void signal()
    {
        m_semaphore.signal();
    }

    const Semaphore& semaphore() const { return m_semaphore; }
#endif

private:
    friend struct IPC::ArgumentCoder<Signal, void>;

    friend std::optional<EventSignalPair> createEventSignalPair();

#if PLATFORM(COCOA)
    Signal(MachSendRight&& sendRight)
        : m_sendRight(WTFMove(sendRight))
    { }
#else
    Signal(Semaphore&& semaphore)
        : m_semaphore(WTFMove(semaphore))
    { }
#endif

#if PLATFORM(COCOA)
    MachSendRight m_sendRight;
#else
    Semaphore m_semaphore;
#endif
};

class Event {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Event);
public:
#if PLATFORM(COCOA)
    ~Event();
    Event(Event&& other)
        : m_receiveRight(WTFMove(other.m_receiveRight))
    {
        other.m_receiveRight = MACH_PORT_NULL;
    }

    Event& operator=(Event&& other)
    {
        m_receiveRight = WTFMove(other.m_receiveRight);
        other.m_receiveRight = MACH_PORT_NULL;
        return *this;
    }

    bool wait();
    bool waitFor(Timeout);
#else
    Event(Event&& other) = default;
    Event& operator=(Event&& other) = default;

    bool wait()
    {
        return m_semaphore.wait();
    }

    bool waitFor(Timeout timeout)
    {
        return m_semaphore.waitFor(timeout);
    }
#endif

private:
    friend std::optional<EventSignalPair> createEventSignalPair();

#if PLATFORM(COCOA)
    Event(mach_port_t port)
        : m_receiveRight(port)
    { }
#else
    Event(Semaphore&& semaphore)
        : m_semaphore(WTFMove(semaphore))
    { }
#endif

#if PLATFORM(COCOA)
    mach_port_t m_receiveRight;
#else
    Semaphore m_semaphore;
#endif
};

struct EventSignalPair {
    Event event;
    Signal signal;
};

#if !PLATFORM(COCOA)
inline std::optional<EventSignalPair> createEventSignalPair()
{
    Semaphore event;
#if PLATFORM(WIN)
    Semaphore signal(Win32Handle { event.m_semaphoreHandle });
#else
    Semaphore signal(event.m_fd.duplicate());
#endif
    return EventSignalPair { Event { WTFMove(event) }, Signal { WTFMove(signal) } };
}
#endif

} // namespace IPC
