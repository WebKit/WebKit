/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "MockRealtimeVideoSource.h"
#include "OrientationNotifier.h"
#include "PixelBufferConformerCV.h"

typedef struct __CVBuffer *CVBufferRef;
typedef CVBufferRef CVImageBufferRef;
typedef CVImageBufferRef CVPixelBufferRef;
typedef struct __CVPixelBufferPool *CVPixelBufferPoolRef;

namespace WebCore {

class PixelBufferResizer;

class MockRealtimeVideoSourceMac final : public MockRealtimeVideoSource, private OrientationNotifier::Observer {
public:
    virtual ~MockRealtimeVideoSourceMac() = default;

private:
    friend class MockRealtimeVideoSource;
    MockRealtimeVideoSourceMac(const String& deviceID, const String& name);

    RetainPtr<CMSampleBufferRef> CMSampleBufferFromPixelBuffer(CVPixelBufferRef);
    RetainPtr<CVPixelBufferRef> pixelBufferFromCGImage(CGImageRef) const;

    PlatformLayer* platformLayer() const;
    void updateSampleBuffer() final;
    void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) final;
    bool canResizeVideoFrames() const final { return true; }
    void setSizeAndFrameRateWithPreset(IntSize, double, RefPtr<VideoPreset>) final;

    void orientationChanged(int orientation) final;
    void monitorOrientation(OrientationNotifier&) final;

    mutable RetainPtr<CVPixelBufferPoolRef> m_bufferPool;
    MediaSample::VideoRotation m_deviceOrientation { MediaSample::VideoRotation::None };
    std::unique_ptr<PixelBufferConformerCV> m_pixelBufferConformer;
    std::unique_ptr<PixelBufferResizer> m_pixelBufferResizer;
    IntSize m_presetSize;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
