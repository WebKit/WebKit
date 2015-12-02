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

#ifndef AVVideoCaptureSource_h
#define AVVideoCaptureSource_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "AVMediaCaptureSource.h"

OBJC_CLASS CALayer;

typedef struct CGImage *CGImageRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class FloatRect;
class GraphicsContext;

class AVVideoCaptureSource : public AVMediaCaptureSource {
public:
    static RefPtr<AVMediaCaptureSource> create(AVCaptureDevice*, const AtomicString&, PassRefPtr<MediaConstraints>);

    int32_t width() const { return m_width; }
    int32_t height() const { return m_height; }

private:
    AVVideoCaptureSource(AVCaptureDevice*, const AtomicString&, PassRefPtr<MediaConstraints>);
    virtual ~AVVideoCaptureSource();

    void setupCaptureSession() override;
    void shutdownCaptureSession() override;

    void updateSettings(RealtimeMediaSourceSettings&) override;

    void initializeCapabilities(RealtimeMediaSourceCapabilities&) override;
    void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) override;

    bool applyConstraints(MediaConstraints*);
    bool setFrameRateConstraint(float minFrameRate, float maxFrameRate);

    bool updateFramerate(CMSampleBufferRef);

    void captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef, AVCaptureConnection*) override;
    void processNewFrame(RetainPtr<CMSampleBufferRef>);

    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    PlatformLayer* platformLayer() const override;

    RetainPtr<CGImageRef> currentFrameCGImage();
    RefPtr<Image> currentFrameImage() override;

    RetainPtr<CMSampleBufferRef> m_buffer;
    RetainPtr<CGImageRef> m_lastImage;
    Vector<Float64> m_videoFrameTimeStamps;
    mutable RetainPtr<PlatformLayer> m_videoPreviewLayer;
    Float64 m_frameRate { 0 };
    int32_t m_width { 0 };
    int32_t m_height { 0 };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // AVVideoCaptureSource_h
