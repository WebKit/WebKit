/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2015 Igalia S.L.
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
#include "Syscall.h"

#if ENABLE(SECCOMP_FILTERS)

#include "ArgumentCoders.h"
#include "OpenSyscall.h"
#include "SigactionSyscall.h"
#include "SigprocmaskSyscall.h"
#include <limits>
#include <seccomp.h>
#include <string.h>
#include <unistd.h>

namespace WebKit {

// The redundant "constexpr const" is to placate Clang's -Wwritable-strings.
static constexpr const char* const message = "Blocked unexpected syscall: ";

// Since "sprintf" is not signal-safe, reimplement %d here. Based on code from
// http://outflux.net/teach-seccomp by Will Drewry and Kees Cook, released under
// the Chromium BSD license.
static void writeUnsignedInt(char* buf, unsigned val)
{
    int width = 0;
    unsigned tens;

    if (!val) {
        strcpy(buf, "0");
        return;
    }
    for (tens = val; tens; tens /= 10)
        ++width;
    buf[width] = '\0';
    for (tens = val; tens; tens /= 10)
        buf[--width] = '0' + (tens % 10);
}

static void reportUnexpectedSyscall(unsigned syscall)
{
    char buf[128];
#if defined(__has_builtin)
#if __has_builtin(__builtin_strlen)
    // Buffer must be big enough for the literal, plus the number of digits in the largest possible
    // unsigned int, plus one for the newline, plus one more for the trailing null.
    static_assert(__builtin_strlen(message) + std::numeric_limits<unsigned>::digits10 + 2 < sizeof(buf), "Buffer too small");
#endif
#endif
    strcpy(buf, message);
    writeUnsignedInt(buf + strlen(buf), syscall);
    strcat(buf, "\n");
    int unused __attribute__((unused));
    unused = write(STDERR_FILENO, buf, strlen(buf));
}

std::unique_ptr<Syscall> Syscall::createFromContext(ucontext_t* ucontext)
{
    mcontext_t* mcontext = &ucontext->uc_mcontext;

    switch (mcontext->gregs[REG_SYSCALL]) {
    case __NR_open:
        return std::make_unique<OpenSyscall>(mcontext);
    case __NR_openat:
        return OpenSyscall::createFromOpenatContext(mcontext);
    case __NR_creat:
        return OpenSyscall::createFromCreatContext(mcontext);
    case __NR_sigprocmask:
    case __NR_rt_sigprocmask:
        return SigprocmaskSyscall::createFromContext(ucontext);
    case __NR_sigaction:
    case __NR_rt_sigaction:
        return SigactionSyscall::createFromContext(mcontext);
    default:
        reportUnexpectedSyscall(mcontext->gregs[REG_SYSCALL]);
        ASSERT_NOT_REACHED();
    }

    return nullptr;
}

std::unique_ptr<Syscall> Syscall::createFromDecoder(IPC::ArgumentDecoder* decoder)
{
    int type;
    if (!decoder->decode(type))
        return nullptr;

    std::unique_ptr<Syscall> syscall;
    if (type == __NR_open)
        syscall = std::make_unique<OpenSyscall>(nullptr);

    if (!syscall->decode(decoder))
        return nullptr;

    return syscall;
}

Syscall::Syscall(int type, mcontext_t* context)
    : m_type(type)
    , m_context(context)
{
}

std::unique_ptr<SyscallResult> SyscallResult::createFromDecoder(IPC::ArgumentDecoder* decoder, int fd)
{
    int type;
    if (!decoder->decode(type))
        return nullptr;

    std::unique_ptr<SyscallResult> result;
    if (type == __NR_open)
        result = std::make_unique<OpenSyscallResult>(-1, 0);

    if (!result->decode(decoder, fd))
        return nullptr;

    return result;
}

SyscallResult::SyscallResult(int type)
    : m_type(type)
{
}

} // namespace WebKit

#endif // ENABLE(SECCOMP_FILTERS)
