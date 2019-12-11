/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaTrackConstraints.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaConstraints.h"

namespace WebCore {

enum class ConstraintSetType { Mandatory, Advanced };

static void set(MediaTrackConstraintSetMap& map, ConstraintSetType setType, const char* typeAsString, MediaConstraintType type, const ConstrainLong& value)
{
    IntConstraint constraint(typeAsString, type);
    WTF::switchOn(value,
        [&] (int integer) {
            if (setType == ConstraintSetType::Mandatory)
                constraint.setIdeal(integer);
            else
                constraint.setExact(integer);
        },
        [&] (const ConstrainLongRange& range) {
            if (range.min)
                constraint.setMin(range.min.value());
            if (range.max)
                constraint.setMax(range.max.value());
            if (range.exact)
                constraint.setExact(range.exact.value());
            if (range.ideal)
                constraint.setIdeal(range.ideal.value());
        }
    );
    map.set(type, WTFMove(constraint));
}

static void set(MediaTrackConstraintSetMap& map, ConstraintSetType setType, const char* typeAsString, MediaConstraintType type, const ConstrainDouble& value)
{
    DoubleConstraint constraint(typeAsString, type);
    WTF::switchOn(value,
        [&] (double number) {
            if (setType == ConstraintSetType::Mandatory)
                constraint.setIdeal(number);
            else
                constraint.setExact(number);
        },
        [&] (const ConstrainDoubleRange& range) {
            if (range.min)
                constraint.setMin(range.min.value());
            if (range.max)
                constraint.setMax(range.max.value());
            if (range.exact)
                constraint.setExact(range.exact.value());
            if (range.ideal)
                constraint.setIdeal(range.ideal.value());
        }
    );
    map.set(type, WTFMove(constraint));
}

static void set(MediaTrackConstraintSetMap& map, ConstraintSetType setType, const char* typeAsString, MediaConstraintType type, const ConstrainBoolean& value)
{
    BooleanConstraint constraint(typeAsString, type);
    WTF::switchOn(value,
        [&] (bool boolean) {
            if (setType == ConstraintSetType::Mandatory)
                constraint.setIdeal(boolean);
            else
                constraint.setExact(boolean);
        },
        [&] (const ConstrainBooleanParameters& parameters) {
            if (parameters.exact)
                constraint.setExact(parameters.exact.value());
            if (parameters.ideal)
                constraint.setIdeal(parameters.ideal.value());
        }
    );
    map.set(type, WTFMove(constraint));
}

static void set(MediaTrackConstraintSetMap& map, ConstraintSetType setType, const char* typeAsString, MediaConstraintType type, const ConstrainDOMString& value)
{
    StringConstraint constraint(typeAsString, type);
    WTF::switchOn(value,
        [&] (const String& string) {
            if (setType == ConstraintSetType::Mandatory)
                constraint.appendIdeal(string);
            else
                constraint.appendExact(string);
        },
        [&] (const Vector<String>& vector) {
            if (setType == ConstraintSetType::Mandatory) {
                for (auto& string : vector)
                    constraint.appendIdeal(string);
            } else {
                for (auto& string : vector)
                    constraint.appendExact(string);
            }
        },
        [&] (const ConstrainDOMStringParameters& parameters) {
            if (parameters.exact) {
                WTF::switchOn(parameters.exact.value(),
                    [&] (const String& string) {
                        constraint.appendExact(string);
                    },
                    [&] (const Vector<String>& vector) {
                        for (auto& string : vector)
                            constraint.appendExact(string);
                    }
                );
            }
            if (parameters.ideal) {
                WTF::switchOn(parameters.ideal.value(),
                    [&] (const String& string) {
                        constraint.appendIdeal(string);
                    },
                    [&] (const Vector<String>& vector) {
                        for (auto& string : vector)
                            constraint.appendIdeal(string);
                    }
                );
            }
        }
    );
    map.set(type, WTFMove(constraint));
}

template<typename T> static inline void set(MediaTrackConstraintSetMap& map, ConstraintSetType setType, const char* typeAsString, MediaConstraintType type, const Optional<T>& value)
{
    if (!value)
        return;
    set(map, setType, typeAsString, type, value.value());
}

static MediaTrackConstraintSetMap convertToInternalForm(ConstraintSetType setType, const MediaTrackConstraintSet& constraintSet)
{
    MediaTrackConstraintSetMap result;
    set(result, setType, "width", MediaConstraintType::Width, constraintSet.width);
    set(result, setType, "height", MediaConstraintType::Height, constraintSet.height);
    set(result, setType, "aspectRatio", MediaConstraintType::AspectRatio, constraintSet.aspectRatio);
    set(result, setType, "frameRate", MediaConstraintType::FrameRate, constraintSet.frameRate);
    set(result, setType, "facingMode", MediaConstraintType::FacingMode, constraintSet.facingMode);
    set(result, setType, "volume", MediaConstraintType::Volume, constraintSet.volume);
    set(result, setType, "sampleRate", MediaConstraintType::SampleRate, constraintSet.sampleRate);
    set(result, setType, "sampleSize", MediaConstraintType::SampleSize, constraintSet.sampleSize);
    set(result, setType, "echoCancellation", MediaConstraintType::EchoCancellation, constraintSet.echoCancellation);
    // FIXME: add latency
    // FIXME: add channelCount
    set(result, setType, "deviceId", MediaConstraintType::DeviceId, constraintSet.deviceId);
    set(result, setType, "groupId", MediaConstraintType::GroupId, constraintSet.groupId);
    set(result, setType, "displaySurface", MediaConstraintType::DisplaySurface, constraintSet.displaySurface);
    set(result, setType, "logicalSurface", MediaConstraintType::LogicalSurface, constraintSet.logicalSurface);
    return result;
}

static Vector<MediaTrackConstraintSetMap> convertAdvancedToInternalForm(const Vector<MediaTrackConstraintSet>& vector)
{
    Vector<MediaTrackConstraintSetMap> result;
    result.reserveInitialCapacity(vector.size());
    for (auto& set : vector)
        result.uncheckedAppend(convertToInternalForm(ConstraintSetType::Advanced, set));
    return result;
}

static Vector<MediaTrackConstraintSetMap> convertAdvancedToInternalForm(const Optional<Vector<MediaTrackConstraintSet>>& optionalVector)
{
    if (!optionalVector)
        return { };
    return convertAdvancedToInternalForm(optionalVector.value());
}

MediaConstraints createMediaConstraints(const MediaTrackConstraints& trackConstraints)
{
    MediaConstraints constraints;
    constraints.mandatoryConstraints = convertToInternalForm(ConstraintSetType::Mandatory, trackConstraints);
    constraints.advancedConstraints = convertAdvancedToInternalForm(trackConstraints.advanced);
    constraints.isValid = true;
    return constraints;
}

}

#endif
