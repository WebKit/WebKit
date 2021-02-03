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

#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/Seconds.h>

#if OS(DARWIN)
#include <mach/semaphore.h>
#include <wtf/MachSendRight.h>
#endif

namespace IPC {

class Decoder;
class Encoder;

class Semaphore {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Semaphore);
public:
    Semaphore();
    Semaphore(Semaphore&&);
    ~Semaphore();
    Semaphore& operator=(Semaphore&&);

    void encode(Encoder&) const;
    static Optional<Semaphore> decode(Decoder&);

#if OS(DARWIN)
    explicit Semaphore(MachSendRight&&);

    void signal();
    void wait();
    void waitFor(Seconds);
    MachSendRight createSendRight() const;
    explicit operator bool() const { return m_sendRight || m_semaphore != SEMAPHORE_NULL; }
#else
    explicit operator bool() const { return true; }
#endif

private:
#if OS(DARWIN)
    void destroy();
    MachSendRight m_sendRight;
    semaphore_t m_semaphore { SEMAPHORE_NULL };
#endif
};

} // namespace IPC
