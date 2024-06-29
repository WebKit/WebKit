/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSource.h"

#include "JSMeteringMode.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "RealtimeMediaSourceCenter.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/MediaTime.h>
#include <wtf/NativePromise.h>
#include <wtf/UUID.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include "ImageTransferSessionVT.h"
#include "VideoFrameCV.h"
#endif

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

namespace WebCore {

struct VideoFrameAdaptor {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    VideoFrameAdaptor(IntSize size, double frameRate)
        : size(size)
        , frameRate(frameRate)
    {
    }

    IntSize size;
    double frameRate;

    size_t frameDecimation { 1 };
    size_t frameDecimationCounter { 0 };
#if PLATFORM(COCOA)
    std::unique_ptr<ImageTransferSessionVT> imageTransferSession;
#endif
};

static RealtimeMediaSource::Type toSourceType(CaptureDevice::DeviceType type)
{
    switch (type) {
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::SystemAudio:
        return RealtimeMediaSource::Type::Audio;
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        return RealtimeMediaSource::Type::Video;
    case CaptureDevice::DeviceType::Unknown:
    case CaptureDevice::DeviceType::Speaker:
        ASSERT_NOT_REACHED();
        return RealtimeMediaSource::Type::Audio;
    }
    ASSERT_NOT_REACHED();
    return RealtimeMediaSource::Type::Audio;
}

RealtimeMediaSourceObserver::RealtimeMediaSourceObserver() = default;

RealtimeMediaSourceObserver::~RealtimeMediaSourceObserver() = default;

RealtimeMediaSource::RealtimeMediaSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : m_pageIdentifier(pageIdentifier)
    , m_idHashSalts(WTFMove(hashSalts))
    , m_type(toSourceType(device.type()))
    , m_name({ device.label() })
    , m_device(device)
{
    initializePersistentId();
}

RealtimeMediaSource::~RealtimeMediaSource()
{
}

void RealtimeMediaSource::setPersistentId(const String& persistentID)
{
    m_device.setPersistentId(persistentID);
    initializePersistentId();
}

void RealtimeMediaSource::initializePersistentId()
{
    if (m_device.persistentId().isEmpty())
        m_device.setPersistentId(createVersion4UUIDString());

    m_hashedID = RealtimeMediaSourceCenter::hashStringWithSalt(m_device.persistentId(), m_idHashSalts.persistentDeviceSalt);
    m_ephemeralHashedID = RealtimeMediaSourceCenter::hashStringWithSalt(m_device.persistentId(), m_idHashSalts.ephemeralDeviceSalt);
}

void RealtimeMediaSource::addAudioSampleObserver(AudioSampleObserver& observer)
{
    ASSERT(isMainThread());
    Locker locker { m_audioSampleObserversLock };
    m_audioSampleObservers.add(&observer);
}

void RealtimeMediaSource::removeAudioSampleObserver(AudioSampleObserver& observer)
{
    ASSERT(isMainThread());
    Locker locker { m_audioSampleObserversLock };
    m_audioSampleObservers.remove(&observer);
}

void RealtimeMediaSource::addVideoFrameObserver(VideoFrameObserver& observer)
{
    ASSERT(isMainThread());
    Locker locker { m_videoFrameObserversLock };
    m_videoFrameObservers.add(&observer, nullptr);
}

void RealtimeMediaSource::addVideoFrameObserver(VideoFrameObserver& observer, IntSize size, double frameRate)
{
    ASSERT(isMainThread());
    Locker locker { m_videoFrameObserversLock };
    ASSERT(!m_videoFrameObservers.contains(&observer));
    m_videoFrameObservers.add(&observer, makeUnique<VideoFrameAdaptor>(size, frameRate));
    ++m_videoFrameObserversWithAdaptors;
}

void RealtimeMediaSource::removeVideoFrameObserver(VideoFrameObserver& observer)
{
    ASSERT(isMainThread());
    Locker locker { m_videoFrameObserversLock };
    if (auto result = m_videoFrameObservers.take(&observer)) {
        ASSERT(m_videoFrameObserversWithAdaptors);
        --m_videoFrameObserversWithAdaptors;
    }
}

void RealtimeMediaSource::addObserver(RealtimeMediaSourceObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void RealtimeMediaSource::removeObserver(RealtimeMediaSourceObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
    if (m_observers.isEmptyIgnoringNullReferences())
        stopBeingObserved();
}

void RealtimeMediaSource::setMuted(bool muted)
{
    // Changed m_muted before calling start/stop so muted() will reflect the correct state.
    bool changed = m_muted != muted;

    ALWAYS_LOG_IF(m_logger && changed, LOGIDENTIFIER, muted);
    if (changed && !muted && m_isProducingData) {
        // Let's uninterrupt by doing a stop/start cycle.
        stop();
    }

    m_muted = muted;
    if (muted)
        stop();
    else
        start();

    if (changed)
        notifyMutedObservers();
}

void RealtimeMediaSource::notifyMutedChange(bool muted)
{
    if (m_muted == muted)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, muted);
    m_muted = muted;

    notifyMutedObservers();
}

void RealtimeMediaSource::setInterruptedForTesting(bool interrupted)
{
    notifyMutedChange(interrupted);
}

void RealtimeMediaSource::forEachObserver(const Function<void(RealtimeMediaSourceObserver&)>& apply)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    m_observers.forEach(apply);
}

void RealtimeMediaSource::forEachVideoFrameObserver(const Function<void(VideoFrameObserver&)>& apply)
{
    Locker locker { m_videoFrameObserversLock };
    for (auto* observer : m_videoFrameObservers.keys())
        apply(*observer);
}

void RealtimeMediaSource::notifyMutedObservers()
{
    forEachObserver([](auto& observer) {
        observer.sourceMutedChanged();
    });
}

void RealtimeMediaSource::notifySettingsDidChangeObservers(OptionSet<RealtimeMediaSourceSettings::Flag> flags)
{
    ASSERT(isMainThread());

    settingsDidChange(flags);

    if (m_pendingSettingsDidChangeNotification)
        return;
    m_pendingSettingsDidChangeNotification = true;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, flags);

    scheduleDeferredTask([this] {
        m_pendingSettingsDidChangeNotification = false;
        forEachObserver([](auto& observer) {
            observer.sourceSettingsChanged();
        });
    });
}

void RealtimeMediaSource::updateHasStartedProducingData()
{
    if (m_hasStartedProducingData)
        return;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThread([protectedThis = Ref { *this }] {
        if (protectedThis->m_hasStartedProducingData)
            return;
        protectedThis->m_hasStartedProducingData = true;
        protectedThis->forEachObserver([&](auto& observer) {
            observer.hasStartedProducingData();
        });
    });
}

VideoFrameRotation RealtimeMediaSource::videoFrameRotation() const
{
    return VideoFrameRotation::None;
}

static RefPtr<VideoFrame> adaptVideoFrame(VideoFrameAdaptor& adaptor, VideoFrame& videoFrame, IntSize desiredSize)
{
#if PLATFORM(COCOA)
    if (!adaptor.imageTransferSession || adaptor.imageTransferSession->pixelFormat() != videoFrame.pixelFormat())
        adaptor.imageTransferSession = ImageTransferSessionVT::create(videoFrame.pixelFormat(), true);

    ASSERT(adaptor.imageTransferSession);
    if (!adaptor.imageTransferSession)
        return nullptr;

    return adaptor.imageTransferSession->convertVideoFrame(videoFrame, desiredSize);
#elif USE(GSTREAMER)
    UNUSED_PARAM(adaptor);
    return reinterpret_cast<VideoFrameGStreamer&>(videoFrame).resizeTo(desiredSize);
#else
    UNUSED_PARAM(adaptor);
    UNUSED_PARAM(videoFrame);
    UNUSED_PARAM(desiredSize);
    notImplemented();
    return nullptr;
#endif
}

IntSize RealtimeMediaSource::computeResizedVideoFrameSize(IntSize desiredSize, IntSize actualSize)
{
    ASSERT(!actualSize.isZero());
    if (desiredSize.width() && !desiredSize.height())
        return { desiredSize.width(), desiredSize.width() * actualSize.height() / actualSize.width() };
    if (desiredSize.height() && !desiredSize.width())
        return { desiredSize.height() * actualSize.width() / actualSize.height(), desiredSize.height() };
    return desiredSize;
}

void RealtimeMediaSource::videoFrameAvailable(VideoFrame& videoFrame, VideoFrameTimeMetadata metadata)
{
#if !RELEASE_LOG_DISABLED
    ++m_frameCount;

    auto timestamp = MonotonicTime::now();
    auto delta = timestamp - m_lastFrameLogTime;
    if (!m_lastFrameLogTime || delta >= 1_s) {
        if (m_lastFrameLogTime) {
            INFO_LOG_IF(loggerPtr(), LOGIDENTIFIER, m_frameCount, " frames sent in ", delta.value(), " seconds");
            m_frameCount = 0;
        }
        m_lastFrameLogTime = timestamp;
    }
#endif

    updateHasStartedProducingData();

    Locker locker { m_videoFrameObserversLock };
    for (auto& [key, value] : m_videoFrameObservers) {
        if (auto* adaptor = value.get()) {
            if (adaptor->frameDecimation > 1 && ++adaptor->frameDecimationCounter % adaptor->frameDecimation)
                continue;

            adaptor->frameDecimation = adaptor->frameRate ? static_cast<size_t>(observedFrameRate() / adaptor->frameRate) : 1;
            if (!adaptor->frameDecimation)
                adaptor->frameDecimation = 1;

            if (!adaptor->size.isZero()) {
                auto actualSize = expandedIntSize(videoFrame.presentationSize());
                auto desiredSize = computeResizedVideoFrameSize(adaptor->size, actualSize);

                if (desiredSize != actualSize) {
                    if (auto newVideoFrame = adaptVideoFrame(*adaptor, videoFrame, desiredSize)) {
                        key->videoFrameAvailable(*newVideoFrame, metadata);
                        continue;
                    }
                }
            }
        }
        key->videoFrameAvailable(videoFrame, metadata);
    }
}

void RealtimeMediaSource::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames)
{
    updateHasStartedProducingData();

    Locker locker { m_audioSampleObserversLock };
    for (auto& observer : m_audioSampleObservers)
        observer->audioSamplesAvailable(time, audioData, description, numberOfFrames);
}

void RealtimeMediaSource::start()
{
    if (m_isProducingData || m_isEnded)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);

    m_isProducingData = true;
    startProducingData();

    if (m_registerOwnerCallback)
        m_registerOwnerCallback(*this, false);

    if (!m_isProducingData)
        return;

    forEachObserver([](auto& observer) {
        observer.sourceStarted();
    });
}

void RealtimeMediaSource::stop()
{
    if (!m_isProducingData)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);

    m_isProducingData = false;
    stopProducingData();
}

void RealtimeMediaSource::requestToEnd(RealtimeMediaSourceObserver& callingObserver)
{
    bool hasObserverPreventingEnding = false;
    forEachObserver([&](auto& observer) {
        if (observer.preventSourceFromEnding())
            hasObserverPreventingEnding = true;
    });
    if (hasObserverPreventingEnding)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);
    end(&callingObserver);
}

void RealtimeMediaSource::end(RealtimeMediaSourceObserver* callingObserver)
{
    ASSERT(isMainThread());

    if (m_isEnded)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);

    Ref protectedThis { *this };

    endProducingData();
    m_isEnded = true;
    didEnd();

    if (m_registerOwnerCallback)
        m_registerOwnerCallback(*this, false);

    forEachObserver([&callingObserver](auto& observer) {
        if (&observer != callingObserver)
            observer.sourceStopped();
    });
}

void RealtimeMediaSource::registerOwnerCallback(OwnerCallback&& callback)
{
    ASSERT(isMainThread());
    ASSERT(!m_registerOwnerCallback);
    m_registerOwnerCallback = WTFMove(callback);
}

void RealtimeMediaSource::captureFailed()
{
    ERROR_LOG_IF(m_logger, LOGIDENTIFIER);

    m_captureDidFailed = true;
    end();
}

bool RealtimeMediaSource::supportsSizeFrameRateAndZoom(const VideoPresetConstraints&)
{
    // The size and frame rate are within the capability limits, so they are supported.
    return true;
}

std::optional<MediaConstraintType> RealtimeMediaSource::hasInvalidSizeFrameRateAndZoomConstraints(std::optional<IntConstraint> widthConstraint, std::optional<IntConstraint> heightConstraint, std::optional<DoubleConstraint> frameRateConstraint, std::optional<DoubleConstraint> zoomConstraint, double& distance)
{
    if (!widthConstraint && !heightConstraint && !frameRateConstraint && !zoomConstraint)
        return { };

    auto& capabilities = this->capabilities();

    distance = std::numeric_limits<double>::infinity();

    std::optional<int> width;
    if (widthConstraint && capabilities.supportsWidth()) {
        double constraintDistance = fitnessDistance(MediaConstraintType::Width, *widthConstraint);
        if (std::isinf(constraintDistance)) {
#if !RELEASE_LOG_DISABLED
            auto range = capabilities.width();
            ERROR_LOG_IF(m_logger, LOGIDENTIFIER, "RealtimeMediaSource::supportsSizeFrameRateAndZoom failed width constraint, capabilities are [", range.min(), ", ", range.max(), "]");
#endif
            return MediaConstraintType::Width;
        }

        distance = std::min(distance, constraintDistance);
        if (widthConstraint->isMandatory()) {
            auto range = capabilities.width();
            width = widthConstraint->valueForCapabilityRange(size().width(), range);
        }
    }

    std::optional<int> height;
    if (heightConstraint && capabilities.supportsHeight()) {
        double constraintDistance = fitnessDistance(MediaConstraintType::Height, *heightConstraint);
        if (std::isinf(constraintDistance)) {
#if !RELEASE_LOG_DISABLED
            auto range = capabilities.height();
            ERROR_LOG_IF(m_logger, LOGIDENTIFIER, "RealtimeMediaSource::supportsSizeFrameRateAndZoom failed height constraint, capabilities are [%d, %d]", range.min(), range.max());
#endif
            return MediaConstraintType::Height;
        }

        distance = std::min(distance, constraintDistance);
        if (heightConstraint->isMandatory()) {
            auto range = capabilities.height();
            height = heightConstraint->valueForCapabilityRange(size().height(), range);
        }
    }

    std::optional<double> frameRate;
    if (frameRateConstraint && capabilities.supportsFrameRate()) {
        double constraintDistance = fitnessDistance(MediaConstraintType::FrameRate, *frameRateConstraint);
        if (std::isinf(constraintDistance)) {
#if !RELEASE_LOG_DISABLED
            auto range = capabilities.frameRate();
            ERROR_LOG_IF(m_logger, LOGIDENTIFIER, "RealtimeMediaSource::supportsSizeFrameRateAndZoom failed frame rate constraint, capabilities are [", range.min(), ", ", range.max(), "]");
#endif
            return MediaConstraintType::FrameRate;
        }

        distance = std::min(distance, constraintDistance);
        if (frameRateConstraint->isMandatory()) {
            auto range = capabilities.frameRate();
            frameRate = frameRateConstraint->valueForCapabilityRange(this->frameRate(), range);
        }
    }

    std::optional<double> zoom;
    if (zoomConstraint && capabilities.supportsZoom()) {
        double constraintDistance = fitnessDistance(MediaConstraintType::Zoom, *zoomConstraint);
        if (std::isinf(constraintDistance)) {
#if !RELEASE_LOG_DISABLED
            auto range = capabilities.zoom();
            ERROR_LOG_IF(m_logger, LOGIDENTIFIER, "RealtimeMediaSource::supportsSizeFrameRateAndZoom failed zoom constraint, capabilities are [", range.min(), ", ", range.max(), "]");
#endif
            return MediaConstraintType::Zoom;
        }

        distance = std::min(distance, constraintDistance);
        if (zoomConstraint->isMandatory()) {
            auto range = capabilities.zoom();
            zoom = zoomConstraint->valueForCapabilityRange(this->zoom(), range);
        }
    }

    // Each of the non-null values is supported individually, see if they all can be applied at the same time.
    if (!supportsSizeFrameRateAndZoom({ width, height, frameRate, zoom })) {
        // Let's try without frame rate and zoom constraints if not mandatory.
        if ((!frameRateConstraint || !frameRateConstraint->isMandatory()) && (!zoomConstraint || !zoomConstraint->isMandatory()) && supportsSizeFrameRateAndZoom({ width, height, { }, { } }))
            return { };

        if (widthConstraint)
            return MediaConstraintType::Width;
        else if (heightConstraint)
            return MediaConstraintType::Height;
        else
            return MediaConstraintType::FrameRate;
    }

    return { };
}

double RealtimeMediaSource::fitnessDistance(MediaConstraintType constraintType, const IntConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraintType) {
    case MediaConstraintType::Width:
        if (!capabilities.supportsWidth())
            return 0;

        return constraint.fitnessDistance(capabilities.width());
    case MediaConstraintType::Height:
        if (!capabilities.supportsHeight())
            return 0;

        return constraint.fitnessDistance(capabilities.height());
    case MediaConstraintType::SampleRate:
        if (!capabilities.supportsSampleRate())
            return 0;

        if (auto discreteRates = discreteSampleRates())
            return constraint.fitnessDistance(*discreteRates);

        return constraint.fitnessDistance(capabilities.sampleRate());
    case MediaConstraintType::SampleSize:
        if (!capabilities.supportsSampleSize())
            return 0;

        if (auto discreteSizes = discreteSampleSizes())
            return constraint.fitnessDistance(*discreteSizes);

        return constraint.fitnessDistance(capabilities.sampleSize());
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::Volume:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::Torch:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        break;
    }

    return 0;
}

double RealtimeMediaSource::fitnessDistance(MediaConstraintType constraintType, const DoubleConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraintType) {
    case MediaConstraintType::FrameRate:
        if (!capabilities.supportsFrameRate())
            return 0;

        return constraint.fitnessDistance(capabilities.frameRate());
    case MediaConstraintType::AspectRatio:
        if (!capabilities.supportsAspectRatio())
            return 0;

        return constraint.fitnessDistance(capabilities.aspectRatio());
    case MediaConstraintType::Volume:
        if (!capabilities.supportsVolume())
            return 0;

        return constraint.fitnessDistance(capabilities.volume());
    case MediaConstraintType::Zoom:
        if (!capabilities.supportsZoom())
            return 0;

        return constraint.fitnessDistance(capabilities.zoom());
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Torch:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        break;
    }

    return 0;
}

double RealtimeMediaSource::fitnessDistance(MediaConstraintType constraintType, const StringConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraintType) {
    case MediaConstraintType::FacingMode:
        if (!capabilities.supportsFacingMode())
            return 0;

        return constraint.fitnessDistance(capabilities.facingMode().map([](auto& mode) {
            return convertEnumerationToString(mode);
        }));
    case MediaConstraintType::DeviceId:
        ASSERT(constraint.isString());
        ASSERT(!hashedId().isEmpty());
        return constraint.fitnessDistance(hashedId());
    case MediaConstraintType::GroupId:
        if (!capabilities.supportsDeviceId())
            return 0;

        return constraint.fitnessDistance(settings().groupId());
    case MediaConstraintType::WhiteBalanceMode:
        if (!capabilities.supportsWhiteBalanceMode())
            return 0;

        return constraint.fitnessDistance(capabilities.whiteBalanceModes().map([](auto& mode) {
            return convertEnumerationToString(mode);
        }));
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::Volume:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::Torch:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        break;
    }

    return 0;
}

double RealtimeMediaSource::fitnessDistance(MediaConstraintType constraintType, const BooleanConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraintType) {
    case MediaConstraintType::EchoCancellation:
        if (!capabilities.supportsEchoCancellation())
            return 0;

        return constraint.fitnessDistance(capabilities.echoCancellation() == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
    case MediaConstraintType::Torch:
        if (!capabilities.supportsTorch())
            return 0;

        if (constraint.isMandatory())
            return 0;

        return constraint.fitnessDistance(capabilities.torch());
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
        return 0;
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::Volume:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Unknown:
        break;
    }

    return 0;
}

double RealtimeMediaSource::fitnessDistance(MediaConstraintType constraintType, const MediaConstraint& constraint)
{
    switch (constraint.dataType()) {
    case MediaConstraint::DataType::Integer:
        return fitnessDistance(constraintType, downcast<const IntConstraint>(constraint));
    case MediaConstraint::DataType::Double:
        return fitnessDistance(constraintType, downcast<const DoubleConstraint>(constraint));
    case MediaConstraint::DataType::Boolean:
        return fitnessDistance(constraintType, downcast<const BooleanConstraint>(constraint));
    case MediaConstraint::DataType::String:
        return fitnessDistance(constraintType, downcast<const StringConstraint>(constraint));
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template <typename ValueType>
static void applyNumericConstraint(const NumericConstraint<ValueType>& constraint, ValueType current, std::optional<Vector<ValueType>> discreteCapabilityValues, ValueType capabilityMin, ValueType capabilityMax, RealtimeMediaSource& source, void (RealtimeMediaSource::*applier)(ValueType))
{
    if (discreteCapabilityValues) {
        auto value = constraint.valueForDiscreteCapabilityValues(current, *discreteCapabilityValues);
        if (value && *value != current)
            (source.*applier)(*value);
        return;
    }

    ValueType value = constraint.valueForCapabilityRange(current, capabilityMin, capabilityMax);
    if (value != current)
        (source.*applier)(value);
}

void RealtimeMediaSource::setSizeFrameRateAndZoom(const VideoPresetConstraints& constraints)
{
    IntSize size;
    if (constraints.width)
        size.setWidth(*constraints.width);
    if (constraints.height)
        size.setHeight(*constraints.height);
    setSize(size);
    if (constraints.frameRate)
        setFrameRate(*constraints.frameRate);
    if (constraints.zoom)
        setZoom(*constraints.zoom);
}

void RealtimeMediaSource::applyConstraint(MediaConstraintType constraintType, const MediaConstraint& constraint)
{
    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, constraintType);

    auto& capabilities = this->capabilities();
    switch (constraintType) {
    case MediaConstraintType::Width:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::Height:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::FrameRate:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::AspectRatio: {
        ASSERT_NOT_REACHED();
        break;
    }

    case MediaConstraintType::Zoom: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsZoom())
            return;

        auto range = capabilities.zoom();
        applyNumericConstraint(downcast<DoubleConstraint>(constraint), zoom(), { }, range.min(), range.max(), *this, &RealtimeMediaSource::setZoom);
        break;
    }

    case MediaConstraintType::SampleRate: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleRate())
            return;

        auto range = capabilities.sampleRate();
        applyNumericConstraint(downcast<IntConstraint>(constraint), sampleRate(), discreteSampleRates(), range.min(), range.max(), *this, &RealtimeMediaSource::setSampleRate);
        break;
    }

    case MediaConstraintType::SampleSize: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleSize())
            return;

        auto range = capabilities.sampleSize();
        applyNumericConstraint(downcast<IntConstraint>(constraint), sampleSize(), { }, range.min(), range.max(), *this, &RealtimeMediaSource::setSampleSize);
        break;
    }

    case MediaConstraintType::EchoCancellation: {
        ASSERT(constraint.isBoolean());
        if (!capabilities.supportsEchoCancellation())
            return;

        bool setting;
        const BooleanConstraint& boolConstraint = downcast<BooleanConstraint>(constraint);
        if (boolConstraint.getExact(setting) || boolConstraint.getIdeal(setting))
            setEchoCancellation(setting);
        break;
    }

    case MediaConstraintType::FacingMode: {
        ASSERT(constraint.isString());
        if (!capabilities.supportsFacingMode())
            return;

        auto& supportedModes = capabilities.facingMode();
        auto filter = [supportedModes](const String& modeString) {
            auto mode = RealtimeMediaSourceSettings::videoFacingModeEnum(modeString);
            for (auto& supportedMode : supportedModes) {
                if (mode == supportedMode)
                    return true;
            }
            return false;
        };

        auto modeString = downcast<StringConstraint>(constraint).find(WTFMove(filter));
        if (!modeString.isEmpty())
            setFacingMode(RealtimeMediaSourceSettings::videoFacingModeEnum(modeString));
        break;
    }

    case MediaConstraintType::WhiteBalanceMode: {
        ASSERT(constraint.isString());
        if (!capabilities.supportsWhiteBalanceMode())
            return;

        auto& supportedModes = capabilities.whiteBalanceModes();
        std::optional<MeteringMode> whiteBalanceMode;
        downcast<StringConstraint>(constraint).find([supportedModes, &whiteBalanceMode](const String& modeString) mutable {
            auto mode = parseEnumerationFromString<MeteringMode>(modeString);
            if (!mode)
                return false;

            for (auto& supportedMode : supportedModes) {
                if (mode.value() == supportedMode) {
                    whiteBalanceMode = mode.value();
                    return true;
                }
            }

            return false;
        });

        if (whiteBalanceMode)
            setWhiteBalanceMode(whiteBalanceMode.value());
        break;
    }

    case MediaConstraintType::Volume: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsVolume())
            return;

        auto range = capabilities.volume();
        applyNumericConstraint(downcast<DoubleConstraint>(constraint), volume(), { }, range.min(), range.max(), *this, &RealtimeMediaSource::setVolume);
        break;
    }

    case MediaConstraintType::Torch: {
        ASSERT(constraint.isBoolean());
        if (!capabilities.supportsTorch())
            return;

        bool setting;
        const BooleanConstraint& boolConstraint = downcast<BooleanConstraint>(constraint);
        if (boolConstraint.getExact(setting) || boolConstraint.getIdeal(setting))
            setTorch(setting);
        break;
    }
    case MediaConstraintType::BackgroundBlur: {
        ASSERT(constraint.isBoolean());
        // FIXME: Implement support, https://bugs.webkit.org/show_bug.cgi?id=275491
        break;
    }
    case MediaConstraintType::PowerEfficient: {
        ASSERT(constraint.isBoolean());
        // FIXME: Implement support, https://bugs.webkit.org/show_bug.cgi?id=275491
        break;
    }

    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        ASSERT(constraint.isString());
        // There is nothing to do here, neither can be changed.
        break;

    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
        ASSERT(constraint.isBoolean());
        break;

    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Unknown:
        break;
    }
}

std::optional<MediaConstraintType> RealtimeMediaSource::selectSettings(const MediaConstraints& constraints, MediaTrackConstraintSetMap& candidates)
{
    double minimumDistance = std::numeric_limits<double>::infinity();

    // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
    //
    // 1. Each constraint specifies one or more values (or a range of values) for its property.
    //    A property may appear more than once in the list of 'advanced' ConstraintSets. If an
    //    empty object or list has been given as the value for a constraint, it must be interpreted
    //    as if the constraint were not specified (in other words, an empty constraint == no constraint).
    //
    //    Note that unknown properties are discarded by WebIDL, which means that unknown/unsupported required
    //    constraints will silently disappear. To avoid this being a surprise, application authors are
    //    expected to first use the getSupportedConstraints() method as shown in the Examples below.

    // 2. Let object be the ConstrainablePattern object on which this algorithm is applied. Let copy be an
    //    unconstrained copy of object (i.e., copy should behave as if it were object with all ConstraintSets
    //    removed.)

    // 3. For every possible settings dictionary of copy compute its fitness distance, treating bare values of
    //    properties as ideal values. Let candidates be the set of settings dictionaries for which the fitness
    //    distance is finite.

    // Check width, height, frame rate and zoom jointly, because while they may be supported individually the combination may not be supported.
    if (auto invalidConstraint = hasInvalidSizeFrameRateAndZoomConstraints(constraints.mandatoryConstraints.width(), constraints.mandatoryConstraints.height(), constraints.mandatoryConstraints.frameRate(), constraints.mandatoryConstraints.zoom(), minimumDistance))
        return invalidConstraint;

    double distance = std::numeric_limits<double>::infinity();
    std::optional<MediaConstraintType> invalidConstraint;
    constraints.mandatoryConstraints.filter([&](auto constraintType, auto& constraint) {
        if (!supportsConstraint(constraintType))
            return false;

        if (constraintType == MediaConstraintType::Width || constraintType == MediaConstraintType::Height || constraintType == MediaConstraintType::FrameRate || constraintType == MediaConstraintType::Zoom || constraintType == MediaConstraintType::PowerEfficient) {
            candidates.set(constraintType, constraint);
            return false;
        }

        double constraintDistance = fitnessDistance(constraintType, constraint);
        if (std::isinf(constraintDistance)) {
            ERROR_LOG_IF(m_logger, LOGIDENTIFIER, "RealtimeMediaSource::selectSettings failed constraint %d", static_cast<int>(constraintType));
            invalidConstraint = constraintType;
            return true;
        }

        distance = std::min(distance, constraintDistance);
        candidates.set(constraintType, constraint);
        return false;
    });

    if (invalidConstraint)
        return invalidConstraint;

    minimumDistance = distance;

    // 4. If candidates is empty, return undefined as the result of the SelectSettings() algorithm.
    // We skip this check since our implementation will compute empty candidates in case no mandatory constraints is given.

    // 5. Iterate over the 'advanced' ConstraintSets in newConstraints in the order in which they were specified.
    //    For each ConstraintSet:

    // 5.1 compute the fitness distance between it and each settings dictionary in candidates, treating bare
    //     values of properties as exact.
    Vector<std::pair<double, MediaTrackConstraintSetMap>> supportedConstraints;

    for (const auto& advancedConstraint : constraints.advancedConstraints) {
        double constraintDistance = 0;
        bool supported = false;

        if (advancedConstraint.width() || advancedConstraint.height() || advancedConstraint.frameRate() || advancedConstraint.zoom()) {
            if (auto invalidConstraint = hasInvalidSizeFrameRateAndZoomConstraints(advancedConstraint.width(), advancedConstraint.height(), advancedConstraint.frameRate(), advancedConstraint.zoom(), constraintDistance))
                continue;

            supported = true;
        }

        advancedConstraint.forEach([&](auto constraintType, const MediaConstraint& constraint) {

            if (constraintType == MediaConstraintType::Width || constraintType == MediaConstraintType::Height || constraintType == MediaConstraintType::FrameRate || constraintType == MediaConstraintType::Zoom || constraintType == MediaConstraintType::PowerEfficient)
                return;

            distance = fitnessDistance(constraintType, constraint);
            constraintDistance += distance;
            if (!std::isinf(distance))
                supported = true;
        });

        minimumDistance = std::min(minimumDistance, constraintDistance);

        // 5.2 If the fitness distance is finite for one or more settings dictionaries in candidates, keep those
        //     settings dictionaries in candidates, discarding others.
        //     If the fitness distance is infinite for all settings dictionaries in candidates, ignore this ConstraintSet.
        if (supported)
            supportedConstraints.append({constraintDistance, advancedConstraint});
    }

    // 6. Select one settings dictionary from candidates, and return it as the result of the SelectSettings() algorithm.
    //    The UA should use the one with the smallest fitness distance, as calculated in step 3.
    if (!supportedConstraints.isEmpty()) {
        supportedConstraints.removeAllMatching([&](const std::pair<double, MediaTrackConstraintSetMap>& pair) -> bool {
            return std::isinf(pair.first) || pair.first > minimumDistance;
        });

        if (!supportedConstraints.isEmpty()) {
            auto& advancedConstraint = supportedConstraints[0].second;
            advancedConstraint.forEach([&](auto constraintType, const MediaConstraint& constraint) {
                candidates.merge(constraintType, constraint);
            });

            minimumDistance = std::min(minimumDistance, supportedConstraints[0].first);
        }
    }

    return { };
}

bool RealtimeMediaSource::supportsConstraint(MediaConstraintType constraintType)
{
    auto& capabilities = this->capabilities();

    switch (constraintType) {
    case MediaConstraintType::Width:
        return capabilities.supportsWidth();
    case MediaConstraintType::Height:
        return capabilities.supportsHeight();
    case MediaConstraintType::FrameRate:
        return capabilities.supportsFrameRate();
    case MediaConstraintType::AspectRatio:
        return capabilities.supportsAspectRatio();
    case MediaConstraintType::Volume:
        return capabilities.supportsVolume();
    case MediaConstraintType::SampleRate:
        return capabilities.supportsSampleRate();
    case MediaConstraintType::SampleSize:
        return capabilities.supportsSampleSize();
    case MediaConstraintType::FacingMode:
        return capabilities.supportsFacingMode();
    case MediaConstraintType::EchoCancellation:
        return capabilities.supportsEchoCancellation();
    case MediaConstraintType::DeviceId:
        return capabilities.supportsDeviceId();
    case MediaConstraintType::GroupId:
        return capabilities.supportsDeviceId();
    case MediaConstraintType::WhiteBalanceMode:
        return capabilities.supportsWhiteBalanceMode();
    case MediaConstraintType::Zoom:
        return capabilities.supportsZoom();
    case MediaConstraintType::Torch:
        return capabilities.supportsTorch();
    case MediaConstraintType::BackgroundBlur:
        return capabilities.supportsBackgroundBlur();
    case MediaConstraintType::PowerEfficient:
        return deviceType() == CaptureDevice::DeviceType::Camera;
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
        // https://www.w3.org/TR/screen-capture/#new-constraints-for-captured-display-surfaces
        // 5.2.1 New Constraints for Captured Display Surfaces
        // Since the source of media cannot be changed after a MediaStreamTrack has been returned,
        // these constraints cannot be changed by an application.
        return false;
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Unknown:
        break;
    }
    
    return false;
}

std::optional<MediaConstraintType> RealtimeMediaSource::hasAnyInvalidConstraint(const MediaConstraints& constraints)
{
    ASSERT(constraints.isValid);

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);

    MediaTrackConstraintSetMap candidates;
    if (auto invalidConstraint = selectSettings(constraints, candidates))
        return invalidConstraint;

    m_fitnessScore = 0;
    candidates.forEach([&] (auto constraintType, auto& variant) {
        double distance = fitnessDistance(constraintType, variant);
        switch (constraintType) {
        case MediaConstraintType::DeviceId:
            m_fitnessScore += distance ? 1 : 32;
            break;
        case MediaConstraintType::FacingMode:
            m_fitnessScore += facingModeFitnessScoreAdjustment() + (distance ? 1 : 32);
            break;

        case MediaConstraintType::Width:
        case MediaConstraintType::Height:
        case MediaConstraintType::FrameRate:
        case MediaConstraintType::AspectRatio:
        case MediaConstraintType::Volume:
        case MediaConstraintType::SampleRate:
        case MediaConstraintType::SampleSize:
        case MediaConstraintType::EchoCancellation:
        case MediaConstraintType::GroupId:
        case MediaConstraintType::DisplaySurface:
        case MediaConstraintType::LogicalSurface:
        case MediaConstraintType::FocusDistance:
        case MediaConstraintType::WhiteBalanceMode:
        case MediaConstraintType::Zoom:
        case MediaConstraintType::Torch:
        case MediaConstraintType::BackgroundBlur:
        case MediaConstraintType::PowerEfficient:
        case MediaConstraintType::Unknown:
            m_fitnessScore += distance ? 1 : 2;
            break;
        }
    });

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, "fitness distance : ", m_fitnessScore);

    return { };
}

RealtimeMediaSource::VideoPresetConstraints RealtimeMediaSource::extractVideoPresetConstraints(const MediaConstraints& constraints)
{
    MediaTrackConstraintSetMap candidates;
    if (auto invalidConstraint = selectSettings(constraints, candidates))
        return { };
    return extractVideoPresetConstraints(candidates);
}

RealtimeMediaSource::VideoPresetConstraints RealtimeMediaSource::extractVideoPresetConstraints(const MediaTrackConstraintSetMap& constraints)
{
    VideoPresetConstraints result;
    auto& capabilities = this->capabilities();

    if (auto constraint = constraints.width()) {
        if (capabilities.supportsWidth())
            result.width = constraint->valueForCapabilityRange(size().width(), capabilities.width());
    }

    if (auto constraint = constraints.height()) {
        if (capabilities.supportsHeight())
            result.height = constraint->valueForCapabilityRange(size().height(), capabilities.height());
    }

    if (auto constraint = constraints.aspectRatio()) {
        if (capabilities.supportsAspectRatio()) {
            auto size = this->size();
            auto range = capabilities.aspectRatio();
            auto currentAspectRatio = size.width() ? size.width() / static_cast<double>(size.height()) : 0;
            if (auto aspectRatio = constraint->valueForCapabilityRange(currentAspectRatio, range)) {
                if (!result.width && result.height)
                    result.width = *result.height * aspectRatio;
                if (result.width && !result.height)
                    result.height = *result.width / aspectRatio;
            }
        }
    }

    if (auto constraint = constraints.frameRate()) {
        if (capabilities.supportsFrameRate()) {
            auto range = capabilities.frameRate();
            result.frameRate = constraint->valueForCapabilityRange(this->frameRate(), range);
        }
    }

    if (auto constraint = constraints.zoom()) {
        if (capabilities.supportsZoom()) {
            auto range = capabilities.zoom();
            result.zoom = constraint->valueForCapabilityRange(this->zoom(), range);
        }
    }

    if (auto contraint = constraints.powerEfficient())
        contraint->getExact(result.shouldPreferPowerEfficiency) || contraint->getIdeal(result.shouldPreferPowerEfficiency);

    return result;
}

void RealtimeMediaSource::applyConstraints(const MediaTrackConstraintSetMap& constraints)
{
    if (constraints.isEmpty())
        return;

    startApplyingConstraints();

    auto videoPresetConstraints = extractVideoPresetConstraints(constraints);

    if (videoPresetConstraints.hasConstraints())
        setSizeFrameRateAndZoom(videoPresetConstraints);

    constraints.forEach([&] (auto constraintType, auto& constraint) {
        if (constraintType == MediaConstraintType::Width || constraintType == MediaConstraintType::Height || constraintType == MediaConstraintType::AspectRatio || constraintType == MediaConstraintType::FrameRate || constraintType == MediaConstraintType::Zoom)
            return;

        applyConstraint(constraintType, constraint);
    });

    endApplyingConstraints();
}

std::optional<RealtimeMediaSource::ApplyConstraintsError> RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints)
{
    ASSERT(constraints.isValid);

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER);

    MediaTrackConstraintSetMap candidates;
    if (auto invalidConstraint = selectSettings(constraints, candidates))
        return ApplyConstraintsError { *invalidConstraint, "Invalid constraint"_s };

    applyConstraints(candidates);
    return { };
}

void RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    completionHandler(applyConstraints(constraints));
}

void RealtimeMediaSource::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, size);
    
    m_size = size;
    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
}

const IntSize RealtimeMediaSource::size() const
{
    auto size = m_size;

    if (size.isEmpty() && !m_intrinsicSize.isEmpty()) {
        if (size.isZero())
            size = m_intrinsicSize;
        else if (size.width())
            size.setHeight(size.width() * (m_intrinsicSize.height() / static_cast<double>(m_intrinsicSize.width())));
        else if (size.height())
            size.setWidth(size.height() * (m_intrinsicSize.width() / static_cast<double>(m_intrinsicSize.height())));
    }

    return size;
}

void RealtimeMediaSource::setIntrinsicSize(const IntSize& intrinsicSize, bool notifyObservers)
{
    if (m_intrinsicSize == intrinsicSize)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, intrinsicSize);

    m_intrinsicSize = intrinsicSize;
    if (notifyObservers) {
        scheduleDeferredTask([this] {
            notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
        });
    }
}

IntSize RealtimeMediaSource::intrinsicSize() const
{
    return m_intrinsicSize;
}

void RealtimeMediaSource::setFrameRate(double rate)
{
    if (m_frameRate == rate)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, rate);
    
    m_frameRate = rate;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::FrameRate);
}

void RealtimeMediaSource::setZoom(double zoom)
{
    if (m_zoom == zoom)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, zoom);

    m_zoom = zoom;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::Zoom);
}

void RealtimeMediaSource::setTorch(bool torch)
{
    if (m_torch == torch)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, torch);
    m_torch = torch;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::Torch);
}


void RealtimeMediaSource::setFacingMode(VideoFacingMode mode)
{
    if (m_facingMode == mode)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, mode);

    m_facingMode = mode;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::FacingMode);
}

void RealtimeMediaSource::setWhiteBalanceMode(MeteringMode mode)
{
    if (m_whiteBalanceMode == mode)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, mode);

    m_whiteBalanceMode = mode;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::WhiteBalanceMode);
}

void RealtimeMediaSource::setVolume(double volume)
{
    if (m_volume == volume)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, volume);

    m_volume = volume;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::Volume);
}

void RealtimeMediaSource::setSampleRate(int rate)
{
    if (m_sampleRate == rate)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, rate);

    m_sampleRate = rate;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::SampleRate);
}

std::optional<Vector<int>> RealtimeMediaSource::discreteSampleRates() const
{
    return std::nullopt;
}

void RealtimeMediaSource::setSampleSize(int size)
{
    if (m_sampleSize == size)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, size);

    m_sampleSize = size;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::SampleSize);
}

std::optional<Vector<int>> RealtimeMediaSource::discreteSampleSizes() const
{
    return std::nullopt;
}

void RealtimeMediaSource::setEchoCancellation(bool echoCancellation)
{
    if (m_echoCancellation == echoCancellation)
        return;

    ALWAYS_LOG_IF(m_logger, LOGIDENTIFIER, echoCancellation);
    m_echoCancellation = echoCancellation;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::EchoCancellation);
}

void RealtimeMediaSource::scheduleDeferredTask(Function<void()>&& function)
{
    ASSERT(function);
    callOnMainThread([protectedThis = Ref { *this }, function = WTFMove(function)] {
        function();
    });
}

const String& RealtimeMediaSource::hashedId() const
{
    ASSERT(!m_hashedID.isEmpty());
    ASSERT(!m_ephemeralHashedID.isEmpty());

    if (isEphemeral())
        return m_ephemeralHashedID;

    return m_hashedID;
}

const MediaDeviceHashSalts& RealtimeMediaSource::deviceIDHashSalts() const
{
    return m_idHashSalts;
}

void RealtimeMediaSource::setType(Type type)
{
    if (type == m_type)
        return;

    m_type = type;

    scheduleDeferredTask([this] {
        forEachObserver([](auto& observer) {
            observer.sourceSettingsChanged();
        });
    });
}

auto RealtimeMediaSource::takePhoto(PhotoSettings&&) -> Ref<TakePhotoNativePromise>
{
    return TakePhotoNativePromise::createAndReject("Not supported"_s);
}

auto RealtimeMediaSource::getPhotoCapabilities() -> Ref<PhotoCapabilitiesNativePromise>
{
    return PhotoCapabilitiesNativePromise::createAndReject("Not supported"_s);
}

auto RealtimeMediaSource::getPhotoSettings() -> Ref<PhotoSettingsNativePromise>
{
    return PhotoSettingsNativePromise::createAndReject("Not supported"_s);
}

#if !RELEASE_LOG_DISABLED
void RealtimeMediaSource::setLogger(const Logger& newLogger, const void* newLogIdentifier)
{
    m_logger = &newLogger;
    m_logIdentifier = newLogIdentifier;
    ALWAYS_LOG(LOGIDENTIFIER, m_type, ", ", name(), ", ", m_hashedID, ", ", m_ephemeralHashedID);
}

WTFLogChannel& RealtimeMediaSource::logChannel() const
{
    return LogWebRTC;
}
#endif

String convertEnumerationToString(RealtimeMediaSource::Type enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Audio"),
        MAKE_STATIC_STRING_IMPL("Video")
    };
    static_assert(static_cast<size_t>(RealtimeMediaSource::Type::Audio) == 0, "RealtimeMediaSource::Type::Audio is not 0 as expected");
    static_assert(static_cast<size_t>(RealtimeMediaSource::Type::Video) == 1, "RealtimeMediaSource::Type::Video is not 1 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
