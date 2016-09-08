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

namespace WebCore {

Ref<MediaConstraint> MediaConstraint::create(const String& name)
{
    MediaConstraintType constraintType = RealtimeMediaSourceSupportedConstraints::constraintFromName(name);

    switch (constraintType) {
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
        return IntConstraint::create(name, constraintType);
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
        return DoubleConstraint::create(name, constraintType);
    case MediaConstraintType::EchoCancellation:
        return BooleanConstraint::create(name, constraintType);
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        return StringConstraint::create(name, constraintType);
    case MediaConstraintType::Unknown:
        return UnknownConstraint::create(name, constraintType);
    }
}

Ref<MediaConstraint> MediaConstraint::copy() const
{
    return MediaConstraint::create(name());
}

Ref<MediaConstraint> IntConstraint::copy() const
{
    auto copy = IntConstraint::create(name(), type());
    copy->m_min = m_min;
    copy->m_max = m_max;
    copy->m_exact = m_exact;
    copy->m_ideal = m_ideal;

    return copy.leakRef();
}

Ref<MediaConstraint> DoubleConstraint::copy() const
{
    auto copy = DoubleConstraint::create(name(), type());
    copy->m_min = m_min;
    copy->m_max = m_max;
    copy->m_exact = m_exact;
    copy->m_ideal = m_ideal;
    
    return copy.leakRef();
}

Ref<MediaConstraint> BooleanConstraint::copy() const
{
    auto copy = BooleanConstraint::create(name(), type());
    copy->m_exact = m_exact;
    copy->m_ideal = m_ideal;

    return copy.leakRef();
}

Ref<MediaConstraint> StringConstraint::copy() const
{
    auto copy = StringConstraint::create(name(), type());
    copy->m_exact = m_exact;
    copy->m_ideal = m_ideal;

    return copy.leakRef();
}

bool BooleanConstraint::getExact(bool& exact) const
{
    if (!m_exact)
        return false;
    
    exact = m_exact.value();
    return true;
}

bool BooleanConstraint::getIdeal(bool& ideal) const
{
    if (!m_ideal)
        return false;
    
    ideal = m_ideal.value();
    return true;
}

void StringConstraint::setExact(const String& value)
{
    m_exact.clear();
    m_exact.append(value);
}

void StringConstraint::appendExact(const String& value)
{
    m_exact.clear();
    m_exact.append(value);
}

void StringConstraint::setIdeal(const String& value)
{
    m_ideal.clear();
    m_ideal.append(value);
}

void StringConstraint::appendIdeal(const String& value)
{
    m_ideal.append(value);
}

bool StringConstraint::getExact(Vector<String>& exact) const
{
    if (!m_exact.isEmpty())
        return false;

    exact = m_exact;
    return true;
}

bool StringConstraint::getIdeal(Vector<String>& ideal) const
{
    if (!m_ideal.isEmpty())
        return false;

    ideal = m_ideal;
    return true;
}

const String& StringConstraint::find(std::function<bool(ConstraintType, const String&)> filter) const
{
    for (auto& constraint : m_exact) {
        if (filter(ConstraintType::ExactConstraint, constraint))
            return constraint;
    }

    for (auto& constraint : m_ideal) {
        if (filter(ConstraintType::IdealConstraint, constraint))
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
    if (m_exact.isEmpty())
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

void FlattenedConstraint::set(const MediaConstraint& constraint)
{
    for (auto existingConstraint : m_constraints) {
        if (existingConstraint->type() == constraint.type()) {
            existingConstraint = constraint.copy();
            return;
        }
    }

    m_constraints.append(constraint.copy());
}

void FlattenedConstraint::merge(const MediaConstraint& constraint)
{
    for (auto existingConstraint : m_constraints) {
        if (existingConstraint->type() == constraint.type()) {
            existingConstraint->merge(constraint);
            return;
        }
    }

    m_constraints.append(constraint.copy());
}

}

#endif // ENABLE(MEDIA_STREAM)
