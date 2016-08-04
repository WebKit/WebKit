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

#include "Dictionary.h"
#include "Logging.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSupportedConstraints.h"

namespace WebCore {

RefPtr<BaseConstraint> BaseConstraint::create(const String& name, const Dictionary& dictionary)
{
    RefPtr<BaseConstraint> constraint = createEmptyDerivedConstraint(name);
    if (!constraint)
        return nullptr;

    constraint->initializeWithDictionary(dictionary);
    return constraint;
}

RefPtr<BaseConstraint> BaseConstraint::createEmptyDerivedConstraint(const String& name)
{
    const RealtimeMediaSourceSupportedConstraints& supportedConstraints = RealtimeMediaSourceCenter::singleton().supportedConstraints();
    MediaConstraintType constraintType = supportedConstraints.constraintFromName(name);

    switch (constraintType) {
    case MediaConstraintType::Width:
    case MediaConstraintType::Height:
    case MediaConstraintType::SampleRate:
    case MediaConstraintType::SampleSize:
        return LongConstraint::create(name);
    case MediaConstraintType::AspectRatio:
    case MediaConstraintType::FrameRate:
    case MediaConstraintType::Volume:
        return DoubleConstraint::create(name);
    case MediaConstraintType::EchoCancellation:
        return BooleanConstraint::create(name);
    case MediaConstraintType::FacingMode:
    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        return StringConstraint::create(name);
    case MediaConstraintType::Unknown:
        return nullptr;
    }
}

Ref<LongConstraint> LongConstraint::create(const String& name)
{
    return adoptRef(*new LongConstraint(name));
}

void LongConstraint::setMin(long value)
{
    m_min = value;
    setHasMin(true);
}

void LongConstraint::setMax(long value)
{
    m_max = value;
    setHasMax(true);
}

void LongConstraint::setExact(long value)
{
    m_exact = value;
    setHasExact(true);
}

void LongConstraint::setIdeal(long value)
{
    m_ideal = value;
    setHasIdeal(true);
}

void LongConstraint::initializeWithDictionary(const Dictionary& dictionaryConstraintValue)
{

    long minValue;
    if (dictionaryConstraintValue.get("min", minValue))
        setMin(minValue);

    long maxValue;
    if (dictionaryConstraintValue.get("max", maxValue))
        setMax(maxValue);

    long exactValue;
    if (dictionaryConstraintValue.get("exact", exactValue))
        setExact(exactValue);

    long idealValue;
    if (dictionaryConstraintValue.get("ideal", idealValue))
        setIdeal(idealValue);

    if (isEmpty())
        LOG(Media, "LongConstraint::initializeWithDictionary(%p) - ignoring constraint since it has no valid or supported key/value pairs.", this);
}

Ref<DoubleConstraint> DoubleConstraint::create(const String& name)
{
    return adoptRef(*new DoubleConstraint(name));
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

void DoubleConstraint::initializeWithDictionary(const Dictionary& dictionaryConstraintValue)
{
    double minValue;
    if (dictionaryConstraintValue.get("min", minValue))
        setMin(minValue);

    double maxValue;
    if (dictionaryConstraintValue.get("max", maxValue))
        setMax(maxValue);

    double exactValue;
    if (dictionaryConstraintValue.get("exact", exactValue))
        setExact(exactValue);

    double idealValue;
    if (dictionaryConstraintValue.get("ideal", idealValue))
        setIdeal(idealValue);

    if (isEmpty())
        LOG(Media, "DoubleConstraints::initializeWithDictionary(%p) - ignoring constraint since it has no valid or supported key/value pairs.", this);
}

Ref<BooleanConstraint> BooleanConstraint::create(const String& name)
{
    return adoptRef(*new BooleanConstraint(name));
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

void BooleanConstraint::initializeWithDictionary(const Dictionary& dictionaryConstraintValue)
{
    bool exactValue;
    if (dictionaryConstraintValue.get("exact", exactValue))
        setExact(exactValue);

    bool idealValue;
    if (dictionaryConstraintValue.get("ideal", idealValue))
        setIdeal(idealValue);

    if (isEmpty())
        LOG(Media, "BooleanConstraint::initializeWithDictionary(%p) - ignoring constraint since it has no valid or supported key/value pairs.", this);
}

Ref<StringConstraint> StringConstraint::create(const String& name)
{
    return adoptRef(*new StringConstraint(name));
}

void StringConstraint::setExact(const String& value)
{
    m_exact = value;
}

void StringConstraint::setIdeal(const String& value)
{
    m_ideal = value;
}

void StringConstraint::initializeWithDictionary(const Dictionary& dictionaryConstraintValue)
{
    String exactValue;
    if (dictionaryConstraintValue.get("exact", exactValue) && !exactValue.isNull() && !exactValue.isEmpty())
        setExact(exactValue);

    String idealValue;
    if (dictionaryConstraintValue.get("ideal", idealValue) && !idealValue.isNull() && !idealValue.isEmpty())
        setIdeal(idealValue);

    if (isEmpty())
        LOG(Media, "StringConstraint::initializeWithDictionary(%p) - ignoring constraint since it has no valid or supported key/value pairs.", this);
}

}
