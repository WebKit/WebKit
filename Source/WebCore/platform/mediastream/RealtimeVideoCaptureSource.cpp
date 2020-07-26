/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RealtimeVideoCaptureSource.h"

#if ENABLE(MEDIA_STREAM)
#include "CaptureDevice.h"
#include "Logging.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include "RemoteVideoSample.h"
#include <wtf/JSONValues.h>

namespace WebCore {

RealtimeVideoCaptureSource::RealtimeVideoCaptureSource(String&& name, String&& id, String&& hashSalt)
    : RealtimeMediaSource(Type::Video, WTFMove(name), WTFMove(id), WTFMove(hashSalt))
{
}

RealtimeVideoCaptureSource::~RealtimeVideoCaptureSource()
{
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
}

void RealtimeVideoCaptureSource::prepareToProduceData()
{
    ASSERT(frameRate());

#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif

    if (size().isEmpty() && !m_defaultSize.isEmpty())
        setSize(m_defaultSize);
}

const Vector<Ref<VideoPreset>>& RealtimeVideoCaptureSource::presets()
{
    if (m_presets.isEmpty())
        generatePresets();

    ASSERT(!m_presets.isEmpty());
    return m_presets;
}

void RealtimeVideoCaptureSource::setSupportedPresets(Vector<VideoPresetData>&& presetData)
{
    Vector<Ref<VideoPreset>> presets;

    for (auto& data : presetData)
        presets.append(VideoPreset::create(WTFMove(data)));

    setSupportedPresets(WTFMove(presets));
}

void RealtimeVideoCaptureSource::setSupportedPresets(const Vector<Ref<VideoPreset>>& presets)
{
    m_presets = WTF::map(presets, [](auto& preset) {
        return preset.copyRef();
    });

    for (auto& preset : m_presets) {
        std::sort(preset->frameRateRanges.begin(), preset->frameRateRanges.end(),
            [&] (const auto& a, const auto& b) -> bool {
                return a.minimum < b.minimum;
        });
    }
}

const Vector<IntSize>& RealtimeVideoCaptureSource::standardVideoSizes()
{
    static const auto sizes = makeNeverDestroyed([] {
        static IntSize videoSizes[] = {
            { 112, 112 },
            { 160, 160 },
            { 160, 120 }, // 4:3, QQVGA
            { 176, 144 }, // 4:3, QCIF
            { 192, 192 },
            { 192, 112 }, // 16:9
            { 192, 144 }, // 3:4
            { 240, 240 },
            { 240, 160 }, // 3:2, HQVGA
            { 320, 320 },
            { 320, 180 }, // 16:9
            { 320, 240 }, // 4:3, QVGA
            { 352, 288 }, // CIF
            { 480, 272 }, // 16:9
            { 480, 360 }, // 4:3
            { 480, 480 },
            { 640, 640 },
            { 640, 360 }, // 16:9, 360p nHD
            { 640, 480 }, // 4:3
            { 720, 720 },
            { 800, 600 }, // 4:3, SVGA
            { 960, 540 }, // 16:9, qHD
            { 1024, 600 }, // 16:9, WSVGA
            { 1024, 768 }, // 4:3, XGA
            { 1280, 960 }, // 4:3
            { 1280, 1024 }, // 5:4, SXGA
            { 1280, 720 }, // 16:9, WXGA
            { 1366, 768 }, // 16:9, HD
            { 1600, 1200}, // 4:3, UXGA
            { 1920, 1080 }, // 16:9, 1080p FHD
            { 2560, 1440 }, // 16:9, QHD
            { 2592, 1936 },
            { 3264, 2448 }, // 3:4
            { 3840, 2160 }, // 16:9, 4K UHD
        };
        Vector<IntSize> sizes;
        for (auto& size : videoSizes)
            sizes.append(size);

        return sizes;
    }());

    return sizes.get();
}
template <typename ValueType>
static void updateMinMax(ValueType& min, ValueType& max, ValueType value)
{
    min = std::min<ValueType>(min, value);
    max = std::max<ValueType>(max, value);
}

void RealtimeVideoCaptureSource::updateCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
    ASSERT(!presets().isEmpty());

    int minimumWidth = std::numeric_limits<int>::max();
    int maximumWidth = 0;
    int minimumHeight = std::numeric_limits<int>::max();
    int maximumHeight = 0;
    double minimumAspectRatio = std::numeric_limits<double>::max();
    double maximumAspectRatio = 0;
    // RealtimeVideoSource will decimate frame rate if the source cannot go below a given value.
    double minimumFrameRate = 1;
    double maximumFrameRate = 0;
    for (const auto& preset : presets()) {
        const auto& size = preset->size;
        updateMinMax(minimumWidth, maximumWidth, size.width());
        updateMinMax(minimumHeight, maximumHeight, size.height());
        updateMinMax(minimumAspectRatio, maximumAspectRatio, static_cast<double>(size.width()) / size.height());

        for (const auto& rate : preset->frameRateRanges)
            maximumFrameRate = std::max(maximumFrameRate, rate.maximum);
    }

    if (canResizeVideoFrames()) {
        minimumWidth = 1;
        minimumHeight = 1;
        minimumAspectRatio = 1.0 / maximumHeight;
        maximumAspectRatio = maximumWidth;
    }

    capabilities.setWidth({ minimumWidth, maximumWidth });
    capabilities.setHeight({ minimumHeight, maximumHeight });
    capabilities.setAspectRatio({ minimumAspectRatio, maximumAspectRatio });
    capabilities.setFrameRate({ minimumFrameRate, maximumFrameRate });
}

bool RealtimeVideoCaptureSource::supportsSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double> frameRate)
{
    if (!width && !height && !frameRate)
        return true;

    return !!bestSupportedSizeAndFrameRate(width, height, frameRate);
}

bool RealtimeVideoCaptureSource::frameRateRangeIncludesRate(const FrameRateRange& range, double frameRate)
{
    const double epsilon = 0.001;
    return frameRate + epsilon >= range.minimum && frameRate - epsilon <= range.maximum;
}

bool RealtimeVideoCaptureSource::presetSupportsFrameRate(RefPtr<VideoPreset> preset, double frameRate)
{
    for (const auto& range : preset->frameRateRanges) {
        if (frameRateRangeIncludesRate(range, frameRate))
            return true;
    }

    return false;
}

bool RealtimeVideoCaptureSource::supportsCaptureSize(Optional<int> width, Optional<int> height, const Function<bool(const IntSize&)>&& function)
{
    if (width && height)
        return function({ width.value(), height.value() });

    if (width) {
        for (auto& size : standardVideoSizes()) {
            if (width.value() == size.width() && function({ size.width(), size.height() }))
                return true;
        }

        return false;
    }

    for (auto& size : standardVideoSizes()) {
        if (height.value() == size.height() && function({ size.width(), size.height() }))
            return true;
    }

    return false;
}

bool RealtimeVideoCaptureSource::shouldUsePreset(VideoPreset& current, VideoPreset& candidate)
{
    return candidate.size.width() <= current.size.width() && candidate.size.height() <= current.size.height() && prefersPreset(candidate);
}

static inline double frameRateFromPreset(const VideoPreset& preset, double currentFrameRate)
{
    auto minFrameRate = preset.minFrameRate();
    auto maxFrameRate = preset.maxFrameRate();
    return currentFrameRate >= minFrameRate && currentFrameRate <= maxFrameRate ? currentFrameRate : maxFrameRate;
}

Optional<RealtimeVideoCaptureSource::CaptureSizeAndFrameRate> RealtimeVideoCaptureSource::bestSupportedSizeAndFrameRate(Optional<int> requestedWidth, Optional<int> requestedHeight, Optional<double> requestedFrameRate)
{
    if (!requestedWidth && !requestedHeight && !requestedFrameRate)
        return { };

    if (!requestedWidth && !requestedHeight && !size().isEmpty()) {
        requestedWidth = size().width();
        requestedHeight = size().height();
    }

    RefPtr<VideoPreset> exactSizePreset;
    RefPtr<VideoPreset> aspectRatioPreset;
    IntSize aspectRatioMatchSize;
    RefPtr<VideoPreset> resizePreset;
    IntSize resizeSize;

    for (const auto& preset : presets()) {
        const auto& presetSize = preset->size;

        if (requestedFrameRate && !presetSupportsFrameRate(&preset.get(), requestedFrameRate.value()))
            continue;

        if (!requestedWidth && !requestedHeight) {
            exactSizePreset = preset.ptr();
            break;
        }

        // Don't look at presets smaller than the requested resolution because we never want to resize larger.
        if ((requestedWidth && presetSize.width() < requestedWidth.value()) || (requestedHeight && presetSize.height() < requestedHeight.value()))
            continue;

        auto lookForExactSizeMatch = [&] (const IntSize& size) -> bool {
            return preset->size == size;
        };
        if (supportsCaptureSize(requestedWidth, requestedHeight, WTFMove(lookForExactSizeMatch))) {
            if (!exactSizePreset || prefersPreset(preset))
                exactSizePreset = &preset.get();
            continue;
        }

        IntSize encodingSize;
        auto lookForAspectRatioMatch = [this, &preset, &encodingSize] (const IntSize& size) -> bool {
            auto aspectRatio = [] (const IntSize size) -> double {
                return size.width() / static_cast<double>(size.height());
            };
            if (std::abs(aspectRatio(preset->size) - aspectRatio(size)) > 10e-7 || !canResizeVideoFrames())
                return false;

            encodingSize = size;
            return true;
        };
        if (supportsCaptureSize(requestedWidth, requestedHeight, WTFMove(lookForAspectRatioMatch))) {
            if (!aspectRatioPreset || shouldUsePreset(*aspectRatioPreset, preset)) {
                aspectRatioPreset = &preset.get();
                aspectRatioMatchSize = encodingSize;
            }
        }

        if (exactSizePreset || aspectRatioPreset)
            continue;

        if ((requestedWidth && requestedWidth.value() > preset->size.width()) || (requestedHeight && requestedHeight.value() > preset->size.height()))
            continue;

        if (requestedWidth && requestedHeight) {
            if (!resizePreset || shouldUsePreset(*resizePreset, preset)) {
                resizePreset = &preset.get();
                resizeSize = { requestedWidth.value(), requestedHeight.value() };
            }
        } else {
            for (auto& standardSize : standardVideoSizes()) {
                if (standardSize.width() > preset->size.width() || standardSize.height() > preset->size.height())
                    break;
                if ((requestedWidth && requestedWidth.value() != standardSize.width()) || (requestedHeight && requestedHeight.value() != standardSize.height()))
                    continue;

                if (!resizePreset || shouldUsePreset(*resizePreset, preset)) {
                    resizePreset = &preset.get();
                    resizeSize = standardSize;
                }
            }

            if (!resizePreset || shouldUsePreset(*resizePreset, preset)) {
                resizePreset = &preset.get();
                if (requestedWidth)
                    resizeSize = { requestedWidth.value(), requestedWidth.value() * preset->size.height() / preset->size.width()};
                else
                    resizeSize = { requestedHeight.value() * preset->size.width() / preset->size.height(), requestedHeight.value() };
            }
        }
    }

    if (!exactSizePreset && !aspectRatioPreset && !resizePreset) {
        WTFLogAlways("RealtimeVideoCaptureSource::bestSupportedSizeAndFrameRate failed supporting constraints %d %d %f", requestedWidth ? *requestedWidth : -1, requestedHeight ? *requestedHeight : -1, requestedFrameRate ? *requestedFrameRate : -1);
        for (const auto& preset : presets())
            preset->log();

        return { };
    }

    if (exactSizePreset) {
        auto size = exactSizePreset->size;
        auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*exactSizePreset, frameRate());
        return CaptureSizeAndFrameRate { WTFMove(exactSizePreset), size, captureFrameRate };
    }

    if (aspectRatioPreset) {
        auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*aspectRatioPreset, frameRate());
        return CaptureSizeAndFrameRate { WTFMove(aspectRatioPreset), aspectRatioMatchSize, captureFrameRate };
    }

    auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*resizePreset, frameRate());
    return CaptureSizeAndFrameRate { WTFMove(resizePreset), resizeSize, captureFrameRate };
}

void RealtimeVideoCaptureSource::setSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double> frameRate)
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, SizeAndFrameRate { width, height, frameRate });

    auto size = this->size();
    if (!width && !height && !size.isEmpty()) {
        width = size.width();
        height = size.height();
    }

    auto match = bestSupportedSizeAndFrameRate(width, height, frameRate);
    if (!match) {
        match = bestSupportedSizeAndFrameRate(width, height, { });
        ASSERT(match);
        if (!match)
            return;
    }

    setFrameRateWithPreset(match->requestedFrameRate, match->encodingPreset);

    if (!match->requestedSize.isEmpty())
        setSize(match->requestedSize);
    setFrameRate(match->requestedFrameRate);
}

void RealtimeVideoCaptureSource::dispatchMediaSampleToObservers(MediaSample& sample)
{
    MediaTime sampleTime = sample.presentationTime();

    auto frameTime = sampleTime.toDouble();
    m_observedFrameTimeStamps.append(frameTime);
    m_observedFrameTimeStamps.removeAllMatching([&](auto time) {
        return time <= frameTime - 2;
    });

    auto interval = m_observedFrameTimeStamps.last() - m_observedFrameTimeStamps.first();
    if (interval > 1)
        m_observedFrameRate = (m_observedFrameTimeStamps.size() / interval);

    videoSampleAvailable(sample);
}

void RealtimeVideoCaptureSource::clientUpdatedSizeAndFrameRate(Optional<int> width, Optional<int> height, Optional<double> frameRate)
{
    // FIXME: We only change settings if capture resolution is below requested one. We should get the best preset for all clients.
    auto& settings = this->settings();
    if (width && *width < static_cast<int>(settings.width()))
        width = { };
    if (height && *height < static_cast<int>(settings.height()))
        height = { };
    if (frameRate && *frameRate < static_cast<double>(settings.frameRate()))
        frameRate = { };

    if (!width && !height && !frameRate)
        return;

    auto match = bestSupportedSizeAndFrameRate(width, height, frameRate);
    ERROR_LOG_IF(loggerPtr() && !match, LOGIDENTIFIER, "unable to find a preset that would match the size and frame rate");
    if (!match)
        return;

    setFrameRateWithPreset(match->requestedFrameRate, match->encodingPreset);
    setSize(match->encodingPreset->size);
    setFrameRate(match->requestedFrameRate);
}

#if !RELEASE_LOG_DISABLED
Ref<JSON::Object> SizeAndFrameRate::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setDouble("width"_s, width ? width.value() : 0);
    object->setDouble("height"_s, height ? height.value() : 0);
    object->setDouble("frameRate"_s, frameRate ? frameRate.value() : 0);

    return object;
}

String SizeAndFrameRate::toJSONString() const
{
    return toJSONObject()->toJSONString();
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
