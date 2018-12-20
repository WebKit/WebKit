/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaSample.h"
#include "RealtimeMediaSource.h"
#include "VideoPreset.h"
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>

namespace WebCore {

class ImageTransferSessionVT;

class RealtimeVideoSource : public RealtimeMediaSource {
public:
    virtual ~RealtimeVideoSource();

protected:
    RealtimeVideoSource(String&& name, String&& id, String&& hashSalt);

    void prepareToProduceData();
    bool supportsSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double>) override;
    void setSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double>) override;

    virtual void generatePresets() = 0;
    virtual bool prefersPreset(VideoPreset&) { return true; }
    virtual void setSizeAndFrameRateWithPreset(IntSize, double, RefPtr<VideoPreset>) { };
    virtual bool canResizeVideoFrames() const { return false; }
    bool shouldUsePreset(VideoPreset& current, VideoPreset& candidate);

    void setSupportedPresets(const Vector<Ref<VideoPreset>>&);
    void setSupportedPresets(Vector<VideoPresetData>&&);
    const Vector<Ref<VideoPreset>>& presets();

    bool frameRateRangeIncludesRate(const FrameRateRange&, double);

    void updateCapabilities(RealtimeMediaSourceCapabilities&);

    void setDefaultSize(const IntSize& size) { m_defaultSize = size; }

    double observedFrameRate() const { return m_observedFrameRate; }

    void dispatchMediaSampleToObservers(MediaSample&);
    const Vector<IntSize>& standardVideoSizes();

private:
    struct CaptureSizeAndFrameRate {
        RefPtr<VideoPreset> encodingPreset;
        IntSize requestedSize;
        double requestedFrameRate { 0 };
    };
    bool supportsCaptureSize(Optional<int>, Optional<int>, const Function<bool(const IntSize&)>&&);
    Optional<CaptureSizeAndFrameRate> bestSupportedSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double>);
    bool presetSupportsFrameRate(RefPtr<VideoPreset>, double);

    Vector<Ref<VideoPreset>> m_presets;
    Deque<double> m_observedFrameTimeStamps;
    double m_observedFrameRate { 0 };
    IntSize m_defaultSize;
#if PLATFORM(COCOA)
    std::unique_ptr<ImageTransferSessionVT> m_imageTransferSession;
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

