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

enum class VideoFrameRotation : uint16_t;

class WEBCORE_EXPORT RealtimeVideoCaptureSource : public RealtimeMediaSource, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeVideoCaptureSource, WTF::DestructionThread::MainRunLoop> {
public:
    virtual ~RealtimeVideoCaptureSource();

    void clientUpdatedSizeFrameRateAndZoom(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate, std::optional<double> zoom);

    bool supportsSizeFrameRateAndZoom(std::optional<int> width, std::optional<int> height, std::optional<double>, std::optional<double>) override;
    virtual void generatePresets() = 0;

    double observedFrameRate() const final { return m_observedFrameRate; }
    Vector<VideoPresetData> presetsData();

    void ensureIntrinsicSizeMaintainsAspectRatio();

    void ref() const final;
    void deref() const final;
    ThreadSafeWeakPtrControlBlock& controlBlock() const final;

protected:
    RealtimeVideoCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, PageIdentifier);

    void setSizeFrameRateAndZoom(std::optional<int> width, std::optional<int> height, std::optional<double>, std::optional<double>) override;

    virtual bool prefersPreset(const VideoPreset&) { return true; }
    virtual void setFrameRateAndZoomWithPreset(double, double, std::optional<VideoPreset>&&) { };
    virtual bool canResizeVideoFrames() const { return false; }
    bool shouldUsePreset(const VideoPreset& current, const VideoPreset& candidate);

    void setSupportedPresets(Vector<VideoPreset>&&);
    void setSupportedPresets(Vector<VideoPresetData>&&);
    virtual const Vector<VideoPreset>& presets();

    bool frameRateRangeIncludesRate(const FrameRateRange&, double);

    void updateCapabilities(RealtimeMediaSourceCapabilities&);

    void dispatchVideoFrameToObservers(VideoFrame&, VideoFrameTimeMetadata);

    static std::span<const IntSize> standardVideoSizes();

    virtual Ref<TakePhotoNativePromise> takePhotoInternal(PhotoSettings&&);
    bool mutedForPhotoCapture() const { return m_mutedForPhotoCapture; }

private:
    struct CaptureSizeFrameRateAndZoom {
        std::optional<VideoPreset> encodingPreset;
        IntSize requestedSize;
        double requestedFrameRate { 0 };
        double requestedZoom { 0 };
    };
    bool supportsCaptureSize(std::optional<int>, std::optional<int>, const Function<bool(const IntSize&)>&&);

    enum class TryPreservingSize { No, Yes };
    std::optional<CaptureSizeFrameRateAndZoom> bestSupportedSizeFrameRateAndZoom(std::optional<int> width, std::optional<int> height, std::optional<double>, std::optional<double>, TryPreservingSize = TryPreservingSize::Yes);
    std::optional<CaptureSizeFrameRateAndZoom> bestSupportedSizeFrameRateAndZoomConsideringObservers(std::optional<int> width, std::optional<int> height, std::optional<double>, std::optional<double>);

    bool presetSupportsFrameRate(const VideoPreset&, double);
    bool presetSupportsZoom(const VideoPreset&, double);

    Ref<TakePhotoNativePromise> takePhoto(PhotoSettings&&) final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "RealtimeVideoCaptureSource"; }
#endif

    std::optional<VideoPreset> m_currentPreset;
    Vector<VideoPreset> m_presets;
    Deque<double> m_observedFrameTimeStamps;
    double m_observedFrameRate { 0 };
    bool m_mutedForPhotoCapture { false };
};

struct SizeFrameRateAndZoom {
    std::optional<int> width;
    std::optional<int> height;
    std::optional<double> frameRate;
    std::optional<double> zoom;

    String toJSONString() const;
    Ref<JSON::Object> toJSONObject() const;
};

} // namespace WebCore

namespace WTF {
template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::SizeFrameRateAndZoom> {
    static String toString(const WebCore::SizeFrameRateAndZoom& size)
    {
        return size.toJSONString();
    }
};
}; // namespace WTF

#endif // ENABLE(MEDIA_STREAM)

