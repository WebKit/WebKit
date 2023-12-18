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

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"
#include <wtf/UniStdExtras.h>

#if OS(LINUX)
#include <poll.h>
#include <sys/eventfd.h>
#endif

namespace IPC {

Semaphore::Semaphore()
{
#if OS(LINUX)
    m_fd = { eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE), UnixFileDescriptor::Adopt };
#endif
}

Semaphore::Semaphore(UnixFileDescriptor&& fd)
    : m_fd(WTFMove(fd))
{ }

Semaphore::Semaphore(Semaphore&&) = default;
Semaphore& Semaphore::operator=(Semaphore&&) = default;

Semaphore::~Semaphore()
{
    destroy();
}

void Semaphore::signal()
{
#if OS(LINUX)
    ASSERT_WITH_MESSAGE(m_fd.value() >= 0, "Signalling on an invalid semaphore object");

    // Matching waitImpl() and EFD_SEMAPHORE semantics, increment the semaphore value by 1.
    uint64_t value = 1;
    while (true) {
        int ret = write(m_fd.value(), &value, sizeof(uint64_t));
        if (LIKELY(ret != -1 || errno != EINTR))
            break;
    }
#endif
}

#if OS(LINUX)
static bool waitImpl(int fd, int timeout)
{
    struct pollfd pollfdValue { .fd = fd, .events = POLLIN, .revents = 0 };
    int ret = 0;

    // Iterate on polling while interrupts are thrown, otherwise bail out of the loop immediately.
    while (true) {
        ret = poll(&pollfdValue, 1, timeout);
        if (LIKELY(ret != -1 || errno != EINTR))
            break;
    }

    // Fail if the return value doesn't indicate the single file descriptor with only input events available.
    if (ret != 1 || !!(pollfdValue.revents ^ POLLIN))
        return false;

    // There should be data for reading -- due to EFD_SEMAPHORE, it should be 1 packed into an 8-byte value.
    uint64_t value = 0;
    ret = read(fd, &value, sizeof(uint64_t));
    if (ret != sizeof(uint64_t) || value != 1)
        return false;

    return true;
}
#endif

bool Semaphore::wait()
{
#if OS(LINUX)
    ASSERT_WITH_MESSAGE(m_fd.value() >= 0, "Waiting on an invalid semaphore object");

    return waitImpl(m_fd.value(), -1);
#else
    return false;
#endif
}

bool Semaphore::waitFor(Timeout timeout)
{
#if OS(LINUX)
    ASSERT_WITH_MESSAGE(m_fd.value() >= 0, "Waiting on an invalid semaphore object");

    int timeoutValue = -1;
    if (!timeout.isInfinity())
        timeoutValue = int(timeout.secondsUntilDeadline().milliseconds());
    return waitImpl(m_fd.value(), timeoutValue);
#else
    return false;
#endif
}

UnixFileDescriptor Semaphore::duplicateDescriptor() const
{
    return m_fd.duplicate();
}

void Semaphore::destroy()
{
    m_fd = { };
}

} // namespace IPC
