/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "MachSemaphore.h"

#include <mach/mach.h>

namespace WTF {

MachSemaphore::MachSemaphore()
{
    auto ret = semaphore_create(mach_task_self(), &m_semaphore, SYNC_POLICY_FIFO, 0);
    ASSERT_UNUSED(ret, ret == KERN_SUCCESS);
}

MachSemaphore::MachSemaphore(MachSendRight&& sendRight)
    : m_sendRight(WTFMove(sendRight))
    , m_semaphore(m_sendRight.sendRight())
{
    ASSERT(m_sendRight);
}

MachSemaphore::~MachSemaphore()
{
    if (!m_sendRight)
        semaphore_destroy(mach_task_self(), m_semaphore);
}

void MachSemaphore::signal()
{
    semaphore_signal(m_semaphore);
}

void MachSemaphore::wait()
{
    semaphore_wait(m_semaphore);
}

void MachSemaphore::waitFor(Seconds timeout)
{
    auto seconds = timeout.secondsAs<unsigned>();
    semaphore_timedwait(m_semaphore, { seconds, static_cast<clock_res_t>(timeout.nanosecondsAs<uint64_t>() - seconds * NSEC_PER_SEC) });
}

MachSendRight MachSemaphore::createSendRight()
{
    return MachSendRight::create(m_semaphore);
}

} // namespace WTF
