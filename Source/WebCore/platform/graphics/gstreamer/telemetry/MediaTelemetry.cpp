/*
 * Copyright (C) 2025 Comcast
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaTelemetry.h"

#if ENABLE(MEDIA_TELEMETRY)

#include "NotImplemented.h"

namespace WebCore {

MediaTelemetryReport& MediaTelemetryReport::singleton()
{
    static MediaTelemetryReport instance;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        instance.m_privateMembers = makeUnique<MediaTelemetryReportPrivateMembers>();
        instance.m_name = "WPEWebKit"_s;
        notImplemented();
    });
    return instance;
}

// deInit can be implemented here.
MediaTelemetryReport::~MediaTelemetryReport()
{
    notImplemented();
}

void MediaTelemetryReport::reportPlaybackState(AVPipelineState state, const String& additionalInfo, MediaType mediaType)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(additionalInfo);
    UNUSED_PARAM(mediaType);
    notImplemented();
}

void MediaTelemetryReport::reportDrmInfo(DrmType drmType, const String& additionalInfo)
{
    UNUSED_PARAM(drmType);
    UNUSED_PARAM(additionalInfo);
    notImplemented();
}

void MediaTelemetryReport::reportWaylandInfo(const MediaTelemetryWaylandInfoGetter& getter, WaylandAction action,
    WaylandGraphicsState gfxState, WaylandInputsState inputsState)
{
    UNUSED_PARAM(getter);
    UNUSED_PARAM(action);
    UNUSED_PARAM(gfxState);
    UNUSED_PARAM(inputsState);
    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_TELEMETRY)
