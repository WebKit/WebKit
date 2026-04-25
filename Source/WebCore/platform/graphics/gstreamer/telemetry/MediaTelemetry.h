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

#pragma once

#if ENABLE(MEDIA_TELEMETRY)

#include "MediaTelemetryReportPrivateMembers.h"
#include <mutex>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaTelemetryWaylandInfoGetter {
public:
    typedef void *EGLConfig;
    typedef void *EGLContext;
    typedef void *EGLDisplay;
    typedef void *EGLSurface;
    virtual EGLDisplay eglDisplay() const = 0;
    virtual EGLConfig eglConfig() const = 0;
    virtual EGLSurface eglSurface() const = 0;
    virtual EGLContext eglContext() const = 0;
    virtual unsigned windowWidth() const = 0;
    virtual unsigned windowHeight() const = 0;
};

class MediaTelemetryReport {
public:
    WTF_MAKE_NONCOPYABLE(MediaTelemetryReport);
    WTF_MAKE_NONMOVABLE(MediaTelemetryReport);

    static MediaTelemetryReport& singleton();

    enum class AVPipelineState {
        Create,
        Play,
        Pause,
        Stop,
        Destroy,
        FirstFrameDecoded,
        EndOfStream,
        DecryptError,
        PlaybackError,
        DrmError,
        Error,
        SeekStart,
        SeekDone,
        VideoResolutionChanged,
        Unknown
    };
    enum class MediaType {
        Audio,
        Video,
        None
    };
    enum class DrmType {
        PlayReady,
        Widevine,
        None,
        Unknown
    };
    enum class WaylandAction {
        InitGfx,
        DeinitGfx,
        InitInputs,
        DeinitInputs
    };
    enum class WaylandGraphicsState {
        GfxNotInitialized,
        GfxInitialized
    };
    enum class WaylandInputsState {
        InputsNotInitialized,
        InputsInitialized
    };

    ~MediaTelemetryReport();
    void reportPlaybackState(AVPipelineState, const String& additionalInfo = emptyString(), MediaType = MediaType::None);
    void reportDrmInfo(DrmType, const String& additionalInfo = emptyString());
    void reportWaylandInfo(const MediaTelemetryWaylandInfoGetter&, WaylandAction, WaylandGraphicsState, WaylandInputsState);

private:
    MediaTelemetryReport() { }

    std::unique_ptr<MediaTelemetryReportPrivateMembers> m_privateMembers;
    String m_name;
};

} // namespace WebCore

#endif // ENABLE(TELEMETRY)
