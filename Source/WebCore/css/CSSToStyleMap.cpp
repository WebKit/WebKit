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
#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueKeywords.h"
#include "CompositeOperation.h"
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

void CSSToStyleMap::mapFillRepeat(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    if (value.treatAsInitialValue(propertyID)) {
        layer.setRepeat(FillLayer::initialFillRepeat(layer.type()));
        return;
    }

    if (!is<CSSBackgroundRepeatValue>(value))
        return;

    auto& backgroundRepeatValue = downcast<CSSBackgroundRepeatValue>(value);
    FillRepeat repeatX = backgroundRepeatValue.xValue();
    FillRepeat repeatY = backgroundRepeatValue.yValue();
    layer.setRepeat(FillRepeatXY { repeatX, repeatY });
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

void CSSToStyleMap::mapFillMaskMode(CSSPropertyID propertyID, FillLayer& layer, const CSSValue& value)
{
    MaskMode maskMode = FillLayer::initialFillMaskMode(layer.type());
    if (value.treatAsInitialValue(propertyID)) {
        layer.setMaskMode(maskMode);
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (downcast<CSSPrimitiveValue>(value).valueID()) {
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
    if (value.treatAsInitialValue(CSSPropertyAnimationTimingFunction))
        animation.setTimingFunction(Animation::initialTimingFunction());
    else if (auto timingFunction = TimingFunction::createFromCSSValue(value))
        animation.setTimingFunction(WTFMove(timingFunction));
}

void CSSToStyleMap::mapAnimationCompositeOperation(Animation& animation, const CSSValue& value)
{
    if (value.treatAsInitialValue(CSSPropertyAnimationComposition))
        animation.setCompositeOperation(Animation::initialCompositeOperation());
    else if (auto compositeOperation = toCompositeOperation(value))
        animation.setCompositeOperation(*compositeOperation);
}

void CSSToStyleMap::mapNinePieceImage(CSSValue* value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    if (!is<CSSValueList>(value))
        return;

    // Retrieve the border image value.
    CSSValueList& borderImage = downcast<CSSValueList>(*value);

    for (auto& current : borderImage) {
        if (current->isImage())
            image.setImage(styleImage(current));
        else if (auto* imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(current.get()))
            mapNinePieceImageSlice(*imageSlice, image);
        else if (auto* slashList = dynamicDowncast<CSSValueList>(current.get())) {
            // Map in the image slices.
            if (auto* imageSlice = dynamicDowncast<CSSBorderImageSliceValue>(slashList->item(0)))
                mapNinePieceImageSlice(*imageSlice, image);

            // Map in the border slices.
            if (auto* borderImageWidth = dynamicDowncast<CSSBorderImageWidthValue>(slashList->item(1)))
                mapNinePieceImageWidth(*borderImageWidth, image);

            // Map in the outset.
            if (slashList->item(2))
                image.setOutset(mapNinePieceImageQuad(*slashList->item(2)));
        } else if (auto* imageRepeat = dynamicDowncast<CSSPrimitiveValue>(current.get())) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(*imageRepeat, image);
        }
    }
}

void CSSToStyleMap::mapNinePieceImageSlice(CSSValue& value, NinePieceImage& image)
{
    if (!is<CSSBorderImageSliceValue>(value))
        return;

    mapNinePieceImageSlice(downcast<CSSBorderImageSliceValue>(value), image);
}

void CSSToStyleMap::mapNinePieceImageSlice(CSSBorderImageSliceValue& value, NinePieceImage& image)
{
    // Set up a length box to represent our image slices.
    LengthBox box;
    Quad* slices = value.slices();
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
    image.setFill(value.m_fill);
}

void CSSToStyleMap::mapNinePieceImageWidth(CSSValue& value, NinePieceImage& image)
{
    if (!is<CSSBorderImageWidthValue>(value))
        return;

    return mapNinePieceImageWidth(downcast<CSSBorderImageWidthValue>(value), image);
}

void CSSToStyleMap::mapNinePieceImageWidth(CSSBorderImageWidthValue& value, NinePieceImage& image)
{
    if (!is<CSSBorderImageWidthValue>(value))
        return;

    image.setBorderSlices(mapNinePieceImageQuad(value.widths()));
    image.setOverridesBorderWidths(value.m_overridesBorderWidths);
}

LengthBox CSSToStyleMap::mapNinePieceImageQuad(CSSValue& value)
{
    if (!is<CSSPrimitiveValue>(value))
        return LengthBox();

    // Retrieve the primitive value.
    auto& borderWidths = downcast<CSSPrimitiveValue>(value);

    return mapNinePieceImageQuad(borderWidths.quadValue());
}

LengthBox CSSToStyleMap::mapNinePieceImageQuad(Quad* quad)
{
    // Get our zoom value.
    CSSToLengthConversionData conversionData = useSVGZoomRules() ? m_builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f) : m_builderState.cssToLengthConversionData();

    // Set up a length box to represent our image slices.
    LengthBox box; // Defaults to 'auto' so we don't have to handle that explicitly below.
    if (quad->top()->isNumber())
        box.top() = Length(quad->top()->floatValue(), LengthType::Relative);
    else if (quad->top()->isPercentage())
        box.top() = Length(quad->top()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (quad->top()->isCalculatedPercentageWithLength())
        box.top() = Length(quad->top()->cssCalcValue()->createCalculationValue(conversionData));
    else if (quad->top()->valueID() != CSSValueAuto)
        box.top() = quad->top()->computeLength<Length>(conversionData);

    if (quad->right()->isNumber())
        box.right() = Length(quad->right()->floatValue(), LengthType::Relative);
    else if (quad->right()->isPercentage())
        box.right() = Length(quad->right()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (quad->right()->isCalculatedPercentageWithLength())
        box.right() = Length(quad->right()->cssCalcValue()->createCalculationValue(conversionData));
    else if (quad->right()->valueID() != CSSValueAuto)
        box.right() = quad->right()->computeLength<Length>(conversionData);

    if (quad->bottom()->isNumber())
        box.bottom() = Length(quad->bottom()->floatValue(), LengthType::Relative);
    else if (quad->bottom()->isPercentage())
        box.bottom() = Length(quad->bottom()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (quad->bottom()->isCalculatedPercentageWithLength())
        box.bottom() = Length(quad->bottom()->cssCalcValue()->createCalculationValue(conversionData));
    else if (quad->bottom()->valueID() != CSSValueAuto)
        box.bottom() = quad->bottom()->computeLength<Length>(conversionData);

    if (quad->left()->isNumber())
        box.left() = Length(quad->left()->floatValue(), LengthType::Relative);
    else if (quad->left()->isPercentage())
        box.left() = Length(quad->left()->doubleValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
    else if (quad->left()->isCalculatedPercentageWithLength())
        box.left() = Length(quad->left()->cssCalcValue()->createCalculationValue(conversionData));
    else if (quad->left()->valueID() != CSSValueAuto)
        box.left() = quad->left()->computeLength<Length>(conversionData);

    return box;
}

void CSSToStyleMap::mapNinePieceImageRepeat(CSSValue& value, NinePieceImage& image)
{
    if (!is<CSSPrimitiveValue>(value))
        return;

    mapNinePieceImageRepeat(downcast<CSSPrimitiveValue>(value), image);
}

void CSSToStyleMap::mapNinePieceImageRepeat(CSSPrimitiveValue& value, NinePieceImage& image)
{
    Pair* pair = value.pairValue();
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
