/*
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

#pragma once

#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/ProcessMemoryStatus.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WTF {

class MemoryStatusMonitor {
    WTF_MAKE_NONCOPYABLE(MemoryStatusMonitor);
    friend NeverDestroyed<MemoryStatusMonitor>;
public:
    static MemoryStatusMonitor& singleton();

    void updateMemoryStatus();

    ProcessMemoryStatus memoryStatus() const
    {
        Locker locker { m_lock };
        return m_memoryStatus;
    }

    ~MemoryStatusMonitor() = default;

private:
    MemoryStatusMonitor();

    std::optional<size_t> parseNextColumn(StringView&);

    WTF::UnixFileDescriptor m_statmFd;

    mutable Lock m_lock;
    ProcessMemoryStatus m_memoryStatus WTF_GUARDED_BY_LOCK(m_lock);
    MonotonicTime m_lastUpdateTime WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WTF
