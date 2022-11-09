/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "RealtimeVideoCaptureSource.h"

namespace WebCore {

class ImageTransferSessionVT;

class RealtimeVideoSource final
    : public RealtimeMediaSource
    , public RealtimeMediaSource::Observer
    , public RealtimeMediaSource::VideoFrameObserver {
public:
    static Ref<RealtimeVideoSource> create(Ref<RealtimeVideoCaptureSource>&& source, bool shouldUseIOSurface = true) { return adoptRef(*new RealtimeVideoSource(WTFMove(source), shouldUseIOSurface)); }

    Vector<VideoPresetData> presetsData() { return m_source->presetsData(); }

private:
    WEBCORE_EXPORT RealtimeVideoSource(Ref<RealtimeVideoCaptureSource>&&, bool shouldUseIOSurface);
    ~RealtimeVideoSource();

    // RealtimeMediaSource
    void startProducingData() final;
    void stopProducingData() final;
    bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate) final;
    void setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate) final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;
    Ref<RealtimeMediaSource> clone() final;
    void endProducingData() final;
    void stopBeingObserved() final;

    const RealtimeMediaSourceCapabilities& capabilities() final { return m_source->capabilities(); }
    const RealtimeMediaSourceSettings& settings() final { return m_currentSettings; }
    bool isCaptureSource() const final { return m_source->isCaptureSource(); }
    CaptureDevice::DeviceType deviceType() const final { return m_source->deviceType(); }
    void monitorOrientation(OrientationNotifier& notifier) final { m_source->monitorOrientation(notifier); }
    bool interrupted() const final { return m_source->interrupted(); }
    bool isSameAs(RealtimeMediaSource& source) const final { return this == &source || m_source.ptr() == &source; }
    void whenReady(CompletionHandler<void(String)>&&) final;
    bool isVideoSource() const final { return true; }

    // RealtimeMediaSource::Observer
    void sourceMutedChanged() final;
    void sourceSettingsChanged() final;
    void sourceStopped() final;
    bool preventSourceFromStopping() final;

    // RealtimeMediaSource::VideoFrameObserver
    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) final;

    RefPtr<VideoFrame> adaptVideoFrame(VideoFrame&);

#if !RELEASE_LOG_DISABLED
    void setLogger(const Logger&, const void*) final;
#endif

    Ref<RealtimeVideoCaptureSource> m_source;
    RealtimeMediaSourceSettings m_currentSettings;
#if PLATFORM(COCOA)
    std::unique_ptr<ImageTransferSessionVT> m_imageTransferSession;
    bool m_shouldUseIOSurface { true };
#endif
    size_t m_frameDecimation { 1 };
    size_t m_frameDecimationCounter { 0 };
#if !RELEASE_LOG_DISABLED
    uint64_t m_cloneCounter { 0 };
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

