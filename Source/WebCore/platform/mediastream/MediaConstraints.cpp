/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "MediaConstraints.h"

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

const String& StringConstraint::find(const Function<bool(const String&)>& filter) const
{
    for (auto& constraint : m_exact) {
        if (filter(constraint))
            return constraint;
    }

    for (auto& constraint : m_ideal) {
        if (filter(constraint))
            return constraint;
    }
    
    return emptyString();
}

double StringConstraint::fitnessDistance(const String& value) const
{
    // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints

    // 1. If the constraint is not supported by the browser, the fitness distance is 0.
    if (isEmpty())
        return 0;

    // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
    //    dictionary's value for the constraint does not satisfy the constraint, the
    //    fitness distance is positive infinity.
    if (!m_exact.isEmpty() && m_exact.find(value) == notFound)
        return std::numeric_limits<double>::infinity();

    // 3. If no ideal value is specified, the fitness distance is 0.
    if (m_ideal.isEmpty())
        return 0;

    // 5. For all string and enum non-required constraints (deviceId, groupId, facingMode,
    // echoCancellation), the fitness distance is the result of the formula
    //        (actual == ideal) ? 0 : 1
    return m_ideal.find(value) != notFound ? 0 : 1;
}

double StringConstraint::fitnessDistance(const Vector<String>& values) const
{
    if (isEmpty())
        return 0;

    double minimumDistance = std::numeric_limits<double>::infinity();
    for (auto& value : values)
        minimumDistance = std::min(minimumDistance, fitnessDistance(value));

    return minimumDistance;
}

void StringConstraint::merge(const StringConstraint& other)
{
    if (other.isEmpty())
        return;

    Vector<String> values;
    if (other.getExact(values)) {
        if (m_exact.isEmpty())
            m_exact = values;
        else {
            for (auto& value : values) {
                if (m_exact.find(value) == notFound)
                    m_exact.append(value);
            }
        }
    }

    if (other.getIdeal(values)) {
        if (m_ideal.isEmpty())
            m_ideal = values;
        else {
            for (auto& value : values) {
                if (m_ideal.find(value) == notFound)
                    m_ideal.append(value);
            }
        }
    }
}

void MediaTrackConstraintSetMap::forEach(Function<void(MediaConstraintType, const MediaConstraint&)>&& callback) const
{
    filter([callback = WTFMove(callback)] (auto type, auto& constraint) mutable {
        callback(type, constraint);
        return false;
    });
}

void MediaTrackConstraintSetMap::filter(const Function<bool(MediaConstraintType, const MediaConstraint&)>& callback) const
{
    if (m_width && !m_width->isEmpty() && callback(MediaConstraintType::Width, *m_width))
        return;
    if (m_height && !m_height->isEmpty() && callback(MediaConstraintType::Height, *m_height))
        return;
    if (m_sampleRate && !m_sampleRate->isEmpty() && callback(MediaConstraintType::SampleRate, *m_sampleRate))
        return;
    if (m_sampleSize && !m_sampleSize->isEmpty() && callback(MediaConstraintType::SampleSize, *m_sampleSize))
        return;

    if (m_aspectRatio && !m_aspectRatio->isEmpty() && callback(MediaConstraintType::AspectRatio, *m_aspectRatio))
        return;
    if (m_frameRate && !m_frameRate->isEmpty() && callback(MediaConstraintType::FrameRate, *m_frameRate))
        return;
    if (m_volume && !m_volume->isEmpty() && callback(MediaConstraintType::Volume, *m_volume))
        return;

    if (m_echoCancellation && !m_echoCancellation->isEmpty() && callback(MediaConstraintType::EchoCancellation, *m_echoCancellation))
        return;

    if (m_facingMode && !m_facingMode->isEmpty() && callback(MediaConstraintType::FacingMode, *m_facingMode))
        return;
    if (m_deviceId && !m_deviceId->isEmpty() && callback(MediaConstraintType::DeviceId, *m_deviceId))
        return;
    if (m_groupId && !m_groupId->isEmpty() && callback(MediaConstraintType::GroupId, *m_groupId))
        return;

    if (m_whiteBalanceMode && !m_whiteBalanceMode->isEmpty() && callback(MediaConstraintType::WhiteBalanceMode, *m_whiteBalanceMode))
        return;
    if (m_zoom && !m_zoom->isEmpty() && callback(MediaConstraintType::Zoom, *m_zoom))
        return;
    if (m_torch && !m_torch->isEmpty() && callback(MediaConstraintType::Torch, *m_torch))
        return;
    if (m_backgroundBlur && !m_backgroundBlur->isEmpty() && callback(MediaConstraintType::BackgroundBlur, *m_backgroundBlur))
        return;
    if (m_powerEfficient && !m_powerEfficient->isEmpty() && callback(MediaConstraintType::PowerEfficient, *m_powerEfficient))
        return;
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, std::optional<IntConstraint>&& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::Width:
        m_width = WTFMove(constraint);
        break;
    case MediaConstraintType::Height:
        m_height = WTFMove(constraint);
        break;
    case MediaConstraintType::SampleRate:
        m_sampleRate = WTFMove(constraint);
        break;
    case MediaConstraintType::SampleSize:
        m_sampleSize = WTFMove(constraint);
        break;

    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
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
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, std::optional<DoubleConstraint>&& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::AspectRatio:
        m_aspectRatio = WTFMove(constraint);
        break;
    case MediaConstraintType::FrameRate:
        m_frameRate = WTFMove(constraint);
        break;
    case MediaConstraintType::Volume:
        m_volume = WTFMove(constraint);
        break;
    case MediaConstraintType::Zoom:
        m_zoom = WTFMove(constraint);
        break;

    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Torch:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, std::optional<BooleanConstraint>&& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::EchoCancellation:
        m_echoCancellation = WTFMove(constraint);
        break;
    case MediaConstraintType::DisplaySurface:
        m_displaySurface = WTFMove(constraint);
        break;
    case MediaConstraintType::LogicalSurface:
        m_logicalSurface = WTFMove(constraint);
        break;

    case MediaConstraintType::Torch:
        m_torch = WTFMove(constraint);
        break;
    case MediaConstraintType::BackgroundBlur:
        m_backgroundBlur = WTFMove(constraint);
        break;
    case MediaConstraintType::PowerEfficient:
        m_powerEfficient = WTFMove(constraint);
        break;
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, std::optional<StringConstraint>&& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::FacingMode:
        m_facingMode = WTFMove(constraint);
        break;
    case MediaConstraintType::DeviceId:
        if (constraint)
            constraint->removeEmptyStringConstraint();
        m_deviceId = WTFMove(constraint);
        break;
    case MediaConstraintType::GroupId:
        m_groupId = WTFMove(constraint);
        break;
    case MediaConstraintType::WhiteBalanceMode:
        m_whiteBalanceMode = WTFMove(constraint);
        break;

    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::Torch:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, const MediaConstraint& constraint)
{
    switch (constraint.dataType()) {
    case MediaConstraint::DataType::Integer:
        set(constraintType, std::make_optional(downcast<const IntConstraint>(constraint)));
        return;
    case MediaConstraint::DataType::Double:
        set(constraintType, std::make_optional(downcast<const DoubleConstraint>(constraint)));
        return;
    case MediaConstraint::DataType::Boolean:
        set(constraintType, std::make_optional(downcast<const BooleanConstraint>(constraint)));
        return;
    case MediaConstraint::DataType::String:
        set(constraintType, std::make_optional(downcast<const StringConstraint>(constraint)));
        return;
    }
}

void MediaTrackConstraintSetMap::merge(MediaConstraintType constraintType, const IntConstraint& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::Width:
        if (!m_width)
            m_width = constraint;
        else
            m_width->merge(constraint);
        break;
    case MediaConstraintType::Height:
        if (!m_height)
            m_height = constraint;
        else
            m_height->merge(constraint);
        break;
    case MediaConstraintType::SampleRate:
        if (!m_sampleRate)
            m_sampleRate = constraint;
        else
            m_sampleRate->merge(constraint);
        break;
    case MediaConstraintType::SampleSize:
        if (!m_sampleSize)
            m_sampleSize = constraint;
        else
            m_sampleSize->merge(constraint);
        break;
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::Torch:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::merge(MediaConstraintType constraintType, const DoubleConstraint& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::AspectRatio:
        if (!m_aspectRatio)
            m_aspectRatio = constraint;
        else
            m_aspectRatio->merge(constraint);
        break;
    case MediaConstraintType::FrameRate:
        if (!m_frameRate)
            m_frameRate = constraint;
        else
            m_frameRate->merge(constraint);
        break;
    case MediaConstraintType::Volume:
        if (!m_volume)
            m_volume = constraint;
        else
            m_volume->merge(constraint);
        break;
    case MediaConstraintType::Zoom:
        if (!m_zoom)
            m_zoom = constraint;
        else
            m_zoom->merge(constraint);
        break;
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::Torch:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::merge(MediaConstraintType constraintType, const StringConstraint& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::FacingMode:
        if (!m_facingMode)
            m_facingMode = constraint;
        else
            m_facingMode->merge(constraint);
        break;
    case MediaConstraintType::DeviceId:
        if (!m_deviceId)
            m_deviceId = constraint;
        else
            m_deviceId->merge(constraint);
        break;
    case MediaConstraintType::GroupId:
        if (!m_groupId)
            m_groupId = constraint;
        else
            m_groupId->merge(constraint);
        break;
    case MediaConstraintType::WhiteBalanceMode:
        if (!m_whiteBalanceMode)
            m_whiteBalanceMode = constraint;
        else
            m_whiteBalanceMode->merge(constraint);
        break;
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::EchoCancellation:
    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
    case MediaConstraintType::Torch:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::BackgroundBlur:
    case MediaConstraintType::PowerEfficient:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::merge(MediaConstraintType constraintType, const BooleanConstraint& constraint)
{
    switch (constraintType) {
    case MediaConstraintType::EchoCancellation:
        if (!m_echoCancellation)
            m_echoCancellation = constraint;
        else
            m_echoCancellation->merge(constraint);
        break;
    case MediaConstraintType::DisplaySurface:
        if (!m_displaySurface)
            m_displaySurface = constraint;
        else
            m_displaySurface->merge(constraint);
        break;
    case MediaConstraintType::LogicalSurface:
        if (!m_logicalSurface)
            m_logicalSurface = constraint;
        else
            m_logicalSurface->merge(constraint);
        break;
    case MediaConstraintType::Torch:
        if (!m_torch)
            m_torch = constraint;
        else
            m_torch->merge(constraint);
        break;
    case MediaConstraintType::BackgroundBlur:
        if (!m_backgroundBlur)
            m_backgroundBlur = constraint;
        else
            m_backgroundBlur->merge(constraint);
        break;
    case MediaConstraintType::PowerEfficient:
        if (!m_powerEfficient)
            m_powerEfficient = constraint;
        else
            m_powerEfficient->merge(constraint);
        break;

    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
    case MediaConstraintType::WhiteBalanceMode:
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
    case MediaConstraintType::Zoom:
    case MediaConstraintType::FocusDistance:
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::merge(MediaConstraintType constraintType, const MediaConstraint& constraint)
{
    switch (constraint.dataType()) {
    case MediaConstraint::DataType::Integer:
        merge(constraintType, downcast<const IntConstraint>(constraint));
        return;
    case MediaConstraint::DataType::Double:
        merge(constraintType, downcast<const DoubleConstraint>(constraint));
        return;
    case MediaConstraint::DataType::Boolean:
        merge(constraintType, downcast<const BooleanConstraint>(constraint));
        return;
    case MediaConstraint::DataType::String:
        merge(constraintType, downcast<const StringConstraint>(constraint));
        return;
    }
}

size_t MediaTrackConstraintSetMap::size() const
{
    size_t count = 0;
    forEach([&count] (auto, const MediaConstraint&) mutable {
        ++count;
    });

    return count;
}

bool MediaTrackConstraintSetMap::isEmpty() const
{
    return !size();
}

bool MediaTrackConstraintSetMap::isValid() const
{
    bool isValid = true;
    if (isEmpty())
        return true;
    forEach([&isValid] (MediaConstraintType constraintType, const MediaConstraint& constraint) {
        switch (constraintType) {
        case MediaConstraintType::Width:
        case MediaConstraintType::Height:
        case MediaConstraintType::SampleRate:
        case MediaConstraintType::SampleSize:
            isValid &= (constraint.dataType() == MediaConstraint::DataType::Integer);
            break;
        case MediaConstraintType::AspectRatio:
        case MediaConstraintType::FrameRate:
        case MediaConstraintType::Volume:
        case MediaConstraintType::Zoom:
        case MediaConstraintType::FocusDistance:
            isValid &= (constraint.dataType() == MediaConstraint::DataType::Double);
            break;
        case MediaConstraintType::FacingMode:
        case MediaConstraintType::DeviceId:
        case MediaConstraintType::GroupId:
        case MediaConstraintType::WhiteBalanceMode:
            isValid &= (constraint.dataType() == MediaConstraint::DataType::String);
            break;
        case MediaConstraintType::EchoCancellation:
        case MediaConstraintType::DisplaySurface:
        case MediaConstraintType::LogicalSurface:
        case MediaConstraintType::Torch:
        case MediaConstraintType::BackgroundBlur:
            isValid &= (constraint.dataType() == MediaConstraint::DataType::Boolean);
            break;
        case MediaConstraintType::PowerEfficient:
            isValid &= (constraint.dataType() == MediaConstraint::DataType::Boolean);
            break;
        case MediaConstraintType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }
    });
    return isValid;
}

static inline void addDefaultVideoConstraints(MediaTrackConstraintSetMap& videoConstraints, bool addFrameRateConstraint, bool addWidthConstraint, bool addHeightConstraint)
{
    if (addFrameRateConstraint) {
        DoubleConstraint frameRateConstraint;
        frameRateConstraint.setIdeal(30);
        videoConstraints.set(MediaConstraintType::FrameRate, WTFMove(frameRateConstraint));
    }
    if (addWidthConstraint) {
        IntConstraint widthConstraint;
        widthConstraint.setIdeal(640);
        videoConstraints.set(MediaConstraintType::Width, WTFMove(widthConstraint));
    }
    if (addHeightConstraint) {
        IntConstraint heightConstraint;
        heightConstraint.setIdeal(480);
        videoConstraints.set(MediaConstraintType::Height, WTFMove(heightConstraint));
    }
}

bool MediaConstraints::isConstraintSet(const Function<bool(const MediaTrackConstraintSetMap&)>& callback)
{
    if (callback(mandatoryConstraints))
        return true;
    
    for (const auto& constraint : advancedConstraints) {
        if (callback(constraint))
            return true;
    }
    return false;
}

void MediaConstraints::setDefaultAudioConstraints()
{
    bool needsEchoCancellationConstraint = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.echoCancellation();
    });

    if (needsEchoCancellationConstraint) {
        BooleanConstraint echoCancellationConstraint;
        echoCancellationConstraint.setIdeal(true);
        mandatoryConstraints.set(MediaConstraintType::EchoCancellation, WTFMove(echoCancellationConstraint));
    }
}

void MediaConstraints::setDefaultVideoConstraints()
{
    // 640x480, 30fps camera
    bool needsFrameRateConstraint = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.frameRate() || !!constraint.width() || !!constraint.height();
    });
    
    bool needsWidthConstraint = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.width() || !!constraint.height();
    });
    
    bool needsHeightConstraint = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.width() || !!constraint.height() || !!constraint.aspectRatio() || !!constraint.powerEfficient();
    });

    addDefaultVideoConstraints(mandatoryConstraints, needsFrameRateConstraint, needsWidthConstraint, needsHeightConstraint);
}

void MediaConstraint::log(MediaConstraintType constraintType) const
{
    switch (dataType()) {
    case DataType::Boolean:
        downcast<const BooleanConstraint>(*this).logAsBoolean(constraintType);
        break;
    case DataType::Double:
        downcast<const DoubleConstraint>(*this).logAsDouble(constraintType);
        break;
    case DataType::Integer:
        downcast<const IntConstraint>(*this).logAsInt(constraintType);
        break;
    case DataType::String:
        WTFLogAlways("MediaConstraint %d of type %d", static_cast<int>(constraintType), static_cast<int>(dataType()));
    }
}

void BooleanConstraint::logAsBoolean(MediaConstraintType constraintType) const
{
    WTFLogAlways("BooleanConstraint %d, exact %d, ideal %d", static_cast<int>(constraintType), m_exact ? *m_exact : -1, m_ideal ? *m_ideal : -1);
}

void DoubleConstraint::logAsDouble(MediaConstraintType constraintType) const
{
    WTFLogAlways("DoubleConstraint %d, min %f, max %f, exact %f, ideal %f", static_cast<int>(constraintType), m_min ? *m_min : -1, m_max ? *m_max : -1, m_exact ? *m_exact : -1, m_ideal ? *m_ideal : -1);
}

void IntConstraint::logAsInt(MediaConstraintType constraintType) const
{
    WTFLogAlways("IntConstraint %d, min %d, max %d, exact %d, ideal %d", static_cast<int>(constraintType), m_min ? *m_min : -1, m_max ? *m_max : -1, m_exact ? *m_exact : -1, m_ideal ? *m_ideal : -1);
}

StringConstraint StringConstraint::isolatedCopy() const
{
    return StringConstraint(MediaConstraint { DataType::String }, crossThreadCopy(m_exact), crossThreadCopy(m_ideal));
}

MediaTrackConstraintSetMap MediaTrackConstraintSetMap::isolatedCopy() const
{
    return { m_width, m_height, m_sampleRate, m_sampleSize, m_aspectRatio, m_frameRate, m_volume, m_echoCancellation, m_displaySurface, m_logicalSurface, crossThreadCopy(m_facingMode), crossThreadCopy(m_deviceId), crossThreadCopy(m_groupId), crossThreadCopy(m_whiteBalanceMode), m_zoom, m_torch, m_backgroundBlur, m_powerEfficient };
}

MediaConstraints MediaConstraints::isolatedCopy() const
{
    return { crossThreadCopy(mandatoryConstraints), crossThreadCopy(advancedConstraints), isValid };
}

static bool isAllowedRequiredConstraintForDeviceSelection(MediaConstraints::DeviceType deviceType, MediaConstraintType type)
{
    // https://w3c.github.io/mediacapture-main/#dfn-allowed-required-constraints-for-device-selection
    if (type <= MediaConstraintType::GroupId)
        return true;

    if (deviceType == MediaConstraints::DeviceType::Microphone)
        return true;

    return deviceType == MediaConstraints::DeviceType::Microphone || type == MediaConstraintType::DisplaySurface || type == MediaConstraintType::LogicalSurface;
}

bool MediaConstraints::hasDisallowedRequiredConstraintForDeviceSelection(DeviceType deviceType) const
{
    bool result = false;
    mandatoryConstraints.forEach([&] (auto type, const MediaConstraint& constraint) mutable {
        result |= !isAllowedRequiredConstraintForDeviceSelection(deviceType, type) && constraint.isRequired();
    });
    return result;
}

}

#endif // ENABLE(MEDIA_STREAM)
