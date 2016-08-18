/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaConstraintsMock.h"

#include "MediaConstraints.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/text/StringHash.h>

namespace WebCore {

static bool isIntMediaConstraintSatisfiable(const MediaConstraint& constraint)
{
    int ceiling = 10;
    int floor = 0;

    int min = floor;
    if (constraint.getMin(min) && min > ceiling)
        return false;

    int max = ceiling;
    if (constraint.getMax(max) && max < min)
        return false;

    int exact;
    if (constraint.getExact(exact) && (exact < min || exact > max))
        return false;

    return true;
}

static bool isDoubleMediaConstraintSatisfiable(const MediaConstraint& constraint)
{
    double ceiling = 10;
    double floor = 0;

    double min = floor;
    if (constraint.getMin(min) && min > ceiling)
        return false;

    double max = ceiling;
    if (constraint.getMax(max) && max < min)
        return false;

    double exact;
    if (constraint.getExact(exact) && (exact < min || exact > max))
        return false;

    return true;
}

static bool isBooleanMediaConstraintSatisfiable(const MediaConstraint& constraint)
{
    bool exact;
    if (constraint.getExact(exact))
        return exact;

    return true;
}

static bool isStringMediaConstraintSatisfiable(const MediaConstraint& constraint)
{
    Vector<String> exact;
    if (constraint.getExact(exact)) {
        for (auto& constraintValue : exact) {
            if (constraintValue.find("invalid") != notFound)
                return false;
        }
    }
    
    return true;
}

static bool isSatisfiable(RealtimeMediaSource::Type type, const MediaConstraint& constraint)
{
    MediaConstraintType constraintType = constraint.type();

    if (type == RealtimeMediaSource::Audio) {
        if (constraintType == MediaConstraintType::SampleRate || constraintType == MediaConstraintType::SampleSize)
            return isIntMediaConstraintSatisfiable(constraint);
        if (constraintType == MediaConstraintType::Volume)
            return isDoubleMediaConstraintSatisfiable(constraint);
        if (constraintType == MediaConstraintType::EchoCancellation)
            return isBooleanMediaConstraintSatisfiable(constraint);
        if (constraintType == MediaConstraintType::DeviceId || constraintType == MediaConstraintType::GroupId)
            return isStringMediaConstraintSatisfiable(constraint);
    } else if (type == RealtimeMediaSource::Video) {
        if (constraintType == MediaConstraintType::Width || constraintType == MediaConstraintType::Height)
            return isIntMediaConstraintSatisfiable(constraint);
        if (constraintType == MediaConstraintType::AspectRatio || constraintType == MediaConstraintType::FrameRate)
            return isDoubleMediaConstraintSatisfiable(constraint);
        if (constraintType == MediaConstraintType::FacingMode || constraintType == MediaConstraintType::DeviceId || constraintType == MediaConstraintType::GroupId)
            return isStringMediaConstraintSatisfiable(constraint);
    }

    return false;
}

const String& MediaConstraintsMock::verifyConstraints(RealtimeMediaSource::Type type, const MediaConstraints& constraints)
{
    auto& mandatoryConstraints = constraints.mandatoryConstraints();
    for (auto& nameConstraintPair : mandatoryConstraints) {
        if (!isSatisfiable(type, *nameConstraintPair.value))
            return nameConstraintPair.key;
    }

    return emptyString();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
