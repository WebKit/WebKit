/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#include <sys/eventfd.h>
#include <wtf/Noncopyable.h>
#include <wtf/SafeStrerror.h>
#include <wtf/text/CString.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebCore {

struct DMABufReleaseFlag {
    WTF_MAKE_NONCOPYABLE(DMABufReleaseFlag);

    DMABufReleaseFlag() = default;

    enum InitializeTag { Initialize };
    DMABufReleaseFlag(InitializeTag)
    {
        fd = { eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK), UnixFileDescriptor::Adopt };
    }

    ~DMABufReleaseFlag() = default;

    DMABufReleaseFlag(DMABufReleaseFlag&&) = default;
    DMABufReleaseFlag& operator=(DMABufReleaseFlag&&) = default;

    DMABufReleaseFlag dup() const
    {
        DMABufReleaseFlag flag;
        flag.fd = fd.duplicate();
        return flag;
    }

    bool released() const
    {
        if (!fd)
            return true;

        uint64_t value { 0 };
        if (read(fd.value(), &value, sizeof(uint64_t)) == sizeof(uint64_t))
            return !!value;
        return false;
    }

    void release()
    {
        if (!fd)
            return;

        uint64_t value { 1 };
        if (write(fd.value(), &value, sizeof(value)) != sizeof(value))
            WTFLogAlways("Error writing to the eventfd at DMABufReleaseFlag: %s", safeStrerror(errno).data());
    }

    UnixFileDescriptor fd;
};

} // namespace WebCore
