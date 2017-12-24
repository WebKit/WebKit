/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "KeyframeEffect.h"

#include "Animation.h"
#include "CSSPropertyAnimation.h"
#include "CSSStyleDeclaration.h"
#include "Element.h"
#include "RenderStyle.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "WillChangeData.h"
#include <wtf/UUID.h>

namespace WebCore {
using namespace JSC;

static inline CSSPropertyID IDLAttributeNameToAnimationPropertyName(String idlAttributeName)
{
    // https://drafts.csswg.org/web-animations-1/#idl-attribute-name-to-animation-property-name
    // 1. If attribute conforms to the <custom-property-name> production, return attribute.
    // 2. If attribute is the string "cssFloat", then return an animation property representing the CSS float property.
    if (idlAttributeName == "cssFloat")
        return CSSPropertyFloat;
    // 3. If attribute is the string "cssOffset", then return an animation property representing the CSS offset property.
    // FIXME: we don't support the CSS "offset" property
    // 4. Otherwise, return the result of applying the IDL attribute to CSS property algorithm [CSSOM] to attribute.
    return CSSStyleDeclaration::getCSSPropertyIDFromJavaScriptPropertyName(idlAttributeName);
}

static inline void computeMissingKeyframeOffsets(Vector<KeyframeEffect::ProcessedKeyframe>& keyframes)
{
    // https://drafts.csswg.org/web-animations-1/#compute-missing-keyframe-offsets

    if (keyframes.isEmpty())
        return;

    // 1. For each keyframe, in keyframes, let the computed keyframe offset of the keyframe be equal to its keyframe offset value.

    // 2. If keyframes contains more than one keyframe and the computed keyframe offset of the first keyframe in keyframes is null,
    //    set the computed keyframe offset of the first keyframe to 0.
    if (keyframes.size() > 1 && !keyframes[0].offset)
        keyframes[0].offset = 0;

    // 3. If the computed keyframe offset of the last keyframe in keyframes is null, set its computed keyframe offset to 1.
    if (!keyframes.last().offset)
        keyframes.last().offset = 1;

    // 4. For each pair of keyframes A and B where:
    //    - A appears before B in keyframes, and
    //    - A and B have a computed keyframe offset that is not null, and
    //    - all keyframes between A and B have a null computed keyframe offset,
    //    calculate the computed keyframe offset of each keyframe between A and B as follows:
    //    1. Let offsetk be the computed keyframe offset of a keyframe k.
    //    2. Let n be the number of keyframes between and including A and B minus 1.
    //    3. Let index refer to the position of keyframe in the sequence of keyframes between A and B such that the first keyframe after A has an index of 1.
    //    4. Set the computed keyframe offset of keyframe to offsetA + (offsetB − offsetA) × index / n.
    size_t indexOfLastKeyframeWithNonNullOffset = 0;
    for (size_t i = 1; i < keyframes.size(); ++i) {
        auto keyframe = keyframes[i];
        if (!keyframe.offset)
            continue;
        if (indexOfLastKeyframeWithNonNullOffset == i - 1)
            continue;

        double lastNonNullOffset = keyframes[indexOfLastKeyframeWithNonNullOffset].offset.value();
        double offsetDelta = keyframe.offset.value() - lastNonNullOffset;
        double offsetIncrement = offsetDelta / (i - indexOfLastKeyframeWithNonNullOffset);
        size_t indexOfFirstKeyframeWithNullOffset = indexOfLastKeyframeWithNonNullOffset + 1;
        for (size_t j = indexOfFirstKeyframeWithNullOffset; j < i; ++j)
            keyframes[j].offset = lastNonNullOffset + (j - indexOfLastKeyframeWithNonNullOffset) * offsetIncrement;

        indexOfLastKeyframeWithNonNullOffset = i;
    }
}

static inline ExceptionOr<void> processIterableKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput, JSValue method, Vector<KeyframeEffect::ProcessedKeyframe>& processedKeyframes)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let iter be GetIterator(object, method).
    forEachInIterable(state, keyframesInput.get(), method, [&processedKeyframes](VM& vm, ExecState& state, JSValue nextValue) -> ExceptionOr<void> {
        if (!nextValue || !nextValue.isObject())
            return Exception { TypeError };

        auto scope = DECLARE_THROW_SCOPE(vm);

        JSObject* keyframe = nextValue.toObject(&state);
        PropertyNameArray ownPropertyNames(&vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        JSObject::getOwnPropertyNames(keyframe, &state, ownPropertyNames, EnumerationMode());
        size_t numberOfProperties = ownPropertyNames.size();

        KeyframeEffect::ProcessedKeyframe keyframeOutput;

        String easing = "linear";
        std::optional<double> offset;
        std::optional<CompositeOperation> composite;

        for (size_t j = 0; j < numberOfProperties; ++j) {
            auto ownPropertyName = ownPropertyNames[j];
            auto ownPropertyRawValue = keyframe->get(&state, ownPropertyName);
            if (ownPropertyName == "easing")
                easing = convert<IDLDOMString>(state, ownPropertyRawValue);
            else if (ownPropertyName == "offset")
                offset = convert<IDLNullable<IDLDouble>>(state, ownPropertyRawValue);
            else if (ownPropertyName == "composite")
                composite = convert<IDLEnumeration<CompositeOperation>>(state, ownPropertyRawValue);
            else {
                auto cssPropertyId = IDLAttributeNameToAnimationPropertyName(ownPropertyName.string());
                if (CSSPropertyAnimation::isPropertyAnimatable(cssPropertyId))
                    keyframeOutput.cssPropertiesAndValues.set(cssPropertyId, convert<IDLDOMString>(state, ownPropertyRawValue));
            }
            RETURN_IF_EXCEPTION(scope, Exception { TypeError });
        }

        keyframeOutput.easing = easing;
        keyframeOutput.offset = offset;
        keyframeOutput.composite = composite;

        processedKeyframes.append(WTFMove(keyframeOutput));

        return { };
    });
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    return { };
}

static inline ExceptionOr<KeyframeEffect::KeyframeLikeObject> processKeyframeLikeObject(ExecState& state, Strong<JSObject>&& keyframesInput)
{
    // https://drafts.csswg.org/web-animations-1/#process-a-keyframe-like-object

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Run the procedure to convert an ECMAScript value to a dictionary type [WEBIDL] with keyframe input as the ECMAScript value as follows:
    // 
    //    dictionary BasePropertyIndexedKeyframe {
    //        (double? or sequence<double?>)                       offset = [];
    //        (DOMString or sequence<DOMString>)                   easing = [];
    //        (CompositeOperation or sequence<CompositeOperation>) composite = [];
    //    };
    //
    //    Store the result of this procedure as keyframe output.
    auto baseProperties = convert<IDLDictionary<KeyframeEffect::BasePropertyIndexedKeyframe>>(state, keyframesInput.get());
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    KeyframeEffect::KeyframeLikeObject keyframeOuput;
    keyframeOuput.baseProperties = baseProperties;

    // 2. Build up a list of animatable properties as follows:
    //
    //    1. Let animatable properties be a list of property names (including shorthand properties that have longhand sub-properties
    //       that are animatable) that can be animated by the implementation.
    //    2. Convert each property name in animatable properties to the equivalent IDL attribute by applying the animation property
    //       name to IDL attribute name algorithm.

    // 3. Let input properties be the result of calling the EnumerableOwnNames operation with keyframe input as the object.
    PropertyNameArray inputProperties(&vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    JSObject::getOwnPropertyNames(keyframesInput.get(), &state, inputProperties, EnumerationMode());

    // 4. Make up a new list animation properties that consists of all of the properties that are in both input properties and animatable
    //    properties, or which are in input properties and conform to the <custom-property-name> production.

    // 5. Sort animation properties in ascending order by the Unicode codepoints that define each property name.
    //    We only actually perform this after step 6.

    // 6. For each property name in animation properties,
    size_t numberOfProperties = inputProperties.size();
    for (size_t i = 0; i < numberOfProperties; ++i) {
        auto cssPropertyID = IDLAttributeNameToAnimationPropertyName(inputProperties[i].string());
        if (!CSSPropertyAnimation::isPropertyAnimatable(cssPropertyID))
            continue;

        // 1. Let raw value be the result of calling the [[Get]] internal method on keyframe input, with property name as the property
        //    key and keyframe input as the receiver.
        auto rawValue = keyframesInput->get(&state, inputProperties[i]);

        // 2. Check the completion record of raw value.
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });

        // 3. Convert raw value to a DOMString or sequence of DOMStrings property values as follows:
        Vector<String> propertyValues;
        // Let property values be the result of converting raw value to IDL type (DOMString or sequence<DOMString>)
        // using the procedures defined for converting an ECMAScript value to an IDL value [WEBIDL].
        // If property values is a single DOMString, replace property values with a sequence of DOMStrings with the original value of property
        // Values as the only element.
        if (rawValue.isString())
            propertyValues = { rawValue.toWTFString(&state) };
        else
            propertyValues = convert<IDLSequence<IDLDOMString>>(state, rawValue);
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });

        // 4. Calculate the normalized property name as the result of applying the IDL attribute name to animation property name algorithm to property name.
        // 5. Add a property to to keyframe output with normalized property name as the property name, and property values as the property value.
        keyframeOuput.propertiesAndValues.append({ cssPropertyID, propertyValues });
    }

    // Now we can perform step 5.
    std::sort(keyframeOuput.propertiesAndValues.begin(), keyframeOuput.propertiesAndValues.end(), [](auto& lhs, auto& rhs) {
        return getPropertyNameString(lhs.property).utf8() < getPropertyNameString(rhs.property).utf8();
    });

    // 7. Return keyframe output.
    return { WTFMove(keyframeOuput) };
}

static inline ExceptionOr<void> processPropertyIndexedKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput, Vector<KeyframeEffect::ProcessedKeyframe>& processedKeyframes)
{
    // 1. Let property-indexed keyframe be the result of running the procedure to process a keyframe-like object passing object as the keyframe input.
    auto processKeyframeLikeObjectResult = processKeyframeLikeObject(state, WTFMove(keyframesInput));
    if (processKeyframeLikeObjectResult.hasException())
        return processKeyframeLikeObjectResult.releaseException();
    auto propertyIndexedKeyframe = processKeyframeLikeObjectResult.returnValue();

    // 2. For each member, m, in property-indexed keyframe, perform the following steps:
    for (auto& m : propertyIndexedKeyframe.propertiesAndValues) {
        // 1. Let property name be the key for m.
        auto propertyName = m.property;
        // 2. If property name is “composite”, or “easing”, or “offset”, skip the remaining steps in this loop and continue from the next member in property-indexed
        //    keyframe after m.
        //    We skip this test since we split those properties and the actual CSS properties that we're currently iterating over.
        // 3. Let property values be the value for m.
        auto propertyValues = m.values;
        // 4. Let property keyframes be an empty sequence of keyframes.
        Vector<KeyframeEffect::ProcessedKeyframe> propertyKeyframes;
        // 5. For each value, v, in property values perform the following steps:
        for (auto& v : propertyValues) {
            // 1. Let k be a new keyframe with a null keyframe offset.
            KeyframeEffect::ProcessedKeyframe k;
            // 2. Add the property-value pair, property name → v, to k.
            k.cssPropertiesAndValues.set(propertyName, v);
            // 3. Append k to property keyframes.
            propertyKeyframes.append(k);
        }
        // 6. Apply the procedure to compute missing keyframe offsets to property keyframes.
        computeMissingKeyframeOffsets(propertyKeyframes);

        // 7. Add keyframes in property keyframes to processed keyframes.
        for (auto& keyframe : propertyKeyframes)
            processedKeyframes.append(keyframe);
    }

    // 3. Sort processed keyframes by the computed keyframe offset of each keyframe in increasing order.
    std::sort(processedKeyframes.begin(), processedKeyframes.end(), [](auto& lhs, auto& rhs) {
        return lhs.offset.value() < rhs.offset.value();
    });

    // 4. Merge adjacent keyframes in processed keyframes when they have equal computed keyframe offsets.
    size_t i = 1;
    while (i < processedKeyframes.size()) {
        auto& keyframe = processedKeyframes[i];
        auto& previousKeyframe = processedKeyframes[i - 1];
        // If the offsets of this keyframe and the previous keyframe are different,
        // this means that the two keyframes should not be merged and we can move
        // on to the next keyframe.
        if (keyframe.offset.value() != previousKeyframe.offset.value()) {
            i++;
            continue;
        }
        // Otherwise, both this keyframe and the previous keyframe should be merged.
        // Unprocessed keyframes in processedKeyframes at this stage have a single
        // property in cssPropertiesAndValues, so just set this on the previous keyframe.
        auto singleValueInKeyframe = keyframe.cssPropertiesAndValues.begin();
        previousKeyframe.cssPropertiesAndValues.set(singleValueInKeyframe->key, singleValueInKeyframe->value);
        // Since we've processed this keyframe, we can remove it and keep i the same
        // so that we process the next keyframe in the next loop iteration.
        processedKeyframes.remove(i);
    }

    // 5. Let offsets be a sequence of nullable double values assigned based on the type of the “offset” member of the property-indexed keyframe as follows:
    //    - sequence<double?>, the value of “offset” as-is.
    //    - double?, a sequence of length one with the value of “offset” as its single item, i.e. « offset »,
    // FIXME: update this when we figure out how to deal with nullable doubles here.
    Vector<std::optional<double>> offsets;
    if (WTF::holds_alternative<Vector<std::optional<double>>>(propertyIndexedKeyframe.baseProperties.offset))
        offsets = WTF::get<Vector<std::optional<double>>>(propertyIndexedKeyframe.baseProperties.offset);
    else if (WTF::holds_alternative<double>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(WTF::get<double>(propertyIndexedKeyframe.baseProperties.offset));
    else if (WTF::holds_alternative<std::nullptr_t>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(std::nullopt);

    // 6. Assign each value in offsets to the keyframe offset of the keyframe with corresponding position in property keyframes until the end of either sequence is reached.
    for (size_t i = 0; i < offsets.size() && i < processedKeyframes.size(); ++i)
        processedKeyframes[i].offset = offsets[i];

    // 7. Let easings be a sequence of DOMString values assigned based on the type of the “easing” member of the property-indexed keyframe as follows:
    //    - sequence<DOMString>, the value of “easing” as-is.
    //    - DOMString, a sequence of length one with the value of “easing” as its single item, i.e. « easing »,
    Vector<String> easings;
    if (WTF::holds_alternative<Vector<String>>(propertyIndexedKeyframe.baseProperties.easing))
        easings = WTF::get<Vector<String>>(propertyIndexedKeyframe.baseProperties.easing);
    else if (WTF::holds_alternative<String>(propertyIndexedKeyframe.baseProperties.easing))
        easings.append(WTF::get<String>(propertyIndexedKeyframe.baseProperties.easing));

    // 8. If easings is an empty sequence, let it be a sequence of length one containing the single value “linear”, i.e. « "linear" ».
    if (easings.isEmpty())
        easings.append("linear");

    // 9. If easings has fewer items than property keyframes, repeat the elements in easings successively starting from the beginning of the list until easings has as many
    //    items as property keyframes.
    if (easings.size() < processedKeyframes.size()) {
        size_t initialNumberOfEasings = easings.size();
        for (i = initialNumberOfEasings + 1; i < processedKeyframes.size() - 1; ++i)
            easings.append(easings[i % initialNumberOfEasings]);
    }

    // 10. If easings has more items than property keyframes, store the excess items as unused easings.
    // FIXME: We don't deal with this yet.

    // 11. Assign each value in easings to a property named “easing” on the keyframe with the corresponding position in property keyframes until the end of property keyframes
    //     is reached.
    for (size_t i = 0; i < easings.size() && i < processedKeyframes.size(); ++i)
        processedKeyframes[i].easing = easings[i];

    // 12. If the “composite” member of the property-indexed keyframe is not an empty sequence:
    Vector<CompositeOperation> compositeModes;
    if (WTF::holds_alternative<Vector<CompositeOperation>>(propertyIndexedKeyframe.baseProperties.composite))
        compositeModes = WTF::get<Vector<CompositeOperation>>(propertyIndexedKeyframe.baseProperties.composite);
    else if (WTF::holds_alternative<CompositeOperation>(propertyIndexedKeyframe.baseProperties.composite))
        compositeModes.append(WTF::get<CompositeOperation>(propertyIndexedKeyframe.baseProperties.composite));
    if (!compositeModes.isEmpty()) {
        // 1. Let composite modes be a sequence of composite operations assigned from the “composite” member of property-indexed keyframe. If that member is a single composite
        //    operation, let composite modes be a sequence of length one, with the value of the “composite” as its single item.
        // 2. As with easings, if composite modes has fewer items than property keyframes, repeat the elements in composite modes successively starting from the beginning of
        //    the list until composite modes has as many items as property keyframes.
        if (compositeModes.size() < processedKeyframes.size()) {
            size_t initialNumberOfCompositeModes = compositeModes.size();
            for (i = initialNumberOfCompositeModes + 1; i < processedKeyframes.size() - 1; ++i)
                compositeModes.append(compositeModes[i % initialNumberOfCompositeModes]);
        }
        // 3. Assign each value in composite modes to the keyframe-specific composite operation on the keyframe with the corresponding position in property keyframes until
        //    the end of property keyframes is reached.
        for (size_t i = 0; i < compositeModes.size() && i < processedKeyframes.size(); ++i)
            processedKeyframes[i].composite = compositeModes[i];
    }

    return { };
}

ExceptionOr<Ref<KeyframeEffect>> KeyframeEffect::create(ExecState& state, Element* target, Strong<JSObject>&& keyframes, std::optional<Variant<double, KeyframeEffectOptions>>&& options)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(target));

    auto setKeyframesResult = keyframeEffect->setKeyframes(state, WTFMove(keyframes));
    if (setKeyframesResult.hasException())
        return setKeyframesResult.releaseException();

    if (options) {
        auto optionsValue = options.value();
        Variant<double, String> bindingsDuration;
        if (WTF::holds_alternative<double>(optionsValue))
            bindingsDuration = WTF::get<double>(optionsValue);
        else
            bindingsDuration = WTF::get<KeyframeEffectOptions>(optionsValue).duration;
        auto setBindingsDurationResult = keyframeEffect->timing()->setBindingsDuration(WTFMove(bindingsDuration));
        if (setBindingsDurationResult.hasException())
            return setBindingsDurationResult.releaseException();
    }

    return WTFMove(keyframeEffect);
}

KeyframeEffect::KeyframeEffect(Element* target)
    : AnimationEffect(KeyframeEffectClass)
    , m_target(target)
    , m_keyframes(emptyString())
{
}

ExceptionOr<void> KeyframeEffect::setKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput)
{
    return processKeyframes(state, WTFMove(keyframesInput));
}

ExceptionOr<void> KeyframeEffect::processKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput)
{
    // 1. If object is null, return an empty sequence of keyframes.
    if (!m_target || !keyframesInput.get())
        return { };

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 2. Let processed keyframes be an empty sequence of keyframes.
    Vector<ProcessedKeyframe> processedKeyframes;

    // 3. Let method be the result of GetMethod(object, @@iterator).
    auto method = keyframesInput.get()->get(&state, vm.propertyNames->iteratorSymbol);

    // 4. Check the completion record of method.
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    // 5. Perform the steps corresponding to the first matching condition from below,
    if (!method.isUndefined())
        processIterableKeyframes(state, WTFMove(keyframesInput), WTFMove(method), processedKeyframes);
    else
        processPropertyIndexedKeyframes(state, WTFMove(keyframesInput), processedKeyframes);

    if (!processedKeyframes.size())
        return { };

    // 6. If processed keyframes is not loosely sorted by offset, throw a TypeError and abort these steps.
    // 7. If there exist any keyframe in processed keyframes whose keyframe offset is non-null and less than
    //    zero or greater than one, throw a TypeError and abort these steps.
    double lastNonNullOffset = -1;
    for (auto& keyframe : processedKeyframes) {
        if (!keyframe.offset)
            continue;
        auto offset = keyframe.offset.value();
        if (offset <= lastNonNullOffset || offset < 0 || offset > 1)
            return Exception { TypeError };
        lastNonNullOffset = offset;
    }

    // We take a slight detour from the spec text and compute the missing keyframe offsets right away
    // since they can be computed up-front.
    computeMissingKeyframeOffsets(processedKeyframes);

    KeyframeList keyframeList("keyframe-effect-" + createCanonicalUUIDString());
    StyleResolver& styleResolver = m_target->styleResolver();
    auto parserContext = CSSParserContext(HTMLStandardMode);

    // 8. For each frame in processed keyframes, perform the following steps:
    for (auto& keyframe : processedKeyframes) {
        // 1. For each property-value pair in frame, parse the property value using the syntax specified for that property.
        //    If the property value is invalid according to the syntax for the property, discard the property-value pair.
        //    User agents that provide support for diagnosing errors in content SHOULD produce an appropriate warning
        //    highlighting the invalid property value.

        StringBuilder cssText;
        for (auto it = keyframe.cssPropertiesAndValues.begin(), end = keyframe.cssPropertiesAndValues.end(); it != end; ++it) {
            cssText.append(getPropertyNameString(it->key));
            cssText.appendLiteral(": ");
            cssText.append(it->value);
            cssText.appendLiteral("; ");
        }

        KeyframeValue keyframeValue(keyframe.offset.value(), nullptr);
        auto renderStyle = RenderStyle::createPtr();
        auto styleProperties = MutableStyleProperties::create();
        styleProperties->parseDeclaration(cssText.toString(), parserContext);
        unsigned numberOfCSSProperties = styleProperties->propertyCount();

        for (unsigned i = 0; i < numberOfCSSProperties; ++i) {
            auto cssPropertyId = styleProperties->propertyAt(i).id();
            keyframeValue.addProperty(cssPropertyId);
            keyframeList.addProperty(cssPropertyId);
            styleResolver.applyPropertyToStyle(cssPropertyId, styleProperties->propertyAt(i).value(), WTFMove(renderStyle));
            renderStyle = styleResolver.state().takeStyle();
        }

        keyframeValue.setStyle(RenderStyle::clonePtr(*renderStyle));
        keyframeList.insert(WTFMove(keyframeValue));

        // 2. Let the timing function of frame be the result of parsing the “easing” property on frame using the CSS syntax
        //    defined for the easing property of the AnimationEffectTimingReadOnly interface.
        //    If parsing the “easing” property fails, throw a TypeError and abort this procedure.
        // FIXME: implement when we support easing.
    }

    // 9. Parse each of the values in unused easings using the CSS syntax defined for easing property of the
    //    AnimationEffectTimingReadOnly interface, and if any of the values fail to parse, throw a TypeError
    //    and abort this procedure.
    // FIXME: implement when we support easing.

    m_keyframes = WTFMove(keyframeList);

    computeStackingContextImpact();

    return { };
}

void KeyframeEffect::computeStackingContextImpact()
{
    m_triggersStackingContext = false;
    for (auto cssPropertyId : m_keyframes.properties()) {
        if (WillChangeData::propertyCreatesStackingContext(cssPropertyId)) {
            m_triggersStackingContext = true;
            break;
        }
    }
}

void KeyframeEffect::applyAtLocalTime(Seconds localTime, RenderStyle& targetStyle)
{
    if (!m_target)
        return;

    if (m_startedAccelerated && localTime >= timing()->duration()) {
        m_startedAccelerated = false;
        animation()->acceleratedRunningStateDidChange();
    }

    // FIXME: Assume animations only apply in the range [0, duration[
    // until we support fill modes, delays and iterations.
    if (localTime < 0_s || localTime >= timing()->duration())
        return;

    if (!timing()->duration())
        return;

    bool needsToStartAccelerated = false;

    if (!m_started && !m_startedAccelerated) {
        needsToStartAccelerated = shouldRunAccelerated();
        m_startedAccelerated = needsToStartAccelerated;
        if (needsToStartAccelerated)
            animation()->acceleratedRunningStateDidChange();
    }
    m_started = true;

    if (!needsToStartAccelerated && !m_startedAccelerated)
        setAnimatedPropertiesInStyle(targetStyle, localTime / timing()->duration());

    // https://w3c.github.io/web-animations/#side-effects-section
    // For every property targeted by at least one animation effect that is current or in effect, the user agent
    // must act as if the will-change property ([css-will-change-1]) on the target element includes the property.
    if (m_triggersStackingContext && targetStyle.hasAutoZIndex())
        targetStyle.setZIndex(0);
}

bool KeyframeEffect::shouldRunAccelerated()
{
    for (auto cssPropertyId : m_keyframes.properties()) {
        if (!CSSPropertyAnimation::animationOfPropertyIsAccelerated(cssPropertyId))
            return false;
    }
    return true;
}

void KeyframeEffect::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    if (!animation() || !timing()->duration())
        return;

    auto localTime = animation()->currentTime();

    // FIXME: Assume animations only apply in the range [0, duration[
    // until we support fill modes, delays and iterations.
    if (!localTime || localTime < 0_s || localTime >= timing()->duration())
        return;

    if (!m_keyframes.size())
        return;

    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(renderer()->style());

    setAnimatedPropertiesInStyle(*animatedStyle.get(), localTime.value() / timing()->duration());
}

void KeyframeEffect::setAnimatedPropertiesInStyle(RenderStyle& targetStyle, double progress)
{
    size_t numberOfKeyframes = m_keyframes.size();
    for (auto cssPropertyId : m_keyframes.properties()) {
        int startKeyframeIndex = -1;
        int endKeyframeIndex = -1;

        for (size_t i = 0; i < numberOfKeyframes; ++i) {
            auto& keyframe = m_keyframes[i];
            if (!keyframe.containsProperty(cssPropertyId))
                continue;

            if (keyframe.key() > progress) {
                endKeyframeIndex = i;
                break;
            }

            startKeyframeIndex = i;
        }

        // If we didn't find a start keyframe, this means there is an implicit start keyframe.
        double startOffset;
        const RenderStyle* startStyle;
        if (startKeyframeIndex == -1) {
            startOffset = 0;
            startStyle = &targetStyle;
        } else {
            startOffset = m_keyframes[startKeyframeIndex].key();
            startStyle = m_keyframes[startKeyframeIndex].style();
        }

        // If we didn't find an end keyframe, this means there is an implicit end keyframe.
        double endOffset;
        const RenderStyle* endStyle;
        if (endKeyframeIndex == -1) {
            endOffset = 1;
            endStyle = &targetStyle;
        } else {
            endOffset = m_keyframes[endKeyframeIndex].key();
            endStyle = m_keyframes[endKeyframeIndex].style();
        }

        auto progressInRange = (progress - startOffset) / (endOffset - startOffset);
        CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, startStyle, endStyle, progressInRange);
    }
}

void KeyframeEffect::startOrStopAccelerated()
{
    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited())
        return;

    auto* compositedRenderer = downcast<RenderBoxModelObject>(renderer);
    if (m_startedAccelerated) {
        auto animation = Animation::create();
        animation->setDuration(timing()->duration().value());
        compositedRenderer->startAnimation(0, animation.ptr(), m_keyframes);
    } else {
        compositedRenderer->animationFinished(m_keyframes.animationName());
        if (!m_target->document().renderTreeBeingDestroyed())
            m_target->invalidateStyleAndLayerComposition();
    }
}

RenderElement* KeyframeEffect::renderer() const
{
    return m_target ? m_target->renderer() : nullptr;
}

const RenderStyle& KeyframeEffect::currentStyle() const
{
    if (auto* renderer = this->renderer())
        return renderer->style();
    return RenderStyle::defaultStyle();
}

} // namespace WebCore
