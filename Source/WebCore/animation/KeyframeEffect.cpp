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
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSTimingFunctionValue.h"
#include "Element.h"
#include "JSCompositeOperation.h"
#include "JSKeyframeEffect.h"
#include "RenderStyle.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TimingFunction.h"
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

static inline String CSSPropertyIDToIDLAttributeName(CSSPropertyID cssPropertyId)
{
    // https://drafts.csswg.org/web-animations-1/#animation-property-name-to-idl-attribute-name
    // 1. If property follows the <custom-property-name> production, return property.
    // 2. If property refers to the CSS float property, return the string "cssFloat".
    if (cssPropertyId == CSSPropertyFloat)
        return "cssFloat";
    // 3. If property refers to the CSS offset property, return the string "cssOffset".
    // FIXME: we don't support the CSS "offset" property
    // 4. Otherwise, return the result of applying the CSS property to IDL attribute algorithm [CSSOM] to property.
    return getJSPropertyName(cssPropertyId);
}

static inline void computeMissingKeyframeOffsets(Vector<KeyframeEffect::ProcessedKeyframe>& keyframes)
{
    // https://drafts.csswg.org/web-animations-1/#compute-missing-keyframe-offsets

    if (keyframes.isEmpty())
        return;

    // 1. For each keyframe, in keyframes, let the computed keyframe offset of the keyframe be equal to its keyframe offset value.
    for (auto& keyframe : keyframes)
        keyframe.computedOffset = keyframe.offset;

    // 2. If keyframes contains more than one keyframe and the computed keyframe offset of the first keyframe in keyframes is null,
    //    set the computed keyframe offset of the first keyframe to 0.
    if (keyframes.size() > 1 && !keyframes[0].computedOffset)
        keyframes[0].computedOffset = 0;

    // 3. If the computed keyframe offset of the last keyframe in keyframes is null, set its computed keyframe offset to 1.
    if (!keyframes.last().computedOffset)
        keyframes.last().computedOffset = 1;

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
        auto& keyframe = keyframes[i];
        if (!keyframe.computedOffset)
            continue;
        if (indexOfLastKeyframeWithNonNullOffset == i - 1)
            continue;

        double lastNonNullOffset = keyframes[indexOfLastKeyframeWithNonNullOffset].computedOffset.value();
        double offsetDelta = keyframe.computedOffset.value() - lastNonNullOffset;
        double offsetIncrement = offsetDelta / (i - indexOfLastKeyframeWithNonNullOffset);
        size_t indexOfFirstKeyframeWithNullOffset = indexOfLastKeyframeWithNonNullOffset + 1;
        for (size_t j = indexOfFirstKeyframeWithNullOffset; j < i; ++j)
            keyframes[j].computedOffset = lastNonNullOffset + (j - indexOfLastKeyframeWithNonNullOffset) * offsetIncrement;

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

        String easing("linear");
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

static inline ExceptionOr<void> processPropertyIndexedKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput, Vector<KeyframeEffect::ProcessedKeyframe>& processedKeyframes, Vector<String>& unusedEasings)
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
        return lhs.computedOffset.value() < rhs.computedOffset.value();
    });

    // 4. Merge adjacent keyframes in processed keyframes when they have equal computed keyframe offsets.
    size_t i = 1;
    while (i < processedKeyframes.size()) {
        auto& keyframe = processedKeyframes[i];
        auto& previousKeyframe = processedKeyframes[i - 1];
        // If the offsets of this keyframe and the previous keyframe are different,
        // this means that the two keyframes should not be merged and we can move
        // on to the next keyframe.
        if (keyframe.computedOffset.value() != previousKeyframe.computedOffset.value()) {
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
        for (i = initialNumberOfEasings + 1; i <= processedKeyframes.size(); ++i)
            easings.append(easings[i % initialNumberOfEasings]);
    }

    // 10. If easings has more items than property keyframes, store the excess items as unused easings.
    while (easings.size() > processedKeyframes.size())
        unusedEasings.append(easings.takeLast());

    // 11. Assign each value in easings to a property named “easing” on the keyframe with the corresponding position in property keyframes until the end of property keyframes
    //     is reached.
    for (size_t i = 0; i < processedKeyframes.size(); ++i)
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
            for (i = initialNumberOfCompositeModes + 1; i <= processedKeyframes.size(); ++i)
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
        else {
            auto keyframeEffectOptions = WTF::get<KeyframeEffectOptions>(optionsValue);
            bindingsDuration = keyframeEffectOptions.duration;
            keyframeEffect->timing()->setBindingsDelay(keyframeEffectOptions.delay);
            keyframeEffect->timing()->setBindingsEndDelay(keyframeEffectOptions.endDelay);
            keyframeEffect->timing()->setFill(keyframeEffectOptions.fill);
            auto setIterationStartResult = keyframeEffect->timing()->setIterationStart(keyframeEffectOptions.iterationStart);
            if (setIterationStartResult.hasException())
                return setIterationStartResult.releaseException();
            auto setIterationsResult = keyframeEffect->timing()->setIterations(keyframeEffectOptions.iterations);
            if (setIterationsResult.hasException())
                return setIterationsResult.releaseException();
            keyframeEffect->timing()->setDirection(keyframeEffectOptions.direction);
            auto setEasingResult = keyframeEffect->timing()->setEasing(keyframeEffectOptions.easing);
            if (setEasingResult.hasException())
                return setEasingResult.releaseException();
        }

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

Vector<Strong<JSObject>> KeyframeEffect::getKeyframes(ExecState& state)
{
    // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffectreadonly-getkeyframes

    auto lock = JSLockHolder { &state };

    // Since keyframes are represented by a partially open-ended dictionary type that is not currently able to be expressed with WebIDL,
    // the procedure used to prepare the result of this method is defined in prose below:
    //
    // 1. Let result be an empty sequence of objects.
    Vector<Strong<JSObject>> result;

    // 2. Let keyframes be the result of applying the procedure to compute missing keyframe offsets to the keyframes for this keyframe effect.

    // 3. For each keyframe in keyframes perform the following steps:
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        auto& keyframe = m_keyframes[i];

        // 1. Initialize a dictionary object, output keyframe, using the following definition:
        //
        // dictionary BaseComputedKeyframe {
        //      double?             offset = null;
        //      double              computedOffset;
        //      DOMString           easing = "linear";
        //      CompositeOperation? composite = null;
        // };

        // 2. Set offset, computedOffset, easing, composite members of output keyframe to the respective values keyframe offset, computed keyframe
        // offset, keyframe-specific timing function and keyframe-specific composite operation of keyframe.
        BaseComputedKeyframe computedKeyframe;
        computedKeyframe.offset = m_offsets[i];
        computedKeyframe.computedOffset = keyframe.key();
        computedKeyframe.easing = m_timingFunctions[i]->cssText();
        computedKeyframe.composite = m_compositeOperations[i];

        auto outputKeyframe = convertDictionaryToJS(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), computedKeyframe);

        auto& style = *keyframe.style();
        auto computedStyleExtractor = ComputedStyleExtractor(m_target.get());

        // 3. For each animation property-value pair specified on keyframe, declaration, perform the following steps:
        for (auto cssPropertyId : keyframe.properties()) {
            // 1. Let property name be the result of applying the animation property name to IDL attribute name algorithm to the property name of declaration.
            auto propertyName = CSSPropertyIDToIDLAttributeName(cssPropertyId);
            // 2. Let IDL value be the result of serializing the property value of declaration by passing declaration to the algorithm to serialize a CSS value.
            auto idlValue = computedStyleExtractor.valueForPropertyinStyle(style, cssPropertyId)->cssText();
            // 3. Let value be the result of converting IDL value to an ECMAScript String value.
            auto value = toJS<IDLDOMString>(state, idlValue);
            // 4. Call the [[DefineOwnProperty]] internal method on output keyframe with property name property name,
            //    Property Descriptor { [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true, [[Value]]: value } and Boolean flag false.
            JSObject::defineOwnProperty(outputKeyframe, &state, AtomicString(propertyName).impl(), PropertyDescriptor(value, 0), false);
        }

        // 4. Append output keyframe to result.
        result.append(JSC::Strong<JSC::JSObject> { state.vm(), outputKeyframe });
    }

    // 4. Return result.
    return result;
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
    Vector<String> unusedEasings;
    if (!method.isUndefined())
        processIterableKeyframes(state, WTFMove(keyframesInput), WTFMove(method), processedKeyframes);
    else
        processPropertyIndexedKeyframes(state, WTFMove(keyframesInput), processedKeyframes, unusedEasings);

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
    Vector<std::optional<double>> offsets;
    Vector<RefPtr<TimingFunction>> timingFunctions;
    Vector<std::optional<CompositeOperation>> compositeOperations;

    StyleResolver& styleResolver = m_target->styleResolver();
    auto parserContext = CSSParserContext(HTMLStandardMode);

    // 8. For each frame in processed keyframes, perform the following steps:
    for (auto& keyframe : processedKeyframes) {
        offsets.append(keyframe.offset);
        compositeOperations.append(keyframe.composite);

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

        KeyframeValue keyframeValue(keyframe.computedOffset.value(), nullptr);
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
        auto timingFunctionResult = TimingFunction::createFromCSSText(keyframe.easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
        timingFunctions.append(timingFunctionResult.returnValue());
    }

    // 9. Parse each of the values in unused easings using the CSS syntax defined for easing property of the
    //    AnimationEffectTimingReadOnly interface, and if any of the values fail to parse, throw a TypeError
    //    and abort this procedure.
    for (auto& easing : unusedEasings) {
        auto timingFunctionResult = TimingFunction::createFromCSSText(easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
    }

    m_offsets = WTFMove(offsets);
    m_keyframes = WTFMove(keyframeList);
    m_timingFunctions = WTFMove(timingFunctions);
    m_compositeOperations = WTFMove(compositeOperations);

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

void KeyframeEffect::apply(RenderStyle& targetStyle)
{
    if (!m_target)
        return;

    auto progress = iterationProgress();
    if (!progress)
        return;

    if (m_startedAccelerated && progress.value() >= 1) {
        m_startedAccelerated = false;
        animation()->acceleratedRunningStateDidChange();
    }

    bool needsToStartAccelerated = false;

    if (!m_started && !m_startedAccelerated) {
        needsToStartAccelerated = shouldRunAccelerated();
        m_startedAccelerated = needsToStartAccelerated;
        if (needsToStartAccelerated)
            animation()->acceleratedRunningStateDidChange();
    }
    m_started = true;

    if (!needsToStartAccelerated && !m_startedAccelerated)
        setAnimatedPropertiesInStyle(targetStyle, progress.value());

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
    if (!animation())
        return;

    if (!m_keyframes.size())
        return;

    auto progress = iterationProgress();
    if (!progress)
        return;

    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(renderer()->style());

    setAnimatedPropertiesInStyle(*animatedStyle.get(), progress.value());
}

void KeyframeEffect::setAnimatedPropertiesInStyle(RenderStyle& targetStyle, double iterationProgress)
{
    // 4.4.3. The effect value of a keyframe effect
    // https://drafts.csswg.org/web-animations-1/#the-effect-value-of-a-keyframe-animation-effect
    //
    // The effect value of a single property referenced by a keyframe effect as one of its target properties,
    // for a given iteration progress, current iteration and underlying value is calculated as follows.

    for (auto cssPropertyId : m_keyframes.properties()) {
        // 1. If iteration progress is unresolved abort this procedure.
        // 2. Let target property be the longhand property for which the effect value is to be calculated.
        // 3. If animation type of the target property is not animatable abort this procedure since the effect cannot be applied.
        // 4. Define the neutral value for composition as a value which, when combined with an underlying value using the add composite operation,
        //    produces the underlying value.

        // 5. Let property-specific keyframes be the result of getting the set of computed keyframes for this keyframe effect.
        // 6. Remove any keyframes from property-specific keyframes that do not have a property value for target property.
        unsigned numberOfKeyframesWithZeroOffset = 0;
        unsigned numberOfKeyframesWithOneOffset = 0;
        Vector<std::optional<size_t>> propertySpecificKeyframes;
        for (size_t i = 0; i < m_keyframes.size(); ++i) {
            auto& keyframe = m_keyframes[i];
            if (!keyframe.containsProperty(cssPropertyId))
                continue;
            auto offset = keyframe.key();
            if (!offset)
                numberOfKeyframesWithZeroOffset++;
            if (offset == 1)
                numberOfKeyframesWithOneOffset++;
            propertySpecificKeyframes.append(i);
        }

        // 7. If property-specific keyframes is empty, return underlying value.
        if (propertySpecificKeyframes.isEmpty())
            continue;

        // 8. If there is no keyframe in property-specific keyframes with a computed keyframe offset of 0, create a new keyframe with a computed keyframe
        //    offset of 0, a property value set to the neutral value for composition, and a composite operation of add, and prepend it to the beginning of
        //    property-specific keyframes.
        if (!numberOfKeyframesWithZeroOffset) {
            propertySpecificKeyframes.insert(0, std::nullopt);
            numberOfKeyframesWithZeroOffset = 1;
        }

        // 9. Similarly, if there is no keyframe in property-specific keyframes with a computed keyframe offset of 1, create a new keyframe with a computed
        //    keyframe offset of 1, a property value set to the neutral value for composition, and a composite operation of add, and append it to the end of
        //    property-specific keyframes.
        if (!numberOfKeyframesWithOneOffset) {
            propertySpecificKeyframes.append(std::nullopt);
            numberOfKeyframesWithOneOffset = 1;
        }

        // 10. Let interval endpoints be an empty sequence of keyframes.
        Vector<std::optional<size_t>> intervalEndpoints;

        // 11. Populate interval endpoints by following the steps from the first matching condition from below:
        if (iterationProgress < 0 && numberOfKeyframesWithZeroOffset > 1) {
            // If iteration progress < 0 and there is more than one keyframe in property-specific keyframes with a computed keyframe offset of 0,
            // Add the first keyframe in property-specific keyframes to interval endpoints.
            intervalEndpoints.append(propertySpecificKeyframes.first());
        } else if (iterationProgress >= 1 && numberOfKeyframesWithOneOffset > 1) {
            // If iteration progress ≥ 1 and there is more than one keyframe in property-specific keyframes with a computed keyframe offset of 1,
            // Add the last keyframe in property-specific keyframes to interval endpoints.
            intervalEndpoints.append(propertySpecificKeyframes.last());
        } else {
            // Otherwise,
            // 1. Append to interval endpoints the last keyframe in property-specific keyframes whose computed keyframe offset is less than or equal
            //    to iteration progress and less than 1. If there is no such keyframe (because, for example, the iteration progress is negative),
            //    add the last keyframe whose computed keyframe offset is 0.
            // 2. Append to interval endpoints the next keyframe in property-specific keyframes after the one added in the previous step.
            size_t indexOfLastKeyframeWithZeroOffset = 0;
            int indexOfFirstKeyframeToAddToIntervalEndpoints = -1;
            for (size_t i = 0; i < propertySpecificKeyframes.size(); ++i) {
                auto keyframeIndex = propertySpecificKeyframes[i];
                auto offset = [&] () -> double {
                    if (!keyframeIndex)
                        return i ? 1 : 0;
                    return m_keyframes[keyframeIndex.value()].key();
                }();
                if (!offset)
                    indexOfLastKeyframeWithZeroOffset = i;
                if (offset <= iterationProgress && offset < 1)
                    indexOfFirstKeyframeToAddToIntervalEndpoints = i;
                else
                    break;
            }

            if (indexOfFirstKeyframeToAddToIntervalEndpoints >= 0) {
                intervalEndpoints.append(propertySpecificKeyframes[indexOfFirstKeyframeToAddToIntervalEndpoints]);
                intervalEndpoints.append(propertySpecificKeyframes[indexOfFirstKeyframeToAddToIntervalEndpoints + 1]);
            } else {
                ASSERT(indexOfLastKeyframeWithZeroOffset < propertySpecificKeyframes.size() - 1);
                intervalEndpoints.append(propertySpecificKeyframes[indexOfLastKeyframeWithZeroOffset]);
                intervalEndpoints.append(propertySpecificKeyframes[indexOfLastKeyframeWithZeroOffset + 1]);
            }
        }

        // 12. For each keyframe in interval endpoints…
        // FIXME: we don't support this step yet since we don't deal with any composite operation other than "replace".

        // 13. If there is only one keyframe in interval endpoints return the property value of target property on that keyframe.
        if (intervalEndpoints.size() == 1) {
            auto keyframeIndex = intervalEndpoints[0];
            auto keyframeStyle = !keyframeIndex ? &targetStyle : m_keyframes[keyframeIndex.value()].style();
            CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, keyframeStyle, keyframeStyle, 0);
            continue;
        }

        // 14. Let start offset be the computed keyframe offset of the first keyframe in interval endpoints.
        auto startKeyframeIndex = intervalEndpoints.first();
        auto startOffset = !startKeyframeIndex ? 0 : m_keyframes[startKeyframeIndex.value()].key();

        // 15. Let end offset be the computed keyframe offset of last keyframe in interval endpoints.
        auto endKeyframeIndex = intervalEndpoints.last();
        auto endOffset = !endKeyframeIndex ? 1 : m_keyframes[endKeyframeIndex.value()].key();

        // 16. Let interval distance be the result of evaluating (iteration progress - start offset) / (end offset - start offset).
        auto intervalDistance = (iterationProgress - startOffset) / (endOffset - startOffset);

        // 17. Let transformed distance be the result of evaluating the timing function associated with the first keyframe in interval endpoints
        //     passing interval distance as the input progress.
        auto transformedDistance = intervalDistance;
        if (startKeyframeIndex) {
            if (auto iterationDuration = timing()->iterationDuration()) {
                auto rangeDuration = (endOffset - startOffset) * iterationDuration.seconds();
                transformedDistance = m_timingFunctions[startKeyframeIndex.value()]->transformTime(intervalDistance, rangeDuration);
            }
        }

        // 18. Return the result of applying the interpolation procedure defined by the animation type of the target property, to the values of the target
        //     property specified on the two keyframes in interval endpoints taking the first such value as Vstart and the second as Vend and using transformed
        //     distance as the interpolation parameter p.
        auto startStyle = !startKeyframeIndex ? &targetStyle : m_keyframes[startKeyframeIndex.value()].style();
        auto endStyle = !endKeyframeIndex ? &targetStyle : m_keyframes[endKeyframeIndex.value()].style();
        CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, startStyle, endStyle, transformedDistance);
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
        animation->setDuration(timing()->iterationDuration().seconds());
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
