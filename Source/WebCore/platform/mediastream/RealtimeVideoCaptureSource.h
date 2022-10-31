/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "ImageBuffer.h"
#include "RealtimeMediaSource.h"
#include "VideoPreset.h"
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>

namespace WebCore {

class ImageTransferSessionVT;

class WEBCORE_EXPORT RealtimeVideoCaptureSource : public RealtimeMediaSource {
public:
    virtual ~RealtimeVideoCaptureSource();

    void clientUpdatedSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate);

    bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) override;
    virtual void generatePresets() = 0;
    virtual VideoFrame::Rotation videoFrameRotation() const { return VideoFrame::Rotation::None; }

    double observedFrameRate() const { return m_observedFrameRate; }
    Vector<VideoPresetData> presetsData();

    void ensureIntrinsicSizeMaintainsAspectRatio();

    const VideoPreset* currentPreset() const { return m_currentPreset.get(); }

protected:
    RealtimeVideoCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, PageIdentifier);

    void setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>) override;

    virtual bool prefersPreset(VideoPreset&) { return true; }
    virtual void setFrameRateWithPreset(double, RefPtr<VideoPreset>) { };
    virtual bool canResizeVideoFrames() const { return false; }
    bool shouldUsePreset(VideoPreset& current, VideoPreset& candidate);

    void setSupportedPresets(const Vector<Ref<VideoPreset>>&);
    void setSupportedPresets(Vector<VideoPresetData>&&);
    const Vector<Ref<VideoPreset>>& presets();

    bool frameRateRangeIncludesRate(const FrameRateRange&, double);

    void updateCapabilities(RealtimeMediaSourceCapabilities&);

    void dispatchVideoFrameToObservers(VideoFrame&, VideoFrameTimeMetadata);

    static Span<const IntSize> standardVideoSizes();

private:
    struct CaptureSizeAndFrameRate {
        RefPtr<VideoPreset> encodingPreset;
        IntSize requestedSize;
        double requestedFrameRate { 0 };
    };
    bool supportsCaptureSize(std::optional<int>, std::optional<int>, const Function<bool(const IntSize&)>&&);
    std::optional<CaptureSizeAndFrameRate> bestSupportedSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);
    bool presetSupportsFrameRate(RefPtr<VideoPreset>, double);

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "RealtimeVideoCaptureSource"; }
#endif

    RefPtr<VideoPreset> m_currentPreset;
    Vector<Ref<VideoPreset>> m_presets;
    Deque<double> m_observedFrameTimeStamps;
    double m_observedFrameRate { 0 };
};

struct SizeAndFrameRate {
    std::optional<int> width;
    std::optional<int> height;
    std::optional<double> frameRate;

    String toJSONString() const;
    Ref<JSON::Object> toJSONObject() const;
};

} // namespace WebCore

namespace WTF {
template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::SizeAndFrameRate> {
    static String toString(const WebCore::SizeAndFrameRate& size)
    {
        return size.toJSONString();
    }
};
}; // namespace WTF

#endif // ENABLE(MEDIA_STREAM)

