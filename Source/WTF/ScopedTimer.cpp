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

#include "config.h"
#include <wtf/ScopedTimer.h>

#include <wtf/Logging.h>

namespace WTF {

template<typename... Arguments>
static inline void log(UNUSED_FUNCTION const Arguments&... arguments)
{
    auto logMessage = makeString(LogArgument<Arguments>::toString(arguments)...);
    auto channel = WTFLogScopedTimer;
    
#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", logMessage.utf8().data());
#elif USE(OS_LOG)
        os_log(channel.osLogChannel, "%{public}s", logMessage.utf8().data());
#elif ENABLE(JOURNALD_LOG)
        sd_journal_send("WEBKIT_SUBSYSTEM=%s", channel.subsystem, "WEBKIT_CHANNEL=%s", channel.name, "MESSAGE=%s", logMessage.utf8().data(), nullptr);
#else
        fprintf(stderr, "[%s:%s:-] %s\n", channel.subsystem, channel.name, logMessage.utf8().data());
#endif
}

ScopedTimer::~ScopedTimer()
{
    auto duration = MonotonicTime::now() - m_startTime;
    log(m_name, " took ", static_cast<uint64_t>(duration.nanoseconds()), " ns");
}

}
