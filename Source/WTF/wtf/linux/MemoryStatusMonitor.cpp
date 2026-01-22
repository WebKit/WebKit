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

#include "config.h"
#include "MemoryStatusMonitor.h"

#include <fcntl.h>
#include <unistd.h>
#include <wtf/PageBlock.h>
#include <wtf/Threading.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

namespace WTF {

static const Seconds s_maxUpdateInterval { 10_s };

MemoryStatusMonitor& MemoryStatusMonitor::singleton()
{
    static NeverDestroyed<MemoryStatusMonitor> memoryStatusMonitor;
    return memoryStatusMonitor;
}

MemoryStatusMonitor::MemoryStatusMonitor()
{
    m_statmFd = UnixFileDescriptor { open("/proc/self/statm", O_RDONLY | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (!m_statmFd)
        return;

    Thread::create("MemoryStatusMonitor"_s, [this] {
        while (true) {
            MonotonicTime nextUpdateTime;
            {
                Locker locker { m_lock };
                nextUpdateTime = m_lastUpdateTime + s_maxUpdateInterval;
            }

            if (nextUpdateTime <= MonotonicTime::now()) {
                updateMemoryStatus();
                sleep(s_maxUpdateInterval);
                continue;
            }

            sleep(nextUpdateTime - MonotonicTime::now());
        }
    })->detach();
}

void MemoryStatusMonitor::updateMemoryStatus()
{
    std::array<char, 256> buffer;

    ssize_t numBytes = pread(m_statmFd.value(), buffer.data(), buffer.size() - 1, 0);
    if (numBytes <= 0)
        return;

    buffer[numBytes] = '\0';

    auto view = StringView::fromLatin1(buffer.data());

    auto size = parseNextColumn(view);
    if (!size)
        return;

    auto resident = parseNextColumn(view);
    if (!resident)
        return;

    auto shared = parseNextColumn(view);
    if (!shared)
        return;

    auto text = parseNextColumn(view);
    if (!text)
        return;

    auto lib = parseNextColumn(view);
    if (!lib)
        return;

    auto data = parseNextColumn(view);
    if (!data)
        return;

    auto dt = parseNextColumn(view);
    if (!dt)
        return;

    size_t pageSize = WTF::pageSize();

    Locker locker { m_lock };
    m_memoryStatus = {
        .size = *size * pageSize,
        .resident = *resident * pageSize,
        .shared = *shared * pageSize,
        .text = *text * pageSize,
        .lib = *lib * pageSize,
        .data = *data * pageSize,
        .dt = *dt * pageSize
    };
    m_lastUpdateTime = MonotonicTime::now();
}

std::optional<size_t> MemoryStatusMonitor::parseNextColumn(StringView& view)
{
    size_t whitspaceIndex = view.find(' ');
    if (whitspaceIndex == notFound)
        return std::nullopt;
    view = view.substring(0, whitspaceIndex + 1);
    return parseIntegerAllowingTrailingJunk<size_t>(view);
}

} // namespace WTF
