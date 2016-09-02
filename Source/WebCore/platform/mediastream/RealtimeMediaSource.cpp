/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "MediaConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "UUID.h"
#include <wtf/MainThread.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

RealtimeMediaSource::RealtimeMediaSource(const String& id, Type type, const String& name)
    : m_weakPtrFactory(this)
    , m_id(id)
    , m_type(type)
    , m_name(name)
{
    // FIXME(147205): Need to implement fitness score for constraints

    if (m_id.isEmpty())
        m_id = createCanonicalUUIDString();
    m_persistentID = m_id;
    m_suppressNotifications = false;
}

void RealtimeMediaSource::reset()
{
    m_stopped = false;
    m_muted = false;
    m_readonly = false;
    m_remote = false;
}

void RealtimeMediaSource::addObserver(RealtimeMediaSource::Observer* observer)
{
    m_observers.append(observer);
}

void RealtimeMediaSource::removeObserver(RealtimeMediaSource::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);

    if (!m_observers.size())
        stop();
}

void RealtimeMediaSource::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    if (stopped())
        return;

    for (auto& observer : m_observers)
        observer->sourceMutedChanged();
}

void RealtimeMediaSource::settingsDidChange()
{
    ASSERT(isMainThread());

    if (m_pendingSettingsDidChangeNotification || m_suppressNotifications)
        return;

    m_pendingSettingsDidChangeNotification = true;

    scheduleDeferredTask([this] {
        m_pendingSettingsDidChangeNotification = false;
        for (auto& observer : m_observers)
            observer->sourceSettingsChanged();
    });
}

void RealtimeMediaSource::mediaDataUpdated(MediaSample& mediaSample)
{
    for (auto& observer : m_observers)
        observer->sourceHasMoreMediaData(mediaSample);
}

bool RealtimeMediaSource::readonly() const
{
    return m_readonly;
}

void RealtimeMediaSource::stop(Observer* callingObserver)
{
    if (stopped())
        return;

    m_stopped = true;

    for (auto* observer : m_observers) {
        if (observer != callingObserver)
            observer->sourceStopped();
    }

    stopProducingData();
}

void RealtimeMediaSource::requestStop(Observer* callingObserver)
{
    if (stopped())
        return;

    for (auto* observer : m_observers) {
        if (observer->preventSourceFromStopping())
            return;
    }
    stop(callingObserver);
}

RealtimeMediaSource::ConstraintSupport RealtimeMediaSource::supportsConstraint(const MediaConstraint& constraint)
{
    RealtimeMediaSourceCapabilities& capabilities = *this->capabilities();

    switch (constraint.type()) {
    case MediaConstraintType::Width: {
        if (!capabilities.supportsWidth())
            return ConstraintSupport::Ignored;

        auto widthRange = capabilities.width();
        return constraint.validForRange(widthRange.rangeMin().asInt, widthRange.rangeMax().asInt) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::Height: {
        if (!capabilities.supportsHeight())
            return ConstraintSupport::Ignored;

        auto heightRange = capabilities.height();
        return constraint.validForRange(heightRange.rangeMin().asInt, heightRange.rangeMax().asInt) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::FrameRate: {
        if (!capabilities.supportsFrameRate())
            return ConstraintSupport::Ignored;

        auto rateRange = capabilities.frameRate();
        return constraint.validForRange(rateRange.rangeMin().asDouble, rateRange.rangeMax().asDouble) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::AspectRatio: {
        if (!capabilities.supportsAspectRatio())
            return ConstraintSupport::Ignored;

        auto range = capabilities.aspectRatio();
        return constraint.validForRange(range.rangeMin().asDouble, range.rangeMax().asDouble) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::Volume: {
        if (!capabilities.supportsVolume())
            return ConstraintSupport::Ignored;

        auto range = capabilities.volume();
        return constraint.validForRange(range.rangeMin().asDouble, range.rangeMax().asDouble) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::SampleRate: {
        if (!capabilities.supportsSampleRate())
            return ConstraintSupport::Ignored;

        auto range = capabilities.sampleRate();
        return constraint.validForRange(range.rangeMin().asDouble, range.rangeMax().asDouble) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::SampleSize: {
        if (!capabilities.supportsSampleSize())
            return ConstraintSupport::Ignored;

        auto range = capabilities.sampleSize();
        return constraint.validForRange(range.rangeMin().asDouble, range.rangeMax().asDouble) ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
        break;
    }

    case MediaConstraintType::FacingMode: {
        if (!capabilities.supportsFacingMode())
            return ConstraintSupport::Ignored;

        ConstraintSupport support = ConstraintSupport::Ignored;
        auto& supportedModes = capabilities.facingMode();
        std::function<bool(MediaConstraint::ConstraintType, const String&)> filter = [supportedModes, &support](MediaConstraint::ConstraintType type, const String& modeString) {
            if (type == MediaConstraint::ConstraintType::ExactConstraint)
                support = ConstraintSupport::Unsupported;

            auto mode = RealtimeMediaSourceSettings::videoFacingModeEnum(modeString);
            for (auto& supportedMode : supportedModes) {
                if (supportedMode == mode) {
                    support = ConstraintSupport::Supported;
                    break;
                }
            }

            return type == MediaConstraint::ConstraintType::ExactConstraint ? true : false;
        };

        constraint.find(filter);
        return support;
        break;
    }

    case MediaConstraintType::EchoCancellation:
        if (!capabilities.supportsEchoCancellation())
            return ConstraintSupport::Ignored;

        if (capabilities.echoCancellation() == RealtimeMediaSourceCapabilities::EchoCancellation::ReadOnly)
            return constraint.isMandatory() ? ConstraintSupport::Unsupported : ConstraintSupport::Ignored;

        return ConstraintSupport::Supported;
        break;

    case MediaConstraintType::DeviceId: {
        if (!capabilities.supportsDeviceId())
            return ConstraintSupport::Ignored;

        ConstraintSupport support = ConstraintSupport::Ignored;
        std::function<bool(MediaConstraint::ConstraintType, const String&)> filter = [this, &support](MediaConstraint::ConstraintType type, const String& idString) {
            if (type != MediaConstraint::ConstraintType::ExactConstraint)
                return false; // Keep looking.

            support = idString == m_id ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
            return false;
        };

        constraint.find(filter);
        return support;
        break;
    }

    case MediaConstraintType::GroupId: {
        if (!capabilities.supportsDeviceId())
            return ConstraintSupport::Ignored;

        ConstraintSupport support = ConstraintSupport::Ignored;
        String groupId = settings().groupId();
        std::function<bool(MediaConstraint::ConstraintType, const String&)> filter = [groupId, &support](MediaConstraint::ConstraintType type, const String& idString) {
            if (type != MediaConstraint::ConstraintType::ExactConstraint)
                return false; // Keep looking.

            support = idString == groupId ? ConstraintSupport::Supported : ConstraintSupport::Unsupported;
            return false;
        };

        constraint.find(filter);
        return support;
        break;
    }

    case MediaConstraintType::Unknown:
        // Unknown (or unsupported) constraints should be ignored.
        break;
    }

    return ConstraintSupport::Ignored;
}


template <typename T>
T value(const MediaConstraint& constraint, T rangeMin, T rangeMax)
{
    T result;

    if (constraint.getExact(result)) {
        ASSERT(result >= rangeMin && result <= rangeMax);
        return result;
    }

    if (constraint.getIdeal(result)) {
        if (result < rangeMin)
            result = rangeMin;
        else if (result > rangeMax)
            result = rangeMax;

        return result;
    }

    if (constraint.getMin(result) && result > rangeMax)
        return false;

    if (constraint.getMax(result) && result < rangeMin)
        return false;
    
    return result;
}


void RealtimeMediaSource::applyConstraint(const MediaConstraint& constraint)
{
    RealtimeMediaSourceCapabilities& capabilities = *this->capabilities();
    switch (constraint.type()) {
    case MediaConstraintType::Width: {
        if (!capabilities.supportsWidth())
            return;

        auto widthRange = capabilities.width();
        setWidth(value(constraint, widthRange.rangeMin().asInt, widthRange.rangeMax().asInt));
        break;
    }

    case MediaConstraintType::Height: {
        if (!capabilities.supportsHeight())
            return;

        auto heightRange = capabilities.height();
        setHeight(value(constraint, heightRange.rangeMin().asInt, heightRange.rangeMax().asInt));
        break;
    }

    case MediaConstraintType::FrameRate: {
        if (!capabilities.supportsFrameRate())
            return;

        auto rateRange = capabilities.frameRate();
        setFrameRate(value(constraint, rateRange.rangeMin().asDouble, rateRange.rangeMax().asDouble));
        break;
    }

    case MediaConstraintType::AspectRatio: {
        if (!capabilities.supportsAspectRatio())
            return;

        auto range = capabilities.aspectRatio();
        setAspectRatio(value(constraint, range.rangeMin().asDouble, range.rangeMax().asDouble));
        break;
    }

    case MediaConstraintType::Volume: {
        if (!capabilities.supportsVolume())
            return;

        auto range = capabilities.volume();
        // std::pair<T, T> valuesForRange(constraint, range.rangeMin().asDouble, range.rangeMax().asDouble)
        setVolume(value(constraint, range.rangeMin().asDouble, range.rangeMax().asDouble));
        break;
    }

    case MediaConstraintType::SampleRate: {
        if (!capabilities.supportsSampleRate())
            return;

        auto range = capabilities.sampleRate();
        setSampleRate(value(constraint, range.rangeMin().asDouble, range.rangeMax().asDouble));
        break;
    }

    case MediaConstraintType::SampleSize: {
        if (!capabilities.supportsSampleSize())
            return;

        auto range = capabilities.sampleSize();
        setSampleSize(value(constraint, range.rangeMin().asDouble, range.rangeMax().asDouble));
        break;
    }

    case MediaConstraintType::EchoCancellation:
        if (!capabilities.supportsEchoCancellation())
            return;

        bool setting;
        if (constraint.getExact(setting) || constraint.getIdeal(setting))
            setEchoCancellation(setting);
        break;

    case MediaConstraintType::FacingMode: {
        if (!capabilities.supportsFacingMode())
            return;

        auto& supportedModes = capabilities.facingMode();
        std::function<bool(MediaConstraint::ConstraintType, const String&)> filter = [supportedModes](MediaConstraint::ConstraintType, const String& modeString) {
            auto mode = RealtimeMediaSourceSettings::videoFacingModeEnum(modeString);
            for (auto& supportedMode : supportedModes) {
                if (mode == supportedMode)
                    return true;
            }
            return false;
        };

        auto modeString = constraint.find(filter);
        if (!modeString.isEmpty())
            setFacingMode(RealtimeMediaSourceSettings::videoFacingModeEnum(modeString));
        break;
    }

    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        // There is nothing to do here, neither can be changed.
        break;

    case MediaConstraintType::Unknown:
        break;
    }
}

void RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler successHandler, FailureHandler failureHandler)
{
    ASSERT(constraints.isValid());

    auto& mandatoryConstraints = constraints.mandatoryConstraints();
    for (auto& nameConstraintPair : mandatoryConstraints) {
        auto& constraint = *nameConstraintPair.value;
        if (supportsConstraint(constraint) == ConstraintSupport::Unsupported) {
            failureHandler(constraint.name(), "Constraint not supported");
            return;
        }
    }

    for (auto& nameConstraintPair : mandatoryConstraints)
        applyConstraint(*nameConstraintPair.value);

    successHandler();
}

void RealtimeMediaSource::setWidth(int width)
{
    if (width == m_size.width())
        return;

    int height = m_aspectRatio ? width / m_aspectRatio : m_size.height();
    if (!applySize(IntSize(width, height)))
        return;

    m_size.setWidth(width);
    if (m_aspectRatio)
        m_size.setHeight(width / m_aspectRatio);

    settingsDidChange();
}

void RealtimeMediaSource::setHeight(int height)
{
    if (height == m_size.height())
        return;

    int width = m_aspectRatio ? height * m_aspectRatio : m_size.width();
    if (!applySize(IntSize(width, height)))
        return;

    if (m_aspectRatio)
        m_size.setWidth(width);
    m_size.setHeight(height);

    settingsDidChange();
}

void RealtimeMediaSource::setFrameRate(double rate)
{
    if (m_frameRate == rate || !applyFrameRate(rate))
        return;

    m_frameRate = rate;
    settingsDidChange();
}

void RealtimeMediaSource::setAspectRatio(double ratio)
{
    if (m_aspectRatio == ratio || !applyAspectRatio(ratio))
        return;

    m_aspectRatio = ratio;
    m_size.setHeight(m_size.width() / ratio);
    settingsDidChange();
}

void RealtimeMediaSource::setFacingMode(RealtimeMediaSourceSettings::VideoFacingMode mode)
{
    if (m_facingMode == mode || !applyFacingMode(mode))
        return;

    m_facingMode = mode;
    settingsDidChange();
}

void RealtimeMediaSource::setVolume(double volume)
{
    if (m_volume == volume || !applyVolume(volume))
        return;

    m_volume = volume;
    settingsDidChange();
}

void RealtimeMediaSource::setSampleRate(double rate)
{
    if (m_sampleRate == rate || !applySampleRate(rate))
        return;

    m_sampleRate = rate;
    settingsDidChange();
}

void RealtimeMediaSource::setSampleSize(double size)
{
    if (m_sampleSize == size || !applySampleSize(size))
        return;

    m_sampleSize = size;
    settingsDidChange();
}

void RealtimeMediaSource::setEchoCancellation(bool echoCancellation)
{
    if (m_echoCancellation == echoCancellation || !applyEchoCancellation(echoCancellation))
        return;

    m_echoCancellation = echoCancellation;
    settingsDidChange();
}

void RealtimeMediaSource::scheduleDeferredTask(std::function<void()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = createWeakPtr(), function = WTFMove(function)] {
        if (!weakThis)
            return;

        function();
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
