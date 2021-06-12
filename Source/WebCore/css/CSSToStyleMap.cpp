/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "CSSBorderImageSliceValue.h"
#include "CSSImageGeneratorValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueKeywords.h"
#include "FillLayer.h"
#include "Pair.h"
#include "Rect.h"
#include "StyleBuilderConverter.h"
#include "StyleResolver.h"

namespace WebCore {

CSSToStyleMap::CSSToStyleMap(Style::BuilderState& builderState)
    : m_builderState(builderState)
{
}

RenderStyle* CSSToStyleMap::style() const
{
    return &m_builderState.style();
}

bool CSSToStyleMap::useSVGZoomRules() const
{
    return m_builderState.useSVGZoomRules();
}

RefPtr<StyleImage> CSSToStyleMap::styleImage(CSSValue& value)
{
    return m_builderState.createStyleImage(value);
}

void CSSToStyleMap::mapFillAttachment(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setAttachment(FillLayer::initialFillAttachment(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (downcast<CSSPrimitiveValue>(value).valueID()) {
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
    if (value.treatAsInitialValue(propertyID)) {
        layer.setClip(FillLayer::initialFillClip(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setClip(downcast<CSSPrimitiveValue>(value));
}

void CSSToStyleMap::mapFillComposite(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setComposite(FillLayer::initialFillComposite(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setComposite(downcast<CSSPrimitiveValue>(value));
}

void CSSToStyleMap::mapFillBlendMode(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setBlendMode(FillLayer::initialFillBlendMode(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setBlendMode(downcast<CSSPrimitiveValue>(value));
}

void CSSToStyleMap::mapFillOrigin(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setOrigin(FillLayer::initialFillOrigin(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setOrigin(downcast<CSSPrimitiveValue>(value));
}

void CSSToStyleMap::mapFillImage(CSSPropertyID propertyID, FillLayer& layer, CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setImage(FillLayer::initialFillImage(layer.type()));
        return;
    }

    layer.setImage(styleImage(value));
}

void CSSToStyleMap::mapFillRepeatX(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setRepeatX(FillLayer::initialFillRepeatX(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setRepeatX(downcast<CSSPrimitiveValue>(value));
}

void CSSToStyleMap::mapFillRepeatY(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setRepeatY(FillLayer::initialFillRepeatY(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    layer.setRepeatY(downcast<CSSPrimitiveValue>(value));
}

static inline bool convertToLengthSize(const CSSPrimitiveValue& primitiveValue, CSSToLengthConversionData conversionData, LengthSize& size)
{
    if (auto* pair = primitiveValue.pairValue()) {
        size.width = pair->first()->convertToLength<AnyConversion>(conversionData);
        size.height = pair->second()->convertToLength<AnyConversion>(conversionData);
    } else
        size.width = primitiveValue.convertToLength<AnyConversion>(conversionData);
    return !size.width.isUndefined() && !size.height.isUndefined();
}

void CSSToStyleMap::mapFillSize(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setSize(FillLayer::initialFillSize(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    FillSize fillSize;
    switch (primitiveValue.valueID()) {
    case CSSValueContain:
        fillSize.type = FillSizeType::Contain;
        break;
    case CSSValueCover:
        fillSize.type = FillSizeType::Cover;
        break;
    default:
        ASSERT(fillSize.type == FillSizeType::Size);
        if (!convertToLengthSize(primitiveValue, m_builderState.cssToLengthConversionData(), fillSize.size))
            return;
        break;
    }
    layer.setSize(fillSize);
}

void CSSToStyleMap::mapFillXPosition(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setXPosition(FillLayer::initialFillXPosition(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto* primitiveValue = &downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue->pairValue();
    Length length;
    if (pair) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionX || propertyID == CSSPropertyWebkitMaskPositionX);
        length = Style::BuilderConverter::convertLength(m_builderState, *pair->second());
    } else
        length = Style::BuilderConverter::convertPositionComponentX(m_builderState, value);

    layer.setXPosition(length);
    if (pair)
        layer.setBackgroundXOrigin(*pair->first());
}

void CSSToStyleMap::mapFillYPosition(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setYPosition(FillLayer::initialFillYPosition(layer.type()));
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto* primitiveValue = &downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue->pairValue();
    Length length;
    if (pair) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionY || propertyID == CSSPropertyWebkitMaskPositionY);
        length = Style::BuilderConverter::convertLength(m_builderState, *pair->second());
    } else
        length = Style::BuilderConverter::convertPositionComponentY(m_builderState, value);
    
    layer.setYPosition(length);
    if (pair)
        layer.setBackgroundYOrigin(*pair->first());
}

void CSSToStyleMap::mapFillMaskSourceType(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    MaskSourceType type = FillLayer::initialFillMaskSourceType(layer.type());
    if (value.treatAsInitialValue(propertyID)) {
        layer.setMaskSourceType(type);
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (downcast<CSSPrimitiveValue>(value).valueID()) {
    case CSSValueAlpha:
        type = MaskSourceType::Alpha;
        break;
    case CSSValueLuminance:
        type = MaskSourceType::Luminance;
        break;
    case CSSValueAuto:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    layer.setMaskSourceType(type);
}

void CSSToStyleMap::mapAnimationDelay(Animation& animation, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationDelay)) {
        animation.setDelay(Animation::initialDelay());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    animation.setDelay(downcast<CSSPrimitiveValue>(value).computeTime<double, CSSPrimitiveValue::Seconds>());
}

void CSSToStyleMap::mapAnimationDirection(Animation& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationDirection)) {
        layer.setDirection(Animation::initialDirection());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (downcast<CSSPrimitiveValue>(value).valueID()) {
    case CSSValueNormal:
        layer.setDirection(Animation::AnimationDirectionNormal);
        break;
    case CSSValueAlternate:
        layer.setDirection(Animation::AnimationDirectionAlternate);
        break;
    case CSSValueReverse:
        layer.setDirection(Animation::AnimationDirectionReverse);
        break;
    case CSSValueAlternateReverse:
        layer.setDirection(Animation::AnimationDirectionAlternateReverse);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationDuration(Animation& animation, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationDuration)) {
        animation.setDuration(Animation::initialDuration());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    animation.setDuration(downcast<CSSPrimitiveValue>(value).computeTime<double, CSSPrimitiveValue::Seconds>());
}

void CSSToStyleMap::mapAnimationFillMode(Animation& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationFillMode)) {
        layer.setFillMode(Animation::initialFillMode());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (downcast<CSSPrimitiveValue>(value).valueID()) {
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
    if (value.treatAsInitialValue(CSSPropertyAnimationIterationCount)) {
        animation.setIterationCount(Animation::initialIterationCount());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueInfinite)
        animation.setIterationCount(Animation::IterationCountInfinite);
    else
        animation.setIterationCount(primitiveValue.floatValue());
}

void CSSToStyleMap::mapAnimationName(Animation& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationName)) {
        layer.setName(Animation::initialName());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone)
        layer.setIsNoneAnimation(true);
    else
        layer.setName({ primitiveValue.stringValue(), primitiveValue.isCustomIdent() }, m_builderState.styleScopeOrdinal());
}

void CSSToStyleMap::mapAnimationPlayState(Animation& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationPlayState)) {
        layer.setPlayState(Animation::initialPlayState());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    AnimationPlayState playState = (downcast<CSSPrimitiveValue>(value).valueID() == CSSValuePaused) ? AnimationPlayState::Paused : AnimationPlayState::Playing;
    layer.setPlayState(playState);
}

void CSSToStyleMap::mapAnimationProperty(Animation& animation, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimation)) {
        animation.setProperty(Animation::initialProperty());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAll) {
        animation.setProperty({ Animation::TransitionMode::All, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue.valueID() == CSSValueNone) {
        animation.setProperty({ Animation::TransitionMode::None, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue.propertyID() == CSSPropertyInvalid) {
        animation.setProperty({ Animation::TransitionMode::UnknownProperty, CSSPropertyInvalid });
        animation.setUnknownProperty(primitiveValue.stringValue());
        return;
    }

    animation.setProperty({ Animation::TransitionMode::SingleProperty, primitiveValue.propertyID() });
}

void CSSToStyleMap::mapAnimationTimingFunction(Animation& animation, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationTimingFunction)) {
        animation.setTimingFunction(Animation::initialTimingFunction());
        return;
    }

    if (is<CSSPrimitiveValue>(value)) {
        switch (downcast<CSSPrimitiveValue>(value).valueID()) {
        case CSSValueLinear:
            animation.setTimingFunction(LinearTimingFunction::create());
            break;
        case CSSValueEase:
            animation.setTimingFunction(CubicBezierTimingFunction::create());
            break;
        case CSSValueEaseIn:
            animation.setTimingFunction(CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseIn));
            break;
        case CSSValueEaseOut:
            animation.setTimingFunction(CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseOut));
            break;
        case CSSValueEaseInOut:
            animation.setTimingFunction(CubicBezierTimingFunction::create(CubicBezierTimingFunction::EaseInOut));
            break;
        case CSSValueStepStart:
            animation.setTimingFunction(StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::Start));
            break;
        case CSSValueStepEnd:
            animation.setTimingFunction(StepsTimingFunction::create(1, StepsTimingFunction::StepPosition::End));
            break;
        default:
            break;
        }
        return;
    }

    if (is<CSSCubicBezierTimingFunctionValue>(value)) {
        auto& cubicTimingFunction = downcast<CSSCubicBezierTimingFunctionValue>(value);
        animation.setTimingFunction(CubicBezierTimingFunction::create(cubicTimingFunction.x1(), cubicTimingFunction.y1(), cubicTimingFunction.x2(), cubicTimingFunction.y2()));
    } else if (is<CSSStepsTimingFunctionValue>(value)) {
        auto& stepsTimingFunction = downcast<CSSStepsTimingFunctionValue>(value);
        animation.setTimingFunction(StepsTimingFunction::create(stepsTimingFunction.numberOfSteps(), stepsTimingFunction.stepPosition()));
    } else if (is<CSSSpringTimingFunctionValue>(value)) {
        auto& springTimingFunction = downcast<CSSSpringTimingFunctionValue>(value);
        animation.setTimingFunction(SpringTimingFunction::create(springTimingFunction.mass(), springTimingFunction.stiffness(), springTimingFunction.damping(), springTimingFunction.initialVelocity()));
    }
}

void CSSToStyleMap::mapNinePieceImage(CSSPropertyID property, CSSValue* value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    if (!is<CSSValueList>(value))
        return;

    // Retrieve the border image value.
    CSSValueList& borderImage = downcast<CSSValueList>(*value);

    for (auto& current : borderImage) {
        if (is<CSSImageValue>(current) || is<CSSImageGeneratorValue>(current) || is<CSSImageSetValue>(current))
            image.setImage(styleImage(current.get()));
        else if (is<CSSBorderImageSliceValue>(current))
            mapNinePieceImageSlice(current, image);
        else if (is<CSSValueList>(current)) {
            CSSValueList& slashList = downcast<CSSValueList>(current.get());
            // Map in the image slices.
            if (is<CSSBorderImageSliceValue>(slashList.item(0)))
                mapNinePieceImageSlice(*slashList.item(0), image);

            // Map in the border slices.
            if (slashList.item(1))
                image.setBorderSlices(mapNinePieceImageQuad(*slashList.item(1)));

            // Map in the outset.
            if (slashList.item(2))
                image.setOutset(mapNinePieceImageQuad(*slashList.item(2)));
        } else if (is<CSSPrimitiveValue>(current)) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(current, image);
        }
    }

    if (property == CSSPropertyWebkitBorderImage) {
        // We have to preserve the legacy behavior of -webkit-border-image and make the border slices
        // also set the border widths. We don't need to worry about percentages, since we don't even support
        // those on real borders yet.
        if (image.borderSlices().top().isFixed())
            style()->setBorderTopWidth(image.borderSlices().top().value());
        if (image.borderSlices().right().isFixed())
            style()->setBorderRightWidth(image.borderSlices().right().value());
        if (image.borderSlices().bottom().isFixed())
            style()->setBorderBottomWidth(image.borderSlices().bottom().value());
        if (image.borderSlices().left().isFixed())
            style()->setBorderLeftWidth(image.borderSlices().left().value());
    }
}

void CSSToStyleMap::mapNinePieceImageSlice(CSSValue& value, NinePieceImage& image)
{
    if (!is<CSSBorderImageSliceValue>(value))
        return;

    // Retrieve the border image value.
    auto& borderImageSlice = downcast<CSSBorderImageSliceValue>(value);

    // Set up a length box to represent our image slices.
    LengthBox box;
    Quad* slices = borderImageSlice.slices();
    if (slices->top()->isPercentage())
        box.top() = Length(slices->top()->doubleValue(), LengthType::Percent);
    else
        box.top() = Length(slices->top()->intValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed);
    if (slices->bottom()->isPercentage())
        box.bottom() = Length(slices->bottom()->doubleValue(), LengthType::Percent);
    else
        box.bottom() = Length((int)slices->bottom()->floatValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed);
    if (slices->left()->isPercentage())
        box.left() = Length(slices->left()->doubleValue(), LengthType::Percent);
    else
        box.left() = Length(slices->left()->intValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed);
    if (slices->right()->isPercentage())
        box.right() = Length(slices->right()->doubleValue(), LengthType::Percent);
    else
        box.right() = Length(slices->right()->intValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed);
    image.setImageSlices(box);

    // Set our fill mode.
    image.setFill(borderImageSlice.m_fill);
}

LengthBox CSSToStyleMap::mapNinePieceImageQuad(CSSValue& value)
{
    if (!is<CSSPrimitiveValue>(value))
        return LengthBox();

    // Get our zoom value.
    CSSToLengthConversionData conversionData = useSVGZoomRules() ? m_builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f) : m_builderState.cssToLengthConversionData();

    // Retrieve the primitive value.
    auto& borderWidths = downcast<CSSPrimitiveValue>(value);

    // Set up a length box to represent our image slices.
    LengthBox box; // Defaults to 'auto' so we don't have to handle that explicitly below.
    Quad* slices = borderWidths.quadValue();
    if (slices->top()->isNumber())
        box.top() = Length(slices->top()->floatValue(), LengthType::Relative);
    else if (slices->top()->isPercentage())
        box.top() = Length(slices->top()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (slices->top()->isCalculatedPercentageWithLength())
        box.top() = Length(slices->top()->cssCalcValue()->createCalculationValue(conversionData));
    else if (slices->top()->valueID() != CSSValueAuto)
        box.top() = slices->top()->computeLength<Length>(conversionData);

    if (slices->right()->isNumber())
        box.right() = Length(slices->right()->floatValue(), LengthType::Relative);
    else if (slices->right()->isPercentage())
        box.right() = Length(slices->right()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (slices->right()->isCalculatedPercentageWithLength())
        box.right() = Length(slices->right()->cssCalcValue()->createCalculationValue(conversionData));
    else if (slices->right()->valueID() != CSSValueAuto)
        box.right() = slices->right()->computeLength<Length>(conversionData);

    if (slices->bottom()->isNumber())
        box.bottom() = Length(slices->bottom()->floatValue(), LengthType::Relative);
    else if (slices->bottom()->isPercentage())
        box.bottom() = Length(slices->bottom()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (slices->bottom()->isCalculatedPercentageWithLength())
        box.bottom() = Length(slices->bottom()->cssCalcValue()->createCalculationValue(conversionData));
    else if (slices->bottom()->valueID() != CSSValueAuto)
        box.bottom() = slices->bottom()->computeLength<Length>(conversionData);

    if (slices->left()->isNumber())
        box.left() = Length(slices->left()->floatValue(), LengthType::Relative);
    else if (slices->left()->isPercentage())
        box.left() = Length(slices->left()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (slices->left()->isCalculatedPercentageWithLength())
        box.left() = Length(slices->left()->cssCalcValue()->createCalculationValue(conversionData));
    else if (slices->left()->valueID() != CSSValueAuto)
        box.left() = slices->left()->computeLength<Length>(conversionData);

    return box;
}

void CSSToStyleMap::mapNinePieceImageRepeat(CSSValue& value, NinePieceImage& image)
{
    if (!is<CSSPrimitiveValue>(value))
        return;

    CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Pair* pair = primitiveValue.pairValue();
    if (!pair || !pair->first() || !pair->second())
        return;

    CSSValueID firstIdentifier = pair->first()->valueID();
    CSSValueID secondIdentifier = pair->second()->valueID();

    NinePieceImageRule horizontalRule;
    switch (firstIdentifier) {
    case CSSValueStretch:
        horizontalRule = NinePieceImageRule::Stretch;
        break;
    case CSSValueRound:
        horizontalRule = NinePieceImageRule::Round;
        break;
    case CSSValueSpace:
        horizontalRule = NinePieceImageRule::Space;
        break;
    default: // CSSValueRepeat
        horizontalRule = NinePieceImageRule::Repeat;
        break;
    }
    image.setHorizontalRule(horizontalRule);

    NinePieceImageRule verticalRule;
    switch (secondIdentifier) {
    case CSSValueStretch:
        verticalRule = NinePieceImageRule::Stretch;
        break;
    case CSSValueRound:
        verticalRule = NinePieceImageRule::Round;
        break;
    case CSSValueSpace:
        verticalRule = NinePieceImageRule::Space;
        break;
    default: // CSSValueRepeat
        verticalRule = NinePieceImageRule::Repeat;
        break;
    }
    image.setVerticalRule(verticalRule);
}

};
