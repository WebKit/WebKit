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

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/rtc_base/logging.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {
class LibWebRTCLogSink;
}

namespace WebCore {

class LibWebRTCLogSink final : webrtc::LogSink {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCLogSink);
public:
    using LogCallback = Function<void(webrtc::LoggingSeverity, const std::string&)>;
    explicit LibWebRTCLogSink(LogCallback&&);

    ~LibWebRTCLogSink();

    void start(webrtc::LoggingSeverity = webrtc::LoggingSeverity::LS_VERBOSE);
    void stop();

private:
    void OnLogMessage(const std::string& message, webrtc::LoggingSeverity severity) final { logMessage(message, severity); }
    void OnLogMessage(const std::string& message) final { logMessage(message, webrtc::LoggingSeverity::LS_INFO); }

    void logMessage(const std::string&, webrtc::LoggingSeverity);

    LogCallback m_callback;
    std::optional<webrtc::LoggingSeverity> m_loggingLevel;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
