/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "IPCSemaphore.h"

#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"

namespace IPC {

Semaphore::Semaphore()
{
    m_semaphoreHandle = CreateSemaphoreA(nullptr, 0, 1, nullptr);
    RELEASE_ASSERT(m_semaphoreHandle);
}

Semaphore::Semaphore(Win32Handle&& handle)
    : m_semaphoreHandle(WTFMove(handle))
{
}

Semaphore::Semaphore(Semaphore&& other) = default;

Semaphore::~Semaphore()
{
    destroy();
}

Semaphore& Semaphore::operator=(Semaphore&& other)
{
    if (this != &other) {
        destroy();
        m_semaphoreHandle = WTFMove(other.m_semaphoreHandle);
    }

    return *this;
}

void Semaphore::signal()
{
    ReleaseSemaphore(m_semaphoreHandle.get(), 1, nullptr);
}

bool Semaphore::wait()
{
    return WAIT_OBJECT_0 == WaitForSingleObject(m_semaphoreHandle.get(), INFINITE);
}

bool Semaphore::waitFor(Timeout timeout)
{
    Seconds waitTime = timeout.secondsUntilDeadline();
    auto milliseconds = waitTime.millisecondsAs<DWORD>();
    return WAIT_OBJECT_0 == WaitForSingleObject(m_semaphoreHandle.get(), milliseconds);
}

void Semaphore::encode(Encoder& encoder) const
{
    encoder << m_semaphoreHandle;
}

std::optional<Semaphore> Semaphore::decode(Decoder& decoder)
{
    auto semaphoreHandle = decoder.decode<Win32Handle>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return Semaphore { WTFMove(*semaphoreHandle) };
}

void Semaphore::destroy()
{
    m_semaphoreHandle = { };
}

} // namespace IPC
