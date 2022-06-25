/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "IntSizeHash.h"
#include "OrientationNotifier.h"
#include "RealtimeVideoCaptureSource.h"
#include "Timer.h"
#include <wtf/Lock.h>
#include <wtf/text/StringHash.h>

typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;

OBJC_CLASS AVCaptureConnection;
OBJC_CLASS AVCaptureDevice;
OBJC_CLASS AVCaptureDeviceFormat;
OBJC_CLASS AVCaptureOutput;
OBJC_CLASS AVCaptureSession;
OBJC_CLASS AVCaptureVideoDataOutput;
OBJC_CLASS AVFrameRateRange;
OBJC_CLASS NSError;
OBJC_CLASS NSNotification;
OBJC_CLASS WebCoreAVVideoCaptureSourceObserver;

namespace WebCore {

class AVVideoPreset;
class ImageTransferSessionVT;

class AVVideoCaptureSource : public RealtimeVideoCaptureSource, private OrientationNotifier::Observer {
public:
    static CaptureSourceOrError create(const CaptureDevice&, String&& hashSalt, const MediaConstraints*, PageIdentifier);

    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    void captureSessionBeginInterruption(RetainPtr<NSNotification>);
    void captureSessionEndInterruption(RetainPtr<NSNotification>);
    void deviceDisconnected(RetainPtr<NSNotification>);

    AVCaptureSession* session() const { return m_session.get(); }

    void captureSessionIsRunningDidChange(bool);
    void captureSessionRuntimeError(RetainPtr<NSError>);
    void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*);
    void captureDeviceSuspendedDidChange();

private:
    AVVideoCaptureSource(AVCaptureDevice*, const CaptureDevice&, String&& hashSalt, PageIdentifier);
    virtual ~AVVideoCaptureSource();

    void clearSession();

    bool setupSession();
    bool setupCaptureSession();
    void shutdownCaptureSession();

    const RealtimeMediaSourceCapabilities& capabilities() final;
    const RealtimeMediaSourceSettings& settings() final;
    void startProducingData() final;
    void stopProducingData() final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;
    void monitorOrientation(OrientationNotifier&) final;
    void beginConfiguration() final;
    void commitConfiguration() final;
    bool isCaptureSource() const final { return true; }
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Camera; }
    bool interrupted() const final;

    VideoFrame::Rotation videoFrameRotation() const final { return m_videoFrameRotation; }
    void setFrameRateWithPreset(double, RefPtr<VideoPreset>) final;
    bool prefersPreset(VideoPreset&) final;
    void generatePresets() final;
    bool canResizeVideoFrames() const final { return true; }

    void setSessionSizeAndFrameRate();
    bool setPreset(NSString*);
    void computeVideoFrameRotation();
    AVFrameRateRange* frameDurationForFrameRate(double);

    // OrientationNotifier::Observer API
    void orientationChanged(int orientation) final;

    bool setFrameRateConstraint(double minFrameRate, double maxFrameRate);

    IntSize sizeForPreset(NSString*);

    AVCaptureDevice* device() const { return m_device.get(); }

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "AVVideoCaptureSource"; }
#endif

    void updateVerifyCapturingTimer();
    void verifyIsCapturing();

    RefPtr<VideoFrame> m_buffer;
    RetainPtr<AVCaptureVideoDataOutput> m_videoOutput;
    std::unique_ptr<ImageTransferSessionVT> m_imageTransferSession;

    int m_sensorOrientation { 0 };
    int m_deviceOrientation { 0 };
    VideoFrame::Rotation m_videoFrameRotation { VideoFrame::Rotation::None };

    std::optional<RealtimeMediaSourceSettings> m_currentSettings;
    std::optional<RealtimeMediaSourceCapabilities> m_capabilities;
    RetainPtr<WebCoreAVVideoCaptureSourceObserver> m_objcObserver;
    RetainPtr<AVCaptureSession> m_session;
    RetainPtr<AVCaptureDevice> m_device;

    RefPtr<AVVideoPreset> m_currentPreset;
    RefPtr<AVVideoPreset> m_appliedPreset;
    RetainPtr<AVFrameRateRange> m_appliedFrameRateRange;

    double m_currentFrameRate;
    bool m_interrupted { false };
    bool m_isRunning { false };

    static constexpr Seconds verifyCaptureInterval = 30_s;
    static const uint64_t framesToDropWhenStarting = 4;

    Timer m_verifyCapturingTimer;
    uint64_t m_framesCount { 0 };
    uint64_t m_lastFramesCount { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AVVideoPreset)
    static bool isType(const WebCore::VideoPreset& preset) { return preset.type == WebCore::VideoPreset::VideoPresetType::AVCapture; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_STREAM)
