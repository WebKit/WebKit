/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSMediaDevices.h"

#if ENABLE(MEDIA_STREAM)

#include "ArrayValue.h"
#include "Dictionary.h"
#include "ExceptionCode.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MediaConstraintsImpl.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

using namespace JSC;

namespace WebCore {

enum class ConstraintSetType { Mandatory, Advanced };

static void initializeStringConstraintWithList(StringConstraint& constraint, void (StringConstraint::*appendValue)(const String&), const ArrayValue& list)
{
    size_t size;
    if (!list.length(size))
        return;

    for (size_t i = 0; i < size; ++i) {
        String value;
        if (list.get(i, value))
            (constraint.*appendValue)(value);
    }
}

static RefPtr<StringConstraint> createStringConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = StringConstraint::create(type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        ArrayValue exactArrayValue;
        if (dictionaryValue.get("exact", exactArrayValue) && !exactArrayValue.isUndefinedOrNull())
            initializeStringConstraintWithList(constraint, &StringConstraint::appendExact, exactArrayValue);
        else {
            String exactStringValue;
            if (dictionaryValue.get("exact", exactStringValue))
                constraint->setExact(exactStringValue);
        }

        ArrayValue idealArrayValue;
        if (dictionaryValue.get("ideal", idealArrayValue) && !idealArrayValue.isUndefinedOrNull())
            initializeStringConstraintWithList(constraint, &StringConstraint::appendIdeal, idealArrayValue);
        else {
            String idealStringValue;
            if (!dictionaryValue.get("ideal", idealStringValue))
                constraint->setIdeal(idealStringValue);
        }

        if (constraint->isEmpty()) {
            LOG(Media, "createStringConstraint() - ignoring string constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return nullptr;
        }
        
        return WTFMove(constraint);
    }

    // Array constraint value.
    ArrayValue arrayValue;
    if (mediaTrackConstraintSet.get(name, arrayValue) && !arrayValue.isUndefinedOrNull()) {
        initializeStringConstraintWithList(constraint, &StringConstraint::appendIdeal, arrayValue);

        if (constraint->isEmpty()) {
            LOG(Media, "createStringConstraint() - ignoring string constraint '%s' with array value since it is empty.", name.utf8().data());
            return nullptr;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    String value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint->setIdeal(value);
        else
            constraint->setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createStringConstraint() - ignoring string constraint '%s' since it has neither a dictionary nor sequence nor scalar value.", name.utf8().data());
    return nullptr;
}

static RefPtr<BooleanConstraint> createBooleanConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = BooleanConstraint::create(type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        bool exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint->setExact(exactValue);

        bool idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint->setIdeal(idealValue);

        if (constraint->isEmpty()) {
            LOG(Media, "createBooleanConstraint() - ignoring boolean constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return nullptr;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    bool value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint->setIdeal(value);
        else
            constraint->setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createBooleanConstraint() - ignoring boolean constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return nullptr;
}

static RefPtr<DoubleConstraint> createDoubleConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = DoubleConstraint::create(type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        double minValue;
        if (dictionaryValue.get("min", minValue))
            constraint->setMin(minValue);

        double maxValue;
        if (dictionaryValue.get("max", maxValue))
            constraint->setMax(maxValue);

        double exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint->setExact(exactValue);

        double idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint->setIdeal(idealValue);

        if (constraint->isEmpty()) {
            LOG(Media, "createDoubleConstraint() - ignoring double constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return nullptr;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    double value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint->setIdeal(value);
        else
            constraint->setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createDoubleConstraint() - ignoring double constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return nullptr;
}

static RefPtr<IntConstraint> createIntConstraint(const Dictionary& mediaTrackConstraintSet, const String& name, MediaConstraintType type, ConstraintSetType constraintSetType)
{
    auto constraint = IntConstraint::create(type);

    // Dictionary constraint value.
    Dictionary dictionaryValue;
    if (mediaTrackConstraintSet.get(name, dictionaryValue) && !dictionaryValue.isUndefinedOrNull()) {
        int minValue;
        if (dictionaryValue.get("min", minValue))
            constraint->setMin(minValue);

        int maxValue;
        if (dictionaryValue.get("max", maxValue))
            constraint->setMax(maxValue);

        int exactValue;
        if (dictionaryValue.get("exact", exactValue))
            constraint->setExact(exactValue);

        int idealValue;
        if (dictionaryValue.get("ideal", idealValue))
            constraint->setIdeal(idealValue);

        if (constraint->isEmpty()) {
            LOG(Media, "createIntConstraint() - ignoring long constraint '%s' with dictionary value since it has no valid or supported key/value pairs.", name.utf8().data());
            return nullptr;
        }

        return WTFMove(constraint);
    }

    // Scalar constraint value.
    int value;
    if (mediaTrackConstraintSet.get(name, value)) {
        if (constraintSetType == ConstraintSetType::Mandatory)
            constraint->setIdeal(value);
        else
            constraint->setExact(value);
        
        return WTFMove(constraint);
    }

    // Invalid constraint value.
    LOG(Media, "createIntConstraint() - ignoring long constraint '%s' since it has neither a dictionary nor scalar value.", name.utf8().data());
    return nullptr;
}

static void parseMediaTrackConstraintSetForKey(const Dictionary& mediaTrackConstraintSet, const String& name, MediaTrackConstraintSetMap& map, ConstraintSetType constraintSetType, RealtimeMediaSource::Type sourceType)
{
    MediaConstraintType constraintType = RealtimeMediaSourceSupportedConstraints::constraintFromName(name);

    RefPtr<MediaConstraint> mediaConstraint;
    if (sourceType == RealtimeMediaSource::Audio) {
        switch (constraintType) {
        case MediaConstraintType::SampleRate:
        case MediaConstraintType::SampleSize:
            mediaConstraint = createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        case MediaConstraintType::Volume:
            mediaConstraint = createDoubleConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        case MediaConstraintType::EchoCancellation:
            mediaConstraint = createBooleanConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        case MediaConstraintType::DeviceId:
        case MediaConstraintType::GroupId:
            mediaConstraint = createStringConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        default:
            LOG(Media, "parseMediaTrackConstraintSetForKey() - ignoring unsupported constraint '%s' for audio.", name.utf8().data());
            mediaConstraint = nullptr;
            break;
        }
    } else if (sourceType == RealtimeMediaSource::Video) {
        switch (constraintType) {
        case MediaConstraintType::Width:
        case MediaConstraintType::Height:
            mediaConstraint = createIntConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        case MediaConstraintType::AspectRatio:
        case MediaConstraintType::FrameRate:
            mediaConstraint = createDoubleConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        case MediaConstraintType::FacingMode:
        case MediaConstraintType::DeviceId:
        case MediaConstraintType::GroupId:
            mediaConstraint = createStringConstraint(mediaTrackConstraintSet, name, constraintType, constraintSetType);
            break;
        default:
            LOG(Media, "parseMediaTrackConstraintSetForKey() - ignoring unsupported constraint '%s' for video.", name.utf8().data());
            mediaConstraint = nullptr;
            break;
        }
    }

    if (!mediaConstraint)
        return;

    map.add(name, WTFMove(mediaConstraint));
}

static void parseAdvancedConstraints(const Dictionary& mediaTrackConstraints, Vector<MediaTrackConstraintSetMap>& advancedConstraints, RealtimeMediaSource::Type sourceType)
{
    ArrayValue sequenceOfMediaTrackConstraintSets;
    if (!mediaTrackConstraints.get("advanced", sequenceOfMediaTrackConstraintSets) || sequenceOfMediaTrackConstraintSets.isUndefinedOrNull()) {
        LOG(Media, "parseAdvancedConstraints() - value of advanced key is not a list.");
        return;
    }

    size_t numberOfConstraintSets;
    if (!sequenceOfMediaTrackConstraintSets.length(numberOfConstraintSets)) {
        LOG(Media, "parseAdvancedConstraints() - ignoring empty advanced sequence of MediaTrackConstraintSets.");
        return;
    }

    for (size_t i = 0; i < numberOfConstraintSets; ++i) {
        Dictionary mediaTrackConstraintSet;
        if (!sequenceOfMediaTrackConstraintSets.get(i, mediaTrackConstraintSet) || mediaTrackConstraintSet.isUndefinedOrNull()) {
            LOG(Media, "parseAdvancedConstraints() - ignoring constraint set with index '%zu' in advanced list.", i);
            continue;
        }

        MediaTrackConstraintSetMap map;

        Vector<String> localKeys;
        mediaTrackConstraintSet.getOwnPropertyNames(localKeys);
        for (auto& localKey : localKeys)
            parseMediaTrackConstraintSetForKey(mediaTrackConstraintSet, localKey, map, ConstraintSetType::Advanced, sourceType);

        if (!map.isEmpty())
            advancedConstraints.append(WTFMove(map));
    }
}

static void parseConstraints(const Dictionary& mediaTrackConstraints, MediaTrackConstraintSetMap& mandatoryConstraints, Vector<MediaTrackConstraintSetMap>& advancedConstraints, RealtimeMediaSource::Type sourceType)
{
    if (mediaTrackConstraints.isUndefinedOrNull())
        return;

    Vector<String> keys;
    mediaTrackConstraints.getOwnPropertyNames(keys);

    for (auto& key : keys) {
        if (key == "advanced")
            parseAdvancedConstraints(mediaTrackConstraints, advancedConstraints, sourceType);
        else
            parseMediaTrackConstraintSetForKey(mediaTrackConstraints, key, mandatoryConstraints, ConstraintSetType::Mandatory, sourceType);
    }
}

JSValue JSMediaDevices::getUserMedia(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return JSValue::decode(throwVMError(&state, scope, createNotEnoughArgumentsError(&state)));
    ExceptionCode ec = 0;
    auto constraintsDictionary = Dictionary(&state, state.uncheckedArgument(0));

    MediaTrackConstraintSetMap mandatoryAudioConstraints;
    Vector<MediaTrackConstraintSetMap> advancedAudioConstraints;
    bool areAudioConstraintsValid = false;

    Dictionary audioConstraintsDictionary;
    if (constraintsDictionary.get("audio", audioConstraintsDictionary) && !audioConstraintsDictionary.isUndefinedOrNull()) {
        parseConstraints(audioConstraintsDictionary, mandatoryAudioConstraints, advancedAudioConstraints, RealtimeMediaSource::Audio);
        areAudioConstraintsValid = true;
    } else
        constraintsDictionary.get("audio", areAudioConstraintsValid);

    MediaTrackConstraintSetMap mandatoryVideoConstraints;
    Vector<MediaTrackConstraintSetMap> advancedVideoConstraints;
    bool areVideoConstraintsValid = false;

    Dictionary videoConstraintsDictionary;
    if (constraintsDictionary.get("video", videoConstraintsDictionary) && !videoConstraintsDictionary.isUndefinedOrNull()) {
        parseConstraints(videoConstraintsDictionary, mandatoryVideoConstraints, advancedVideoConstraints, RealtimeMediaSource::Video);
        areVideoConstraintsValid = true;
    } else
        constraintsDictionary.get("video", areVideoConstraintsValid);

    auto audioConstraints = MediaConstraintsImpl::create(WTFMove(mandatoryAudioConstraints), WTFMove(advancedAudioConstraints), areAudioConstraintsValid);
    auto videoConstraints = MediaConstraintsImpl::create(WTFMove(mandatoryVideoConstraints), WTFMove(advancedVideoConstraints), areVideoConstraintsValid);
    JSC::JSPromiseDeferred* promiseDeferred = JSC::JSPromiseDeferred::create(&state, globalObject());
    wrapped().getUserMedia(WTFMove(audioConstraints), WTFMove(videoConstraints), DeferredWrapper(&state, globalObject(), promiseDeferred), ec);
    setDOMException(&state, ec);
    return promiseDeferred->promise();
}

}

#endif
