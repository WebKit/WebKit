/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(WORK_INTERVAL_API)

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <optional>
#include <sys/work_interval.h>
#include <thread>
#include <wtf/MonotonicTime.h>

namespace WTF {

class WorkgroupInterval {
public:
    WTF_EXPORT_PRIVATE WorkgroupInterval(const char* name);
    WTF_EXPORT_PRIVATE ~WorkgroupInterval();
    WTF_EXPORT_PRIVATE bool join();
    WTF_EXPORT_PRIVATE bool leave();
    WTF_EXPORT_PRIVATE bool start(MonotonicTime start, MonotonicTime deadline);
    WTF_EXPORT_PRIVATE bool update(MonotonicTime deadline);
    WTF_EXPORT_PRIVATE bool finish();

private:
    WTF_EXPORT_PRIVATE static work_interval_t createInterval(const char* name);

    work_interval_t m_interval { nullptr };
    work_interval_instance_t m_instance { nullptr };
    std::optional<std::thread::id> m_startThread;
};

class WorkgroupIntervalScope {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WorkgroupIntervalScope(WorkgroupInterval& interval, MonotonicTime start, MonotonicTime deadline)
        : m_interval(interval)
    {
        m_started = m_interval.start(start, deadline);
    }
    ~WorkgroupIntervalScope()
    {
        if (m_started)
            RELEASE_ASSERT(m_interval.finish());
    }
private:
    bool m_started { false };
    WorkgroupInterval& m_interval;
};

}

#endif
