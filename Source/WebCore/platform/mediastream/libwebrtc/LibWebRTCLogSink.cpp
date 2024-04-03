/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "LibWebRTCLogSink.h"

#if USE(LIBWEBRTC)
#include "LibWebRTCUtils.h"
#include <wtf/MainThread.h>

namespace WebCore {

LibWebRTCLogSink::LibWebRTCLogSink(LogCallback&& callback)
    : m_callback(WTFMove(callback))
{
}

LibWebRTCLogSink::~LibWebRTCLogSink()
{
}

static String toWebRTCLogLevel(rtc::LoggingSeverity severity)
{
    switch (severity) {
    case rtc::LoggingSeverity::LS_VERBOSE:
        return "verbose"_s;
    case rtc::LoggingSeverity::LS_INFO:
        return "info"_s;
    case rtc::LoggingSeverity::LS_WARNING:
        return "warning"_s;
    case rtc::LoggingSeverity::LS_ERROR:
        return "error"_s;
    case rtc::LoggingSeverity::LS_NONE:
        return "none"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

void LibWebRTCLogSink::logMessage(const std::string& value, rtc::LoggingSeverity severity)
{
    ensureOnMainThread([weakThis = WeakPtr { *this }, message = fromStdString(value).isolatedCopy(), severity] () mutable {
        if (weakThis)
            weakThis->m_callback(toWebRTCLogLevel(severity), WTFMove(message));
    });
}

void LibWebRTCLogSink::start()
{
    rtc::LogMessage::AddLogToStream(this, rtc::LoggingSeverity::LS_VERBOSE);
}

void LibWebRTCLogSink::stop()
{
    rtc::LogMessage::RemoveLogToStream(this);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
