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
#include <wtf/UniStdExtras.h>

namespace WebCore {

struct DMABufReleaseFlag {
    DMABufReleaseFlag() = default;

    enum InitializeTag { Initialize };
    DMABufReleaseFlag(InitializeTag)
    {
        fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    }

    ~DMABufReleaseFlag()
    {
        if (fd != -1)
            close(fd);
        fd = -1;
    }

    DMABufReleaseFlag(const DMABufReleaseFlag&) = delete;
    DMABufReleaseFlag& operator=(const DMABufReleaseFlag&) = delete;

    DMABufReleaseFlag(DMABufReleaseFlag&& o)
    {
        fd = o.fd;
        o.fd = -1;
    }

    DMABufReleaseFlag& operator=(DMABufReleaseFlag&& o)
    {
        if (this == &o)
            return *this;

        this->~DMABufReleaseFlag();
        new (this) DMABufReleaseFlag(WTFMove(o));
        return *this;
    }

    DMABufReleaseFlag dup() const
    {
        if (fd == -1)
            return { };

        DMABufReleaseFlag flag;
        flag.fd = dupCloseOnExec(fd);
        return flag;
    }

    bool released() const
    {
        if (fd == -1)
            return true;

        uint64_t value { 0 };
        if (read(fd, &value, sizeof(uint64_t)) == sizeof(uint64_t))
            return !!value;
        return false;
    }

    void release()
    {
        if (fd == -1)
            return;

        uint64_t value { 1 };
        write(fd, &value, sizeof(uint64_t));
    }

    int fd { -1 };
};

} // namespace WebCore
