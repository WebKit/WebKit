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

double RealtimeMediaSource::fitnessDistance(const MediaConstraint& constraint)
{
    RealtimeMediaSourceCapabilities& capabilities = *this->capabilities();

    switch (constraint.type()) {
    case MediaConstraintType::Width: {
        if (!capabilities.supportsWidth())
            return 0;

        auto range = capabilities.width();
        return constraint.fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::Height: {
        if (!capabilities.supportsHeight())
            return 0;

        auto range = capabilities.height();
        return constraint.fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::FrameRate: {
        if (!capabilities.supportsFrameRate())
            return 0;

        auto range = capabilities.frameRate();
        return constraint.fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::AspectRatio: {
        if (!capabilities.supportsAspectRatio())
            return 0;

        auto range = capabilities.aspectRatio();
        return constraint.fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::Volume: {
        if (!capabilities.supportsVolume())
            return 0;

        auto range = capabilities.volume();
        return constraint.fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::SampleRate: {
        if (!capabilities.supportsSampleRate())
            return 0;

        auto range = capabilities.sampleRate();
        return constraint.fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::SampleSize: {
        if (!capabilities.supportsSampleSize())
            return 0;

        auto range = capabilities.sampleSize();
        return constraint.fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::FacingMode: {
        if (!capabilities.supportsFacingMode())
            return 0;

        auto& modes = capabilities.facingMode();
        Vector<String> supportedModes;
        supportedModes.reserveInitialCapacity(modes.size());
        for (auto& mode : modes)
            supportedModes.append(RealtimeMediaSourceSettings::facingMode(mode));
        return constraint.fitnessDistance(supportedModes);
        break;
    }

    case MediaConstraintType::EchoCancellation: {
        if (!capabilities.supportsEchoCancellation())
            return 0;

        bool echoCancellationReadWrite = capabilities.echoCancellation() == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite;
        return constraint.fitnessDistance(echoCancellationReadWrite);
        break;
    }

    case MediaConstraintType::DeviceId: {
        if (!capabilities.supportsDeviceId())
            return 0;

        return constraint.fitnessDistance(m_id);
        break;
    }

    case MediaConstraintType::GroupId: {
        if (!capabilities.supportsDeviceId())
            return 0;

        return constraint.fitnessDistance(settings().groupId());
        break;
    }

    case MediaConstraintType::Unknown:
        // Unknown (or unsupported) constraints should be ignored.
        break;
    }

    return 0;
}

template <typename ValueType>
static void applyNumericConstraint(const MediaConstraint& constraint, ValueType current, ValueType capabilityMin, ValueType capabilityMax, RealtimeMediaSource* source, void (RealtimeMediaSource::*function)(ValueType))
{
    ValueType value;
    ValueType min = capabilityMin;
    ValueType max = capabilityMax;

    if (constraint.getExact(value)) {
        ASSERT(constraint.validForRange(capabilityMin, capabilityMax));
        (source->*function)(value);
        return;
    }

    if (constraint.getMin(value)) {
        ASSERT(constraint.validForRange(value, capabilityMax));
        if (value > min)
            min = value;

        // If there is no ideal, don't change if minimum is smaller than current.
        ValueType ideal;
        if (!constraint.getIdeal(ideal) && value < current)
            value = current;
    }

    if (constraint.getMax(value)) {
        ASSERT(constraint.validForRange(capabilityMin, value));
        if (value < max)
            max = value;
    }

    if (constraint.getIdeal(value)) {
        if (value < min)
            value = min;
        if (value > max)
            value = max;
    }

    if (value != current)
        (source->*function)(value);
}

void RealtimeMediaSource::applyConstraint(const MediaConstraint& constraint)
{
    RealtimeMediaSourceCapabilities& capabilities = *this->capabilities();
    switch (constraint.type()) {
    case MediaConstraintType::Width: {
        if (!capabilities.supportsWidth())
            return;

        auto range = capabilities.width();
        applyNumericConstraint(constraint, size().width(), range.rangeMin().asInt, range.rangeMax().asInt, this, &RealtimeMediaSource::setWidth);
        break;
    }

    case MediaConstraintType::Height: {
        if (!capabilities.supportsHeight())
            return;

        auto range = capabilities.height();
        applyNumericConstraint(constraint, size().height(), range.rangeMin().asInt, range.rangeMax().asInt, this, &RealtimeMediaSource::setHeight);
        break;
    }

    case MediaConstraintType::FrameRate: {
        if (!capabilities.supportsFrameRate())
            return;

        auto range = capabilities.frameRate();
        applyNumericConstraint(constraint, frameRate(), range.rangeMin().asDouble, range.rangeMax().asDouble, this, &RealtimeMediaSource::setFrameRate);
        break;
    }

    case MediaConstraintType::AspectRatio: {
        if (!capabilities.supportsAspectRatio())
            return;

        auto range = capabilities.aspectRatio();
        applyNumericConstraint(constraint, aspectRatio(), range.rangeMin().asDouble, range.rangeMax().asDouble, this, &RealtimeMediaSource::setAspectRatio);
        break;
    }

    case MediaConstraintType::Volume: {
        if (!capabilities.supportsVolume())
            return;

        auto range = capabilities.volume();
        applyNumericConstraint(constraint, volume(), range.rangeMin().asDouble, range.rangeMax().asDouble, this, &RealtimeMediaSource::setVolume);
        break;
    }

    case MediaConstraintType::SampleRate: {
        if (!capabilities.supportsSampleRate())
            return;

        auto range = capabilities.sampleRate();
        applyNumericConstraint(constraint, sampleRate(), range.rangeMin().asDouble, range.rangeMax().asDouble, this, &RealtimeMediaSource::setSampleRate);
        break;
    }

    case MediaConstraintType::SampleSize: {
        if (!capabilities.supportsSampleSize())
            return;

        auto range = capabilities.sampleSize();
        applyNumericConstraint(constraint, sampleSize(), range.rangeMin().asDouble, range.rangeMax().asDouble, this, &RealtimeMediaSource::setSampleSize);
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

bool RealtimeMediaSource::selectSettings(const MediaConstraints& constraints, FlattenedConstraint& candidates, String& failedConstraint)
{
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
    auto& mandatoryConstraints = constraints.mandatoryConstraints();
    for (auto& nameConstraintPair : mandatoryConstraints) {
        const auto& constraint = nameConstraintPair.value;
        if (fitnessDistance(*constraint) == std::numeric_limits<double>::infinity()) {
            failedConstraint = constraint->name();
            return false;
        }

        candidates.set(*constraint);
    }

    // 4. If candidates is empty, return undefined as the result of the SelectSettings() algorithm.
    if (candidates.isEmpty())
        return true;

    // 5. Iterate over the 'advanced' ConstraintSets in newConstraints in the order in which they were specified.
    //    For each ConstraintSet:

    // 5.1 compute the fitness distance between it and each settings dictionary in candidates, treating bare
    //     values of properties as exact.
    Vector<std::pair<double, MediaTrackConstraintSetMap>> supportedConstraints;
    double minimumDistance = std::numeric_limits<double>::infinity();
    for (const auto& advancedConstraint : constraints.advancedConstraints()) {
        double constraintDistance = 0;
        bool supported = false;
        for (auto& nameConstraintPair : advancedConstraint) {
            double distance = fitnessDistance(*nameConstraintPair.value);
            constraintDistance += distance;
            if (distance != std::numeric_limits<double>::infinity())
                supported = true;
        }

        if (constraintDistance < minimumDistance)
            minimumDistance = constraintDistance;

        // 5.2 If the fitness distance is finite for one or more settings dictionaries in candidates, keep those
        //     settings dictionaries in candidates, discarding others.
        //     If the fitness distance is infinite for all settings dictionaries in candidates, ignore this ConstraintSet.
        if (supported)
            supportedConstraints.append({constraintDistance, advancedConstraint});
    }

    // 6. Select one settings dictionary from candidates, and return it as the result of the SelectSettings() algorithm.
    //    The UA should use the one with the smallest fitness distance, as calculated in step 3.
    if (minimumDistance != std::numeric_limits<double>::infinity()) {
        supportedConstraints.removeAllMatching( [&] (std::pair<double, MediaTrackConstraintSetMap> pair) -> bool {
            return pair.first > minimumDistance;
        });

        if (!supportedConstraints.isEmpty()) {
            auto& advancedConstraint = supportedConstraints[0].second;
            for (auto& nameConstraintPair : advancedConstraint)
                candidates.merge(*nameConstraintPair.value);
        }
    }

    return true;
}


void RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler successHandler, FailureHandler failureHandler)
{
    ASSERT(constraints.isValid());

    FlattenedConstraint candidates;

    String failedConstraint;
    if (!selectSettings(constraints, candidates, failedConstraint)) {
        failureHandler(failedConstraint, "Constraint not supported");
        return;
    }

    for (auto constraint : candidates)
        applyConstraint(*constraint);

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
