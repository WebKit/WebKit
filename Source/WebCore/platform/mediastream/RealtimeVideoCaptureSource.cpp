/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include <VideoFrame.h>
#include <wtf/JSONValues.h>
#include <wtf/MediaTime.h>

namespace WebCore {

RealtimeVideoCaptureSource::RealtimeVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(device, WTFMove(hashSalts), pageIdentifier)
{
}

RealtimeVideoCaptureSource::~RealtimeVideoCaptureSource() = default;

void RealtimeVideoCaptureSource::ref() const
{
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeVideoCaptureSource, WTF::DestructionThread::MainRunLoop>::ref();
}

void RealtimeVideoCaptureSource::deref() const
{
    ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeVideoCaptureSource, WTF::DestructionThread::MainRunLoop>::deref();
}

ThreadSafeWeakPtrControlBlock& RealtimeVideoCaptureSource::controlBlock() const
{
    return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeVideoCaptureSource, WTF::DestructionThread::MainRunLoop>::controlBlock();
}

const Vector<VideoPreset>& RealtimeVideoCaptureSource::presets()
{
    if (m_presets.isEmpty())
        generatePresets();

    ASSERT(!m_presets.isEmpty());
    return m_presets;
}

void RealtimeVideoCaptureSource::setSupportedPresets(Vector<VideoPresetData>&& presetData)
{
    auto presets = WTF::map(WTFMove(presetData), [](VideoPresetData&& data) {
        return VideoPreset  { WTFMove(data) };
    });
    setSupportedPresets(WTFMove(presets));
}

void RealtimeVideoCaptureSource::setSupportedPresets(Vector<VideoPreset>&& presets)
{
    m_presets = WTFMove(presets);
    for (auto& preset : m_presets)
        preset.sortFrameRateRanges();
}

std::span<const IntSize> RealtimeVideoCaptureSource::standardVideoSizes()
{
    static constexpr IntSize sizes[] = {
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
        { 1600, 1200 }, // 4:3, UXGA
        { 1920, 1080 }, // 16:9, 1080p FHD
        { 2560, 1440 }, // 16:9, QHD
        { 2592, 1936 },
        { 3264, 2448 }, // 3:4
        { 3840, 2160 }, // 16:9, 4K UHD
    };
    return sizes;
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
    double minimumFrameRate = 1;
    double maximumFrameRate = 0;
    double minimumZoom = std::numeric_limits<double>::max();
    double maximumZoom = 1;
    for (const auto& preset : presets()) {
        const auto& size = preset.size();
        updateMinMax(minimumWidth, maximumWidth, size.width());
        updateMinMax(minimumHeight, maximumHeight, size.height());
        updateMinMax(minimumAspectRatio, maximumAspectRatio, static_cast<double>(size.width()) / size.height());

        minimumZoom = std::min(minimumZoom, preset.minZoom());
        maximumZoom = std::max(maximumZoom, preset.maxZoom());

        for (const auto& rate : preset.frameRateRanges())
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
    capabilities.setFrameRate({ minimumFrameRate, maximumFrameRate });

    capabilities.setZoom({ minimumZoom, maximumZoom });
}

bool RealtimeVideoCaptureSource::supportsSizeFrameRateAndZoom(const VideoPresetConstraints& constraints)
{
    return !constraints.hasConstraints() || !!bestSupportedSizeFrameRateAndZoom(constraints);
}

bool RealtimeVideoCaptureSource::frameRateRangeIncludesRate(const FrameRateRange& range, double frameRate)
{
    const double epsilon = 0.001;
    return frameRate + epsilon >= range.minimum && frameRate - epsilon <= range.maximum;
}

bool RealtimeVideoCaptureSource::presetSupportsFrameRate(const VideoPreset& preset, double frameRate)
{
    for (const auto& range : preset.frameRateRanges()) {
        if (frameRateRangeIncludesRate(range, frameRate))
            return true;
    }

    return false;
}

bool RealtimeVideoCaptureSource::presetSupportsZoom(const VideoPreset& preset, double zoom)
{
    return preset.minZoom() <= zoom && zoom <= preset.maxZoom();
}

bool RealtimeVideoCaptureSource::supportsCaptureSize(std::optional<int> width, std::optional<int> height, const Function<bool(const IntSize&)>&& function)
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

static bool shouldUsePreset(const VideoPreset& current, const VideoPreset& candidate, bool shouldPreferPowerEfficiency)
{
    if (shouldPreferPowerEfficiency && candidate.isEfficient() && !current.isEfficient())
        return true;
    return candidate.size().width() <= current.size().width() && candidate.size().height() <= current.size().height() && candidate.isEfficient();
}

static bool isPresetEfficient(const std::optional<VideoPreset>& preset)
{
    return preset && preset->isEfficient();
}

enum PresetToUse : uint8_t { Exact, AspectRatio, Resize };
static PresetToUse computePresetToUse(const std::optional<VideoPreset>& exactSizePreset, const std::optional<VideoPreset>& aspectRatioPreset, const std::optional<VideoPreset>& resizePreset, bool shouldPreferPowerEfficiency)
{
    if (exactSizePreset && (!shouldPreferPowerEfficiency || exactSizePreset->isEfficient() || (!isPresetEfficient(aspectRatioPreset) && !isPresetEfficient(resizePreset))))
        return PresetToUse::Exact;
    if (aspectRatioPreset && (!shouldPreferPowerEfficiency || aspectRatioPreset->isEfficient() || !isPresetEfficient(resizePreset)))
        return PresetToUse::AspectRatio;
    return PresetToUse::Resize;
}

static inline double frameRateFromPreset(const VideoPreset& preset, double currentFrameRate)
{
    auto minFrameRate = preset.minFrameRate();
    auto maxFrameRate = preset.maxFrameRate();
    return currentFrameRate >= minFrameRate && currentFrameRate <= maxFrameRate ? currentFrameRate : maxFrameRate;
}

static inline double zoomFromPreset(const VideoPreset& preset, double currentZoom)
{
    if (currentZoom < preset.minZoom())
        return preset.minZoom();
    if (currentZoom > preset.maxZoom())
        return preset.maxZoom();
    return currentZoom;
}

std::optional<RealtimeVideoCaptureSource::CaptureSizeFrameRateAndZoom> RealtimeVideoCaptureSource::bestSupportedSizeFrameRateAndZoom(const VideoPresetConstraints& constraints, TryPreservingSize tryPreservingSize)
{
    auto requestedWidth = constraints.width;
    auto requestedHeight = constraints.height;
    auto requestedFrameRate = constraints.frameRate;
    auto requestedZoom = constraints.zoom;
    if (!requestedWidth && !requestedHeight && !requestedFrameRate && !requestedZoom)
        return { };

    auto initialRequestedWidth = requestedWidth;
    auto initialRequestedHeight = requestedHeight;
    if (!requestedWidth && !requestedHeight && !size().isEmpty() && tryPreservingSize == TryPreservingSize::Yes) {
        requestedWidth = size().width();
        requestedHeight = size().height();
    }

    std::optional<VideoPreset> exactSizePreset;
    std::optional<VideoPreset> aspectRatioPreset;
    IntSize aspectRatioMatchSize;
    std::optional<VideoPreset> resizePreset;
    IntSize resizeSize;

    for (const auto& preset : presets()) {
        const auto& presetSize = preset.size();

        if (requestedFrameRate && !presetSupportsFrameRate(preset, requestedFrameRate.value()))
            continue;

        if (requestedZoom && !presetSupportsZoom(preset, requestedZoom.value()))
            continue;

        if (!requestedWidth && !requestedHeight) {
            exactSizePreset = preset;
            break;
        }

        // Don't look at presets smaller than the requested resolution because we never want to resize larger.
        if ((requestedWidth && presetSize.width() < requestedWidth.value()) || (requestedHeight && presetSize.height() < requestedHeight.value()))
            continue;

        auto lookForExactSizeMatch = [&] (const IntSize& size) -> bool {
            return preset.size() == size;
        };
        if (supportsCaptureSize(requestedWidth, requestedHeight, WTFMove(lookForExactSizeMatch))) {
            if (!exactSizePreset || preset.isEfficient())
                exactSizePreset = preset;
            continue;
        }

        IntSize encodingSize;
        auto lookForAspectRatioMatch = [this, &preset, &encodingSize] (const IntSize& size) -> bool {
            auto aspectRatio = [] (const IntSize size) -> double {
                return size.width() / static_cast<double>(size.height());
            };
            if (std::abs(aspectRatio(preset.size()) - aspectRatio(size)) > 10e-7 || !canResizeVideoFrames())
                return false;

            encodingSize = size;
            return true;
        };
        if (supportsCaptureSize(requestedWidth, requestedHeight, WTFMove(lookForAspectRatioMatch))) {
            if (!aspectRatioPreset || shouldUsePreset(*aspectRatioPreset, preset, constraints.shouldPreferPowerEfficiency)) {
                aspectRatioPreset = preset;
                aspectRatioMatchSize = encodingSize;
            }
        }

        if (exactSizePreset || aspectRatioPreset)
            continue;

        if ((requestedWidth && requestedWidth.value() > preset.size().width()) || (requestedHeight && requestedHeight.value() > preset.size().height()))
            continue;

        if (requestedWidth && requestedHeight) {
            if (!resizePreset || shouldUsePreset(*resizePreset, preset, constraints.shouldPreferPowerEfficiency)) {
                resizePreset = preset;
                resizeSize = { requestedWidth.value(), requestedHeight.value() };
            }
        } else {
            for (auto& standardSize : standardVideoSizes()) {
                if (standardSize.width() > preset.size().width() || standardSize.height() > preset.size().height())
                    break;
                if ((requestedWidth && requestedWidth.value() != standardSize.width()) || (requestedHeight && requestedHeight.value() != standardSize.height()))
                    continue;

                if (!resizePreset || shouldUsePreset(*resizePreset, preset, constraints.shouldPreferPowerEfficiency)) {
                    resizePreset = preset;
                    resizeSize = standardSize;
                }
            }

            if (!resizePreset || shouldUsePreset(*resizePreset, preset, constraints.shouldPreferPowerEfficiency)) {
                resizePreset = preset;
                if (requestedWidth)
                    resizeSize = { requestedWidth.value(), requestedWidth.value() * preset.size().height() / preset.size().width() };
                else
                    resizeSize = { requestedHeight.value() * preset.size().width() / preset.size().height(), requestedHeight.value() };
            }
        }
    }

    if (!exactSizePreset && !aspectRatioPreset && !resizePreset) {
        if (tryPreservingSize == TryPreservingSize::Yes)
            return bestSupportedSizeFrameRateAndZoom({ initialRequestedWidth, initialRequestedHeight, requestedFrameRate, requestedZoom }, TryPreservingSize::No);
        if (requestedFrameRate || requestedZoom)
            return bestSupportedSizeFrameRateAndZoom({ initialRequestedWidth, initialRequestedHeight, { }, { } }, TryPreservingSize::No);

        WTFLogAlways("RealtimeVideoCaptureSource::bestSupportedSizeFrameRateAndZoom failed supporting constraints %d %d %f %f", requestedWidth ? *requestedWidth : -1, requestedHeight ? *requestedHeight : -1, requestedFrameRate ? *requestedFrameRate : -1, requestedZoom ? *requestedZoom : -1);
        for (const auto& preset : presets())
            preset.log();

        return { };
    }

    switch (computePresetToUse(exactSizePreset, aspectRatioPreset, resizePreset, constraints.shouldPreferPowerEfficiency)) {
    case PresetToUse::Exact: {
        auto size = exactSizePreset->size();
        auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*exactSizePreset, frameRate());
        auto captureZoom = requestedZoom ? *requestedZoom : zoomFromPreset(*exactSizePreset, zoom());
        return CaptureSizeFrameRateAndZoom { WTFMove(exactSizePreset), size, captureFrameRate, captureZoom };
    }
    case PresetToUse::AspectRatio: {
        auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*aspectRatioPreset, frameRate());
        auto captureZoom = requestedZoom ? *requestedZoom : zoomFromPreset(*aspectRatioPreset, zoom());
        return CaptureSizeFrameRateAndZoom { WTFMove(aspectRatioPreset), aspectRatioMatchSize, captureFrameRate, captureZoom };
    }
    case PresetToUse::Resize: {
        auto captureFrameRate = requestedFrameRate ? *requestedFrameRate : frameRateFromPreset(*resizePreset, frameRate());
        auto captureZoom = requestedZoom ? *requestedZoom : zoomFromPreset(*resizePreset, zoom());
        return CaptureSizeFrameRateAndZoom { WTFMove(resizePreset), resizeSize, captureFrameRate, captureZoom };
    }
    }
    ASSERT_NOT_REACHED();
    return { };
}

void RealtimeVideoCaptureSource::dispatchVideoFrameToObservers(VideoFrame& videoFrame, WebCore::VideoFrameTimeMetadata metadata)
{
    MediaTime sampleTime = videoFrame.presentationTime();

    auto frameTime = sampleTime.toDouble();
    m_observedFrameTimeStamps.append(frameTime);
    m_observedFrameTimeStamps.removeAllMatching([&](auto time) {
        return time <= frameTime - 2;
    });

    auto interval = m_observedFrameTimeStamps.last() - m_observedFrameTimeStamps.first();
    if (interval > 1)
        m_observedFrameRate = (m_observedFrameTimeStamps.size() / interval);

    videoFrameAvailable(videoFrame, metadata);
}

std::optional<RealtimeVideoCaptureSource::CaptureSizeFrameRateAndZoom> RealtimeVideoCaptureSource::bestSupportedSizeFrameRateAndZoomConsideringObservers(const VideoPresetConstraints& constraints)
{
    auto& settings = this->settings();

    auto updatedConstraints = constraints;
    if (hasSeveralVideoFrameObserversWithAdaptors()) {
        // FIXME: We only change settings if capture resolution is below requested one. We should get the best preset for all clients.
        if (constraints.width && *constraints.width <= static_cast<int>(settings.width()))
            updatedConstraints.width = { };
        if (constraints.height && *constraints.height <= static_cast<int>(settings.height()))
            updatedConstraints.height = { };

        if (constraints.frameRate && *constraints.frameRate <= static_cast<double>(settings.frameRate()))
            updatedConstraints.frameRate = { };
    }

    if (!updatedConstraints.hasConstraints())
        return { };

    return bestSupportedSizeFrameRateAndZoom(updatedConstraints);
}

void RealtimeVideoCaptureSource::setSizeFrameRateAndZoom(const VideoPresetConstraints& constraints)
{
    auto match = bestSupportedSizeFrameRateAndZoomConsideringObservers(constraints);
    ERROR_LOG_IF(loggerPtr() && !match, LOGIDENTIFIER, "unable to find a preset that would match the size, frame rate and zoom");
    if (!match)
        return;

    m_currentPreset = match->encodingPreset;
    auto newSize = match->encodingPreset->size();

    applyFrameRateAndZoomWithPreset(match->requestedFrameRate, match->requestedZoom, WTFMove(match->encodingPreset));
    setSize(newSize);
    setFrameRate(match->requestedFrameRate);
    setZoom(match->requestedZoom);
}

void RealtimeVideoCaptureSource::setSizeFrameRateAndZoomForPhoto(CaptureSizeFrameRateAndZoom&& preset)
{
    ASSERT(preset.encodingPreset);

    m_currentPreset = preset.encodingPreset;
    auto newSize = preset.encodingPreset->size();
    startApplyingConstraints();
    applyFrameRateAndZoomWithPreset(preset.requestedFrameRate, preset.requestedZoom, WTFMove(preset.encodingPreset));
    setSize(newSize);
    endApplyingConstraints();
}

auto RealtimeVideoCaptureSource::takePhotoInternal(PhotoSettings&&) -> Ref<TakePhotoNativePromise>
{
    return TakePhotoNativePromise::createAndReject("Not supported"_s);
}

auto RealtimeVideoCaptureSource::takePhoto(PhotoSettings&& photoSettings) -> Ref<TakePhotoNativePromise>
{
    ASSERT(isMainThread());

    if (isEnded())
        return TakePhotoNativePromise::createAndResolve();

    if ((photoSettings.imageHeight && !photoSettings.imageWidth) || (!photoSettings.imageHeight && photoSettings.imageWidth)) {
        IntSize sanitizedSize;
        if (photoSettings.imageHeight)
            sanitizedSize.setHeight(*photoSettings.imageHeight);
        if (photoSettings.imageWidth)
            sanitizedSize.setWidth(*photoSettings.imageWidth);

        auto intrinsicSize = this->intrinsicSize();
        if (!sanitizedSize.height())
            sanitizedSize.setHeight(sanitizedSize.width() * (intrinsicSize.height() / static_cast<double>(intrinsicSize.width())));
        else if (!sanitizedSize.width())
            sanitizedSize.setWidth(sanitizedSize.height() * (intrinsicSize.width() / static_cast<double>(intrinsicSize.height())));

        photoSettings.imageHeight = sanitizedSize.height();
        photoSettings.imageWidth = sanitizedSize.width();
    }

    std::optional<CaptureSizeFrameRateAndZoom> newPresetForPhoto;
    if (photoSettings.imageHeight || photoSettings.imageWidth) {
        newPresetForPhoto = bestSupportedSizeFrameRateAndZoomConsideringObservers({ photoSettings.imageWidth, photoSettings.imageHeight, { }, { } });
        ERROR_LOG_IF(loggerPtr() && !newPresetForPhoto, LOGIDENTIFIER, "unable to find a preset to match the size of requested photo, using current preset");

        if (newPresetForPhoto && m_currentPreset && m_currentPreset->size() == newPresetForPhoto->encodingPreset->size())
            newPresetForPhoto = { };
    }

    std::optional<CaptureSizeFrameRateAndZoom> configurationToRestore;
    if (newPresetForPhoto) {
        configurationToRestore = {
            { m_currentPreset },
            size(),
            frameRate(),
            zoom()
        };

        // 3.2.2 - Devices MAY temporarily stop streaming data, reconfigure themselves with the appropriate photo
        // settings, take the photo, and then resume streaming. In this case, the stopping and restarting of
        // streaming SHOULD cause onmute and onunmute events to fire on the track in question.
        if (!muted()) {
            setMuted(true);
            m_mutedForPhotoCapture = true;
        }

        setSizeFrameRateAndZoomForPhoto(WTFMove(*newPresetForPhoto));
    }

    return takePhotoInternal(WTFMove(photoSettings))->whenSettled(RunLoop::main(), [this, protectedThis = Ref { *this }, configurationToRestore = WTFMove(configurationToRestore)] (auto&& result) mutable {

        ASSERT(isMainThread());

        if (configurationToRestore) {
            setSizeFrameRateAndZoomForPhoto(WTFMove(*configurationToRestore));

            if (m_mutedForPhotoCapture) {
                m_mutedForPhotoCapture = false;
                setMuted(false);
            }
        }

        return TakePhotoNativePromise::createAndSettle(WTFMove(result));
    });
}

void RealtimeVideoCaptureSource::ensureIntrinsicSizeMaintainsAspectRatio()
{
    auto intrinsicSize = this->intrinsicSize();
    auto frameSize = size();
    if (!frameSize.height())
        frameSize.setHeight(intrinsicSize.height());
    if (!frameSize.width())
        frameSize.setWidth(intrinsicSize.width());

    auto maxHeight = std::min(frameSize.height(), intrinsicSize.height());
    auto maxWidth = std::min(frameSize.width(), intrinsicSize.width());

    auto heightForMaxWidth = maxWidth * intrinsicSize.height() / intrinsicSize.width();
    auto widthForMaxHeight = maxHeight * intrinsicSize.width() / intrinsicSize.height();

    if (heightForMaxWidth <= maxHeight) {
        setSize({ maxWidth, heightForMaxWidth });
        return;
    }
    if (widthForMaxHeight <= maxWidth) {
        setSize({ widthForMaxHeight, maxHeight });
        return;
    }

    setSize(intrinsicSize);
}

bool RealtimeVideoCaptureSource::isPowerEfficient() const
{
    return m_currentPreset->isEfficient();
}

#if !RELEASE_LOG_DISABLED
Ref<JSON::Object> SizeFrameRateAndZoom::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setDouble("width"_s, width ? width.value() : 0);
    object->setDouble("height"_s, height ? height.value() : 0);
    object->setDouble("frameRate"_s, frameRate ? frameRate.value() : 0);
    object->setDouble("zoom"_s, zoom ? zoom.value() : 0);

    return object;
}

String SizeFrameRateAndZoom::toJSONString() const
{
    return toJSONObject()->toJSONString();
}
#endif

bool RealtimeVideoCaptureSource::canBePowerEfficient()
{
    return anyOf(presets(), [] (auto& preset) { return preset.isEfficient(); }) && anyOf(presets(), [] (auto& preset) { return !preset.isEfficient(); });
}


} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
