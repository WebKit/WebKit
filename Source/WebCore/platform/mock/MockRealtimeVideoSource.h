/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockRealtimeVideoSource_h
#define MockRealtimeVideoSource_h

#if ENABLE(MEDIA_STREAM)

#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MockMediaDevice.h"
#include "OrientationNotifier.h"
#include "RealtimeMediaSourceFactory.h"
#include "RealtimeVideoCaptureSource.h"
#include <wtf/RunLoop.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;

class MockRealtimeVideoSource : public RealtimeVideoCaptureSource, private OrientationNotifier::Observer {
public:
    static CaptureSourceOrError create(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&, const MediaConstraints*, PageIdentifier);
    ~MockRealtimeVideoSource();

    static void setIsInterrupted(bool);

    ImageBuffer* imageBuffer() const;

protected:
    MockRealtimeVideoSource(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&&, PageIdentifier);

    virtual void updateSampleBuffer() = 0;

    Seconds elapsedTime();
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) override;
    VideoFrame::Rotation videoFrameRotation() const final { return m_deviceOrientation; }
    void generatePresets() override;

    IntSize captureSize() const;

private:
    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;

    void startProducingData() final;
    void stopProducingData() final;
    bool isCaptureSource() const final { return true; }
    CaptureDevice::DeviceType deviceType() const final { return mockCamera() ? CaptureDevice::DeviceType::Camera : CaptureDevice::DeviceType::Screen; }
    bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;
    void setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;
    void setFrameRateWithPreset(double, RefPtr<VideoPreset>) final;


    bool isMockSource() const final { return true; }

    // OrientationNotifier::Observer
    void orientationChanged(int orientation) final;
    void monitorOrientation(OrientationNotifier&) final;

    void drawAnimation(GraphicsContext&);
    void drawText(GraphicsContext&);
    void drawBoxes(GraphicsContext&);

    void generateFrame();
    void startCaptureTimer();

    void delaySamples(Seconds) final;

    bool mockCamera() const { return std::holds_alternative<MockCameraProperties>(m_device.properties); }
    bool mockDisplay() const { return std::holds_alternative<MockDisplayProperties>(m_device.properties); }
    bool mockScreen() const { return mockDisplayType(CaptureDevice::DeviceType::Screen); }
    bool mockWindow() const { return mockDisplayType(CaptureDevice::DeviceType::Window); }
    bool mockDisplayType(CaptureDevice::DeviceType) const;

    float m_baseFontSize { 0 };
    float m_bipBopFontSize { 0 };
    float m_statsFontSize { 0 };

    mutable RefPtr<ImageBuffer> m_imageBuffer;

    Path m_path;
    DashArray m_dashWidths;

    MonotonicTime m_startTime { MonotonicTime::nan() };
    Seconds m_elapsedTime { 0_s };
    MonotonicTime m_delayUntil;

    unsigned m_frameNumber { 0 };
    RunLoop::Timer<MockRealtimeVideoSource> m_emitFrameTimer;
    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    std::optional<RealtimeMediaSourceSettings> m_currentSettings;
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    Color m_fillColor { Color::black };
    MockMediaDevice m_device;
    RefPtr<VideoPreset> m_preset;
    VideoFrame::Rotation m_deviceOrientation { VideoFrame::Rotation::None };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MockRealtimeVideoSource)
    static bool isType(const WebCore::RealtimeMediaSource& source) { return source.isCaptureSource() && source.isMockSource() && (source.deviceType() == WebCore::CaptureDevice::DeviceType::Camera || source.deviceType() == WebCore::CaptureDevice::DeviceType::Screen || source.deviceType() == WebCore::CaptureDevice::DeviceType::Window); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_STREAM)

#endif // MockRealtimeVideoSource_h
