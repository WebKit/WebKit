/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>

namespace WebCore {

const String& StringConstraint::find(const WTF::Function<bool(const String&)>& filter) const
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

void StringConstraint::merge(const MediaConstraint& other)
{
    ASSERT(other.isString());
    const StringConstraint& typedOther = downcast<StringConstraint>(other);

    if (typedOther.isEmpty())
        return;

    Vector<String> values;
    if (typedOther.getExact(values)) {
        if (m_exact.isEmpty())
            m_exact = values;
        else {
            for (auto& value : values) {
                if (m_exact.find(value) == notFound)
                    m_exact.append(value);
            }
        }
    }

    if (typedOther.getIdeal(values)) {
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

void FlattenedConstraint::set(const MediaConstraint& constraint)
{
    if (find(constraint.constraintType()))
        return;

    append(constraint);
}

void FlattenedConstraint::merge(const MediaConstraint& constraint)
{
    for (auto& variant : *this) {
        if (variant.constraintType() != constraint.constraintType())
            continue;

        switch (variant.dataType()) {
        case MediaConstraint::DataType::Integer:
            ASSERT(constraint.isInt());
            downcast<const IntConstraint>(variant).merge(downcast<const IntConstraint>(constraint));
            return;
        case MediaConstraint::DataType::Double:
            ASSERT(constraint.isDouble());
            downcast<const DoubleConstraint>(variant).merge(downcast<const DoubleConstraint>(constraint));
            return;
        case MediaConstraint::DataType::Boolean:
            ASSERT(constraint.isBoolean());
            downcast<const BooleanConstraint>(variant).merge(downcast<const BooleanConstraint>(constraint));
            return;
        case MediaConstraint::DataType::String:
            ASSERT(constraint.isString());
            downcast<const StringConstraint>(variant).merge(downcast<const StringConstraint>(constraint));
            return;
        case MediaConstraint::DataType::None:
            ASSERT_NOT_REACHED();
            return;
        }
    }

    append(constraint);
}

void FlattenedConstraint::append(const MediaConstraint& constraint)
{
#ifndef NDEBUG
    ++m_generation;
#endif

    m_variants.append(ConstraintHolder::create(constraint));
}

const MediaConstraint* FlattenedConstraint::find(MediaConstraintType type) const
{
    for (auto& variant : m_variants) {
        if (variant.constraintType() == type)
            return &variant.constraint();
    }

    return nullptr;
}

void MediaTrackConstraintSetMap::forEach(WTF::Function<void(const MediaConstraint&)>&& callback) const
{
    filter([callback = WTFMove(callback)] (const MediaConstraint& constraint) mutable {
        callback(constraint);
        return false;
    });
}

void MediaTrackConstraintSetMap::filter(const WTF::Function<bool(const MediaConstraint&)>& callback) const
{
    if (m_width && !m_width->isEmpty() && callback(*m_width))
        return;
    if (m_height && !m_height->isEmpty() && callback(*m_height))
        return;
    if (m_sampleRate && !m_sampleRate->isEmpty() && callback(*m_sampleRate))
        return;
    if (m_sampleSize && !m_sampleSize->isEmpty() && callback(*m_sampleSize))
        return;

    if (m_aspectRatio && !m_aspectRatio->isEmpty() && callback(*m_aspectRatio))
        return;
    if (m_frameRate && !m_frameRate->isEmpty() && callback(*m_frameRate))
        return;
    if (m_volume && !m_volume->isEmpty() && callback(*m_volume))
        return;

    if (m_echoCancellation && !m_echoCancellation->isEmpty() && callback(*m_echoCancellation))
        return;

    if (m_facingMode && !m_facingMode->isEmpty() && callback(*m_facingMode))
        return;
    if (m_deviceId && !m_deviceId->isEmpty() && callback(*m_deviceId))
        return;
    if (m_groupId && !m_groupId->isEmpty() && callback(*m_groupId))
        return;
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, Optional<IntConstraint>&& constraint)
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
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, Optional<DoubleConstraint>&& constraint)
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
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, Optional<BooleanConstraint>&& constraint)
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
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaTrackConstraintSetMap::set(MediaConstraintType constraintType, Optional<StringConstraint>&& constraint)
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
    case MediaConstraintType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

size_t MediaTrackConstraintSetMap::size() const
{
    size_t count = 0;
    forEach([&count] (const MediaConstraint&) mutable {
        ++count;
    });

    return count;
}

bool MediaTrackConstraintSetMap::isEmpty() const
{
    return !size();
}

static inline void addDefaultVideoConstraints(MediaTrackConstraintSetMap& videoConstraints, bool addFrameRateConstraint, bool addSizeConstraint, bool addFacingModeConstraint)
{
    if (addFrameRateConstraint) {
        DoubleConstraint frameRateConstraint({ }, MediaConstraintType::FrameRate);
        frameRateConstraint.setIdeal(30);
        videoConstraints.set(MediaConstraintType::FrameRate, WTFMove(frameRateConstraint));
    }
    if (addSizeConstraint) {
        IntConstraint widthConstraint({ }, MediaConstraintType::Width);
        widthConstraint.setIdeal(640);
        videoConstraints.set(MediaConstraintType::Width, WTFMove(widthConstraint));
        
        IntConstraint heightConstraint({ }, MediaConstraintType::Height);
        heightConstraint.setIdeal(480);
        videoConstraints.set(MediaConstraintType::Height, WTFMove(heightConstraint));
    }
    if (addFacingModeConstraint) {
        StringConstraint facingModeConstraint({ }, MediaConstraintType::FacingMode);
        facingModeConstraint.setIdeal("user"_s);
        videoConstraints.set(MediaConstraintType::FacingMode, WTFMove(facingModeConstraint));
    }
}

bool MediaConstraints::isConstraintSet(const WTF::Function<bool(const MediaTrackConstraintSetMap&)>& callback)
{
    if (callback(mandatoryConstraints))
        return true;
    
    for (const auto& constraint : advancedConstraints) {
        if (callback(constraint))
            return true;
    }
    return false;
}

void MediaConstraints::setDefaultVideoConstraints()
{
    // 640x480, 30fps, front-facing camera
    bool needsFrameRateConstraints = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.frameRate() || !!constraint.width() || !!constraint.height();
    });
    
    bool needsSizeConstraints = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.width() || !!constraint.height();
    });
    
    bool needsFacingModeConstraints = !isConstraintSet([](const MediaTrackConstraintSetMap& constraint) {
        return !!constraint.facingMode() || !!constraint.deviceId();
    });
    
    addDefaultVideoConstraints(mandatoryConstraints, needsFrameRateConstraints, needsSizeConstraints, needsFacingModeConstraints);
}
    
}

#endif // ENABLE(MEDIA_STREAM)
