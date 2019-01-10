/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "CSSAnimation.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSKeyframeRule.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransition.h"
#include "Element.h"
#include "FontCascade.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "JSCompositeOperation.h"
#include "JSCompositeOperationOrAuto.h"
#include "JSKeyframeEffect.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "StylePendingResources.h"
#include "StyleResolver.h"
#include "TimingFunction.h"
#include "TranslateTransformOperation.h"
#include "WillChangeData.h"
#include <JavaScriptCore/Exception.h>
#include <wtf/UUID.h>

namespace WebCore {
using namespace JSC;

static inline void invalidateElement(Element* element)
{
    if (element)
        element->invalidateStyleAndLayerComposition();
}

static inline String CSSPropertyIDToIDLAttributeName(CSSPropertyID cssPropertyId)
{
    // https://drafts.csswg.org/web-animations-1/#animation-property-name-to-idl-attribute-name
    // 1. If property follows the <custom-property-name> production, return property.
    // FIXME: We don't handle custom properties yet.

    // 2. If property refers to the CSS float property, return the string "cssFloat".
    if (cssPropertyId == CSSPropertyFloat)
        return "cssFloat";

    // 3. If property refers to the CSS offset property, return the string "cssOffset".
    // FIXME: we don't support the CSS "offset" property

    // 4. Otherwise, return the result of applying the CSS property to IDL attribute algorithm [CSSOM] to property.
    return getJSPropertyName(cssPropertyId);
}

static inline CSSPropertyID IDLAttributeNameToAnimationPropertyName(const String& idlAttributeName)
{
    // https://drafts.csswg.org/web-animations-1/#idl-attribute-name-to-animation-property-name
    // 1. If attribute conforms to the <custom-property-name> production, return attribute.
    // FIXME: We don't handle custom properties yet.

    // 2. If attribute is the string "cssFloat", then return an animation property representing the CSS float property.
    if (idlAttributeName == "cssFloat")
        return CSSPropertyFloat;

    // 3. If attribute is the string "cssOffset", then return an animation property representing the CSS offset property.
    // FIXME: We don't support the CSS "offset" property.

    // 4. Otherwise, return the result of applying the IDL attribute to CSS property algorithm [CSSOM] to attribute.
    auto cssPropertyId = CSSStyleDeclaration::getCSSPropertyIDFromJavaScriptPropertyName(idlAttributeName);

    // We need to check that converting the property back to IDL form yields the same result such that a property passed
    // in non-IDL form is rejected, for instance "font-size".
    if (idlAttributeName != CSSPropertyIDToIDLAttributeName(cssPropertyId))
        return CSSPropertyInvalid;

    return cssPropertyId;
}

static inline void computeMissingKeyframeOffsets(Vector<KeyframeEffect::ParsedKeyframe>& keyframes)
{
    // https://drafts.csswg.org/web-animations-1/#compute-missing-keyframe-offsets

    if (keyframes.isEmpty())
        return;

    // 1. For each keyframe, in keyframes, let the computed keyframe offset of the keyframe be equal to its keyframe offset value.
    // In our implementation, we only set non-null values to avoid making computedOffset Optional<double>. Instead, we'll know
    // that a keyframe hasn't had a computed offset by checking if it has a null offset and a 0 computedOffset, since the first
    // keyframe will already have a 0 computedOffset.
    for (auto& keyframe : keyframes) {
        auto computedOffset = keyframe.offset;
        keyframe.computedOffset = computedOffset ? *computedOffset : 0;
    }

    // 2. If keyframes contains more than one keyframe and the computed keyframe offset of the first keyframe in keyframes is null,
    //    set the computed keyframe offset of the first keyframe to 0.
    if (keyframes.size() > 1 && !keyframes[0].offset)
        keyframes[0].computedOffset = 0;

    // 3. If the computed keyframe offset of the last keyframe in keyframes is null, set its computed keyframe offset to 1.
    if (!keyframes.last().offset)
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
        // Keyframes with a null offset that don't yet have a non-zero computed offset are keyframes
        // with an offset that needs to be computed.
        if (!keyframe.offset && !keyframe.computedOffset)
            continue;
        if (indexOfLastKeyframeWithNonNullOffset != i - 1) {
            double lastNonNullOffset = keyframes[indexOfLastKeyframeWithNonNullOffset].computedOffset;
            double offsetDelta = keyframe.computedOffset - lastNonNullOffset;
            double offsetIncrement = offsetDelta / (i - indexOfLastKeyframeWithNonNullOffset);
            size_t indexOfFirstKeyframeWithNullOffset = indexOfLastKeyframeWithNonNullOffset + 1;
            for (size_t j = indexOfFirstKeyframeWithNullOffset; j < i; ++j)
                keyframes[j].computedOffset = lastNonNullOffset + (j - indexOfLastKeyframeWithNonNullOffset) * offsetIncrement;
        }
        indexOfLastKeyframeWithNonNullOffset = i;
    }
}

static inline ExceptionOr<KeyframeEffect::KeyframeLikeObject> processKeyframeLikeObject(ExecState& state, Strong<JSObject>&& keyframesInput, bool allowLists)
{
    // https://drafts.csswg.org/web-animations-1/#process-a-keyframe-like-object

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Run the procedure to convert an ECMAScript value to a dictionary type [WEBIDL] with keyframe input as the ECMAScript value as follows:
    // 
    //    If allow lists is true, use the following dictionary type:
    //
    //    dictionary BasePropertyIndexedKeyframe {
    //        (double? or sequence<double?>)                                   offset = [];
    //        (DOMString or sequence<DOMString>)                               easing = [];
    //        (CompositeOperationOrAuto or sequence<CompositeOperationOrAuto>) composite = [];
    //    };
    //
    //    Otherwise, use the following dictionary type:
    //
    //    dictionary BaseKeyframe {
    //        double?                  offset = null;
    //        DOMString                easing = "linear";
    //        CompositeOperationOrAuto composite = "auto";
    //    };
    //
    //    Store the result of this procedure as keyframe output.
    KeyframeEffect::BasePropertyIndexedKeyframe baseProperties;
    if (allowLists)
        baseProperties = convert<IDLDictionary<KeyframeEffect::BasePropertyIndexedKeyframe>>(state, keyframesInput.get());
    else {
        auto baseKeyframe = convert<IDLDictionary<KeyframeEffect::BaseKeyframe>>(state, keyframesInput.get());
        if (baseKeyframe.offset)
            baseProperties.offset = baseKeyframe.offset.value();
        else
            baseProperties.offset = nullptr;
        baseProperties.easing = baseKeyframe.easing;
        baseProperties.composite = baseKeyframe.composite;
    }
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
    Vector<JSC::Identifier> animationProperties;
    size_t numberOfProperties = inputProperties.size();
    for (size_t i = 0; i < numberOfProperties; ++i) {
        if (CSSPropertyAnimation::isPropertyAnimatable(IDLAttributeNameToAnimationPropertyName(inputProperties[i].string())))
            animationProperties.append(inputProperties[i]);
    }

    // 5. Sort animation properties in ascending order by the Unicode codepoints that define each property name.
    std::sort(animationProperties.begin(), animationProperties.end(), [](auto& lhs, auto& rhs) {
        return lhs.string().utf8() < rhs.string().utf8();
    });

    // 6. For each property name in animation properties,
    size_t numberOfAnimationProperties = animationProperties.size();
    for (size_t i = 0; i < numberOfAnimationProperties; ++i) {
        // 1. Let raw value be the result of calling the [[Get]] internal method on keyframe input, with property name as the property
        //    key and keyframe input as the receiver.
        auto rawValue = keyframesInput->get(&state, animationProperties[i]);

        // 2. Check the completion record of raw value.
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });

        // 3. Convert raw value to a DOMString or sequence of DOMStrings property values as follows:
        Vector<String> propertyValues;
        if (allowLists) {
            // If allow lists is true,
            // Let property values be the result of converting raw value to IDL type (DOMString or sequence<DOMString>)
            // using the procedures defined for converting an ECMAScript value to an IDL value [WEBIDL].
            // If property values is a single DOMString, replace property values with a sequence of DOMStrings with the original value of property
            // Values as the only element.
            if (rawValue.isString())
                propertyValues = { rawValue.toWTFString(&state) };
            else if (rawValue.isObject())
                propertyValues = convert<IDLSequence<IDLDOMString>>(state, rawValue);
        } else {
            // Otherwise,
            // Let property values be the result of converting raw value to a DOMString using the procedure for converting an ECMAScript value to a DOMString.
            propertyValues = { convert<IDLDOMString>(state, rawValue) };
        }
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });

        // 4. Calculate the normalized property name as the result of applying the IDL attribute name to animation property name algorithm to property name.
        auto cssPropertyID = IDLAttributeNameToAnimationPropertyName(animationProperties[i].string());

        // 5. Add a property to to keyframe output with normalized property name as the property name, and property values as the property value.
        keyframeOuput.propertiesAndValues.append({ cssPropertyID, propertyValues });
    }

    // 7. Return keyframe output.
    return { WTFMove(keyframeOuput) };
}

static inline ExceptionOr<void> processIterableKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput, JSValue method, Vector<KeyframeEffect::ParsedKeyframe>& parsedKeyframes)
{
    // 1. Let iter be GetIterator(object, method).
    forEachInIterable(state, keyframesInput.get(), method, [&parsedKeyframes](VM& vm, ExecState& state, JSValue nextValue) -> ExceptionOr<void> {
        // Steps 2 through 6 are already implemented by forEachInIterable().
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!nextValue || !nextValue.isObject()) {
            throwException(&state, scope, JSC::Exception::create(vm, createTypeError(&state)));
            return { };
        }

        // 7. Append to processed keyframes the result of running the procedure to process a keyframe-like object passing nextItem
        // as the keyframe input and with the allow lists flag set to false.
        auto processKeyframeLikeObjectResult = processKeyframeLikeObject(state, Strong<JSObject>(vm, nextValue.toObject(&state)), false);
        if (processKeyframeLikeObjectResult.hasException())
            return processKeyframeLikeObjectResult.releaseException();
        auto keyframeLikeObject = processKeyframeLikeObjectResult.returnValue();

        KeyframeEffect::ParsedKeyframe keyframeOutput;

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only offset
        // alternatives we should expect are double and nullptr.
        if (WTF::holds_alternative<double>(keyframeLikeObject.baseProperties.offset))
            keyframeOutput.offset = WTF::get<double>(keyframeLikeObject.baseProperties.offset);
        else
            ASSERT(WTF::holds_alternative<std::nullptr_t>(keyframeLikeObject.baseProperties.offset));

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only easing
        // alternative we should expect is String.
        ASSERT(WTF::holds_alternative<String>(keyframeLikeObject.baseProperties.easing));
        keyframeOutput.easing = WTF::get<String>(keyframeLikeObject.baseProperties.easing);

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only composite
        // alternatives we should expect is CompositeOperationAuto.
        ASSERT(WTF::holds_alternative<CompositeOperationOrAuto>(keyframeLikeObject.baseProperties.composite));
        keyframeOutput.composite = WTF::get<CompositeOperationOrAuto>(keyframeLikeObject.baseProperties.composite);

        for (auto& propertyAndValue : keyframeLikeObject.propertiesAndValues) {
            auto cssPropertyId = propertyAndValue.property;
            // When calling processKeyframeLikeObject() with the "allow lists" flag set to false,
            // there should only ever be a single value for a given property.
            ASSERT(propertyAndValue.values.size() == 1);
            auto stringValue = propertyAndValue.values[0];
            if (keyframeOutput.style->setProperty(cssPropertyId, stringValue))
                keyframeOutput.unparsedStyle.set(cssPropertyId, stringValue);
        }

        parsedKeyframes.append(WTFMove(keyframeOutput));

        return { };
    });

    return { };
}

static inline ExceptionOr<void> processPropertyIndexedKeyframes(ExecState& state, Strong<JSObject>&& keyframesInput, Vector<KeyframeEffect::ParsedKeyframe>& parsedKeyframes, Vector<String>& unusedEasings)
{
    // 1. Let property-indexed keyframe be the result of running the procedure to process a keyframe-like object passing object as the keyframe input.
    auto processKeyframeLikeObjectResult = processKeyframeLikeObject(state, WTFMove(keyframesInput), true);
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
        Vector<KeyframeEffect::ParsedKeyframe> propertyKeyframes;
        // 5. For each value, v, in property values perform the following steps:
        for (auto& v : propertyValues) {
            // 1. Let k be a new keyframe with a null keyframe offset.
            KeyframeEffect::ParsedKeyframe k;
            // 2. Add the property-value pair, property name → v, to k.
            if (k.style->setProperty(propertyName, v))
                k.unparsedStyle.set(propertyName, v);
            // 3. Append k to property keyframes.
            propertyKeyframes.append(WTFMove(k));
        }
        // 6. Apply the procedure to compute missing keyframe offsets to property keyframes.
        computeMissingKeyframeOffsets(propertyKeyframes);

        // 7. Add keyframes in property keyframes to processed keyframes.
        for (auto& keyframe : propertyKeyframes)
            parsedKeyframes.append(WTFMove(keyframe));
    }

    // 3. Sort processed keyframes by the computed keyframe offset of each keyframe in increasing order.
    std::sort(parsedKeyframes.begin(), parsedKeyframes.end(), [](auto& lhs, auto& rhs) {
        return lhs.computedOffset < rhs.computedOffset;
    });

    // 4. Merge adjacent keyframes in processed keyframes when they have equal computed keyframe offsets.
    size_t i = 1;
    while (i < parsedKeyframes.size()) {
        auto& keyframe = parsedKeyframes[i];
        auto& previousKeyframe = parsedKeyframes[i - 1];
        // If the offsets of this keyframe and the previous keyframe are different,
        // this means that the two keyframes should not be merged and we can move
        // on to the next keyframe.
        if (keyframe.computedOffset != previousKeyframe.computedOffset) {
            i++;
            continue;
        }
        // Otherwise, both this keyframe and the previous keyframe should be merged.
        // Unprocessed keyframes in parsedKeyframes at this stage have at most a single
        // property in cssPropertiesAndValues, so just set this on the previous keyframe.
        // In case an invalid or null value was originally provided, then the property
        // was not set and the property count is 0, in which case there is nothing to merge.
        if (keyframe.style->propertyCount()) {
            auto property = keyframe.style->propertyAt(0);
            previousKeyframe.style->setProperty(property.id(), property.value());
            previousKeyframe.unparsedStyle.set(property.id(), keyframe.unparsedStyle.get(property.id()));
        }
        // Since we've processed this keyframe, we can remove it and keep i the same
        // so that we process the next keyframe in the next loop iteration.
        parsedKeyframes.remove(i);
    }

    // 5. Let offsets be a sequence of nullable double values assigned based on the type of the “offset” member of the property-indexed keyframe as follows:
    //    - sequence<double?>, the value of “offset” as-is.
    //    - double?, a sequence of length one with the value of “offset” as its single item, i.e. « offset »,
    Vector<Optional<double>> offsets;
    if (WTF::holds_alternative<Vector<Optional<double>>>(propertyIndexedKeyframe.baseProperties.offset))
        offsets = WTF::get<Vector<Optional<double>>>(propertyIndexedKeyframe.baseProperties.offset);
    else if (WTF::holds_alternative<double>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(WTF::get<double>(propertyIndexedKeyframe.baseProperties.offset));
    else if (WTF::holds_alternative<std::nullptr_t>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(WTF::nullopt);

    // 6. Assign each value in offsets to the keyframe offset of the keyframe with corresponding position in property keyframes until the end of either sequence is reached.
    for (size_t i = 0; i < offsets.size() && i < parsedKeyframes.size(); ++i)
        parsedKeyframes[i].offset = offsets[i];

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
    if (easings.size() < parsedKeyframes.size()) {
        size_t initialNumberOfEasings = easings.size();
        for (i = initialNumberOfEasings; i < parsedKeyframes.size(); ++i)
            easings.append(easings[i % initialNumberOfEasings]);
    }

    // 10. If easings has more items than property keyframes, store the excess items as unused easings.
    while (easings.size() > parsedKeyframes.size())
        unusedEasings.append(easings.takeLast());

    // 11. Assign each value in easings to a property named “easing” on the keyframe with the corresponding position in property keyframes until the end of property keyframes
    //     is reached.
    for (size_t i = 0; i < parsedKeyframes.size(); ++i)
        parsedKeyframes[i].easing = easings[i];

    // 12. If the “composite” member of the property-indexed keyframe is not an empty sequence:
    Vector<CompositeOperationOrAuto> compositeModes;
    if (WTF::holds_alternative<Vector<CompositeOperationOrAuto>>(propertyIndexedKeyframe.baseProperties.composite))
        compositeModes = WTF::get<Vector<CompositeOperationOrAuto>>(propertyIndexedKeyframe.baseProperties.composite);
    else if (WTF::holds_alternative<CompositeOperationOrAuto>(propertyIndexedKeyframe.baseProperties.composite))
        compositeModes.append(WTF::get<CompositeOperationOrAuto>(propertyIndexedKeyframe.baseProperties.composite));
    if (!compositeModes.isEmpty()) {
        // 1. Let composite modes be a sequence of CompositeOperationOrAuto values assigned from the “composite” member of property-indexed keyframe. If that member is a single
        //    CompositeOperationOrAuto value operation, let composite modes be a sequence of length one, with the value of the “composite” as its single item.
        // 2. As with easings, if composite modes has fewer items than processed keyframes, repeat the elements in composite modes successively starting from the beginning of
        //    the list until composite modes has as many items as processed keyframes.
        if (compositeModes.size() < parsedKeyframes.size()) {
            size_t initialNumberOfCompositeModes = compositeModes.size();
            for (i = initialNumberOfCompositeModes; i < parsedKeyframes.size(); ++i)
                compositeModes.append(compositeModes[i % initialNumberOfCompositeModes]);
        }
        // 3. Assign each value in composite modes that is not auto to the keyframe-specific composite operation on the keyframe with the corresponding position in processed
        //    keyframes until the end of processed keyframes is reached.
        for (size_t i = 0; i < compositeModes.size() && i < parsedKeyframes.size(); ++i) {
            if (compositeModes[i] != CompositeOperationOrAuto::Auto)
                parsedKeyframes[i].composite = compositeModes[i];
        }
    }

    return { };
}

ExceptionOr<Ref<KeyframeEffect>> KeyframeEffect::create(ExecState& state, Element* target, Strong<JSObject>&& keyframes, Optional<Variant<double, KeyframeEffectOptions>>&& options)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(target));

    if (options) {
        OptionalEffectTiming timing;
        auto optionsValue = options.value();
        if (WTF::holds_alternative<double>(optionsValue)) {
            Variant<double, String> duration = WTF::get<double>(optionsValue);
            timing.duration = duration;
        } else {
            auto keyframeEffectOptions = WTF::get<KeyframeEffectOptions>(optionsValue);
            timing = {
                keyframeEffectOptions.duration,
                keyframeEffectOptions.iterations,
                keyframeEffectOptions.delay,
                keyframeEffectOptions.endDelay,
                keyframeEffectOptions.iterationStart,
                keyframeEffectOptions.easing,
                keyframeEffectOptions.fill,
                keyframeEffectOptions.direction
            };
        }
        auto updateTimingResult = keyframeEffect->updateTiming(timing);
        if (updateTimingResult.hasException())
            return updateTimingResult.releaseException();
    }

    auto processKeyframesResult = keyframeEffect->processKeyframes(state, WTFMove(keyframes));
    if (processKeyframesResult.hasException())
        return processKeyframesResult.releaseException();

    return WTFMove(keyframeEffect);
}

ExceptionOr<Ref<KeyframeEffect>> KeyframeEffect::create(JSC::ExecState&, Ref<KeyframeEffect>&& source)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(nullptr));
    keyframeEffect->copyPropertiesFromSource(WTFMove(source));
    return WTFMove(keyframeEffect);
}

Ref<KeyframeEffect> KeyframeEffect::create(const Element& target)
{
    return adoptRef(*new KeyframeEffect(const_cast<Element*>(&target)));
}

KeyframeEffect::KeyframeEffect(Element* target)
    : m_target(target)
{
}

void KeyframeEffect::copyPropertiesFromSource(Ref<KeyframeEffect>&& source)
{
    m_target = source->m_target;
    m_compositeOperation = source->m_compositeOperation;
    m_iterationCompositeOperation = source->m_iterationCompositeOperation;

    Vector<ParsedKeyframe> parsedKeyframes;
    for (auto& sourceParsedKeyframe : source->m_parsedKeyframes) {
        ParsedKeyframe parsedKeyframe;
        parsedKeyframe.easing = sourceParsedKeyframe.easing;
        parsedKeyframe.offset = sourceParsedKeyframe.offset;
        parsedKeyframe.composite = sourceParsedKeyframe.composite;
        parsedKeyframe.unparsedStyle = sourceParsedKeyframe.unparsedStyle;
        parsedKeyframe.computedOffset = sourceParsedKeyframe.computedOffset;
        parsedKeyframe.timingFunction = sourceParsedKeyframe.timingFunction;
        parsedKeyframe.style = sourceParsedKeyframe.style->mutableCopy();
        parsedKeyframes.append(WTFMove(parsedKeyframe));
    }
    m_parsedKeyframes = WTFMove(parsedKeyframes);

    setFill(source->fill());
    setDelay(source->delay());
    setEndDelay(source->endDelay());
    setDirection(source->direction());
    setIterations(source->iterations());
    setTimingFunction(source->timingFunction());
    setIterationStart(source->iterationStart());
    setIterationDuration(source->iterationDuration());

    KeyframeList keyframeList("keyframe-effect-" + createCanonicalUUIDString());
    for (auto& keyframe : source->m_blendingKeyframes.keyframes()) {
        KeyframeValue keyframeValue(keyframe.key(), RenderStyle::clonePtr(*keyframe.style()));
        for (auto propertyId : keyframe.properties())
            keyframeValue.addProperty(propertyId);
        keyframeList.insert(WTFMove(keyframeValue));
    }
    setBlendingKeyframes(keyframeList);
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
    if (is<DeclarativeAnimation>(animation())) {
        auto computedStyleExtractor = ComputedStyleExtractor(m_target.get());
        for (size_t i = 0; i < m_blendingKeyframes.size(); ++i) {
            // 1. Initialize a dictionary object, output keyframe, using the following definition:
            //
            // dictionary BaseComputedKeyframe {
            //      double?                  offset = null;
            //      double                   computedOffset;
            //      DOMString                easing = "linear";
            //      CompositeOperationOrAuto composite = "auto";
            // };

            auto& keyframe = m_blendingKeyframes[i];

            // 2. Set offset, computedOffset, easing members of output keyframe to the respective values keyframe offset, computed keyframe offset,
            // and keyframe-specific timing function of keyframe.
            BaseComputedKeyframe computedKeyframe;
            computedKeyframe.offset = keyframe.key();
            computedKeyframe.computedOffset = keyframe.key();
            // For CSS transitions, there are only two keyframes and the second keyframe should always report "linear". In practice, this value
            // has no bearing since, as the last keyframe, its value will never be used.
            computedKeyframe.easing = is<CSSTransition>(animation()) && i == 1 ? "linear" : timingFunctionForKeyframeAtIndex(0)->cssText();

            auto outputKeyframe = convertDictionaryToJS(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), computedKeyframe);

            // 3. For each animation property-value pair specified on keyframe, declaration, perform the following steps:
            auto& style = *keyframe.style();
            for (auto cssPropertyId : keyframe.properties()) {
                if (cssPropertyId == CSSPropertyCustom)
                    continue;
                // 1. Let property name be the result of applying the animation property name to IDL attribute name algorithm to the property name of declaration.
                auto propertyName = CSSPropertyIDToIDLAttributeName(cssPropertyId);
                // 2. Let IDL value be the result of serializing the property value of declaration by passing declaration to the algorithm to serialize a CSS value.
                String idlValue = "";
                if (auto cssValue = computedStyleExtractor.valueForPropertyinStyle(style, cssPropertyId))
                    idlValue = cssValue->cssText();
                // 3. Let value be the result of converting IDL value to an ECMAScript String value.
                auto value = toJS<IDLDOMString>(state, idlValue);
                // 4. Call the [[DefineOwnProperty]] internal method on output keyframe with property name property name,
                //    Property Descriptor { [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true, [[Value]]: value } and Boolean flag false.
                JSObject::defineOwnProperty(outputKeyframe, &state, AtomicString(propertyName).impl(), PropertyDescriptor(value, 0), false);
            }

            // 5. Append output keyframe to result.
            result.append(JSC::Strong<JSC::JSObject> { state.vm(), outputKeyframe });
        }
    } else {
        for (size_t i = 0; i < m_parsedKeyframes.size(); ++i) {
            // 1. Initialize a dictionary object, output keyframe, using the following definition:
            //
            // dictionary BaseComputedKeyframe {
            //      double?                  offset = null;
            //      double                   computedOffset;
            //      DOMString                easing = "linear";
            //      CompositeOperationOrAuto composite = "auto";
            // };

            auto& parsedKeyframe = m_parsedKeyframes[i];

            // 2. Set offset, computedOffset, easing, composite members of output keyframe to the respective values keyframe offset, computed keyframe
            // offset, keyframe-specific timing function and keyframe-specific composite operation of keyframe.
            BaseComputedKeyframe computedKeyframe;
            computedKeyframe.offset = parsedKeyframe.offset;
            computedKeyframe.computedOffset = parsedKeyframe.computedOffset;
            computedKeyframe.easing = timingFunctionForKeyframeAtIndex(i)->cssText();
            computedKeyframe.composite = parsedKeyframe.composite;

            auto outputKeyframe = convertDictionaryToJS(state, *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject()), computedKeyframe);

            // 3. For each animation property-value pair specified on keyframe, declaration, perform the following steps:
            for (auto it = parsedKeyframe.unparsedStyle.begin(), end = parsedKeyframe.unparsedStyle.end(); it != end; ++it) {
                // 1. Let property name be the result of applying the animation property name to IDL attribute name algorithm to the property name of declaration.
                auto propertyName = CSSPropertyIDToIDLAttributeName(it->key);
                // 2. Let IDL value be the result of serializing the property value of declaration by passing declaration to the algorithm to serialize a CSS value.
                // 3. Let value be the result of converting IDL value to an ECMAScript String value.
                auto value = toJS<IDLDOMString>(state, it->value);
                // 4. Call the [[DefineOwnProperty]] internal method on output keyframe with property name property name,
                //    Property Descriptor { [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true, [[Value]]: value } and Boolean flag false.
                JSObject::defineOwnProperty(outputKeyframe, &state, AtomicString(propertyName).impl(), PropertyDescriptor(value, 0), false);
            }

            // 4. Append output keyframe to result.
            result.append(JSC::Strong<JSC::JSObject> { state.vm(), outputKeyframe });
        }
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
    if (!keyframesInput.get())
        return { };

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 2. Let processed keyframes be an empty sequence of keyframes.
    Vector<ParsedKeyframe> parsedKeyframes;

    // 3. Let method be the result of GetMethod(object, @@iterator).
    auto method = keyframesInput.get()->get(&state, vm.propertyNames->iteratorSymbol);

    // 4. Check the completion record of method.
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    // 5. Perform the steps corresponding to the first matching condition from below,
    Vector<String> unusedEasings;
    if (!method.isUndefined())
        processIterableKeyframes(state, WTFMove(keyframesInput), WTFMove(method), parsedKeyframes);
    else
        processPropertyIndexedKeyframes(state, WTFMove(keyframesInput), parsedKeyframes, unusedEasings);

    // 6. If processed keyframes is not loosely sorted by offset, throw a TypeError and abort these steps.
    // 7. If there exist any keyframe in processed keyframes whose keyframe offset is non-null and less than
    //    zero or greater than one, throw a TypeError and abort these steps.
    double lastNonNullOffset = -1;
    for (auto& keyframe : parsedKeyframes) {
        if (!keyframe.offset)
            continue;
        auto offset = keyframe.offset.value();
        if (offset < lastNonNullOffset || offset < 0 || offset > 1)
            return Exception { TypeError };
        lastNonNullOffset = offset;
    }

    // We take a slight detour from the spec text and compute the missing keyframe offsets right away
    // since they can be computed up-front.
    computeMissingKeyframeOffsets(parsedKeyframes);

    // 8. For each frame in processed keyframes, perform the following steps:
    for (auto& keyframe : parsedKeyframes) {
        // Let the timing function of frame be the result of parsing the “easing” property on frame using the CSS syntax
        // defined for the easing property of the AnimationEffectTiming interface.
        // If parsing the “easing” property fails, throw a TypeError and abort this procedure.
        auto timingFunctionResult = TimingFunction::createFromCSSText(keyframe.easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
        keyframe.timingFunction = timingFunctionResult.returnValue();
    }

    // 9. Parse each of the values in unused easings using the CSS syntax defined for easing property of the
    //    AnimationEffectTiming interface, and if any of the values fail to parse, throw a TypeError
    //    and abort this procedure.
    for (auto& easing : unusedEasings) {
        auto timingFunctionResult = TimingFunction::createFromCSSText(easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
    }

    m_parsedKeyframes = WTFMove(parsedKeyframes);

    m_blendingKeyframes.clear();

    return { };
}

void KeyframeEffect::updateBlendingKeyframes(RenderStyle& elementStyle)
{
    if (!m_blendingKeyframes.isEmpty() || !m_target)
        return;

    KeyframeList keyframeList("keyframe-effect-" + createCanonicalUUIDString());
    StyleResolver& styleResolver = m_target->styleResolver();

    for (auto& keyframe : m_parsedKeyframes) {
        styleResolver.setNewStateWithElement(*m_target);
        KeyframeValue keyframeValue(keyframe.computedOffset, nullptr);

        auto styleProperties = keyframe.style->immutableCopyIfNeeded();
        for (unsigned i = 0; i < styleProperties->propertyCount(); ++i)
            keyframeList.addProperty(styleProperties->propertyAt(i).id());

        auto keyframeRule = StyleRuleKeyframe::create(WTFMove(styleProperties));
        keyframeValue.setStyle(styleResolver.styleForKeyframe(&elementStyle, keyframeRule.ptr(), keyframeValue));
        keyframeList.insert(WTFMove(keyframeValue));
    }

    setBlendingKeyframes(keyframeList);
}

bool KeyframeEffect::forceLayoutIfNeeded()
{
    if (!m_needsForcedLayout || !m_target)
        return false;

    auto* renderer = m_target->renderer();
    if (!renderer || !renderer->parent())
        return false;

    auto* frameView = m_target->document().view();
    if (!frameView)
        return false;

    frameView->forceLayout();
    return true;
}

void KeyframeEffect::setBlendingKeyframes(KeyframeList& blendingKeyframes)
{
    m_blendingKeyframes = WTFMove(blendingKeyframes);

    computedNeedsForcedLayout();
    computeStackingContextImpact();
    computeShouldRunAccelerated();

    checkForMatchingTransformFunctionLists();
    checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    checkForMatchingBackdropFilterFunctionLists();
#endif
    checkForMatchingColorFilterFunctionLists();
}

void KeyframeEffect::checkForMatchingTransformFunctionLists()
{
    m_transformFunctionListsMatch = false;

    if (m_blendingKeyframes.size() < 2 || !m_blendingKeyframes.containsProperty(CSSPropertyTransform))
        return;

    // Empty transforms match anything, so find the first non-empty entry as the reference.
    size_t numKeyframes = m_blendingKeyframes.size();
    size_t firstNonEmptyTransformKeyframeIndex = numKeyframes;

    for (size_t i = 0; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = m_blendingKeyframes[i];
        if (currentKeyframe.style()->transform().operations().size()) {
            firstNonEmptyTransformKeyframeIndex = i;
            break;
        }
    }

    if (firstNonEmptyTransformKeyframeIndex == numKeyframes)
        return;

    const TransformOperations* firstVal = &m_blendingKeyframes[firstNonEmptyTransformKeyframeIndex].style()->transform();
    for (size_t i = firstNonEmptyTransformKeyframeIndex + 1; i < numKeyframes; ++i) {
        const KeyframeValue& currentKeyframe = m_blendingKeyframes[i];
        const TransformOperations* val = &currentKeyframe.style()->transform();

        // An empty transform list matches anything.
        if (val->operations().isEmpty())
            continue;

        if (!firstVal->operationsMatch(*val))
            return;
    }

    m_transformFunctionListsMatch = true;
}

bool KeyframeEffect::checkForMatchingFilterFunctionLists(CSSPropertyID propertyID, const std::function<const FilterOperations& (const RenderStyle&)>& filtersGetter) const
{
    if (m_blendingKeyframes.size() < 2 || !m_blendingKeyframes.containsProperty(propertyID))
        return false;

    // Empty filters match anything, so find the first non-empty entry as the reference.
    size_t numKeyframes = m_blendingKeyframes.size();
    size_t firstNonEmptyKeyframeIndex = numKeyframes;

    for (size_t i = 0; i < numKeyframes; ++i) {
        if (filtersGetter(*m_blendingKeyframes[i].style()).operations().size()) {
            firstNonEmptyKeyframeIndex = i;
            break;
        }
    }

    if (firstNonEmptyKeyframeIndex == numKeyframes)
        return false;

    auto& firstVal = filtersGetter(*m_blendingKeyframes[firstNonEmptyKeyframeIndex].style());
    for (size_t i = firstNonEmptyKeyframeIndex + 1; i < numKeyframes; ++i) {
        auto& value = filtersGetter(*m_blendingKeyframes[i].style());

        // An empty filter list matches anything.
        if (value.operations().isEmpty())
            continue;

        if (!firstVal.operationsMatch(value))
            return false;
    }

    return true;
}

void KeyframeEffect::checkForMatchingFilterFunctionLists()
{
    m_filterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.filter();
    });
}

#if ENABLE(FILTERS_LEVEL_2)
void KeyframeEffect::checkForMatchingBackdropFilterFunctionLists()
{
    m_backdropFilterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyWebkitBackdropFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.backdropFilter();
    });
}
#endif

void KeyframeEffect::checkForMatchingColorFilterFunctionLists()
{
    m_colorFilterFunctionListsMatch = checkForMatchingFilterFunctionLists(CSSPropertyAppleColorFilter, [] (const RenderStyle& style) -> const FilterOperations& {
        return style.appleColorFilter();
    });
}

void KeyframeEffect::computeDeclarativeAnimationBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    ASSERT(is<DeclarativeAnimation>(animation()));
    if (is<CSSAnimation>(animation()))
        computeCSSAnimationBlendingKeyframes();
    else if (is<CSSTransition>(animation()))
        computeCSSTransitionBlendingKeyframes(oldStyle, newStyle);
}

void KeyframeEffect::computeCSSAnimationBlendingKeyframes()
{
    ASSERT(is<CSSAnimation>(animation()));

    auto cssAnimation = downcast<CSSAnimation>(animation());
    auto& backingAnimation = cssAnimation->backingAnimation();

    KeyframeList keyframeList(backingAnimation.name());
    if (auto* styleScope = Style::Scope::forOrdinal(*m_target, backingAnimation.nameStyleScopeOrdinal()))
        styleScope->resolver().keyframeStylesForAnimation(*m_target, &cssAnimation->unanimatedStyle(), keyframeList);

    // Ensure resource loads for all the frames.
    for (auto& keyframe : keyframeList.keyframes()) {
        if (auto* style = const_cast<RenderStyle*>(keyframe.style()))
            Style::loadPendingResources(*style, m_target->document(), m_target.get());
    }

    setBlendingKeyframes(keyframeList);
}

void KeyframeEffect::computeCSSTransitionBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    ASSERT(is<CSSTransition>(animation()));

    if (!oldStyle || m_blendingKeyframes.size())
        return;

    auto property = downcast<CSSTransition>(animation())->property();

    auto toStyle = RenderStyle::clonePtr(newStyle);
    if (m_target)
        Style::loadPendingResources(*toStyle, m_target->document(), m_target.get());

    KeyframeList keyframeList("keyframe-effect-" + createCanonicalUUIDString());
    keyframeList.addProperty(property);

    KeyframeValue fromKeyframeValue(0, RenderStyle::clonePtr(*oldStyle));
    fromKeyframeValue.addProperty(property);
    keyframeList.insert(WTFMove(fromKeyframeValue));

    KeyframeValue toKeyframeValue(1, WTFMove(toStyle));
    toKeyframeValue.addProperty(property);
    keyframeList.insert(WTFMove(toKeyframeValue));

    setBlendingKeyframes(keyframeList);
}

void KeyframeEffect::computedNeedsForcedLayout()
{
    m_needsForcedLayout = false;
    if (is<CSSTransition>(animation()) || !m_blendingKeyframes.containsProperty(CSSPropertyTransform))
        return;

    size_t numberOfKeyframes = m_blendingKeyframes.size();
    for (size_t i = 0; i < numberOfKeyframes; i++) {
        auto* keyframeStyle = m_blendingKeyframes[i].style();
        if (!keyframeStyle) {
            ASSERT_NOT_REACHED();
            continue;
        }
        if (keyframeStyle->hasTransform()) {
            auto& transformOperations = keyframeStyle->transform();
            for (const auto& operation : transformOperations.operations()) {
                if (operation->isTranslateTransformOperationType()) {
                    auto translation = downcast<TranslateTransformOperation>(operation.get());
                    if (translation->x().isPercent() || translation->y().isPercent()) {
                        m_needsForcedLayout = true;
                        return;
                    }
                }
            }
        }
    }
}

void KeyframeEffect::computeStackingContextImpact()
{
    m_triggersStackingContext = false;
    for (auto cssPropertyId : m_blendingKeyframes.properties()) {
        if (WillChangeData::propertyCreatesStackingContext(cssPropertyId)) {
            m_triggersStackingContext = true;
            break;
        }
    }
}

void KeyframeEffect::setTarget(RefPtr<Element>&& newTarget)
{
    if (m_target == newTarget)
        return;

    auto previousTarget = std::exchange(m_target, WTFMove(newTarget));

    if (auto* effectAnimation = animation())
        effectAnimation->effectTargetDidChange(previousTarget.get(), m_target.get());

    m_blendingKeyframes.clear();

    // We need to invalidate the effect now that the target has changed
    // to ensure the effect's styles are applied to the new target right away.
    invalidate();

    // Likewise, we need to invalidate styles on the previous target so that
    // any animated styles are removed immediately.
    invalidateElement(previousTarget.get());
}

void KeyframeEffect::apply(RenderStyle& targetStyle)
{
    if (!m_target)
        return;

    updateBlendingKeyframes(targetStyle);

    updateAcceleratedAnimationState();

    auto progress = getComputedTiming().progress;
    if (!progress)
        return;

    setAnimatedPropertiesInStyle(targetStyle, progress.value());

    // https://w3c.github.io/web-animations/#side-effects-section
    // For every property targeted by at least one animation effect that is current or in effect, the user agent
    // must act as if the will-change property ([css-will-change-1]) on the target element includes the property.
    if (m_triggersStackingContext && targetStyle.hasAutoZIndex())
        targetStyle.setZIndex(0);
}

void KeyframeEffect::invalidate()
{
    invalidateElement(m_target.get());
}

void KeyframeEffect::computeShouldRunAccelerated()
{
    m_shouldRunAccelerated = hasBlendingKeyframes();
    for (auto cssPropertyId : m_blendingKeyframes.properties()) {
        if (!CSSPropertyAnimation::animationOfPropertyIsAccelerated(cssPropertyId)) {
            m_shouldRunAccelerated = false;
            return;
        }
    }
}

void KeyframeEffect::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    if (!m_target || !animation())
        return;

    auto progress = getComputedTiming().progress;
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

    updateBlendingKeyframes(targetStyle);
    if (m_blendingKeyframes.isEmpty())
        return;

    bool isCSSAnimation = is<CSSAnimation>(animation());

    for (auto cssPropertyId : m_blendingKeyframes.properties()) {
        // 1. If iteration progress is unresolved abort this procedure.
        // 2. Let target property be the longhand property for which the effect value is to be calculated.
        // 3. If animation type of the target property is not animatable abort this procedure since the effect cannot be applied.
        // 4. Define the neutral value for composition as a value which, when combined with an underlying value using the add composite operation,
        //    produces the underlying value.

        // 5. Let property-specific keyframes be the result of getting the set of computed keyframes for this keyframe effect.
        // 6. Remove any keyframes from property-specific keyframes that do not have a property value for target property.
        unsigned numberOfKeyframesWithZeroOffset = 0;
        unsigned numberOfKeyframesWithOneOffset = 0;
        Vector<Optional<size_t>> propertySpecificKeyframes;
        for (size_t i = 0; i < m_blendingKeyframes.size(); ++i) {
            auto& keyframe = m_blendingKeyframes[i];
            auto offset = keyframe.key();
            if (!keyframe.containsProperty(cssPropertyId)) {
                // If we're dealing with a CSS animation, we consider the first and last keyframes to always have the property listed
                // since the underlying style was provided and should be captured.
                if (!isCSSAnimation || (offset && offset < 1))
                    continue;
            }
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
            propertySpecificKeyframes.insert(0, WTF::nullopt);
            numberOfKeyframesWithZeroOffset = 1;
        }

        // 9. Similarly, if there is no keyframe in property-specific keyframes with a computed keyframe offset of 1, create a new keyframe with a computed
        //    keyframe offset of 1, a property value set to the neutral value for composition, and a composite operation of add, and append it to the end of
        //    property-specific keyframes.
        if (!numberOfKeyframesWithOneOffset) {
            propertySpecificKeyframes.append(WTF::nullopt);
            numberOfKeyframesWithOneOffset = 1;
        }

        // 10. Let interval endpoints be an empty sequence of keyframes.
        Vector<Optional<size_t>> intervalEndpoints;

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
                    return m_blendingKeyframes[keyframeIndex.value()].key();
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
            auto keyframeStyle = !keyframeIndex ? &targetStyle : m_blendingKeyframes[keyframeIndex.value()].style();
            CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, keyframeStyle, keyframeStyle, 0);
            continue;
        }

        // 14. Let start offset be the computed keyframe offset of the first keyframe in interval endpoints.
        auto startKeyframeIndex = intervalEndpoints.first();
        auto startOffset = !startKeyframeIndex ? 0 : m_blendingKeyframes[startKeyframeIndex.value()].key();

        // 15. Let end offset be the computed keyframe offset of last keyframe in interval endpoints.
        auto endKeyframeIndex = intervalEndpoints.last();
        auto endOffset = !endKeyframeIndex ? 1 : m_blendingKeyframes[endKeyframeIndex.value()].key();

        // 16. Let interval distance be the result of evaluating (iteration progress - start offset) / (end offset - start offset).
        auto intervalDistance = (iterationProgress - startOffset) / (endOffset - startOffset);

        // 17. Let transformed distance be the result of evaluating the timing function associated with the first keyframe in interval endpoints
        //     passing interval distance as the input progress.
        auto transformedDistance = intervalDistance;
        if (startKeyframeIndex) {
            if (auto duration = iterationDuration()) {
                auto rangeDuration = (endOffset - startOffset) * duration.seconds();
                if (auto* timingFunction = timingFunctionForKeyframeAtIndex(startKeyframeIndex.value()))
                    transformedDistance = timingFunction->transformTime(intervalDistance, rangeDuration);
            }
        }

        // 18. Return the result of applying the interpolation procedure defined by the animation type of the target property, to the values of the target
        //     property specified on the two keyframes in interval endpoints taking the first such value as Vstart and the second as Vend and using transformed
        //     distance as the interpolation parameter p.
        auto startStyle = !startKeyframeIndex ? &targetStyle : m_blendingKeyframes[startKeyframeIndex.value()].style();
        auto endStyle = !endKeyframeIndex ? &targetStyle : m_blendingKeyframes[endKeyframeIndex.value()].style();
        CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, startStyle, endStyle, transformedDistance);
    }
}

TimingFunction* KeyframeEffect::timingFunctionForKeyframeAtIndex(size_t index)
{
    if (!m_parsedKeyframes.isEmpty())
        return m_parsedKeyframes[index].timingFunction.get();

    auto effectAnimation = animation();
    if (is<DeclarativeAnimation>(effectAnimation)) {
        // If we're dealing with a CSS Animation, the timing function is specified either on the keyframe itself.
        if (is<CSSAnimation>(effectAnimation)) {
            if (auto* timingFunction = m_blendingKeyframes[index].timingFunction())
                return timingFunction;
        }

        // Failing that, or for a CSS Transition, the timing function is inherited from the backing Animation object.
        return downcast<DeclarativeAnimation>(effectAnimation)->backingAnimation().timingFunction();
    }

    return nullptr;
}

void KeyframeEffect::updateAcceleratedAnimationState()
{
    if (!m_shouldRunAccelerated)
        return;

    if (!renderer()) {
        if (isRunningAccelerated())
            addPendingAcceleratedAction(AcceleratedAction::Stop);
        return;
    }

    auto localTime = animation()->currentTime();

    // If we don't have a localTime or localTime < 0, we either don't have a start time or we're before the startTime
    // so we shouldn't be running.
    if (!localTime || localTime.value() < 0_s) {
        if (isRunningAccelerated())
            addPendingAcceleratedAction(AcceleratedAction::Stop);
        return;
    }

    auto playState = animation()->playState();
    if (playState == WebAnimation::PlayState::Paused) {
        if (m_lastRecordedAcceleratedAction != AcceleratedAction::Pause) {
            if (m_lastRecordedAcceleratedAction == AcceleratedAction::Stop)
                addPendingAcceleratedAction(AcceleratedAction::Play);
            addPendingAcceleratedAction(AcceleratedAction::Pause);
        }
        return;
    }

    if (playState == WebAnimation::PlayState::Finished) {
        if (isRunningAccelerated())
            addPendingAcceleratedAction(AcceleratedAction::Stop);
        else {
            m_lastRecordedAcceleratedAction = AcceleratedAction::Stop;
            m_pendingAcceleratedActions.clear();
            animation()->acceleratedStateDidChange();
        }
        return;
    }

    if (playState == WebAnimation::PlayState::Running && localTime >= 0_s) {
        if (m_lastRecordedAcceleratedAction != AcceleratedAction::Play)
            addPendingAcceleratedAction(AcceleratedAction::Play);
        return;
    }
}

void KeyframeEffect::addPendingAcceleratedAction(AcceleratedAction action)
{
    if (action == AcceleratedAction::Stop)
        m_pendingAcceleratedActions.clear();
    m_pendingAcceleratedActions.append(action);
    if (action != AcceleratedAction::Seek)
        m_lastRecordedAcceleratedAction = action;
    animation()->acceleratedStateDidChange();
}

void KeyframeEffect::animationDidSeek()
{
    // There is no need to seek if we're not playing an animation already. If seeking
    // means we're moving into an active state, we'll pick this up in apply().
    if (m_shouldRunAccelerated && isRunningAccelerated())
        addPendingAcceleratedAction(AcceleratedAction::Seek);
}

void KeyframeEffect::animationSuspensionStateDidChange(bool animationIsSuspended)
{
    if (m_shouldRunAccelerated)
        addPendingAcceleratedAction(animationIsSuspended ? AcceleratedAction::Pause : AcceleratedAction::Play);
}

void KeyframeEffect::applyPendingAcceleratedActions()
{
    // Once an accelerated animation has been committed, we no longer want to force a layout.
    // This should have been performed by a call to forceLayoutIfNeeded() prior to applying
    // pending accelerated actions.
    m_needsForcedLayout = false;

    if (m_pendingAcceleratedActions.isEmpty())
        return;

    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited())
        return;

    auto pendingAcceleratedActions = m_pendingAcceleratedActions;
    m_pendingAcceleratedActions.clear();

    auto* compositedRenderer = downcast<RenderBoxModelObject>(renderer);

    // To simplify the code we use a default of 0s for an unresolved current time since for a Stop action that is acceptable.
    auto timeOffset = animation()->currentTime().valueOr(0_s).seconds() - delay().seconds();

    for (const auto& action : pendingAcceleratedActions) {
        switch (action) {
        case AcceleratedAction::Play:
            if (!compositedRenderer->startAnimation(timeOffset, backingAnimationForCompositedRenderer().ptr(), m_blendingKeyframes)) {
                m_shouldRunAccelerated = false;
                m_lastRecordedAcceleratedAction = AcceleratedAction::Stop;
                animation()->acceleratedStateDidChange();
                return;
            }
            break;
        case AcceleratedAction::Pause:
            compositedRenderer->animationPaused(timeOffset, m_blendingKeyframes.animationName());
            break;
        case AcceleratedAction::Seek:
            compositedRenderer->animationSeeked(timeOffset, m_blendingKeyframes.animationName());
            break;
        case AcceleratedAction::Stop:
            compositedRenderer->animationFinished(m_blendingKeyframes.animationName());
            if (!m_target->document().renderTreeBeingDestroyed())
                m_target->invalidateStyleAndLayerComposition();
            break;
        }
    }
}

Ref<const Animation> KeyframeEffect::backingAnimationForCompositedRenderer() const
{
    auto effectAnimation = animation();
    if (is<DeclarativeAnimation>(effectAnimation))
        return downcast<DeclarativeAnimation>(effectAnimation)->backingAnimation();

    // FIXME: The iterationStart and endDelay AnimationEffectTiming properties do not have
    // corresponding Animation properties.
    auto animation = Animation::create();
    animation->setDuration(iterationDuration().seconds());
    animation->setDelay(delay().seconds());
    animation->setIterationCount(iterations());
    animation->setTimingFunction(timingFunction()->clone());

    switch (fill()) {
    case FillMode::None:
    case FillMode::Auto:
        animation->setFillMode(AnimationFillMode::None);
        break;
    case FillMode::Backwards:
        animation->setFillMode(AnimationFillMode::Backwards);
        break;
    case FillMode::Forwards:
        animation->setFillMode(AnimationFillMode::Forwards);
        break;
    case FillMode::Both:
        animation->setFillMode(AnimationFillMode::Both);
        break;
    }

    switch (direction()) {
    case PlaybackDirection::Normal:
        animation->setDirection(Animation::AnimationDirectionNormal);
        break;
    case PlaybackDirection::Alternate:
        animation->setDirection(Animation::AnimationDirectionAlternate);
        break;
    case PlaybackDirection::Reverse:
        animation->setDirection(Animation::AnimationDirectionReverse);
        break;
    case PlaybackDirection::AlternateReverse:
        animation->setDirection(Animation::AnimationDirectionAlternateReverse);
        break;
    }

    return WTFMove(animation);
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

bool KeyframeEffect::computeExtentOfTransformAnimation(LayoutRect& bounds) const
{
    ASSERT(m_blendingKeyframes.containsProperty(CSSPropertyTransform));

    if (!is<RenderBox>(renderer()))
        return true; // Non-boxes don't get transformed;

    auto& box = downcast<RenderBox>(*renderer());
    auto rendererBox = snapRectToDevicePixels(box.borderBoxRect(), box.document().deviceScaleFactor());

    auto cumulativeBounds = bounds;

    for (const auto& keyframe : m_blendingKeyframes.keyframes()) {
        const auto* keyframeStyle = keyframe.style();

        // FIXME: maybe for declarative animations we always say it's true for the first and last keyframe.
        if (!keyframe.containsProperty(CSSPropertyTransform)) {
            // If the first keyframe is missing transform style, use the current style.
            if (!keyframe.key())
                keyframeStyle = &box.style();
            else
                continue;
        }

        auto keyframeBounds = bounds;

        bool canCompute;
        if (transformFunctionListsMatch())
            canCompute = computeTransformedExtentViaTransformList(rendererBox, *keyframeStyle, keyframeBounds);
        else
            canCompute = computeTransformedExtentViaMatrix(rendererBox, *keyframeStyle, keyframeBounds);

        if (!canCompute)
            return false;

        cumulativeBounds.unite(keyframeBounds);
    }

    bounds = cumulativeBounds;
    return true;
}

static bool containsRotation(const Vector<RefPtr<TransformOperation>>& operations)
{
    for (const auto& operation : operations) {
        if (operation->type() == TransformOperation::ROTATE)
            return true;
    }
    return false;
}

bool KeyframeEffect::computeTransformedExtentViaTransformList(const FloatRect& rendererBox, const RenderStyle& style, LayoutRect& bounds) const
{
    FloatRect floatBounds = bounds;
    FloatPoint transformOrigin;

    bool applyTransformOrigin = containsRotation(style.transform().operations()) || style.transform().affectedByTransformOrigin();
    if (applyTransformOrigin) {
        transformOrigin.setX(rendererBox.x() + floatValueForLength(style.transformOriginX(), rendererBox.width()));
        transformOrigin.setY(rendererBox.y() + floatValueForLength(style.transformOriginY(), rendererBox.height()));
        // Ignore transformOriginZ because we'll bail if we encounter any 3D transforms.

        floatBounds.moveBy(-transformOrigin);
    }

    for (const auto& operation : style.transform().operations()) {
        if (operation->type() == TransformOperation::ROTATE) {
            // For now, just treat this as a full rotation. This could take angle into account to reduce inflation.
            floatBounds = boundsOfRotatingRect(floatBounds);
        } else {
            TransformationMatrix transform;
            operation->apply(transform, rendererBox.size());
            if (!transform.isAffine())
                return false;

            if (operation->type() == TransformOperation::MATRIX || operation->type() == TransformOperation::MATRIX_3D) {
                TransformationMatrix::Decomposed2Type toDecomp;
                transform.decompose2(toDecomp);
                // Any rotation prevents us from using a simple start/end rect union.
                if (toDecomp.angle)
                    return false;
            }

            floatBounds = transform.mapRect(floatBounds);
        }
    }

    if (applyTransformOrigin)
        floatBounds.moveBy(transformOrigin);

    bounds = LayoutRect(floatBounds);
    return true;
}

bool KeyframeEffect::computeTransformedExtentViaMatrix(const FloatRect& rendererBox, const RenderStyle& style, LayoutRect& bounds) const
{
    TransformationMatrix transform;
    style.applyTransform(transform, rendererBox, RenderStyle::IncludeTransformOrigin);
    if (!transform.isAffine())
        return false;

    TransformationMatrix::Decomposed2Type fromDecomp;
    transform.decompose2(fromDecomp);
    // Any rotation prevents us from using a simple start/end rect union.
    if (fromDecomp.angle)
        return false;

    bounds = LayoutRect(transform.mapRect(bounds));
    return true;
}

} // namespace WebCore
