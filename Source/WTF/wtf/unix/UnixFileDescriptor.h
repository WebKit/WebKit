/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <utility>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniStdExtras.h>

namespace WTF {

class UnixFileDescriptor {
public:
    UnixFileDescriptor() = default;

    enum AdoptionTag { Adopt };
    UnixFileDescriptor(int fd, AdoptionTag)
        : m_value(fd)
    { }

    enum DuplicationTag { Duplicate };
    UnixFileDescriptor(int fd, DuplicationTag)
    {
        if (fd >= 0)
            m_value = dupCloseOnExec(fd);
    }

    UnixFileDescriptor(UnixFileDescriptor&& o)
    {
        m_value = o.release();
    }

    UnixFileDescriptor(const UnixFileDescriptor& o)
        : UnixFileDescriptor(o.m_value, Duplicate)
    { }

    UnixFileDescriptor& operator=(UnixFileDescriptor&& o)
    {
        if (&o == this)
            return *this;

        this->~UnixFileDescriptor();
        new (this) UnixFileDescriptor(WTFMove(o));
        return *this;
    }

    UnixFileDescriptor& operator=(const UnixFileDescriptor& o)
    {
        if (&o == this)
            return *this;

        this->~UnixFileDescriptor();
        new (this) UnixFileDescriptor(o);
        return *this;
    }

    ~UnixFileDescriptor()
    {
        if (m_value >= 0)
            closeWithRetry(std::exchange(m_value, -1));
    }

    explicit operator bool() const { return m_value >= 0; }

    int value() const { return m_value; }

    UnixFileDescriptor duplicate() const
    {
        return UnixFileDescriptor { m_value, Duplicate };
    }

    int release() WARN_UNUSED_RETURN { return std::exchange(m_value, -1); }

private:
    int m_value { -1 };
};

} // namespace WTF

using WTF::UnixFileDescriptor;
