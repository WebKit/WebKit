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

}

#endif // ENABLE(MEDIA_STREAM)
