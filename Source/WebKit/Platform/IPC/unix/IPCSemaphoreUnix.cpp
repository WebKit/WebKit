/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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
#include "Logging.h"
#include <wtf/UniStdExtras.h>

#if OS(LINUX)
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <wtf/Atomics.h>
#include <wtf/MonotonicTime.h>
#include <wtf/SafeStrerror.h>
#include <wtf/simde/simde.h>

#if !defined(MFD_ALLOW_SEALING) && HAVE(LINUX_MEMFD_H)
#include <linux/memfd.h>
#endif

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034
#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#endif
#endif

namespace IPC {

#if OS(LINUX)

// The semaphore is a counter in a page shared with the peer. A waiter polls the
// counter for a short while before parking on it with futex(2), and only a
// parked waiter has to be woken with a system call, so a signal that arrives
// while the peer is still polling costs two atomic operations and nothing else.
struct SemaphoreState {
    Atomic<uint32_t> count;
    Atomic<uint32_t> parked;
};

static constexpr size_t semaphoreStateSize = 4096;
static constexpr Seconds semaphorePollDuration = 50_us;
static constexpr unsigned semaphorePollRetryInterval = 64;

struct FutexTimespec {
    long seconds;
    long nanoseconds;
};

static constexpr int requiredSemaphoreSeals = F_SEAL_SHRINK | F_SEAL_GROW;

static void futexWakeOne(Atomic<uint32_t>& word)
{
    syscall(SYS_futex, &word, FUTEX_WAKE, 1, nullptr, nullptr, 0);
}

static bool futexWaitWhileZero(Atomic<uint32_t>& word, const FutexTimespec* relativeTimeout)
{
    if (!syscall(SYS_futex, &word, FUTEX_WAIT, 0u, relativeTimeout, nullptr, 0))
        return true;
    return errno == EAGAIN || errno == EINTR || errno == ETIMEDOUT;
}

static SemaphoreState* mapSemaphoreState(int fd)
{
    if (fd < 0)
        return nullptr;
    int seals = fcntl(fd, F_GET_SEALS);
    if (seals == -1 || (seals & requiredSemaphoreSeals) != requiredSemaphoreSeals)
        return nullptr;
    struct stat status;
    if (fstat(fd, &status) == -1 || static_cast<size_t>(status.st_size) < semaphoreStateSize)
        return nullptr;
    void* mapping = mmap(nullptr, semaphoreStateSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED)
        return nullptr;
    return static_cast<SemaphoreState*>(mapping);
}

static bool tryDecrement(SemaphoreState& state)
{
    for (;;) {
        uint32_t count = state.count.load(std::memory_order_acquire);
        if (!count)
            return false;
        if (state.count.compareExchangeStrong(count, count - 1, std::memory_order_acq_rel, std::memory_order_acquire) == count)
            return true;
    }
}

bool Semaphore::waitImpl(Timeout timeout)
{
    auto& state = *static_cast<SemaphoreState*>(m_state);
    if (tryDecrement(state))
        return true;
    if (timeout.didTimeOut())
        return false;

    // Polling before parking only pays off on a semaphore that gets signalled
    // while the waiter is still awake. Keep polling while that keeps happening,
    // stop when it does not, and retry occasionally so that a semaphore which
    // becomes busy starts polling again.
    unsigned waitCount = m_waitCount.exchangeAdd(1, std::memory_order_relaxed);
    if (m_shouldPoll.load(std::memory_order_relaxed) || !(waitCount % semaphorePollRetryInterval)) {
        MonotonicTime pollDeadline = MonotonicTime::now() + semaphorePollDuration;
        do {
            for (unsigned i = 0; i < 64; ++i)
                simde_mm_pause();
            if (tryDecrement(state)) {
                m_shouldPoll.store(true, std::memory_order_relaxed);
                return true;
            }
        } while (MonotonicTime::now() < pollDeadline);
        m_shouldPoll.store(false, std::memory_order_relaxed);
    }

    // Polling did not find anything, so tell the peer that a system call is
    // needed from here on, and re-check before parking.
    state.parked.exchangeAdd(1);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    bool result = false;
    for (;;) {
        if (tryDecrement(state)) {
            result = true;
            break;
        }
        if (timeout.isInfinity()) {
            if (!futexWaitWhileZero(state.count, nullptr))
                break;
        } else {
            Seconds remaining = timeout.secondsUntilDeadline();
            if (!remaining)
                break;
            uint64_t nanoseconds = remaining.nanosecondsAs<uint64_t>();
            FutexTimespec relativeTimeout = { static_cast<long>(nanoseconds / 1000000000ULL), static_cast<long>(nanoseconds % 1000000000ULL) };
            if (!futexWaitWhileZero(state.count, &relativeTimeout))
                break;
        }
    }
    state.parked.exchangeSub(1);
    return result;
}
#endif

Semaphore::Semaphore()
{
#if OS(LINUX)
    int fd = -1;
    do {
        fd = static_cast<int>(syscall(__NR_memfd_create, "IPCSemaphore", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    } while (fd == -1 && errno == EINTR);
    if (fd == -1) {
        RELEASE_LOG_ERROR(IPC, "Failed to create IPC semaphore: %" PUBLIC_LOG_STRING, safeStrerror(errno).data());
        return;
    }
    int result = -1;
    do {
        result = ftruncate(fd, semaphoreStateSize);
    } while (result == -1 && errno == EINTR);
    if (result == -1 || fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) == -1) {
        RELEASE_LOG_ERROR(IPC, "Failed to prepare IPC semaphore: %" PUBLIC_LOG_STRING, safeStrerror(errno).data());
        closeWithRetry(fd);
        return;
    }
    m_fd = { fd, UnixFileDescriptor::Adopt };
    m_state = mapSemaphoreState(m_fd.value());
    if (!m_state) {
        RELEASE_LOG_ERROR(IPC, "Failed to map IPC semaphore: %" PUBLIC_LOG_STRING, safeStrerror(errno).data());
        m_fd = { };
    }
#endif
}

Semaphore::Semaphore(UnixFileDescriptor&& fd)
    : m_fd(WTF::move(fd))
{
#if OS(LINUX)
    m_state = mapSemaphoreState(m_fd.value());
#endif
}

Semaphore::Semaphore(Semaphore&& other)
    : m_fd(WTF::move(other.m_fd))
#if OS(LINUX)
    , m_state(std::exchange(other.m_state, nullptr))
#endif
{
}

Semaphore& Semaphore::operator=(Semaphore&& other)
{
    if (this != &other) {
        destroy();
        m_fd = WTF::move(other.m_fd);
#if OS(LINUX)
        m_state = std::exchange(other.m_state, nullptr);
#endif
    }
    return *this;
}

Semaphore::~Semaphore()
{
    destroy();
}

void Semaphore::signal()
{
#if OS(LINUX)
    if (!m_state) [[unlikely]]
        return;
    auto& state = *static_cast<SemaphoreState*>(m_state);
    state.count.exchangeAdd(1);
    if (state.parked.load(std::memory_order_seq_cst))
        futexWakeOne(state.count);
#endif
}

bool Semaphore::wait()
{
#if OS(LINUX)
    if (!m_state) [[unlikely]]
        return false;
    return waitImpl(Timeout::infinity());
#else
    return false;
#endif
}

bool Semaphore::waitFor(Timeout timeout)
{
#if OS(LINUX)
    if (!m_state) [[unlikely]]
        return false;
    return waitImpl(timeout);
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
#if OS(LINUX)
    if (m_state) {
        munmap(m_state, semaphoreStateSize);
        m_state = nullptr;
    }
#endif
    m_fd = { };
}

} // namespace IPC
