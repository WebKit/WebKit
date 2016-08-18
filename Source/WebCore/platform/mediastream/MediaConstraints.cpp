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

RefPtr<MediaConstraint> MediaConstraint::create(const String& name)
{
    MediaConstraintType constraintType = RealtimeMediaSourceSupportedConstraints::constraintFromName(name);

    switch (constraintType) {
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
        return IntConstraint::create(constraintType);
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
        return DoubleConstraint::create(constraintType);
    case MediaConstraintType::EchoCancellation:
        return BooleanConstraint::create(constraintType);
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        return StringConstraint::create(constraintType);
    case MediaConstraintType::Unknown:
        return nullptr;
    }
}

bool MediaConstraint::getMin(int&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMax(int&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getExact(int&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getIdeal(int&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMin(double&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMax(double&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getExact(double&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getIdeal(double&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMin(bool&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMax(bool&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getExact(bool&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getIdeal(bool&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMin(Vector<String>&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getMax(Vector<String>&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getExact(Vector<String>&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool MediaConstraint::getIdeal(Vector<String>&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

Ref<IntConstraint> IntConstraint::create(MediaConstraintType type)
{
    return adoptRef(*new IntConstraint(type));
}

void IntConstraint::setMin(int value)
{
    m_min = value;
    setHasMin(true);
}

void IntConstraint::setMax(int value)
{
    m_max = value;
    setHasMax(true);
}

void IntConstraint::setExact(int value)
{
    m_exact = value;
    setHasExact(true);
}

void IntConstraint::setIdeal(int value)
{
    m_ideal = value;
    setHasIdeal(true);
}

bool IntConstraint::getMin(int& min) const
{
    if (!hasMin())
        return false;

    min = m_min;
    return true;
}

bool IntConstraint::getMax(int& max) const
{
    if (!hasMax())
        return false;

    max = m_max;
    return true;
}

bool IntConstraint::getExact(int& exact) const
{
    if (!hasExact())
        return false;

    exact = m_exact;
    return true;
}

bool IntConstraint::getIdeal(int& ideal) const
{
    if (!hasIdeal())
        return false;

    ideal = m_ideal;
    return true;
}

Ref<DoubleConstraint> DoubleConstraint::create(MediaConstraintType type)
{
    return adoptRef(*new DoubleConstraint(type));
}

void DoubleConstraint::setMin(double value)
{
    m_min = value;
    setHasMin(true);
}

void DoubleConstraint::setMax(double value)
{
    m_max = value;
    setHasMax(true);
}

void DoubleConstraint::setExact(double value)
{
    m_exact = value;
    setHasExact(true);
}

void DoubleConstraint::setIdeal(double value)
{
    m_ideal = value;
    setHasIdeal(true);
}

bool DoubleConstraint::getMin(double& min) const
{
    if (!hasMin())
        return false;
    
    min = m_min;
    return true;
}

bool DoubleConstraint::getMax(double& max) const
{
    if (!hasMax())
        return false;
    
    max = m_max;
    return true;
}

bool DoubleConstraint::getExact(double& exact) const
{
    if (!hasExact())
        return false;
    
    exact = m_exact;
    return true;
}

bool DoubleConstraint::getIdeal(double& ideal) const
{
    if (!hasIdeal())
        return false;
    
    ideal = m_ideal;
    return true;
}

Ref<BooleanConstraint> BooleanConstraint::create(MediaConstraintType type)
{
    return adoptRef(*new BooleanConstraint(type));
}

void BooleanConstraint::setExact(bool value)
{
    m_exact = value;
    m_hasExact = true;
}

void BooleanConstraint::setIdeal(bool value)
{
    m_ideal = value;
    m_hasIdeal = true;
}

bool BooleanConstraint::getExact(bool& exact) const
{
    if (!m_hasExact)
        return false;
    
    exact = m_exact;
    return true;
}

bool BooleanConstraint::getIdeal(bool& ideal) const
{
    if (!m_hasIdeal)
        return false;
    
    ideal = m_ideal;
    return true;
}

Ref<StringConstraint> StringConstraint::create(MediaConstraintType type)
{
    return adoptRef(*new StringConstraint(type));
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

}

#endif // ENABLE(MEDIA_STREAM)
