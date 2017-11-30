/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#include "AVMediaCaptureSource.h"
#include "OrientationNotifier.h"

OBJC_CLASS CALayer;
OBJC_CLASS AVFrameRateRange;

typedef struct CGImage *CGImageRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class FloatRect;
class GraphicsContext;
class PixelBufferConformerCV;

class AVVideoCaptureSource : public AVMediaCaptureSource, private OrientationNotifier::Observer {
public:
    static CaptureSourceOrError create(const AtomicString&, const MediaConstraints*);

    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    int32_t width() const { return m_width; }
    int32_t height() const { return m_height; }

private:
    AVVideoCaptureSource(AVCaptureDevice*, const AtomicString&);
    virtual ~AVVideoCaptureSource();

    bool setupCaptureSession() final;
    void shutdownCaptureSession() final;

    void updateSettings(RealtimeMediaSourceSettings&) final;

    void applySizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;
    bool applySize(const IntSize&) final;
    bool applyFrameRate(double) final;
    bool setPreset(NSString*);

    void monitorOrientation(OrientationNotifier&) final;
    void computeSampleRotation();

    bool isFrameRateSupported(double frameRate);

    NSString *bestSessionPresetForVideoDimensions(std::optional<int> width, std::optional<int> height) const;
    bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) final;

    void initializeCapabilities(RealtimeMediaSourceCapabilities&) final;
    void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) final;

    // OrientationNotifier::Observer API
    void orientationChanged(int orientation) final;

    bool setFrameRateConstraint(double minFrameRate, double maxFrameRate);

    void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*) final;
    void processNewFrame(RetainPtr<CMSampleBufferRef>, RetainPtr<AVCaptureConnection>);

    RetainPtr<NSString> m_pendingPreset;
    RetainPtr<CMSampleBufferRef> m_buffer;
    RetainPtr<AVCaptureVideoDataOutput> m_videoOutput;

    int32_t m_width { 0 };
    int32_t m_height { 0 };
    int m_sensorOrientation { 0 };
    int m_deviceOrientation { 0 };
    MediaSample::VideoRotation m_sampleRotation { MediaSample::VideoRotation::None };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
