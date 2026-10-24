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

#include "config.h"
#include <wtf/Logging.h>

#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

DEFINE_LOG_CHANNEL(Language);
DEFINE_LOG_CHANNEL_WITH_DETAILS(RefCountedLeaks, logChannelStateOn,  logLevelError);
DEFINE_LOG_CHANNEL_WITH_DETAILS(Process,         logChannelStateOn,  logLevelError);
DEFINE_LOG_CHANNEL_WITH_DETAILS(Threading,       logChannelStateOn,  logLevelError);
DEFINE_LOG_CHANNEL(MemoryPressure);
DEFINE_LOG_CHANNEL(SuspendableWorkQueue);
DEFINE_LOG_CHANNEL(NativePromise);

void logPreformatted([[maybe_unused]] WTFLogChannel& channel, [[maybe_unused]] const String& message)
{
#if !LOG_DISABLED
    if (channel.state != WTFLogChannelState::Off)
        SAFE_WTFLOG(&channel, "%s", message.utf8());
#endif
}

#if !USE(OS_LOG) && !RELEASE_LOG_DISABLED

static int releaseLogPriority(ReleaseLogType type)
{
    switch (type) {
    case ReleaseLogType::Default:
        return WTF_RELEASE_LOG_PRIORITY_DEFAULT;
    case ReleaseLogType::Info:
        return WTF_RELEASE_LOG_PRIORITY_INFO;
    case ReleaseLogType::Debug:
        return WTF_RELEASE_LOG_PRIORITY_DEBUG;
    case ReleaseLogType::Error:
        return WTF_RELEASE_LOG_PRIORITY_ERROR;
    case ReleaseLogType::Fault:
        return WTF_RELEASE_LOG_PRIORITY_FAULT;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void releaseLogPreformatted(WTFLogChannel& channel, ReleaseLogType type, const String& file, unsigned line, const String& function, const String& message)
{
    auto fileUTF8 = file.utf8();
    auto functionUTF8 = function.utf8();
    auto messageUTF8 = message.utf8();
    WTF_RELEASE_LOG_SEND(channel, releaseLogPriority(type), safePrintfType(fileUTF8), line, safePrintfType(functionUTF8), "%s", messageUTF8);
}

#endif // !USE(OS_LOG) && !RELEASE_LOG_DISABLED

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

} // namespace WTF
