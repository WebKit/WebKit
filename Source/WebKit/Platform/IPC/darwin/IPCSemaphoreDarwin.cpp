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

#include "config.h"
#include "IPCSemaphore.h"

#include "WebCoreArgumentCoders.h"
#include <mach/mach.h>

namespace IPC {

Semaphore::Semaphore()
{
    auto ret = semaphore_create(mach_task_self(), &m_semaphore, SYNC_POLICY_FIFO, 0);
    ASSERT_UNUSED(ret, ret == KERN_SUCCESS);
}

Semaphore::Semaphore(Semaphore&& other)
    : m_sendRight(WTFMove(other.m_sendRight))
    , m_semaphore(std::exchange(other.m_semaphore, SEMAPHORE_NULL))
{
}

Semaphore::Semaphore(MachSendRight&& sendRight)
    : m_sendRight(WTFMove(sendRight))
    , m_semaphore(m_sendRight.sendRight())
{
    ASSERT(m_sendRight);
}

Semaphore::~Semaphore()
{
    destroy();
}

Semaphore& Semaphore::operator=(Semaphore&& other)
{
    if (this != &other) {
        destroy();
        m_sendRight = WTFMove(other.m_sendRight);
        m_semaphore = std::exchange(other.m_semaphore, SEMAPHORE_NULL);
    }

    return *this;
}

void Semaphore::signal()
{
    auto ret = semaphore_signal(m_semaphore);
    ASSERT_UNUSED(ret, ret == KERN_SUCCESS || ret == KERN_TERMINATED);
}

bool Semaphore::wait()
{
    auto ret = semaphore_wait(m_semaphore);
    ASSERT(ret == KERN_SUCCESS || ret == KERN_TERMINATED);
    return ret == KERN_SUCCESS;
}

bool Semaphore::waitFor(Timeout timeout)
{
    Seconds waitTime = timeout.secondsUntilDeadline();
    auto seconds = waitTime.secondsAs<unsigned>();
    auto ret = semaphore_timedwait(m_semaphore, { seconds, static_cast<clock_res_t>(waitTime.nanosecondsAs<uint64_t>() - seconds * NSEC_PER_SEC) });
    ASSERT(ret == KERN_SUCCESS || ret == KERN_OPERATION_TIMED_OUT || ret == KERN_TERMINATED || ret == KERN_ABORTED);
    return ret == KERN_SUCCESS;
}

MachSendRight Semaphore::createSendRight() const
{
    return MachSendRight::create(m_semaphore);
}

void Semaphore::encode(Encoder& encoder) const
{
    encoder << createSendRight();
}

std::optional<Semaphore> Semaphore::decode(Decoder& decoder)
{
    MachSendRight sendRight;
    if (!decoder.decode(sendRight))
        return std::nullopt;
    return std::optional<Semaphore> { std::in_place, WTFMove(sendRight) };
}


void Semaphore::destroy()
{
    if (m_sendRight) {
        m_sendRight = MachSendRight();
        m_semaphore = SEMAPHORE_NULL;
        return;
    }
    if (m_semaphore == SEMAPHORE_NULL)
        return;
    auto ret = semaphore_destroy(mach_task_self(), m_semaphore);
    ASSERT_UNUSED(ret, ret == KERN_SUCCESS);
    m_semaphore = SEMAPHORE_NULL;
}

} // namespace IPC
