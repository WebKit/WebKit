/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSToStyleMap.h"

#include "Animation.h"
#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParser.h"
#include "CSSQuadValue.h"
#include "CSSScrollValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueKeywords.h"
#include "CSSViewValue.h"
#include "CompositeOperation.h"
#include "FillLayer.h"
#include "ScrollTimeline.h"
#include "StyleBuilderConverter.h"
#include "StyleResolver.h"
#include "ViewTimeline.h"

namespace WebCore {

static bool treatAsInitialValue(const CSSValue& value, CSSPropertyID propertyID)
{
    switch (valueID(value)) {
    case CSSValueInitial:
        return true;
    case CSSValueUnset:
        return !CSSProperty::isInheritedProperty(propertyID);
    default:
        return false;
    }
}

CSSToStyleMap::CSSToStyleMap(Style::BuilderState& builderState)
    : m_builderState(builderState)
{
}

RenderStyle* CSSToStyleMap::style() const
{
    return &m_builderState.style();
}

RefPtr<StyleImage> CSSToStyleMap::styleImage(const CSSValue& value)
{
    return m_builderState.createStyleImage(value);
}

void CSSToStyleMap::mapFillAttachment(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setAttachment(FillLayer::initialFillAttachment(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueFixed:
        layer.setAttachment(FillAttachment::FixedBackground);
        break;
    case CSSValueScroll:
        layer.setAttachment(FillAttachment::ScrollBackground);
        break;
    case CSSValueLocal:
        layer.setAttachment(FillAttachment::LocalBackground);
        break;
    default:
        return;
    }
}

void CSSToStyleMap::mapFillClip(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setClip(FillLayer::initialFillClip(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setClip(fromCSSValue<FillBox>(value));
}

void CSSToStyleMap::mapFillComposite(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setComposite(FillLayer::initialFillComposite(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setComposite(fromCSSValue<CompositeOperator>(value));
}

void CSSToStyleMap::mapFillBlendMode(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setBlendMode(FillLayer::initialFillBlendMode(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setBlendMode(fromCSSValue<BlendMode>(value));
}

void CSSToStyleMap::mapFillOrigin(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setOrigin(FillLayer::initialFillOrigin(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setOrigin(fromCSSValue<FillBox>(value));
}

void CSSToStyleMap::mapFillImage(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setImage(FillLayer::initialFillImage(layer.type()));
        return;
    }

    layer.setImage(styleImage(value));
}

void CSSToStyleMap::mapFillRepeat(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setRepeat(FillLayer::initialFillRepeat(layer.type()));
        return;
    }

    auto* backgroundRepeatValue = dynamicDowncast<CSSBackgroundRepeatValue>(value);
    if (!backgroundRepeatValue)
        return;

    auto repeatX = fromCSSValueID<FillRepeat>(backgroundRepeatValue->xValue());
    auto repeatY = fromCSSValueID<FillRepeat>(backgroundRepeatValue->yValue());
    layer.setRepeat(FillRepeatXY { repeatX, repeatY });
}

static inline bool convertToLengthSize(const CSSValue& value, CSSToLengthConversionData conversionData, LengthSize& size)
{
    if (value.isPair()) {
        size.width = downcast<CSSPrimitiveValue>(value.first()).convertToLength<AnyConversion>(conversionData);
        size.height = downcast<CSSPrimitiveValue>(value.second()).convertToLength<AnyConversion>(conversionData);
    } else
        size.width = downcast<CSSPrimitiveValue>(value).convertToLength<AnyConversion>(conversionData);
    return !size.width.isUndefined() && !size.height.isUndefined();
}

void CSSToStyleMap::mapFillSize(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setSize(FillLayer::initialFillSize(layer.type()));
        return;
    }

    FillSize fillSize;
    switch (value.valueID()) {
    case CSSValueContain:
        fillSize.type = FillSizeType::Contain;
        break;
    case CSSValueCover:
        fillSize.type = FillSizeType::Cover;
        break;
    default:
        ASSERT(fillSize.type == FillSizeType::Size);
        if (!convertToLengthSize(value, m_builderState.cssToLengthConversionData(), fillSize.size))
            return;
        break;
    }
    layer.setSize(fillSize);
}

void CSSToStyleMap::mapFillXPosition(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setXPosition(FillLayer::initialFillXPosition(layer.type()));
        return;
    }

    Length length;
    if (value.isPair()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionX || propertyID == CSSPropertyWebkitMaskPositionX);
        length = Style::BuilderConverter::convertLength(m_builderState, value.second());
    } else
        length = Style::BuilderConverter::convertPositionComponentX(m_builderState, value);

    layer.setXPosition(length);
    if (value.isPair())
        layer.setBackgroundXOrigin(fromCSSValue<Edge>(value.first()));
}

void CSSToStyleMap::mapFillYPosition(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, propertyID)) {
        layer.setYPosition(FillLayer::initialFillYPosition(layer.type()));
        return;
    }

    Length length;
    if (value.isPair()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionY || propertyID == CSSPropertyWebkitMaskPositionY);
        length = Style::BuilderConverter::convertLength(m_builderState, value.second());
    } else
        length = Style::BuilderConverter::convertPositionComponentY(m_builderState, value);

    layer.setYPosition(length);
    if (value.isPair())
        layer.setBackgroundYOrigin(fromCSSValue<Edge>(value.first()));
}

void CSSToStyleMap::mapFillMaskMode(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    MaskMode maskMode = FillLayer::initialFillMaskMode(layer.type());
    if (treatAsInitialValue(value, propertyID)) {
        layer.setMaskMode(maskMode);
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueAlpha:
        maskMode = MaskMode::Alpha;
        break;
    case CSSValueLuminance:
        maskMode = MaskMode::Luminance;
        break;
    case CSSValueMatchSource:
        ASSERT(propertyID == CSSPropertyMaskMode);
        maskMode = MaskMode::MatchSource;
        break;
    case CSSValueAuto:
        ASSERT(propertyID == CSSPropertyWebkitMaskSourceType);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    layer.setMaskMode(maskMode);
}

void CSSToStyleMap::mapAnimationDelay(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDelay)) {
        animation.setDelay(Animation::initialDelay());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    animation.setDelay(primitiveValue->computeTime<double, CSSPrimitiveValue::Seconds>());
}

void CSSToStyleMap::mapAnimationDirection(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDirection)) {
        layer.setDirection(Animation::initialDirection());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueNormal:
        layer.setDirection(Animation::Direction::Normal);
        break;
    case CSSValueAlternate:
        layer.setDirection(Animation::Direction::Alternate);
        break;
    case CSSValueReverse:
        layer.setDirection(Animation::Direction::Reverse);
        break;
    case CSSValueAlternateReverse:
        layer.setDirection(Animation::Direction::AlternateReverse);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationDuration(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDuration)) {
        animation.setDuration(Animation::initialDuration());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    auto duration = std::max<double>(primitiveValue->computeTime<double, CSSPrimitiveValue::Seconds>(), 0);
    animation.setDuration(duration);
}

void CSSToStyleMap::mapAnimationFillMode(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationFillMode)) {
        layer.setFillMode(Animation::initialFillMode());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueNone:
        layer.setFillMode(AnimationFillMode::None);
        break;
    case CSSValueForwards:
        layer.setFillMode(AnimationFillMode::Forwards);
        break;
    case CSSValueBackwards:
        layer.setFillMode(AnimationFillMode::Backwards);
        break;
    case CSSValueBoth:
        layer.setFillMode(AnimationFillMode::Both);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationIterationCount(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationIterationCount)) {
        animation.setIterationCount(Animation::initialIterationCount());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueInfinite)
        animation.setIterationCount(Animation::IterationCountInfinite);
    else
        animation.setIterationCount(primitiveValue->floatValue());
}

void CSSToStyleMap::mapAnimationName(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationName)) {
        layer.setName(Animation::initialName());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNone)
        layer.setIsNoneAnimation(true);
    else
        layer.setName({ AtomString { primitiveValue->stringValue() }, m_builderState.styleScopeOrdinal(), primitiveValue->isCustomIdent() });
}

void CSSToStyleMap::mapAnimationPlayState(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationPlayState)) {
        layer.setPlayState(Animation::initialPlayState());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    AnimationPlayState playState = (value.valueID() == CSSValuePaused) ? AnimationPlayState::Paused : AnimationPlayState::Playing;
    layer.setPlayState(playState);
}

void CSSToStyleMap::mapAnimationProperty(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimation)) {
        animation.setProperty(Animation::initialProperty());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueAll) {
        animation.setProperty({ Animation::TransitionMode::All, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue->valueID() == CSSValueNone) {
        animation.setProperty({ Animation::TransitionMode::None, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue->propertyID() == CSSPropertyInvalid) {
        auto stringValue = primitiveValue->stringValue();
        auto transitionMode = isCustomPropertyName(stringValue) ? Animation::TransitionMode::SingleProperty : Animation::TransitionMode::UnknownProperty;
        animation.setProperty({ transitionMode, AtomString { stringValue } });
        return;
    }

    animation.setProperty({ Animation::TransitionMode::SingleProperty, primitiveValue->propertyID() });
}

void CSSToStyleMap::mapAnimationTimeline(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationTimeline))
        animation.setTimeline(Animation::initialTimeline());
    else if (auto* viewValue = dynamicDowncast<CSSViewValue>(value))
        animation.setTimeline(ViewTimeline::createFromCSSValue(*viewValue));
    else if (auto* scrollValue = dynamicDowncast<CSSScrollValue>(value))
        animation.setTimeline(ScrollTimeline::createFromCSSValue(*scrollValue));
    else if (value.isCustomIdent())
        animation.setTimeline(AtomString(value.customIdent()));
    else {
        switch (value.valueID()) {
        case CSSValueNone:
            animation.setTimeline(Animation::TimelineKeyword::None);
            break;
        case CSSValueAuto:
            animation.setTimeline(Animation::TimelineKeyword::Auto);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

void CSSToStyleMap::mapAnimationTimingFunction(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationTimingFunction))
        animation.setTimingFunction(Animation::initialTimingFunction());
    else if (auto timingFunction = TimingFunction::createFromCSSValue(value))
        animation.setTimingFunction(WTFMove(timingFunction));
}

void CSSToStyleMap::mapAnimationCompositeOperation(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationComposition))
        animation.setCompositeOperation(Animation::initialCompositeOperation());
    else if (auto compositeOperation = toCompositeOperation(value))
        animation.setCompositeOperation(*compositeOperation);
}

void CSSToStyleMap::mapAnimationAllowsDiscreteTransitions(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyTransitionBehavior))
        layer.setAllowsDiscreteTransitions(Animation::initialAllowsDiscreteTransitions());
    else if (is<CSSPrimitiveValue>(value))
        layer.setAllowsDiscreteTransitions(value.valueID() == CSSValueAllowDiscrete);
}

void CSSToStyleMap::mapNinePieceImage(const CSSValue* value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    auto* borderImage = dynamicDowncast<CSSValueList>(value);
    if (!borderImage)
        return;

    // Retrieve the border image value.
    for (auto& current : *borderImage) {
        if (current.isImage())
            image.setImage(styleImage(current));
        else if (auto* imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(current))
            mapNinePieceImageSlice(*imageSlice, image);
        else if (auto* slashList = dynamicDowncast<CSSValueList>(current)) {
            // Map in the image slices.
            if (auto* imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(slashList->item(0)))
                mapNinePieceImageSlice(*imageSlice, image);

            // Map in the border slices.
            if (auto* borderImageWidth = dynamicDowncast<CSSBorderImageWidthValue>(slashList->item(1)))
                mapNinePieceImageWidth(*borderImageWidth, image);

            // Map in the outset.
            if (slashList->item(2))
                image.setOutset(mapNinePieceImageQuad(*slashList->item(2)));
        } else if (current.isPair()) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(current, image);
        }
    }
}

void CSSToStyleMap::mapNinePieceImageSlice(const CSSValue& value, NinePieceImage& image)
{
    if (auto* sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value))
        mapNinePieceImageSlice(*sliceValue, image);
}

void CSSToStyleMap::mapNinePieceImageSlice(const CSSBorderImageSliceValue& value, NinePieceImage& image)
{
    // Set up a length box to represent our image slices.
    auto side = [](const CSSPrimitiveValue& value) -> Length {
        if (value.isPercentage())
            return { value.doubleValue(), LengthType::Percent };
        return { value.intValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed };
    };
    auto& slices = value.slices();
    image.setImageSlices({ side(slices.top()), side(slices.right()), side(slices.bottom()), side(slices.left()) });

    // Set our fill mode.
    image.setFill(value.fill());
}

void CSSToStyleMap::mapNinePieceImageWidth(const CSSValue& value, NinePieceImage& image)
{
    if (auto* widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value))
        mapNinePieceImageWidth(*widthValue, image);
}

void CSSToStyleMap::mapNinePieceImageWidth(const CSSBorderImageWidthValue& value, NinePieceImage& image)
{
    if (!is<CSSBorderImageWidthValue>(value))
        return;

    image.setBorderSlices(mapNinePieceImageQuad(value.widths()));
    image.setOverridesBorderWidths(value.overridesBorderWidths());
}

LengthBox CSSToStyleMap::mapNinePieceImageQuad(const CSSValue& value)
{
    if (LIKELY(value.isQuad()))
        return mapNinePieceImageQuad(value.quad());

    // Values coming from CSS Typed OM may not have been converted to a Quad yet.
    auto* primitive = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitive)
        return LengthBox();
    if (!primitive->isNumber() && !primitive->isLength())
        return LengthBox();
    auto side = mapNinePieceImageSide(value);
    return { Length { side }, Length { side }, Length { side }, Length { side } };
}

Length CSSToStyleMap::mapNinePieceImageSide(const CSSValue& side)
{
    auto& value = downcast<CSSPrimitiveValue>(side);
    if (value.valueID() == CSSValueAuto)
        return { };
    if (value.isNumber())
        return { value.floatValue(), LengthType::Relative };
    if (value.isPercentage())
        return { value.doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent };
    auto& conversionData = m_builderState.cssToLengthConversionData();
    if (value.isCalculatedPercentageWithLength())
        return Length { value.cssCalcValue()->createCalculationValue(conversionData) };
    return { value.computeLength<Length>(conversionData) };
}

LengthBox CSSToStyleMap::mapNinePieceImageQuad(const Quad& quad)
{
    auto side = [&](const CSSValue& value) {
        return mapNinePieceImageSide(value);
    };
    return { side(quad.top()), side(quad.right()), side(quad.bottom()), side(quad.left()) };
}

template<> constexpr NinePieceImageRule fromCSSValueID(CSSValueID valueID)
{
    switch (valueID) {
    case CSSValueStretch:
        return NinePieceImageRule::Stretch;
    case CSSValueRound:
        return NinePieceImageRule::Round;
    case CSSValueSpace:
        return NinePieceImageRule::Space;
    case CSSValueRepeat:
        return NinePieceImageRule::Repeat;
    default:
        break;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return NinePieceImageRule::Repeat;
}

void CSSToStyleMap::mapNinePieceImageRepeat(const CSSValue& value, NinePieceImage& image)
{
    if (!value.isPair())
        return;
    image.setHorizontalRule(fromCSSValue<NinePieceImageRule>(value.first()));
    image.setVerticalRule(fromCSSValue<NinePieceImageRule>(value.second()));
}

}
