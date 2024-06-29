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

namespace WebCore {

LibWebRTCLogSink::LibWebRTCLogSink(LogCallback&& callback)
    : m_callback(WTFMove(callback))
{
}

LibWebRTCLogSink::~LibWebRTCLogSink()
{
    ASSERT(!m_loggingLevel);
}

void LibWebRTCLogSink::logMessage(const std::string& message, rtc::LoggingSeverity severity)
{
    m_callback(severity, message);
}

void LibWebRTCLogSink::start(rtc::LoggingSeverity level)
{
    if (level == rtc::LoggingSeverity::LS_NONE) {
        stop();
        return;
    }

    if (m_loggingLevel) {
        if (*m_loggingLevel == level)
            return;
        rtc::LogMessage::RemoveLogToStream(this);
    }

    m_loggingLevel = level;
    rtc::LogMessage::AddLogToStream(this, level);
}

void LibWebRTCLogSink::stop()
{
    if (!m_loggingLevel)
        return;

    m_loggingLevel = { };
    rtc::LogMessage::RemoveLogToStream(this);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
