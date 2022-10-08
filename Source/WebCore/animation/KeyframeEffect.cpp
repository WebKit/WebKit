/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "CSSKeyframeRule.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSSelector.h"
#include "CSSStyleDeclaration.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransition.h"
#include "ComputedStyleExtractor.h"
#include "Element.h"
#include "FontCascade.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "InspectorInstrumentation.h"
#include "JSCompositeOperation.h"
#include "JSCompositeOperationOrAuto.h"
#include "JSDOMConvert.h"
#include "JSKeyframeEffect.h"
#include "KeyframeEffectStack.h"
#include "Logging.h"
#include "PropertyAllowlist.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "Settings.h"
#include "StyleAdjuster.h"
#include "StylePendingResources.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyledElement.h"
#include "TimingFunction.h"
#include "TranslateTransformOperation.h"
#include "WillChangeData.h"
#include <JavaScriptCore/Exception.h>
#include <wtf/UUID.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
using namespace JSC;

static inline void invalidateElement(const std::optional<const Styleable>& styleable)
{
    if (styleable)
        styleable->element.invalidateStyleInternal();
}

String KeyframeEffect::CSSPropertyIDToIDLAttributeName(CSSPropertyID cssPropertyId)
{
    // https://drafts.csswg.org/web-animations-1/#animation-property-name-to-idl-attribute-name
    // 1. If property follows the <custom-property-name> production, return property.

    // 2. If property refers to the CSS float property, return the string "cssFloat".
    if (cssPropertyId == CSSPropertyFloat)
        return "cssFloat"_s;

    // 3. If property refers to the CSS offset property, return the string "cssOffset".
    if (cssPropertyId == CSSPropertyOffset)
        return "cssOffset"_s;

    // 4. Otherwise, return the result of applying the CSS property to IDL attribute algorithm [CSSOM] to property.
    return getJSPropertyName(cssPropertyId);
}

static inline CSSPropertyID IDLAttributeNameToAnimationPropertyName(const AtomString& idlAttributeName)
{
    // https://drafts.csswg.org/web-animations-1/#idl-attribute-name-to-animation-property-name
    // 1. If attribute conforms to the <custom-property-name> production, return attribute.

    // 2. If attribute is the string "cssFloat", then return an animation property representing the CSS float property.
    if (idlAttributeName == "cssFloat"_s)
        return CSSPropertyFloat;

    // 3. If attribute is the string "cssOffset", then return an animation property representing the CSS offset property.
    if (idlAttributeName == "cssOffset"_s)
        return CSSPropertyOffset;

    // 4. Otherwise, return the result of applying the IDL attribute to CSS property algorithm [CSSOM] to attribute.
    auto cssPropertyId = CSSStyleDeclaration::getCSSPropertyIDFromJavaScriptPropertyName(idlAttributeName);

    if (cssPropertyId == CSSPropertyInvalid && isCustomPropertyName(idlAttributeName))
        return CSSPropertyCustom;

    // We need to check that converting the property back to IDL form yields the same result such that a property passed
    // in non-IDL form is rejected, for instance "font-size".
    if (idlAttributeName != KeyframeEffect::CSSPropertyIDToIDLAttributeName(cssPropertyId))
        return CSSPropertyInvalid;

    return cssPropertyId;
}

static inline void computeMissingKeyframeOffsets(Vector<KeyframeEffect::ParsedKeyframe>& keyframes)
{
    // https://drafts.csswg.org/web-animations-1/#compute-missing-keyframe-offsets

    if (keyframes.isEmpty())
        return;

    // 1. For each keyframe, in keyframes, let the computed keyframe offset of the keyframe be equal to its keyframe offset value.
    // In our implementation, we only set non-null values to avoid making computedOffset std::optional<double>. Instead, we'll know
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

static inline ExceptionOr<KeyframeEffect::KeyframeLikeObject> processKeyframeLikeObject(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput, bool allowLists)
{
    // https://drafts.csswg.org/web-animations-1/#process-a-keyframe-like-object

    VM& vm = lexicalGlobalObject.vm();
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
        baseProperties = convert<IDLDictionary<KeyframeEffect::BasePropertyIndexedKeyframe>>(lexicalGlobalObject, keyframesInput.get());
    else {
        auto baseKeyframe = convert<IDLDictionary<KeyframeEffect::BaseKeyframe>>(lexicalGlobalObject, keyframesInput.get());
        if (baseKeyframe.offset)
            baseProperties.offset = baseKeyframe.offset.value();
        else
            baseProperties.offset = nullptr;
        baseProperties.easing = baseKeyframe.easing;

        if (document.settings().webAnimationsCompositeOperationsEnabled())
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
    PropertyNameArray inputProperties(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    JSObject::getOwnPropertyNames(keyframesInput.get(), &lexicalGlobalObject, inputProperties, DontEnumPropertiesMode::Exclude);

    // 4. Make up a new list animation properties that consists of all of the properties that are in both input properties and animatable
    //    properties, or which are in input properties and conform to the <custom-property-name> production.
    Vector<JSC::Identifier> animationProperties;
    for (auto& inputProperty : inputProperties) {
        auto cssProperty = IDLAttributeNameToAnimationPropertyName(inputProperty.string());
        if (!isCSSPropertyExposed(cssProperty, &document.settings()))
            cssProperty = CSSPropertyInvalid;
        auto resolvedCSSProperty = CSSProperty::resolveDirectionAwareProperty(cssProperty, RenderStyle::initialDirection(), RenderStyle::initialWritingMode());
        if (CSSPropertyAnimation::isPropertyAnimatable(resolvedCSSProperty))
            animationProperties.append(inputProperty);
    }

    // 5. Sort animation properties in ascending order by the Unicode codepoints that define each property name.
    std::sort(animationProperties.begin(), animationProperties.end(), [](auto& lhs, auto& rhs) {
        return codePointCompareLessThan(lhs.string().string(), rhs.string().string());
    });

    // 6. For each property name in animation properties,
    size_t numberOfAnimationProperties = animationProperties.size();
    for (size_t i = 0; i < numberOfAnimationProperties; ++i) {
        // 1. Let raw value be the result of calling the [[Get]] internal method on keyframe input, with property name as the property
        //    key and keyframe input as the receiver.
        auto rawValue = keyframesInput->get(&lexicalGlobalObject, animationProperties[i]);

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
            if (rawValue.isObject())
                propertyValues = convert<IDLSequence<IDLDOMString>>(lexicalGlobalObject, rawValue);
            else
                propertyValues = { rawValue.toWTFString(&lexicalGlobalObject) };
        } else {
            // Otherwise,
            // Let property values be the result of converting raw value to a DOMString using the procedure for converting an ECMAScript value to a DOMString.
            propertyValues = { convert<IDLDOMString>(lexicalGlobalObject, rawValue) };
        }
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });

        // 4. Calculate the normalized property name as the result of applying the IDL attribute name to animation property name algorithm to property name.
        auto propertyName = animationProperties[i].string();
        auto cssPropertyID = IDLAttributeNameToAnimationPropertyName(propertyName);
        ASSERT(isCSSPropertyExposed(cssPropertyID, &document.settings()));

        // 5. Add a property to to keyframe output with normalized property name as the property name, and property values as the property value.
        if (cssPropertyID == CSSPropertyCustom)
            keyframeOuput.propertiesAndValues.append({ cssPropertyID, propertyName, propertyValues });
        else
            keyframeOuput.propertiesAndValues.append({ cssPropertyID, emptyAtom(), propertyValues });
    }

    // 7. Return keyframe output.
    return { WTFMove(keyframeOuput) };
}

static inline ExceptionOr<void> processIterableKeyframes(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput, JSValue method, Vector<KeyframeEffect::ParsedKeyframe>& parsedKeyframes)
{
    CSSParserContext parserContext(document);

    // 1. Let iter be GetIterator(object, method).
    forEachInIterable(lexicalGlobalObject, keyframesInput.get(), method, [&parsedKeyframes, &document, &parserContext](VM& vm, JSGlobalObject& lexicalGlobalObject, JSValue nextValue) -> ExceptionOr<void> {
        // Steps 2 through 5 are already implemented by forEachInIterable().
        auto scope = DECLARE_THROW_SCOPE(vm);

        // 6. If Type(nextItem) is not Undefined, Null or Object, then throw a TypeError and abort these steps.
        if (!nextValue.isUndefinedOrNull() && !nextValue.isObject()) {
            throwException(&lexicalGlobalObject, scope, JSC::Exception::create(vm, createTypeError(&lexicalGlobalObject)));
            return { };
        }

        if (!nextValue.isObject()) {
            parsedKeyframes.append({ });
            return { };
        }

        // 7. Append to processed keyframes the result of running the procedure to process a keyframe-like object passing nextItem
        // as the keyframe input and with the allow lists flag set to false.
        auto processKeyframeLikeObjectResult = processKeyframeLikeObject(lexicalGlobalObject, document, Strong<JSObject>(vm, nextValue.toObject(&lexicalGlobalObject)), false);
        if (processKeyframeLikeObjectResult.hasException())
            return processKeyframeLikeObjectResult.releaseException();
        auto keyframeLikeObject = processKeyframeLikeObjectResult.returnValue();

        KeyframeEffect::ParsedKeyframe keyframeOutput;

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only offset
        // alternatives we should expect are double and nullptr.
        if (std::holds_alternative<double>(keyframeLikeObject.baseProperties.offset))
            keyframeOutput.offset = std::get<double>(keyframeLikeObject.baseProperties.offset);
        else
            ASSERT(std::holds_alternative<std::nullptr_t>(keyframeLikeObject.baseProperties.offset));

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only easing
        // alternative we should expect is String.
        ASSERT(std::holds_alternative<String>(keyframeLikeObject.baseProperties.easing));
        keyframeOutput.easing = std::get<String>(keyframeLikeObject.baseProperties.easing);

        // When calling processKeyframeLikeObject() with the "allow lists" flag set to false, the only composite
        // alternatives we should expect is CompositeOperationAuto.
        if (document.settings().webAnimationsCompositeOperationsEnabled()) {
            ASSERT(std::holds_alternative<CompositeOperationOrAuto>(keyframeLikeObject.baseProperties.composite));
            keyframeOutput.composite = std::get<CompositeOperationOrAuto>(keyframeLikeObject.baseProperties.composite);
        }

        for (auto& propertyAndValue : keyframeLikeObject.propertiesAndValues) {
            auto cssPropertyId = propertyAndValue.property;
            // When calling processKeyframeLikeObject() with the "allow lists" flag set to false,
            // there should only ever be a single value for a given property.
            ASSERT(propertyAndValue.values.size() == 1);
            auto stringValue = propertyAndValue.values[0];
            if (cssPropertyId == CSSPropertyCustom) {
                auto customProperty = propertyAndValue.customProperty;
                if (keyframeOutput.style->setCustomProperty(&document, customProperty, stringValue, false, parserContext))
                    keyframeOutput.customStyleStrings.set(customProperty, stringValue);
            } else if (keyframeOutput.style->setProperty(cssPropertyId, stringValue, false, parserContext))
                keyframeOutput.styleStrings.set(cssPropertyId, stringValue);
        }

        parsedKeyframes.append(WTFMove(keyframeOutput));

        return { };
    });

    return { };
}

static inline ExceptionOr<void> processPropertyIndexedKeyframes(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput, Vector<KeyframeEffect::ParsedKeyframe>& parsedKeyframes, Vector<String>& unusedEasings)
{
    // 1. Let property-indexed keyframe be the result of running the procedure to process a keyframe-like object passing object as the keyframe input.
    auto processKeyframeLikeObjectResult = processKeyframeLikeObject(lexicalGlobalObject, document, WTFMove(keyframesInput), true);
    if (processKeyframeLikeObjectResult.hasException())
        return processKeyframeLikeObjectResult.releaseException();
    auto propertyIndexedKeyframe = processKeyframeLikeObjectResult.returnValue();

    CSSParserContext parserContext(document);

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
            if (propertyName == CSSPropertyCustom) {
                auto customProperty = m.customProperty;
                if (k.style->setCustomProperty(&document, customProperty, v, false, parserContext))
                    k.customStyleStrings.set(customProperty, v);
            } else if (k.style->setProperty(propertyName, v, false, parserContext))
                k.styleStrings.set(propertyName, v);
            // 3. Append k to property keyframes.
            propertyKeyframes.append(WTFMove(k));
        }
        // 6. Apply the procedure to compute missing keyframe offsets to property keyframes.
        computeMissingKeyframeOffsets(propertyKeyframes);

        // 7. Add keyframes in property keyframes to processed keyframes.
        parsedKeyframes.appendVector(propertyKeyframes);
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
        if (keyframe.styleStrings.size()) {
            previousKeyframe.style->mergeAndOverrideOnConflict(keyframe.style);
            for (auto& [property, value] : keyframe.styleStrings)
                previousKeyframe.styleStrings.set(property, value);
        }
        if (keyframe.customStyleStrings.size()) {
            previousKeyframe.style->mergeAndOverrideOnConflict(keyframe.style);
            for (auto& [customProperty, value] : keyframe.customStyleStrings)
                previousKeyframe.customStyleStrings.set(customProperty, value);
        }
        // Since we've processed this keyframe, we can remove it and keep i the same
        // so that we process the next keyframe in the next loop iteration.
        parsedKeyframes.remove(i);
    }

    // 5. Let offsets be a sequence of nullable double values assigned based on the type of the “offset” member of the property-indexed keyframe as follows:
    //    - sequence<double?>, the value of “offset” as-is.
    //    - double?, a sequence of length one with the value of “offset” as its single item, i.e. « offset »,
    Vector<std::optional<double>> offsets;
    if (std::holds_alternative<Vector<std::optional<double>>>(propertyIndexedKeyframe.baseProperties.offset))
        offsets = std::get<Vector<std::optional<double>>>(propertyIndexedKeyframe.baseProperties.offset);
    else if (std::holds_alternative<double>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(std::get<double>(propertyIndexedKeyframe.baseProperties.offset));
    else if (std::holds_alternative<std::nullptr_t>(propertyIndexedKeyframe.baseProperties.offset))
        offsets.append(std::nullopt);

    // 6. Assign each value in offsets to the keyframe offset of the keyframe with corresponding position in property keyframes until the end of either sequence is reached.
    for (size_t i = 0; i < offsets.size() && i < parsedKeyframes.size(); ++i)
        parsedKeyframes[i].offset = offsets[i];

    // 7. Let easings be a sequence of DOMString values assigned based on the type of the “easing” member of the property-indexed keyframe as follows:
    //    - sequence<DOMString>, the value of “easing” as-is.
    //    - DOMString, a sequence of length one with the value of “easing” as its single item, i.e. « easing »,
    Vector<String> easings;
    if (std::holds_alternative<Vector<String>>(propertyIndexedKeyframe.baseProperties.easing))
        easings = std::get<Vector<String>>(propertyIndexedKeyframe.baseProperties.easing);
    else if (std::holds_alternative<String>(propertyIndexedKeyframe.baseProperties.easing))
        easings.append(std::get<String>(propertyIndexedKeyframe.baseProperties.easing));

    // 8. If easings is an empty sequence, let it be a sequence of length one containing the single value “linear”, i.e. « "linear" ».
    if (easings.isEmpty())
        easings.append("linear"_s);

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
    if (document.settings().webAnimationsCompositeOperationsEnabled()) {
        Vector<CompositeOperationOrAuto> compositeModes;
        if (std::holds_alternative<Vector<CompositeOperationOrAuto>>(propertyIndexedKeyframe.baseProperties.composite))
            compositeModes = std::get<Vector<CompositeOperationOrAuto>>(propertyIndexedKeyframe.baseProperties.composite);
        else if (std::holds_alternative<CompositeOperationOrAuto>(propertyIndexedKeyframe.baseProperties.composite))
            compositeModes.append(std::get<CompositeOperationOrAuto>(propertyIndexedKeyframe.baseProperties.composite));
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
    }

    return { };
}

ExceptionOr<Ref<KeyframeEffect>> KeyframeEffect::create(JSGlobalObject& lexicalGlobalObject, Document& document, Element* target, Strong<JSObject>&& keyframes, std::optional<std::variant<double, KeyframeEffectOptions>>&& options)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(target, PseudoId::None));

    if (options) {
        OptionalEffectTiming timing;
        auto optionsValue = options.value();
        if (std::holds_alternative<double>(optionsValue)) {
            std::variant<double, String> duration = std::get<double>(optionsValue);
            timing.duration = duration;
        } else {
            auto keyframeEffectOptions = std::get<KeyframeEffectOptions>(optionsValue);

            auto setPseudoElementResult = keyframeEffect->setPseudoElement(keyframeEffectOptions.pseudoElement);
            if (setPseudoElementResult.hasException())
                return setPseudoElementResult.releaseException();

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

            if (document.settings().webAnimationsCompositeOperationsEnabled())
                keyframeEffect->setComposite(keyframeEffectOptions.composite);

            if (document.settings().webAnimationsIterationCompositeEnabled())
                keyframeEffect->setIterationComposite(keyframeEffectOptions.iterationComposite);
        }
        auto updateTimingResult = keyframeEffect->updateTiming(timing);
        if (updateTimingResult.hasException())
            return updateTimingResult.releaseException();
    }

    auto processKeyframesResult = keyframeEffect->processKeyframes(lexicalGlobalObject, document, WTFMove(keyframes));
    if (processKeyframesResult.hasException())
        return processKeyframesResult.releaseException();

    return keyframeEffect;
}

Ref<KeyframeEffect> KeyframeEffect::create(Ref<KeyframeEffect>&& source)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(nullptr, PseudoId::None));
    keyframeEffect->copyPropertiesFromSource(WTFMove(source));
    return keyframeEffect;
}

Ref<KeyframeEffect> KeyframeEffect::create(const Element& target, PseudoId pseudoId)
{
    return adoptRef(*new KeyframeEffect(const_cast<Element*>(&target), pseudoId));
}

KeyframeEffect::KeyframeEffect(Element* target, PseudoId pseudoId)
    : m_keyframesName(makeAtomString("keyframe-effect-"_s, UUID::createVersion4Weak()))
    , m_target(target)
    , m_pseudoId(pseudoId)
{
}

void KeyframeEffect::copyPropertiesFromSource(Ref<KeyframeEffect>&& source)
{
    m_target = source->m_target;
    m_pseudoId = source->m_pseudoId;
    m_compositeOperation = source->m_compositeOperation;
    m_iterationCompositeOperation = source->m_iterationCompositeOperation;

    Vector<ParsedKeyframe> parsedKeyframes;
    for (auto& sourceParsedKeyframe : source->m_parsedKeyframes) {
        ParsedKeyframe parsedKeyframe;
        parsedKeyframe.easing = sourceParsedKeyframe.easing;
        parsedKeyframe.offset = sourceParsedKeyframe.offset;
        parsedKeyframe.composite = sourceParsedKeyframe.composite;
        parsedKeyframe.styleStrings = sourceParsedKeyframe.styleStrings;
        parsedKeyframe.customStyleStrings = sourceParsedKeyframe.customStyleStrings;
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
    updateStaticTimingProperties();

    KeyframeList keyframeList(m_keyframesName);
    keyframeList.copyKeyframes(source->m_blendingKeyframes);
    setBlendingKeyframes(keyframeList);
}

auto KeyframeEffect::getKeyframes(Document& document) -> Vector<ComputedKeyframe>
{
    // https://drafts.csswg.org/web-animations-1/#dom-keyframeeffectreadonly-getkeyframes

    if (auto* declarativeAnimation = dynamicDowncast<DeclarativeAnimation>(animation()))
        declarativeAnimation->flushPendingStyleChanges();

    Vector<ComputedKeyframe> computedKeyframes;

    if (!m_parsedKeyframes.isEmpty() || m_blendingKeyframesSource == BlendingKeyframesSource::WebAnimation || !m_blendingKeyframes.containsAnimatableProperty()) {
        for (size_t i = 0; i < m_parsedKeyframes.size(); ++i) {
            ComputedKeyframe computedKeyframe { m_parsedKeyframes[i] };
            computedKeyframe.easing = timingFunctionForKeyframeAtIndex(i)->cssText();
            computedKeyframes.append(WTFMove(computedKeyframe));
        }
        return computedKeyframes;
    }

    auto* target = m_target.get();
    auto* lastStyleChangeEventStyle = targetStyleable()->lastStyleChangeEventStyle();
    auto& elementStyle = lastStyleChangeEventStyle ? *lastStyleChangeEventStyle : currentStyle();

    ComputedStyleExtractor computedStyleExtractor { target, false, m_pseudoId };

    KeyframeList computedKeyframeList(m_blendingKeyframes.animationName());
    computedKeyframeList.copyKeyframes(m_blendingKeyframes);
    computedKeyframeList.fillImplicitKeyframes(*this, elementStyle);

    auto keyframeRules = [&]() -> const Vector<Ref<StyleRuleKeyframe>> {
        if (!is<CSSAnimation>(animation()))
            return { };

        auto& backingAnimation = downcast<CSSAnimation>(*animation()).backingAnimation();
        auto* styleScope = Style::Scope::forOrdinal(*m_target, backingAnimation.nameStyleScopeOrdinal());
        if (!styleScope)
            return { };

        return styleScope->resolver().keyframeRulesForName(computedKeyframeList.animationName());
    }();

    auto keyframeRuleForKey = [&](double key) -> StyleRuleKeyframe* {
        for (auto& keyframeRule : keyframeRules) {
            for (auto keyframeRuleKey : keyframeRule->keys()) {
                if (keyframeRuleKey == key)
                    return keyframeRule.ptr();
            }
        }
        return nullptr;
    };

    auto styleProperties = MutableStyleProperties::create();
    if (m_blendingKeyframesSource == BlendingKeyframesSource::CSSAnimation) {
        auto matchingRules = m_target->styleResolver().pseudoStyleRulesForElement(target, m_pseudoId, Style::Resolver::AllCSSRules);
        for (auto& matchedRule : matchingRules)
            styleProperties->mergeAndOverrideOnConflict(matchedRule->properties());
        if (is<StyledElement>(m_target) && m_pseudoId == PseudoId::None) {
            if (auto* inlineProperties = downcast<StyledElement>(*m_target).inlineStyle())
                styleProperties->mergeAndOverrideOnConflict(*inlineProperties);
        }
    }

    for (auto& keyframe : computedKeyframeList) {
        auto& style = *keyframe.style();
        auto* keyframeRule = keyframeRuleForKey(keyframe.key());

        ComputedKeyframe computedKeyframe;
        computedKeyframe.offset = keyframe.key();
        computedKeyframe.computedOffset = keyframe.key();
        // For CSS transitions, all keyframes should return "linear" since the effect's global timing function applies.
        computedKeyframe.easing = is<CSSTransition>(animation()) ? "linear"_s : timingFunctionForBlendingKeyframe(keyframe)->cssText();

        if (document.settings().webAnimationsCompositeOperationsEnabled()) {
            if (auto compositeOperation = keyframe.compositeOperation())
                computedKeyframe.composite = toCompositeOperationOrAuto(*compositeOperation);
        }

        auto addPropertyToKeyframe = [&](CSSPropertyID cssPropertyId) {
            String styleString = emptyString();
            if (keyframeRule) {
                if (auto cssValue = keyframeRule->properties().getPropertyCSSValue(cssPropertyId)) {
                    if (!cssValue->hasVariableReferences())
                        styleString = keyframeRule->properties().getPropertyValue(cssPropertyId);
                }
            }
            if (styleString.isEmpty()) {
                if (auto cssValue = styleProperties->getPropertyCSSValue(cssPropertyId)) {
                    if (!cssValue->hasVariableReferences())
                        styleString = styleProperties->getPropertyValue(cssPropertyId);
                }
            }
            if (styleString.isEmpty()) {
                if (auto cssValue = computedStyleExtractor.valueForPropertyInStyle(style, cssPropertyId, nullptr, ComputedStyleExtractor::PropertyValueType::Computed))
                    styleString = cssValue->cssText();
            }
            computedKeyframe.styleStrings.set(cssPropertyId, styleString);
        };

        for (auto cssPropertyId : keyframe.properties()) {
            if (cssPropertyId == CSSPropertyCustom)
                continue;
            addPropertyToKeyframe(cssPropertyId);
        }

        if (m_blendingKeyframesSource != BlendingKeyframesSource::CSSAnimation) {
            auto addCustomPropertyToKeyframe = [&](const AtomString& customProperty) {
                String styleString = emptyString();
                if (keyframeRule) {
                    if (auto cssValue = keyframeRule->properties().getCustomPropertyCSSValue(customProperty)) {
                        if (!cssValue->hasVariableReferences())
                            styleString = keyframeRule->properties().getCustomPropertyValue(customProperty);
                    }
                }
                if (styleString.isEmpty()) {
                    if (auto cssValue = styleProperties->getCustomPropertyCSSValue(customProperty)) {
                        if (!cssValue->hasVariableReferences())
                            styleString = styleProperties->getCustomPropertyValue(customProperty);
                    }
                }
                if (styleString.isEmpty()) {
                    if (auto cssValue = computedStyleExtractor.customPropertyValue(customProperty))
                        styleString = cssValue->cssText();
                }
                computedKeyframe.customStyleStrings.set(customProperty, styleString);
            };

            for (auto customProperty : keyframe.customProperties())
                addCustomPropertyToKeyframe(customProperty);
        }

        computedKeyframes.append(WTFMove(computedKeyframe));
    }

    return computedKeyframes;
}

ExceptionOr<void> KeyframeEffect::setBindingsKeyframes(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput)
{
    auto retVal = setKeyframes(lexicalGlobalObject, document, WTFMove(keyframesInput));
    if (!retVal.hasException() && is<CSSAnimation>(animation()))
        downcast<CSSAnimation>(*animation()).effectKeyframesWereSetUsingBindings();
    return retVal;
}

ExceptionOr<void> KeyframeEffect::setKeyframes(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput)
{
    auto processKeyframesResult = processKeyframes(lexicalGlobalObject, document, WTFMove(keyframesInput));
    if (!processKeyframesResult.hasException() && animation())
        animation()->effectTimingDidChange();
    return processKeyframesResult;
}

void KeyframeEffect::keyframesRuleDidChange()
{
    ASSERT(is<CSSAnimation>(animation()));
    clearBlendingKeyframes();
    invalidate();
}

ExceptionOr<void> KeyframeEffect::processKeyframes(JSGlobalObject& lexicalGlobalObject, Document& document, Strong<JSObject>&& keyframesInput)
{
    Ref protectedDocument { document };

    // 1. If object is null, return an empty sequence of keyframes.
    if (!keyframesInput.get())
        return { };

    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 2. Let processed keyframes be an empty sequence of keyframes.
    Vector<ParsedKeyframe> parsedKeyframes;

    // 3. Let method be the result of GetMethod(object, @@iterator).
    auto method = keyframesInput.get()->get(&lexicalGlobalObject, vm.propertyNames->iteratorSymbol);

    // 4. Check the completion record of method.
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    // 5. Perform the steps corresponding to the first matching condition from below,
    Vector<String> unusedEasings;
    if (!method.isUndefined()) {
        auto retVal = processIterableKeyframes(lexicalGlobalObject, document, WTFMove(keyframesInput), WTFMove(method), parsedKeyframes);
        if (retVal.hasException())
            return retVal.releaseException();
    } else {
        auto retVal = processPropertyIndexedKeyframes(lexicalGlobalObject, document, WTFMove(keyframesInput), parsedKeyframes, unusedEasings);
        if (retVal.hasException())
            return retVal.releaseException();
    }

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

    m_inheritedProperties.clear();

    // 8. For each frame in processed keyframes, perform the following steps:
    for (auto& keyframe : parsedKeyframes) {
        // Let the timing function of frame be the result of parsing the “easing” property on frame using the CSS syntax
        // defined for the easing property of the AnimationEffectTiming interface.
        // If parsing the “easing” property fails, throw a TypeError and abort this procedure.
        auto timingFunctionResult = TimingFunction::createFromCSSText(keyframe.easing);
        if (timingFunctionResult.hasException())
            return timingFunctionResult.releaseException();
        keyframe.timingFunction = timingFunctionResult.returnValue();

        for (auto& [property, value] : keyframe.styleStrings) {
            if (equalLettersIgnoringASCIICase(value, "inherit"_s))
                m_inheritedProperties.add(property);
        }
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

    clearBlendingKeyframes();

    invalidate();

    return { };
}

void KeyframeEffect::updateBlendingKeyframes(RenderStyle& elementStyle, const Style::ResolutionContext& resolutionContext)
{
    if (!m_blendingKeyframes.isEmpty() || !m_target)
        return;

    KeyframeList keyframeList(m_keyframesName);
    auto& styleResolver = m_target->styleResolver();

    for (auto& keyframe : m_parsedKeyframes) {
        KeyframeValue keyframeValue(keyframe.computedOffset, nullptr);
        keyframeValue.setTimingFunction(keyframe.timingFunction->clone());

        switch (keyframe.composite) {
        case CompositeOperationOrAuto::Replace:
            keyframeValue.setCompositeOperation(CompositeOperation::Replace);
            break;
        case CompositeOperationOrAuto::Add:
            keyframeValue.setCompositeOperation(CompositeOperation::Add);
            break;
        case CompositeOperationOrAuto::Accumulate:
            keyframeValue.setCompositeOperation(CompositeOperation::Accumulate);
            break;
        case CompositeOperationOrAuto::Auto:
            break;
        }

        auto keyframeRule = StyleRuleKeyframe::create(keyframe.style->immutableCopyIfNeeded());
        keyframeValue.setStyle(styleResolver.styleForKeyframe(*m_target, elementStyle, resolutionContext, keyframeRule.get(), keyframeValue));
        keyframeList.insert(WTFMove(keyframeValue));
    }

    setBlendingKeyframes(keyframeList);
}

const HashSet<CSSPropertyID>& KeyframeEffect::animatedProperties()
{
    if (!m_blendingKeyframes.isEmpty())
        return m_blendingKeyframes.properties();

    if (m_animatedProperties.isEmpty()) {
        for (auto& keyframe : m_parsedKeyframes) {
            for (auto keyframeProperty : keyframe.styleStrings.keys())
                m_animatedProperties.add(keyframeProperty);
        }
    }

    return m_animatedProperties;
}

const HashSet<AtomString>& KeyframeEffect::animatedCustomProperties()
{
    if (!m_blendingKeyframes.isEmpty())
        return m_blendingKeyframes.customProperties();

    if (m_animatedCustomProperties.isEmpty()) {
        for (auto& keyframe : m_parsedKeyframes) {
            for (auto keyframeCustomProperty : keyframe.customStyleStrings.keys())
                m_animatedCustomProperties.add(keyframeCustomProperty);
        }
    }

    return m_animatedCustomProperties;
}

bool KeyframeEffect::animatesProperty(CSSPropertyID property) const
{
    if (!m_blendingKeyframes.isEmpty())
        return m_blendingKeyframes.properties().contains(property);

    for (auto& keyframe : m_parsedKeyframes) {
        for (auto keyframeProperty : keyframe.styleStrings.keys()) {
            if (keyframeProperty == property)
                return true;
        }
    }
    return false;
}

bool KeyframeEffect::forceLayoutIfNeeded()
{
    if (!m_needsForcedLayout || !m_target)
        return false;

    auto* renderer = this->renderer();
    if (!renderer || !renderer->parent())
        return false;

    ASSERT(document());
    auto* frameView = document()->view();
    if (!frameView)
        return false;

    frameView->forceLayout();
    return true;
}


void KeyframeEffect::clearBlendingKeyframes()
{
    m_blendingKeyframesSource = BlendingKeyframesSource::WebAnimation;
    m_blendingKeyframes.clear();
}

void KeyframeEffect::setBlendingKeyframes(KeyframeList& blendingKeyframes)
{
    CanBeAcceleratedMutationScope mutationScope(this);

    m_blendingKeyframes = WTFMove(blendingKeyframes);
    m_animatedProperties.clear();
    m_animatedCustomProperties.clear();

    computedNeedsForcedLayout();
    computeStackingContextImpact();
    computeAcceleratedPropertiesState();
    computeSomeKeyframesUseStepsTimingFunction();
    computeHasImplicitKeyframeForAcceleratedProperty();
    computeHasKeyframeComposingAcceleratedProperty();

    checkForMatchingTransformFunctionLists();
    checkForMatchingFilterFunctionLists();
#if ENABLE(FILTERS_LEVEL_2)
    checkForMatchingBackdropFilterFunctionLists();
#endif
    checkForMatchingColorFilterFunctionLists();
}

void KeyframeEffect::checkForMatchingTransformFunctionLists()
{
    if (m_blendingKeyframes.size() < 2 || !m_blendingKeyframes.containsProperty(CSSPropertyTransform)) {
        m_transformFunctionListsMatchPrefix = 0;
        return;
    }

    SharedPrimitivesPrefix prefix;
    for (const auto& keyframe : m_blendingKeyframes)
        prefix.update(keyframe.style()->transform());

    m_transformFunctionListsMatchPrefix = prefix.primitives().size();
}

bool KeyframeEffect::checkForMatchingFilterFunctionLists(CSSPropertyID propertyID, const std::function<const FilterOperations& (const RenderStyle&)>& filtersGetter) const
{
    if (!m_blendingKeyframes.containsProperty(propertyID))
        return false;

    if (m_blendingKeyframes.size() < 2)
        return true;

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

void KeyframeEffect::computeDeclarativeAnimationBlendingKeyframes(const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext& resolutionContext)
{
    ASSERT(is<DeclarativeAnimation>(animation()));
    if (is<CSSAnimation>(animation()))
        computeCSSAnimationBlendingKeyframes(newStyle, resolutionContext);
    else if (is<CSSTransition>(animation())) {
        ASSERT(oldStyle);
        computeCSSTransitionBlendingKeyframes(*oldStyle, newStyle);
    }
}

void KeyframeEffect::computeCSSAnimationBlendingKeyframes(const RenderStyle& unanimatedStyle, const Style::ResolutionContext& resolutionContext)
{
    ASSERT(is<CSSAnimation>(animation()));
    ASSERT(document());

    auto cssAnimation = downcast<CSSAnimation>(animation());
    auto& backingAnimation = cssAnimation->backingAnimation();

    KeyframeList keyframeList(AtomString { backingAnimation.name().string });
    if (auto* styleScope = Style::Scope::forOrdinal(*m_target, backingAnimation.nameStyleScopeOrdinal()))
        styleScope->resolver().keyframeStylesForAnimation(*m_target, unanimatedStyle, resolutionContext, keyframeList);

    // Ensure resource loads for all the frames.
    for (auto& keyframe : keyframeList) {
        if (auto* style = const_cast<RenderStyle*>(keyframe.style()))
            Style::loadPendingResources(*style, *document(), m_target.get());
    }

    m_blendingKeyframesSource = BlendingKeyframesSource::CSSAnimation;
    setBlendingKeyframes(keyframeList);
}

void KeyframeEffect::computeCSSTransitionBlendingKeyframes(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    ASSERT(is<CSSTransition>(animation()));
    ASSERT(document());

    if (m_blendingKeyframes.size())
        return;

    auto property = downcast<CSSTransition>(animation())->property();

    auto toStyle = RenderStyle::clonePtr(newStyle);
    if (m_target)
        Style::loadPendingResources(*toStyle, *document(), m_target.get());

    KeyframeList keyframeList(m_keyframesName);

    KeyframeValue fromKeyframeValue(0, RenderStyle::clonePtr(oldStyle));
    fromKeyframeValue.addProperty(property);
    keyframeList.insert(WTFMove(fromKeyframeValue));

    KeyframeValue toKeyframeValue(1, WTFMove(toStyle));
    toKeyframeValue.addProperty(property);
    keyframeList.insert(WTFMove(toKeyframeValue));

    m_blendingKeyframesSource = BlendingKeyframesSource::CSSTransition;
    setBlendingKeyframes(keyframeList);
}

void KeyframeEffect::computedNeedsForcedLayout()
{
    m_needsForcedLayout = false;
    if (is<CSSTransition>(animation()) || !m_blendingKeyframes.containsProperty(CSSPropertyTransform))
        return;

    for (auto& keyframe : m_blendingKeyframes) {
        auto* keyframeStyle = keyframe.style();
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

void KeyframeEffect::animationTimelineDidChange(AnimationTimeline* timeline)
{
    auto target = targetStyleable();
    if (!target)
        return;

    if (timeline)
        m_inTargetEffectStack = target->ensureKeyframeEffectStack().addEffect(*this);
    else {
        target->ensureKeyframeEffectStack().removeEffect(*this);
        m_inTargetEffectStack = false;
    }
}

void KeyframeEffect::animationTimingDidChange()
{
    updateEffectStackMembership();
}

void KeyframeEffect::updateEffectStackMembership()
{
    auto target = targetStyleable();
    if (!target)
        return;

    bool isRelevant = animation() && animation()->isRelevant();
    if (isRelevant && !m_inTargetEffectStack)
        m_inTargetEffectStack = target->ensureKeyframeEffectStack().addEffect(*this);
    else if (!isRelevant && m_inTargetEffectStack) {
        target->ensureKeyframeEffectStack().removeEffect(*this);
        m_inTargetEffectStack = false;
    }
}

void KeyframeEffect::setAnimation(WebAnimation* animation)
{
    bool animationChanged = animation != this->animation();
    AnimationEffect::setAnimation(animation);
    if (animationChanged) {
        if (m_blendingKeyframesSource == BlendingKeyframesSource::CSSAnimation)
            clearBlendingKeyframes();
        updateEffectStackMembership();
    }
}

const std::optional<const Styleable> KeyframeEffect::targetStyleable() const
{
    if (m_target)
        return Styleable(*m_target, m_pseudoId);
    return std::nullopt;
}

bool KeyframeEffect::targetsPseudoElement() const
{
    return m_target.get() && m_pseudoId != PseudoId::None;
}

void KeyframeEffect::setTarget(RefPtr<Element>&& newTarget)
{
    if (m_target == newTarget)
        return;

    auto& previousTargetStyleable = targetStyleable();
    RefPtr<Element> protector;
    if (previousTargetStyleable)
        protector = &previousTargetStyleable->element;
    m_target = WTFMove(newTarget);
    didChangeTargetStyleable(previousTargetStyleable);
}

const String KeyframeEffect::pseudoElement() const
{
    // https://drafts.csswg.org/web-animations/#dom-keyframeeffect-pseudoelement

    // The target pseudo-selector. null if this effect has no effect target or if the effect target is an element (i.e. not a pseudo-element).
    // When the effect target is a pseudo-element, this specifies the pseudo-element selector (e.g. ::before).
    if (targetsPseudoElement())
        return pseudoIdAsString(m_pseudoId);
    return { };
}

ExceptionOr<void> KeyframeEffect::setPseudoElement(const String& pseudoElement)
{
    // https://drafts.csswg.org/web-animations/#dom-keyframeeffect-pseudoelement

    // On setting, sets the target pseudo-selector of the animation effect to the provided value after applying the following exceptions:
    //
    // - If the provided value is not null and is an invalid <pseudo-element-selector>, the user agent must throw a DOMException with error
    // name SyntaxError and leave the target pseudo-selector of this animation effect unchanged. Note, that invalid in this context follows
    // the definition of an invalid selector defined in [SELECTORS-4] such that syntactically invalid pseudo-elements as well as pseudo-elements
    // for which the user agent has no usable level of support are both deemed invalid.
    // - If one of the legacy Selectors Level 2 single-colon selectors (':before', ':after', ':first-letter', or ':first-line') is specified,
    // the target pseudo-selector must be set to the equivalent two-colon selector (e.g. '::before').
    auto pseudoId = PseudoId::None;
    if (!pseudoElement.isNull()) {
        auto isLegacy = pseudoElement == ":before"_s || pseudoElement == ":after"_s || pseudoElement == ":first-letter"_s || pseudoElement == ":first-line"_s;
        if (!isLegacy && !pseudoElement.startsWith("::"_s))
            return Exception { SyntaxError };
        auto pseudoType = CSSSelector::parsePseudoElementType(StringView(pseudoElement).substring(isLegacy ? 1 : 2));
        if (pseudoType == CSSSelector::PseudoElementUnknown || pseudoType == CSSSelector::PseudoElementWebKitCustom)
            return Exception { SyntaxError };
        pseudoId = CSSSelector::pseudoId(pseudoType);
    }

    if (pseudoId == m_pseudoId)
        return { };

    auto& previousTargetStyleable = targetStyleable();
    m_pseudoId = pseudoId;
    didChangeTargetStyleable(previousTargetStyleable);

    return { };
}

void KeyframeEffect::didChangeTargetStyleable(const std::optional<const Styleable>& previousTargetStyleable)
{
    auto newTargetStyleable = targetStyleable();

    if (auto* effectAnimation = animation())
        effectAnimation->effectTargetDidChange(previousTargetStyleable, newTargetStyleable);

    clearBlendingKeyframes();

    // We need to invalidate the effect now that the target has changed
    // to ensure the effect's styles are applied to the new target right away.
    invalidate();

    // Likewise, we need to invalidate styles on the previous target so that
    // any animated styles are removed immediately.
    invalidateElement(previousTargetStyleable);

    if (previousTargetStyleable) {
        previousTargetStyleable->ensureKeyframeEffectStack().removeEffect(*this);
        m_inTargetEffectStack = false;
    }

    if (newTargetStyleable)
        m_inTargetEffectStack = newTargetStyleable->ensureKeyframeEffectStack().addEffect(*this);
}

void KeyframeEffect::apply(RenderStyle& targetStyle, const Style::ResolutionContext& resolutionContext, std::optional<Seconds> startTime)
{
    if (!m_target)
        return;

    updateBlendingKeyframes(targetStyle, resolutionContext);

    auto computedTiming = getComputedTiming(startTime);
    if (!startTime) {
        m_phaseAtLastApplication = computedTiming.phase;
        if (auto target = targetStyleable())
            InspectorInstrumentation::willApplyKeyframeEffect(*target, *this, computedTiming);
    }

    if (!computedTiming.progress)
        return;

    setAnimatedPropertiesInStyle(targetStyle, computedTiming.progress.value());
}

bool KeyframeEffect::isCurrentlyAffectingProperty(CSSPropertyID property, Accelerated accelerated) const
{
    if (accelerated == Accelerated::Yes && !isRunningAccelerated() && !isAboutToRunAccelerated())
        return false;

    if (!m_blendingKeyframes.properties().contains(property))
        return false;

    if (m_pseudoId == PseudoId::Marker && !Style::isValidMarkerStyleProperty(property))
        return false;

    return m_phaseAtLastApplication == AnimationEffectPhase::Active;
}

bool KeyframeEffect::isRunningAcceleratedAnimationForProperty(CSSPropertyID property) const
{
    return isRunningAccelerated() && CSSPropertyAnimation::animationOfPropertyIsAccelerated(property) && m_blendingKeyframes.properties().contains(property);
}

bool KeyframeEffect::isTargetingTransformRelatedProperty() const
{
    return m_blendingKeyframes.properties().contains(CSSPropertyTranslate)
        || m_blendingKeyframes.properties().contains(CSSPropertyScale)
        || m_blendingKeyframes.properties().contains(CSSPropertyRotate)
        || m_blendingKeyframes.properties().contains(CSSPropertyTransform);
}

bool KeyframeEffect::isRunningAcceleratedTransformRelatedAnimation() const
{
    return isRunningAccelerated() && isTargetingTransformRelatedProperty();
}

void KeyframeEffect::invalidate()
{
    LOG_WITH_STREAM(Animations, stream << "KeyframeEffect::invalidate on element " << ValueOrNull(m_target.get()));
    invalidateElement(targetStyleable());
}

void KeyframeEffect::computeAcceleratedPropertiesState()
{
    bool hasSomeAcceleratedProperties = false;
    bool hasSomeUnacceleratedProperties = false;

    for (auto cssPropertyId : m_blendingKeyframes.properties()) {
        // If any animated property can be accelerated, then the animation should run accelerated.
        if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(cssPropertyId))
            hasSomeAcceleratedProperties = true;
        else
            hasSomeUnacceleratedProperties = true;
        if (hasSomeAcceleratedProperties && hasSomeUnacceleratedProperties)
            break;
    }

    if (!hasSomeAcceleratedProperties)
        m_acceleratedPropertiesState = AcceleratedProperties::None;
    else if (hasSomeUnacceleratedProperties)
        m_acceleratedPropertiesState = AcceleratedProperties::Some;
    else
        m_acceleratedPropertiesState = AcceleratedProperties::All;
}

void KeyframeEffect::computeSomeKeyframesUseStepsTimingFunction()
{
    m_someKeyframesUseStepsTimingFunction = false;

    // If we're dealing with a CSS Animation and it specifies a default steps() timing function,
    // we need to check that any of the specified keyframes either does not have an explicit timing
    // function or specifies an explicit steps() timing function.
    if (is<CSSAnimation>(animation()) && is<StepsTimingFunction>(downcast<DeclarativeAnimation>(*animation()).backingAnimation().timingFunction())) {
        for (auto& keyframe : m_blendingKeyframes) {
            auto* timingFunction = keyframe.timingFunction();
            if (!timingFunction || is<StepsTimingFunction>(timingFunction)) {
                m_someKeyframesUseStepsTimingFunction = true;
                return;
            }
        }
        return;
    }

    // For any other type of animation, we just need to check whether any of the keyframes specify
    // an explicit steps() timing function.
    for (auto& keyframe : m_blendingKeyframes) {
        if (is<StepsTimingFunction>(keyframe.timingFunction())) {
            m_someKeyframesUseStepsTimingFunction = true;
            return;
        }
    }
}

bool KeyframeEffect::hasImplicitKeyframes() const
{
    auto numberOfKeyframes = m_parsedKeyframes.size();

    // If we have no keyframes, then there cannot be any implicit keyframes.
    if (!numberOfKeyframes)
        return false;

    // If we have a single keyframe, then there has to be at least one implicit keyframe.
    if (numberOfKeyframes == 1)
        return true;

    // If we have two or more keyframes, then we have implicit keyframes if the first and last
    // keyframes don't have 0 and 1 respectively as their computed offset.
    return m_parsedKeyframes[0].computedOffset || m_parsedKeyframes[numberOfKeyframes - 1].computedOffset != 1;
}

void KeyframeEffect::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    if (!renderer() || !animation())
        return;

    auto progress = getComputedTiming().progress;
    LOG_WITH_STREAM(Animations, stream << "KeyframeEffect " << this << " getAnimatedStyle - progress " << progress);
    if (!progress)
        return;

    if (!animatedStyle) {
        if (auto* style = targetStyleable()->lastStyleChangeEventStyle())
            animatedStyle = RenderStyle::clonePtr(*style);
        else
            animatedStyle = RenderStyle::clonePtr(renderer()->style());
    }

    setAnimatedPropertiesInStyle(*animatedStyle.get(), progress.value());
}

void KeyframeEffect::setAnimatedPropertiesInStyle(RenderStyle& targetStyle, double iterationProgress)
{
    auto& properties = m_blendingKeyframes.properties();

    // In the case of CSS Transitions we already know that there are only two keyframes, one where offset=0 and one where offset=1,
    // and only a single CSS property so we can simply blend based on the style available on those keyframes with the provided iteration
    // progress which already accounts for the transition's timing function.
    if (m_blendingKeyframesSource == BlendingKeyframesSource::CSSTransition) {
        ASSERT(properties.size() == 1);
        CSSPropertyAnimation::blendProperties(this, *properties.begin(), targetStyle, *m_blendingKeyframes[0].style(), *m_blendingKeyframes[1].style(), iterationProgress, m_compositeOperation);
        return;
    }

    // 4.4.3. The effect value of a keyframe effect
    // https://drafts.csswg.org/web-animations-1/#the-effect-value-of-a-keyframe-animation-effect
    //
    // The effect value of a single property referenced by a keyframe effect as one of its target properties,
    // for a given iteration progress, current iteration and underlying value is calculated as follows.

    updateBlendingKeyframes(targetStyle, { nullptr });
    if (m_blendingKeyframes.isEmpty())
        return;

    KeyframeValue propertySpecificKeyframeWithZeroOffset(0, RenderStyle::clonePtr(targetStyle));
    KeyframeValue propertySpecificKeyframeWithOneOffset(1, RenderStyle::clonePtr(targetStyle));

    auto keyframeContainsProperty = [](const KeyframeValue& keyframe, std::variant<CSSPropertyID, AtomString> property) {
        return WTF::switchOn(property,
            [&] (CSSPropertyID propertyId) { return keyframe.containsProperty(propertyId); },
            [&] (AtomString customProperty) { return keyframe.containsCustomProperty(customProperty); }
        );
    };

    auto blendProperty = [&](std::variant<CSSPropertyID, AtomString> property) {
        // 1. If iteration progress is unresolved abort this procedure.
        // 2. Let target property be the longhand property for which the effect value is to be calculated.
        // 3. If animation type of the target property is not animatable abort this procedure since the effect cannot be applied.
        // 4. Define the neutral value for composition as a value which, when combined with an underlying value using the add composite operation,
        //    produces the underlying value.

        // 5. Let property-specific keyframes be the result of getting the set of computed keyframes for this keyframe effect.
        // 6. Remove any keyframes from property-specific keyframes that do not have a property value for target property.
        unsigned numberOfKeyframesWithZeroOffset = 0;
        unsigned numberOfKeyframesWithOneOffset = 0;
        Vector<const KeyframeValue*> propertySpecificKeyframes;
        for (auto& keyframe : m_blendingKeyframes) {
            auto offset = keyframe.key();
            if (!keyframeContainsProperty(keyframe, property))
                continue;
            if (!offset)
                numberOfKeyframesWithZeroOffset++;
            if (offset == 1)
                numberOfKeyframesWithOneOffset++;
            propertySpecificKeyframes.append(&keyframe);
        }

        // 7. If property-specific keyframes is empty, return underlying value.
        if (propertySpecificKeyframes.isEmpty())
            return;

        auto hasImplicitZeroKeyframe = !numberOfKeyframesWithZeroOffset;
        auto hasImplicitOneKeyframe = !numberOfKeyframesWithOneOffset;

        // 8. If there is no keyframe in property-specific keyframes with a computed keyframe offset of 0, create a new keyframe with a computed keyframe
        //    offset of 0, a property value set to the neutral value for composition, and a composite operation of add, and prepend it to the beginning of
        //    property-specific keyframes.
        if (hasImplicitZeroKeyframe) {
            propertySpecificKeyframes.insert(0, &propertySpecificKeyframeWithZeroOffset);
            numberOfKeyframesWithZeroOffset = 1;
        }

        // 9. Similarly, if there is no keyframe in property-specific keyframes with a computed keyframe offset of 1, create a new keyframe with a computed
        //    keyframe offset of 1, a property value set to the neutral value for composition, and a composite operation of add, and append it to the end of
        //    property-specific keyframes.
        if (hasImplicitOneKeyframe) {
            propertySpecificKeyframes.append(&propertySpecificKeyframeWithOneOffset);
            numberOfKeyframesWithOneOffset = 1;
        }

        // 10. Let interval endpoints be an empty sequence of keyframes.
        Vector<const KeyframeValue*> intervalEndpoints;

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
                auto offset = propertySpecificKeyframes[i]->key();
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

        auto& startKeyframe = *intervalEndpoints.first();
        auto& endKeyframe = *intervalEndpoints.last();

        auto startKeyframeStyle = RenderStyle::clone(*startKeyframe.style());
        auto endKeyframeStyle = RenderStyle::clone(*endKeyframe.style());

        // 12. For each keyframe in interval endpoints:
        //     If keyframe has a composite operation that is not replace, or keyframe has no composite operation and the
        //     composite operation of this keyframe effect is not replace, then perform the following steps:
        //         1. Let composite operation to use be the composite operation of keyframe, or if it has none, the composite
        //            operation of this keyframe effect.
        //         2. Let value to combine be the property value of target property specified on keyframe.
        //         3. Replace the property value of target property on keyframe with the result of combining underlying value
        //            (Va) and value to combine (Vb) using the procedure for the composite operation to use corresponding to the
        //            target property’s animation type.
        if (std::holds_alternative<CSSPropertyID>(property)) {
            auto cssPropertyId = std::get<CSSPropertyID>(property);
            if (CSSPropertyAnimation::isPropertyAdditiveOrCumulative(cssPropertyId)) {
                // Only do this for the 0 keyframe if it was provided explicitly, since otherwise we want to use the "neutral value
                // for composition" which really means we don't want to do anything but rather just use the underlying style which
                // is already set on startKeyframe.
                if (!startKeyframe.key() && !hasImplicitZeroKeyframe) {
                    auto startKeyframeCompositeOperation = startKeyframe.compositeOperation().value_or(m_compositeOperation);
                    if (startKeyframeCompositeOperation != CompositeOperation::Replace)
                        CSSPropertyAnimation::blendProperties(this, cssPropertyId, startKeyframeStyle, targetStyle, *startKeyframe.style(), 1, startKeyframeCompositeOperation);
                }

                // Only do this for the 1 keyframe if it was provided explicitly, since otherwise we want to use the "neutral value
                // for composition" which really means we don't want to do anything but rather just use the underlying style which
                // is already set on endKeyframe.
                if (endKeyframe.key() == 1 && !hasImplicitOneKeyframe) {
                    auto endKeyframeCompositeOperation = endKeyframe.compositeOperation().value_or(m_compositeOperation);
                    if (endKeyframeCompositeOperation != CompositeOperation::Replace)
                        CSSPropertyAnimation::blendProperties(this, cssPropertyId, endKeyframeStyle, targetStyle, *endKeyframe.style(), 1, endKeyframeCompositeOperation);
                }
            }
        }

        // 13. If there is only one keyframe in interval endpoints return the property value of target property on that keyframe.
        if (intervalEndpoints.size() == 1) {
            WTF::switchOn(property,
                [&] (CSSPropertyID propertyId) { CSSPropertyAnimation::blendProperties(this, propertyId, targetStyle, startKeyframeStyle, startKeyframeStyle, 0, CompositeOperation::Replace); },
                [&] (AtomString customProperty) { CSSPropertyAnimation::blendCustomProperty(customProperty, targetStyle, startKeyframeStyle, startKeyframeStyle, 0); }
            );
            return;
        }

        // 14. Let start offset be the computed keyframe offset of the first keyframe in interval endpoints.
        auto startOffset = startKeyframe.key();

        // 15. Let end offset be the computed keyframe offset of last keyframe in interval endpoints.
        auto endOffset = endKeyframe.key();

        // 16. Let interval distance be the result of evaluating (iteration progress - start offset) / (end offset - start offset).
        auto intervalDistance = (iterationProgress - startOffset) / (endOffset - startOffset);

        // 17. Let transformed distance be the result of evaluating the timing function associated with the first keyframe in interval endpoints
        //     passing interval distance as the input progress.
        auto transformedDistance = intervalDistance;
        if (auto duration = iterationDuration()) {
            auto rangeDuration = (endOffset - startOffset) * duration.seconds();
            if (auto* timingFunction = timingFunctionForBlendingKeyframe(startKeyframe))
                transformedDistance = timingFunction->transformProgress(intervalDistance, rangeDuration);
        }

        // 18. Return the result of applying the interpolation procedure defined by the animation type of the target property, to the values of the target
        //     property specified on the two keyframes in interval endpoints taking the first such value as Vstart and the second as Vend and using transformed
        //     distance as the interpolation parameter p.
        WTF::switchOn(property,
            [&] (CSSPropertyID propertyId) { CSSPropertyAnimation::blendProperties(this, propertyId, targetStyle, startKeyframeStyle, endKeyframeStyle, transformedDistance, CompositeOperation::Replace); },
            [&] (AtomString customProperty) { CSSPropertyAnimation::blendCustomProperty(customProperty, targetStyle, startKeyframeStyle, endKeyframeStyle, transformedDistance); }
        );
    };

    for (auto cssPropertyId : properties) {
        if (cssPropertyId != CSSPropertyCustom)
            blendProperty(cssPropertyId);
    }

    for (auto customProperty : m_blendingKeyframes.customProperties())
        blendProperty(customProperty);
}

TimingFunction* KeyframeEffect::timingFunctionForBlendingKeyframe(const KeyframeValue& keyframe) const
{
    auto effectAnimation = animation();
    if (is<DeclarativeAnimation>(effectAnimation)) {
        // If we're dealing with a CSS Animation, the timing function is specified either on the keyframe itself.
        if (is<CSSAnimation>(effectAnimation)) {
            if (auto* timingFunction = keyframe.timingFunction())
                return timingFunction;
        }

        // Failing that, or for a CSS Transition, the timing function is inherited from the backing Animation object.
        return downcast<DeclarativeAnimation>(effectAnimation)->backingAnimation().timingFunction();
    }

    return keyframe.timingFunction();
}

TimingFunction* KeyframeEffect::timingFunctionForKeyframeAtIndex(size_t index) const
{
    if (!m_parsedKeyframes.isEmpty()) {
        if (index >= m_parsedKeyframes.size())
            return nullptr;
        return m_parsedKeyframes[index].timingFunction.get();
    }

    if (index >= m_blendingKeyframes.size())
        return nullptr;
    return timingFunctionForBlendingKeyframe(m_blendingKeyframes[index]);
}

bool KeyframeEffect::canBeAccelerated() const
{
    if (m_acceleratedPropertiesState == AcceleratedProperties::None)
        return false;

    if (m_someKeyframesUseStepsTimingFunction || is<StepsTimingFunction>(timingFunction()))
        return false;

    if (m_compositeOperation != CompositeOperation::Replace)
        return false;

    if (m_hasKeyframeComposingAcceleratedProperty)
        return false;

    return true;
}

bool KeyframeEffect::preventsAcceleration() const
{
    // We cannot run accelerated transform animations if a motion path is applied
    // to an element, either through the underlying style, or through a keyframe.
    if (auto target = targetStyleable()) {
        if (auto* lastStyleChangeEventStyle = target->lastStyleChangeEventStyle()) {
            if (lastStyleChangeEventStyle->offsetPath())
                return true;
        }
    }

    if (animatesProperty(CSSPropertyOffsetAnchor)
        || animatesProperty(CSSPropertyOffsetDistance)
        || animatesProperty(CSSPropertyOffsetPath)
        || animatesProperty(CSSPropertyOffsetPosition)
        || animatesProperty(CSSPropertyOffsetRotate)) {
        return true;
    }

    if (m_acceleratedPropertiesState == AcceleratedProperties::None)
        return false;

    return !canBeAccelerated() || m_runningAccelerated == RunningAccelerated::Failed;
}

void KeyframeEffect::updateAcceleratedActions()
{
    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited())
        return;

    if (!canBeAccelerated())
        return;

    auto computedTiming = getComputedTiming();

    // If we're not already running accelerated, the only thing we're interested in is whether we need to start the animation
    // which we need to do once we're in the active phase. Otherwise, there's no change in accelerated state to consider.
    bool isActive = computedTiming.phase == AnimationEffectPhase::Active;
    if (m_runningAccelerated == RunningAccelerated::NotStarted) {
        if (isActive && animation()->playState() == WebAnimation::PlayState::Running)
            addPendingAcceleratedAction(AcceleratedAction::Play);
        return;
    }

    // If we're no longer active, we need to remove the accelerated animation.
    if (!isActive) {
        addPendingAcceleratedAction(AcceleratedAction::Stop);
        return;
    }

    auto playState = animation()->playState();
    // The only thing left to consider is whether we need to pause or resume the animation following a change of play-state.
    if (playState == WebAnimation::PlayState::Paused) {
        if (m_lastRecordedAcceleratedAction != AcceleratedAction::Pause) {
            if (m_lastRecordedAcceleratedAction == AcceleratedAction::Stop)
                addPendingAcceleratedAction(AcceleratedAction::Play);
            addPendingAcceleratedAction(AcceleratedAction::Pause);
        }
    } else if (playState == WebAnimation::PlayState::Running && isActive) {
        if (m_lastRecordedAcceleratedAction != AcceleratedAction::Play)
            addPendingAcceleratedAction(AcceleratedAction::Play);
    }
}

void KeyframeEffect::addPendingAcceleratedAction(AcceleratedAction action)
{
    if (m_runningAccelerated == RunningAccelerated::Prevented || m_runningAccelerated == RunningAccelerated::Failed)
        return;

    if (action == m_lastRecordedAcceleratedAction)
        return;

    if (action == AcceleratedAction::Stop)
        m_pendingAcceleratedActions.clear();
    m_pendingAcceleratedActions.append(action);
    if (action != AcceleratedAction::UpdateTiming && action != AcceleratedAction::TransformChange)
        m_lastRecordedAcceleratedAction = action;
    animation()->acceleratedStateDidChange();
}

void KeyframeEffect::animationDidTick()
{
    invalidate();
    updateAcceleratedActions();
}

void KeyframeEffect::animationDidChangeTimingProperties()
{
    computeSomeKeyframesUseStepsTimingFunction();

    if (isRunningAccelerated() || isAboutToRunAccelerated()) {
        if (canBeAccelerated())
            addPendingAcceleratedAction(AcceleratedAction::UpdateTiming);
        else {
            abilityToBeAcceleratedDidChange();
            addPendingAcceleratedAction(AcceleratedAction::Stop);
        }
    } else if (canBeAccelerated())
        m_runningAccelerated = RunningAccelerated::NotStarted;
}

void KeyframeEffect::transformRelatedPropertyDidChange()
{
    ASSERT(isRunningAcceleratedTransformRelatedAnimation());
    addPendingAcceleratedAction(AcceleratedAction::TransformChange);
}

void KeyframeEffect::propertyAffectingKeyframeResolutionDidChange(RenderStyle& unanimatedStyle, const Style::ResolutionContext& resolutionContext)
{
    switch (m_blendingKeyframesSource) {
    case BlendingKeyframesSource::CSSTransition:
        return;
    case BlendingKeyframesSource::CSSAnimation:
        computeCSSAnimationBlendingKeyframes(unanimatedStyle, resolutionContext);
        return;
    case BlendingKeyframesSource::WebAnimation:
        clearBlendingKeyframes();
        return;
    }
}

void KeyframeEffect::animationWasCanceled()
{
    if (isRunningAccelerated() || isAboutToRunAccelerated())
        addPendingAcceleratedAction(AcceleratedAction::Stop);
}

void KeyframeEffect::willChangeRenderer()
{
    if (isRunningAccelerated() || isAboutToRunAccelerated())
        addPendingAcceleratedAction(AcceleratedAction::Stop);
}

void KeyframeEffect::animationSuspensionStateDidChange(bool animationIsSuspended)
{
    if (isRunningAccelerated() || isAboutToRunAccelerated())
        addPendingAcceleratedAction(animationIsSuspended ? AcceleratedAction::Pause : AcceleratedAction::Play);
}

void KeyframeEffect::applyPendingAcceleratedActions()
{
    CanBeAcceleratedMutationScope mutationScope(this);

    // Once an accelerated animation has been committed, we no longer want to force a layout.
    // This should have been performed by a call to forceLayoutIfNeeded() prior to applying
    // pending accelerated actions.
    m_needsForcedLayout = false;

    if (m_pendingAcceleratedActions.isEmpty())
        return;

    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited()) {
        // The renderer may no longer be composited because the accelerated animation ended before we had a chance to update it,
        // in which case if we asked for the animation to stop, we can discard the current set of accelerated actions.
        if (m_lastRecordedAcceleratedAction == AcceleratedAction::Stop) {
            m_pendingAcceleratedActions.clear();
            m_runningAccelerated = RunningAccelerated::NotStarted;
        }
        return;
    }

    auto pendingAcceleratedActions = m_pendingAcceleratedActions;
    m_pendingAcceleratedActions.clear();

    // To simplify the code we use a default of 0s for an unresolved current time since for a Stop action that is acceptable.
    auto timeOffset = animation()->currentTime().value_or(0_s).seconds() - delay().seconds();

    auto startAnimation = [&]() -> RunningAccelerated {
        if (isRunningAccelerated())
            renderer->animationFinished(m_blendingKeyframes.animationName());

        ASSERT(m_target);
        auto* effectStack = m_target->keyframeEffectStack(m_pseudoId);
        ASSERT(effectStack);

        if (!effectStack->allowsAcceleration())
            return RunningAccelerated::Prevented;

        if (!m_hasImplicitKeyframeForAcceleratedProperty)
            return renderer->startAnimation(timeOffset, backingAnimationForCompositedRenderer(), m_blendingKeyframes) ? RunningAccelerated::Yes : RunningAccelerated::Failed;

        // We need to resolve all animations up to this point to ensure any forward-filling
        // effect is accounted for when computing the "from" value for the accelerated animation.
        auto underlyingStyle = [&]() {
            if (auto* lastStyleChangeEventStyle = m_target->lastStyleChangeEventStyle(m_pseudoId))
                return RenderStyle::clonePtr(*lastStyleChangeEventStyle);
            return RenderStyle::clonePtr(renderer->style());
        }();

        for (const auto& effect : effectStack->sortedEffects()) {
            if (this == effect.get())
                break;
            if (auto progress = effect->getComputedTiming().progress)
                effect->setAnimatedPropertiesInStyle(*underlyingStyle, *progress);
        }

        KeyframeList explicitKeyframes(m_blendingKeyframes.animationName());
        explicitKeyframes.copyKeyframes(m_blendingKeyframes);
        explicitKeyframes.fillImplicitKeyframes(*this, *underlyingStyle);
        return renderer->startAnimation(timeOffset, backingAnimationForCompositedRenderer(), explicitKeyframes) ? RunningAccelerated::Yes : RunningAccelerated::Failed;
    };

    for (const auto& action : pendingAcceleratedActions) {
        switch (action) {
        case AcceleratedAction::Play:
            m_runningAccelerated = startAnimation();
            LOG_WITH_STREAM(Animations, stream << "KeyframeEffect " << this << " applyPendingAcceleratedActions " << m_blendingKeyframes.animationName() << " Play, started accelerated: " << isRunningAccelerated());
            if (!isRunningAccelerated()) {
                m_lastRecordedAcceleratedAction = AcceleratedAction::Stop;
                return;
            }
            break;
        case AcceleratedAction::Pause:
            renderer->animationPaused(timeOffset, m_blendingKeyframes.animationName());
            break;
        case AcceleratedAction::UpdateTiming:
            m_runningAccelerated = startAnimation();
            LOG_WITH_STREAM(Animations, stream << "KeyframeEffect " << this << " applyPendingAcceleratedActions " << m_blendingKeyframes.animationName() << " UpdateTiming, started accelerated: " << isRunningAccelerated());
            if (animation()->playState() == WebAnimation::PlayState::Paused)
                renderer->animationPaused(timeOffset, m_blendingKeyframes.animationName());
            break;
        case AcceleratedAction::Stop:
            ASSERT(document());
            renderer->animationFinished(m_blendingKeyframes.animationName());
            if (!document()->renderTreeBeingDestroyed())
                m_target->invalidateStyleAndLayerComposition();
            m_runningAccelerated = canBeAccelerated() ? RunningAccelerated::NotStarted : RunningAccelerated::Prevented;
            break;
        case AcceleratedAction::TransformChange:
            renderer->transformRelatedPropertyDidChange();
            break;
        }
    }

    return;
}

Ref<const Animation> KeyframeEffect::backingAnimationForCompositedRenderer() const
{
    auto effectAnimation = animation();

    // FIXME: The iterationStart and endDelay AnimationEffectTiming properties do not have
    // corresponding Animation properties.
    auto animation = Animation::create();
    animation->setDuration(iterationDuration().seconds());
    animation->setDelay(delay().seconds());
    animation->setIterationCount(iterations());
    animation->setTimingFunction(timingFunction()->clone());
    animation->setPlaybackRate(effectAnimation->playbackRate());
    animation->setCompositeOperation(m_compositeOperation);

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

    // In the case of CSS Animations, we must set the default timing function for keyframes to match
    // the current value set for animation-timing-function on the target element which affects only
    // keyframes and not the animation-wide timing.
    if (is<CSSAnimation>(effectAnimation))
        animation->setDefaultTimingFunctionForKeyframes(downcast<CSSAnimation>(effectAnimation)->backingAnimation().timingFunction());

    return animation;
}

Document* KeyframeEffect::document() const
{
    return m_target ? &m_target->document() : nullptr;
}

RenderElement* KeyframeEffect::renderer() const
{
    if (auto target = targetStyleable())
        return target->renderer();
    return nullptr;
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

    LayoutRect cumulativeBounds;

    auto* implicitStyle = [&]() {
        if (auto target = targetStyleable()) {
            if (auto* lastStyleChangeEventStyle = target->lastStyleChangeEventStyle())
                return lastStyleChangeEventStyle;
        }
        return &box.style();
    }();

    auto addStyleToCumulativeBounds = [&](const RenderStyle* style) -> bool {
        auto keyframeBounds = bounds;

        bool canCompute;
        if (transformFunctionListPrefix() > 0)
            canCompute = computeTransformedExtentViaTransformList(rendererBox, *style, keyframeBounds);
        else
            canCompute = computeTransformedExtentViaMatrix(rendererBox, *style, keyframeBounds);

        if (!canCompute)
            return false;

        cumulativeBounds.unite(keyframeBounds);
        return true;
    };

    for (const auto& keyframe : m_blendingKeyframes) {
        const auto* keyframeStyle = keyframe.style();

        // FIXME: maybe for declarative animations we always say it's true for the first and last keyframe.
        if (!keyframe.containsProperty(CSSPropertyTransform)) {
            // If the first keyframe is missing transform style, use the current style.
            if (!keyframe.key())
                keyframeStyle = implicitStyle;
            else
                continue;
        }

        if (!addStyleToCumulativeBounds(keyframeStyle))
            return false;
    }

    if (m_blendingKeyframes.hasImplicitKeyframes()) {
        if (!addStyleToCumulativeBounds(implicitStyle))
            return false;
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
        transformOrigin = style.computeTransformOrigin(rendererBox).xy();
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
    style.applyTransform(transform, rendererBox);
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

bool KeyframeEffect::requiresPseudoElement() const
{
    return m_blendingKeyframesSource == BlendingKeyframesSource::WebAnimation && targetsPseudoElement();
}

std::optional<double> KeyframeEffect::progressUntilNextStep(double iterationProgress) const
{
    ASSERT(iterationProgress >= 0 && iterationProgress <= 1);

    if (auto progress = AnimationEffect::progressUntilNextStep(iterationProgress))
        return progress;

    if (!is<LinearTimingFunction>(timingFunction()) || !m_someKeyframesUseStepsTimingFunction)
        return std::nullopt;

    if (m_blendingKeyframes.isEmpty())
        return std::nullopt;

    auto progressUntilNextStepInInterval = [iterationProgress](double intervalStartProgress, double intervalEndProgress, TimingFunction* timingFunction) -> std::optional<double> {
        if (!is<StepsTimingFunction>(timingFunction))
            return std::nullopt;

        auto numberOfSteps = downcast<StepsTimingFunction>(*timingFunction).numberOfSteps();
        auto intervalProgress = intervalEndProgress - intervalStartProgress;
        auto iterationProgressMappedToCurrentInterval = (iterationProgress - intervalStartProgress) / intervalProgress;
        auto nextStepProgress = ceil(iterationProgressMappedToCurrentInterval * numberOfSteps) / numberOfSteps;
        return (nextStepProgress - iterationProgressMappedToCurrentInterval) * intervalProgress;
    };

    for (size_t i = 0; i < m_blendingKeyframes.size(); ++i) {
        auto intervalEndProgress = m_blendingKeyframes[i].key();
        // We can stop once we find a keyframe for which the progress is more than the provided iteration progress.
        if (intervalEndProgress <= iterationProgress)
            continue;

        // In case we're on the first keyframe, then this means we are dealing with an implicit 0% keyframe.
        // This will be a linear timing function unless we're dealing with a CSS Animation which might have
        // the default timing function for its keyframes defined on its backing Animation object.
        if (!i) {
            if (is<CSSAnimation>(animation()))
                return progressUntilNextStepInInterval(0, intervalEndProgress, downcast<DeclarativeAnimation>(*animation()).backingAnimation().timingFunction());
            return std::nullopt;
        }

        return progressUntilNextStepInInterval(m_blendingKeyframes[i - 1].key(), intervalEndProgress, timingFunctionForKeyframeAtIndex(i - 1));
    }

    // If we end up here, then this means we are dealing with an implicit 100% keyframe.
    // This will be a linear timing function unless we're dealing with a CSS Animation which might have
    // the default timing function for its keyframes defined on its backing Animation object.
    auto& lastExplicitKeyframe = m_blendingKeyframes[m_blendingKeyframes.size() - 1];
    if (is<CSSAnimation>(animation()))
        return progressUntilNextStepInInterval(lastExplicitKeyframe.key(), 1, downcast<DeclarativeAnimation>(*animation()).backingAnimation().timingFunction());

    // In any other case, we are not dealing with an interval with a steps() timing function.
    return std::nullopt;
}

bool KeyframeEffect::ticksContinouslyWhileActive() const
{
    auto doesNotAffectStyles = m_blendingKeyframes.isEmpty() || m_blendingKeyframes.properties().isEmpty();
    if (doesNotAffectStyles)
        return false;

    if (isCompletelyAccelerated() && isRunningAccelerated())
        return false;

    return true;
}

Seconds KeyframeEffect::timeToNextTick(BasicEffectTiming timing) const
{
    if (timing.phase == AnimationEffectPhase::Active) {
        // CSS Animations need to trigger "animationiteration" events even if there is no need to
        // update styles while animating, so if we're dealing with one we must wait until the next iteration.
        if (!ticksContinouslyWhileActive() && is<CSSAnimation>(animation())) {
            if (auto iterationProgress = getComputedTiming().simpleIterationProgress)
                return iterationDuration() * (1 - *iterationProgress);
        }
    }

    return AnimationEffect::timeToNextTick(timing);
}

void KeyframeEffect::setComposite(CompositeOperation compositeOperation)
{
    if (m_compositeOperation == compositeOperation)
        return;

    CanBeAcceleratedMutationScope mutationScope(this);
    m_compositeOperation = compositeOperation;
    invalidate();
}

CompositeOperation KeyframeEffect::bindingsComposite() const
{
    if (is<DeclarativeAnimation>(animation()))
        downcast<DeclarativeAnimation>(*animation()).flushPendingStyleChanges();
    return composite();
}

void KeyframeEffect::setBindingsComposite(CompositeOperation compositeOperation)
{
    setComposite(compositeOperation);
    if (is<CSSAnimation>(animation()))
        downcast<CSSAnimation>(*animation()).effectCompositeOperationWasSetUsingBindings();
}

void KeyframeEffect::computeHasImplicitKeyframeForAcceleratedProperty()
{
    m_hasImplicitKeyframeForAcceleratedProperty = [&]() {
        if (m_acceleratedPropertiesState == AcceleratedProperties::None)
            return false;

        if (!m_blendingKeyframes.isEmpty()) {
            // We make a list of all animated properties and consider them all
            // implicit until proven otherwise as we iterate through all keyframes.
            auto implicitZeroProperties = m_blendingKeyframes.properties();
            auto implicitOneProperties = m_blendingKeyframes.properties();
            implicitZeroProperties.remove(CSSPropertyCustom);
            implicitOneProperties.remove(CSSPropertyCustom);
            for (auto& keyframe : m_blendingKeyframes) {
                // If the keyframe is for 0% or 100%, let's remove all of its properties from
                // our list of implicit properties.
                if (!implicitZeroProperties.isEmpty() && !keyframe.key()) {
                    for (auto property : keyframe.properties())
                        implicitZeroProperties.remove(property);
                }
                if (!implicitOneProperties.isEmpty() && keyframe.key() == 1) {
                    for (auto property : keyframe.properties())
                        implicitOneProperties.remove(property);
                }
            }
            // The only properties left are known to be implicit properties, so we must
            // check them for any accelerated property.
            for (auto implicitProperty : implicitZeroProperties) {
                if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(implicitProperty))
                    return true;
            }
            for (auto implicitProperty : implicitOneProperties) {
                if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(implicitProperty))
                    return true;
            }
            return false;
        }

        // We may not have computed keyframes yet, so we should check our parsed keyframes in the
        // same way we checked computed keyframes.
        for (auto& keyframe : m_parsedKeyframes) {
            // We keep three property lists, one which contains all properties seen across keyframes
            // which will be filtered eventually to only contain implicit properties, one containing
            // properties seen on the 0% keyframe and one containing properties seen on the 100% keyframe.
            HashSet<CSSPropertyID> implicitProperties;
            HashSet<CSSPropertyID> explicitZeroProperties;
            HashSet<CSSPropertyID> explicitOneProperties;
            auto styleProperties = keyframe.style;
            for (unsigned i = 0; i < styleProperties->propertyCount(); ++i) {
                auto property = styleProperties->propertyAt(i).id();
                // All properties may end up being implicit.
                implicitProperties.add(property);
                if (!keyframe.computedOffset)
                    explicitZeroProperties.add(property);
                else if (keyframe.computedOffset == 1)
                    explicitOneProperties.add(property);
            }
            // Let's remove all properties found on the 0% and 100% keyframes from the list of potential implicit properties.
            for (auto explicitProperty : explicitZeroProperties)
                implicitProperties.remove(explicitProperty);
            for (auto explicitProperty : explicitOneProperties)
                implicitProperties.remove(explicitProperty);
            // At this point all properties left in implicitProperties are known to be implicit,
            // so we must check them for any accelerated property.
            for (auto implicitProperty : implicitProperties) {
                if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(implicitProperty))
                    return true;
            }
        }
        return false;
    }();
}

void KeyframeEffect::computeHasKeyframeComposingAcceleratedProperty()
{
    m_hasKeyframeComposingAcceleratedProperty = [&]() {
        if (m_acceleratedPropertiesState == AcceleratedProperties::None)
            return false;

        if (!m_blendingKeyframes.isEmpty()) {
            for (auto& keyframe : m_blendingKeyframes) {
                // If we find a keyframe with a composite operation, we check whether one
                // of its properties is accelerated.
                if (auto keyframeComposite = keyframe.compositeOperation()) {
                    if (*keyframeComposite != CompositeOperation::Replace) {
                        for (auto property : keyframe.properties()) {
                            if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(property))
                                return true;
                        }
                    }
                }
            }
            return false;
        }

        // We may not have computed keyframes yet, so we should check our parsed keyframes in the
        // same way we checked computed keyframes.
        for (auto& keyframe : m_parsedKeyframes) {
            if (keyframe.composite != CompositeOperationOrAuto::Add && keyframe.composite != CompositeOperationOrAuto::Accumulate)
                continue;
            auto styleProperties = keyframe.style;
            for (unsigned i = 0; i < styleProperties->propertyCount(); ++i) {
                auto property = styleProperties->propertyAt(i).id();
                if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(property))
                    return true;
            }
        }
        return false;
    }();
}

void KeyframeEffect::effectStackNoLongerPreventsAcceleration()
{
    if (m_runningAccelerated == RunningAccelerated::Failed)
        return;

    if (m_runningAccelerated == RunningAccelerated::Prevented)
        m_runningAccelerated = RunningAccelerated::NotStarted;

    updateAcceleratedActions();
}

void KeyframeEffect::effectStackNoLongerAllowsAcceleration()
{
    addPendingAcceleratedAction(AcceleratedAction::Stop);
}

void KeyframeEffect::abilityToBeAcceleratedDidChange()
{
    if (!m_inTargetEffectStack)
        return;

    ASSERT(m_target);
    if (auto* effectStack = m_target->keyframeEffectStack(m_pseudoId))
        effectStack->effectAbilityToBeAcceleratedDidChange(*this);
}

KeyframeEffect::CanBeAcceleratedMutationScope::CanBeAcceleratedMutationScope(KeyframeEffect* effect)
    : m_effect(effect)
{
    ASSERT(effect);
    m_couldOriginallyPreventAcceleration = effect->preventsAcceleration();
}

KeyframeEffect::CanBeAcceleratedMutationScope::~CanBeAcceleratedMutationScope()
{
    if (m_effect && m_couldOriginallyPreventAcceleration != m_effect->preventsAcceleration())
        m_effect->abilityToBeAcceleratedDidChange();
}

void KeyframeEffect::lastStyleChangeEventStyleDidChange(const RenderStyle* previousStyle, const RenderStyle* currentStyle)
{
    auto hasMotionPath = [](const RenderStyle* style) {
        return style && style->offsetPath();
    };

    if (hasMotionPath(previousStyle) != hasMotionPath(currentStyle))
        abilityToBeAcceleratedDidChange();
}

} // namespace WebCore
