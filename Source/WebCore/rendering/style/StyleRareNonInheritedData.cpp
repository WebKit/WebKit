/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleRareNonInheritedData.h"

#include "PathOperation.h"
#include "RenderCounter.h"
#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "StyleImage.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "StyleResolver.h"
#include <wtf/PointerComparison.h>
#include <wtf/RefPtr.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : containIntrinsicWidth(RenderStyle::initialContainIntrinsicWidth())
    , containIntrinsicHeight(RenderStyle::initialContainIntrinsicHeight())
    , lineClamp(RenderStyle::initialLineClamp())
    , zoom(RenderStyle::initialZoom())
    , maxLines(RenderStyle::initialMaxLines())
    , overflowContinue(RenderStyle::initialOverflowContinue())
    , touchAction(RenderStyle::initialTouchAction())
    , initialLetter(RenderStyle::initialInitialLetter())
    , marquee(StyleMarqueeData::create())
    , backdropFilter(StyleBackdropFilterData::create())
    , grid(StyleGridData::create())
    , gridItem(StyleGridItemData::create())
    , clip(RenderStyle::initialClip())
    // scrollMargin
    // scrollPadding
    // counterDirectives
    , willChange(RenderStyle::initialWillChange())
    , boxReflect(RenderStyle::initialBoxReflect())
    , maskBorder(RenderStyle::initialMaskBorder())
    , pageSize(RenderStyle::initialPageSize())
    , shapeOutside(RenderStyle::initialShapeOutside())
    , shapeMargin(RenderStyle::initialShapeMargin())
    , shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
    , perspective(RenderStyle::initialPerspective())
    , perspectiveOrigin(RenderStyle::initialPerspectiveOrigin())
    , clipPath(RenderStyle::initialClipPath())
    , customProperties(Style::CustomPropertyData::create())
    // customPaintWatchedProperties
    , rotate(RenderStyle::initialRotate())
    , scale(RenderStyle::initialScale())
    , translate(RenderStyle::initialTranslate())
    , containerNames(RenderStyle::initialContainerNames())
    , viewTransitionClasses(RenderStyle::initialViewTransitionClasses())
    , viewTransitionName(RenderStyle::initialViewTransitionName())
    , columnGap(RenderStyle::initialColumnGap())
    , rowGap(RenderStyle::initialRowGap())
    , itemTolerance(RenderStyle::initialItemTolerance())
    , offsetPath(RenderStyle::initialOffsetPath())
    , offsetDistance(RenderStyle::initialOffsetDistance())
    , offsetPosition(RenderStyle::initialOffsetPosition())
    , offsetAnchor(RenderStyle::initialOffsetAnchor())
    , offsetRotate(RenderStyle::initialOffsetRotate())
    , textDecorationColor(RenderStyle::initialTextDecorationColor())
    , textDecorationThickness(RenderStyle::initialTextDecorationThickness())
    // scrollTimelines
    , scrollTimelineAxes(RenderStyle::initialScrollTimelineAxes())
    , scrollTimelineNames(RenderStyle::initialScrollTimelineNames())
    // viewTimelines
    , viewTimelineInsets(RenderStyle::initialViewTimelineInsets())
    , viewTimelineAxes(RenderStyle::initialViewTimelineAxes())
    , viewTimelineNames(RenderStyle::initialViewTimelineNames())
    , timelineScope(RenderStyle::initialTimelineScope())
    , scrollbarGutter(RenderStyle::initialScrollbarGutter())
    , scrollSnapType(RenderStyle::initialScrollSnapType())
    , scrollSnapAlign(RenderStyle::initialScrollSnapAlign())
    , scrollSnapStop(RenderStyle::initialScrollSnapStop())
    , pseudoElementNameArgument(nullAtom())
    , anchorNames(RenderStyle::initialAnchorNames())
    , anchorScope(RenderStyle::initialAnchorScope())
    , positionAnchor(RenderStyle::initialPositionAnchor())
    , positionArea(RenderStyle::initialPositionArea())
    , positionTryFallbacks(RenderStyle::initialPositionTryFallbacks())
    , usedPositionOptionIndex()
    , blockStepSize(RenderStyle::initialBlockStepSize())
    , blockStepAlign(static_cast<unsigned>(RenderStyle::initialBlockStepAlign()))
    , blockStepInsert(static_cast<unsigned>(RenderStyle::initialBlockStepInsert()))
    , blockStepRound(static_cast<unsigned>(RenderStyle::initialBlockStepRound()))
    , overscrollBehaviorX(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorX()))
    , overscrollBehaviorY(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorY()))
    , transformStyle3D(static_cast<unsigned>(RenderStyle::initialTransformStyle3D()))
    , transformStyleForcedToFlat(false)
    , backfaceVisibility(static_cast<unsigned>(RenderStyle::initialBackfaceVisibility()))
    , scrollBehavior(static_cast<unsigned>(RenderStyle::initialScrollBehavior()))
    , textDecorationStyle(static_cast<unsigned>(RenderStyle::initialTextDecorationStyle()))
    , textGroupAlign(static_cast<unsigned>(RenderStyle::initialTextGroupAlign()))
    , contentVisibility(static_cast<unsigned>(RenderStyle::initialContentVisibility()))
    , effectiveBlendMode(static_cast<unsigned>(RenderStyle::initialBlendMode()))
    , isolation(static_cast<unsigned>(RenderStyle::initialIsolation()))
    , inputSecurity(static_cast<unsigned>(RenderStyle::initialInputSecurity()))
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(static_cast<unsigned>(RenderStyle::initialApplePayButtonStyle()))
    , applePayButtonType(static_cast<unsigned>(RenderStyle::initialApplePayButtonType()))
#endif
    , breakBefore(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakAfter(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakInside(static_cast<unsigned>(RenderStyle::initialBreakInside()))
    , containerType(static_cast<unsigned>(RenderStyle::initialContainerType()))
    , textBoxTrim(static_cast<unsigned>(RenderStyle::initialTextBoxTrim()))
    , overflowAnchor(static_cast<unsigned>(RenderStyle::initialOverflowAnchor()))
    , positionTryOrder(static_cast<unsigned>(RenderStyle::initialPositionTryOrder()))
    , positionVisibility(RenderStyle::initialPositionVisibility().toRaw())
    , fieldSizing(static_cast<unsigned>(RenderStyle::initialFieldSizing()))
    , nativeAppearanceDisabled(static_cast<unsigned>(RenderStyle::initialNativeAppearanceDisabled()))
#if HAVE(CORE_MATERIAL)
    , appleVisualEffect(static_cast<unsigned>(RenderStyle::initialAppleVisualEffect()))
#endif
    , scrollbarWidth(static_cast<unsigned>(RenderStyle::initialScrollbarWidth()))
    , usesAnchorFunctions(false)
    , anchorFunctionScrollCompensatedAxes(0)
    , isPopoverInvoker(false)
    , useSVGZoomRulesForLength(false)
    , marginTrim(RenderStyle::initialMarginTrim().toRaw())
    , contain(RenderStyle::initialContain().toRaw())
{
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , containIntrinsicWidth(o.containIntrinsicWidth)
    , containIntrinsicHeight(o.containIntrinsicHeight)
    , lineClamp(o.lineClamp)
    , zoom(o.zoom)
    , maxLines(o.maxLines)
    , overflowContinue(o.overflowContinue)
    , touchAction(o.touchAction)
    , initialLetter(o.initialLetter)
    , marquee(o.marquee)
    , backdropFilter(o.backdropFilter)
    , grid(o.grid)
    , gridItem(o.gridItem)
    , clip(o.clip)
    , scrollMargin(o.scrollMargin)
    , scrollPadding(o.scrollPadding)
    , counterDirectives(o.counterDirectives)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
    , maskBorder(o.maskBorder)
    , pageSize(o.pageSize)
    , shapeOutside(o.shapeOutside)
    , shapeMargin(o.shapeMargin)
    , shapeImageThreshold(o.shapeImageThreshold)
    , perspective(o.perspective)
    , perspectiveOrigin(o.perspectiveOrigin)
    , clipPath(o.clipPath)
    , customProperties(o.customProperties)
    , customPaintWatchedProperties(o.customPaintWatchedProperties)
    , rotate(o.rotate)
    , scale(o.scale)
    , translate(o.translate)
    , containerNames(o.containerNames)
    , viewTransitionClasses(o.viewTransitionClasses)
    , viewTransitionName(o.viewTransitionName)
    , columnGap(o.columnGap)
    , rowGap(o.rowGap)
    , itemTolerance(o.itemTolerance)
    , offsetPath(o.offsetPath)
    , offsetDistance(o.offsetDistance)
    , offsetPosition(o.offsetPosition)
    , offsetAnchor(o.offsetAnchor)
    , offsetRotate(o.offsetRotate)
    , textDecorationColor(o.textDecorationColor)
    , textDecorationThickness(o.textDecorationThickness)
    , scrollTimelines(o.scrollTimelines)
    , scrollTimelineAxes(o.scrollTimelineAxes)
    , scrollTimelineNames(o.scrollTimelineNames)
    , viewTimelines(o.viewTimelines)
    , viewTimelineInsets(o.viewTimelineInsets)
    , viewTimelineAxes(o.viewTimelineAxes)
    , viewTimelineNames(o.viewTimelineNames)
    , timelineScope(o.timelineScope)
    , scrollbarGutter(o.scrollbarGutter)
    , scrollSnapType(o.scrollSnapType)
    , scrollSnapAlign(o.scrollSnapAlign)
    , scrollSnapStop(o.scrollSnapStop)
    , pseudoElementNameArgument(o.pseudoElementNameArgument)
    , anchorNames(o.anchorNames)
    , anchorScope(o.anchorScope)
    , positionAnchor(o.positionAnchor)
    , positionArea(o.positionArea)
    , positionTryFallbacks(o.positionTryFallbacks)
    , usedPositionOptionIndex(o.usedPositionOptionIndex)
    , blockStepSize(o.blockStepSize)
    , blockStepAlign(o.blockStepAlign)
    , blockStepInsert(o.blockStepInsert)
    , blockStepRound(o.blockStepRound)
    , overscrollBehaviorX(o.overscrollBehaviorX)
    , overscrollBehaviorY(o.overscrollBehaviorY)
    , transformStyle3D(o.transformStyle3D)
    , transformStyleForcedToFlat(o.transformStyleForcedToFlat)
    , backfaceVisibility(o.backfaceVisibility)
    , scrollBehavior(o.scrollBehavior)
    , textDecorationStyle(o.textDecorationStyle)
    , textGroupAlign(o.textGroupAlign)
    , contentVisibility(o.contentVisibility)
    , effectiveBlendMode(o.effectiveBlendMode)
    , isolation(o.isolation)
    , inputSecurity(o.inputSecurity)
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(o.applePayButtonStyle)
    , applePayButtonType(o.applePayButtonType)
#endif
    , breakBefore(o.breakBefore)
    , breakAfter(o.breakAfter)
    , breakInside(o.breakInside)
    , containerType(o.containerType)
    , textBoxTrim(o.textBoxTrim)
    , overflowAnchor(o.overflowAnchor)
    , positionTryOrder(o.positionTryOrder)
    , positionVisibility(o.positionVisibility)
    , fieldSizing(o.fieldSizing)
    , nativeAppearanceDisabled(o.nativeAppearanceDisabled)
#if HAVE(CORE_MATERIAL)
    , appleVisualEffect(o.appleVisualEffect)
#endif
    , scrollbarWidth(o.scrollbarWidth)
    , usesAnchorFunctions(o.usesAnchorFunctions)
    , anchorFunctionScrollCompensatedAxes(o.anchorFunctionScrollCompensatedAxes)
    , isPopoverInvoker(o.isPopoverInvoker)
    , useSVGZoomRulesForLength(o.useSVGZoomRulesForLength)
    , marginTrim(o.marginTrim)
    , contain(o.contain)
{
}

Ref<StyleRareNonInheritedData> StyleRareNonInheritedData::copy() const
{
    return adoptRef(*new StyleRareNonInheritedData(*this));
}

StyleRareNonInheritedData::~StyleRareNonInheritedData() = default;

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return containIntrinsicWidth == o.containIntrinsicWidth
        && containIntrinsicHeight == o.containIntrinsicHeight
        && lineClamp == o.lineClamp
        && zoom == o.zoom
        && maxLines == o.maxLines
        && overflowContinue == o.overflowContinue
        && touchAction == o.touchAction
        && initialLetter == o.initialLetter
        && marquee == o.marquee
        && backdropFilter == o.backdropFilter
        && grid == o.grid
        && gridItem == o.gridItem
        && clip == o.clip
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && counterDirectives == o.counterDirectives
        && willChange == o.willChange
        && boxReflect == o.boxReflect
        && maskBorder == o.maskBorder
        && pageSize == o.pageSize
        && shapeOutside == o.shapeOutside
        && shapeMargin == o.shapeMargin
        && shapeImageThreshold == o.shapeImageThreshold
        && perspective == o.perspective
        && perspectiveOrigin == o.perspectiveOrigin
        && clipPath == o.clipPath
        && textDecorationColor == o.textDecorationColor
        && customProperties == o.customProperties
        && customPaintWatchedProperties == o.customPaintWatchedProperties
        && rotate == o.rotate
        && scale == o.scale
        && translate == o.translate
        && containerNames == o.containerNames
        && columnGap == o.columnGap
        && rowGap == o.rowGap
        && itemTolerance == o.itemTolerance
        && offsetPath == o.offsetPath
        && offsetDistance == o.offsetDistance
        && offsetPosition == o.offsetPosition
        && offsetAnchor == o.offsetAnchor
        && offsetRotate == o.offsetRotate
        && textDecorationThickness == o.textDecorationThickness
        && scrollTimelines == o.scrollTimelines
        && scrollTimelineAxes == o.scrollTimelineAxes
        && scrollTimelineNames == o.scrollTimelineNames
        && viewTimelines == o.viewTimelines
        && viewTimelineInsets == o.viewTimelineInsets
        && viewTimelineAxes == o.viewTimelineAxes
        && viewTimelineNames == o.viewTimelineNames
        && timelineScope == o.timelineScope
        && scrollbarGutter == o.scrollbarGutter
        && scrollSnapType == o.scrollSnapType
        && scrollSnapAlign == o.scrollSnapAlign
        && scrollSnapStop == o.scrollSnapStop
        && pseudoElementNameArgument == o.pseudoElementNameArgument
        && anchorNames == o.anchorNames
        && anchorScope == o.anchorScope
        && positionAnchor == o.positionAnchor
        && positionArea == o.positionArea
        && positionTryFallbacks == o.positionTryFallbacks
        && usedPositionOptionIndex == o.usedPositionOptionIndex
        && blockStepSize == o.blockStepSize
        && blockStepAlign == o.blockStepAlign
        && blockStepInsert == o.blockStepInsert
        && blockStepRound == o.blockStepRound
        && overscrollBehaviorX == o.overscrollBehaviorX
        && overscrollBehaviorY == o.overscrollBehaviorY
        && transformStyle3D == o.transformStyle3D
        && transformStyleForcedToFlat == o.transformStyleForcedToFlat
        && backfaceVisibility == o.backfaceVisibility
        && scrollBehavior == o.scrollBehavior
        && textDecorationStyle == o.textDecorationStyle
        && textGroupAlign == o.textGroupAlign
        && effectiveBlendMode == o.effectiveBlendMode
        && isolation == o.isolation
        && inputSecurity == o.inputSecurity
#if ENABLE(APPLE_PAY)
        && applePayButtonStyle == o.applePayButtonStyle
        && applePayButtonType == o.applePayButtonType
#endif
        && contentVisibility == o.contentVisibility
        && breakAfter == o.breakAfter
        && breakBefore == o.breakBefore
        && breakInside == o.breakInside
        && containerType == o.containerType
        && textBoxTrim == o.textBoxTrim
        && overflowAnchor == o.overflowAnchor
        && viewTransitionClasses == o.viewTransitionClasses
        && viewTransitionName == o.viewTransitionName
        && positionTryOrder == o.positionTryOrder
        && positionVisibility == o.positionVisibility
        && fieldSizing == o.fieldSizing
        && nativeAppearanceDisabled == o.nativeAppearanceDisabled
#if HAVE(CORE_MATERIAL)
        && appleVisualEffect == o.appleVisualEffect
#endif
        && scrollbarWidth == o.scrollbarWidth
        && usesAnchorFunctions == o.usesAnchorFunctions
        && anchorFunctionScrollCompensatedAxes == o.anchorFunctionScrollCompensatedAxes
        && isPopoverInvoker == o.isPopoverInvoker
        && useSVGZoomRulesForLength == o.useSVGZoomRulesForLength
        && marginTrim == o.marginTrim
        && contain == o.contain;
}

Style::Contain StyleRareNonInheritedData::usedContain() const
{
    auto result = Style::Contain::fromRaw(contain);

    switch (static_cast<ContainerType>(containerType)) {
    case ContainerType::Normal:
        break;
    case ContainerType::Size:
        result.add({ Style::ContainValue::Style, Style::ContainValue::Size });
        break;
    case ContainerType::InlineSize:
        result.add({ Style::ContainValue::Style, Style::ContainValue::InlineSize });
        break;
    };

    return result;
}

bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return !backdropFilter->backdropFilter.isNone();
}

#if !LOG_DISABLED
void StyleRareNonInheritedData::dumpDifferences(TextStream& ts, const StyleRareNonInheritedData& other) const
{
    marquee->dumpDifferences(ts, other.marquee);
    backdropFilter->dumpDifferences(ts, other.backdropFilter);
    grid->dumpDifferences(ts, other.grid);
    gridItem->dumpDifferences(ts, other.gridItem);

    LOG_IF_DIFFERENT(containIntrinsicWidth);
    LOG_IF_DIFFERENT(containIntrinsicHeight);

    LOG_IF_DIFFERENT(lineClamp);

    LOG_IF_DIFFERENT(zoom);

    LOG_IF_DIFFERENT(maxLines);
    LOG_IF_DIFFERENT(overflowContinue);

    LOG_IF_DIFFERENT(touchAction);

    LOG_IF_DIFFERENT(initialLetter);

    LOG_IF_DIFFERENT(clip);
    LOG_IF_DIFFERENT(scrollMargin);
    LOG_IF_DIFFERENT(scrollPadding);

    LOG_IF_DIFFERENT(counterDirectives);

    LOG_IF_DIFFERENT(willChange);
    LOG_IF_DIFFERENT(boxReflect);

    LOG_IF_DIFFERENT(maskBorder);
    LOG_IF_DIFFERENT(pageSize);

    LOG_IF_DIFFERENT(shapeOutside);
    LOG_IF_DIFFERENT(shapeMargin);
    LOG_IF_DIFFERENT(shapeImageThreshold);

    LOG_IF_DIFFERENT(perspective);
    LOG_IF_DIFFERENT(perspectiveOrigin);

    LOG_IF_DIFFERENT(clipPath);

    LOG_IF_DIFFERENT(textDecorationColor);

    customProperties->dumpDifferences(ts, other.customProperties);
    LOG_IF_DIFFERENT(customPaintWatchedProperties);

    LOG_IF_DIFFERENT(rotate);
    LOG_IF_DIFFERENT(scale);
    LOG_IF_DIFFERENT(translate);

    LOG_IF_DIFFERENT(containerNames);

    LOG_IF_DIFFERENT(viewTransitionClasses);
    LOG_IF_DIFFERENT(viewTransitionName);

    LOG_IF_DIFFERENT(columnGap);
    LOG_IF_DIFFERENT(rowGap);
    LOG_IF_DIFFERENT(itemTolerance);

    LOG_IF_DIFFERENT(offsetPath);
    LOG_IF_DIFFERENT(offsetDistance);
    LOG_IF_DIFFERENT(offsetPosition);
    LOG_IF_DIFFERENT(offsetAnchor);
    LOG_IF_DIFFERENT(offsetRotate);

    LOG_IF_DIFFERENT(textDecorationThickness);

    LOG_IF_DIFFERENT(scrollTimelines);
    LOG_IF_DIFFERENT(scrollTimelineAxes);
    LOG_IF_DIFFERENT(scrollTimelineNames);

    LOG_IF_DIFFERENT(viewTimelines);
    LOG_IF_DIFFERENT(viewTimelineInsets);
    LOG_IF_DIFFERENT(viewTimelineAxes);
    LOG_IF_DIFFERENT(viewTimelineNames);

    LOG_IF_DIFFERENT(timelineScope);

    LOG_IF_DIFFERENT(scrollbarGutter);

    LOG_IF_DIFFERENT(scrollSnapType);
    LOG_IF_DIFFERENT(scrollSnapAlign);
    LOG_IF_DIFFERENT(scrollSnapStop);

    LOG_IF_DIFFERENT(pseudoElementNameArgument);

    LOG_IF_DIFFERENT(anchorNames);
    LOG_IF_DIFFERENT(anchorScope);
    LOG_IF_DIFFERENT(positionAnchor);
    LOG_IF_DIFFERENT(positionArea);
    LOG_IF_DIFFERENT(positionTryFallbacks);
    LOG_IF_DIFFERENT(usedPositionOptionIndex);
    LOG_IF_DIFFERENT(positionVisibility);

    LOG_IF_DIFFERENT(blockStepSize);

    LOG_IF_DIFFERENT_WITH_CAST(BlockStepAlign, blockStepAlign);
    LOG_IF_DIFFERENT_WITH_CAST(BlockStepInsert, blockStepInsert);
    LOG_IF_DIFFERENT_WITH_CAST(BlockStepRound, blockStepRound);

    LOG_IF_DIFFERENT_WITH_CAST(OverscrollBehavior, overscrollBehaviorX);
    LOG_IF_DIFFERENT_WITH_CAST(OverscrollBehavior, overscrollBehaviorY);

    LOG_IF_DIFFERENT_WITH_CAST(TransformStyle3D, transformStyle3D);
    LOG_IF_DIFFERENT_WITH_CAST(bool, transformStyleForcedToFlat);
    LOG_IF_DIFFERENT_WITH_CAST(BackfaceVisibility, backfaceVisibility);

    LOG_IF_DIFFERENT_WITH_CAST(Style::ScrollBehavior, scrollBehavior);
    LOG_IF_DIFFERENT_WITH_CAST(TextDecorationStyle, textDecorationStyle);
    LOG_IF_DIFFERENT_WITH_CAST(TextGroupAlign, textGroupAlign);

    LOG_IF_DIFFERENT_WITH_CAST(ContentVisibility, contentVisibility);
    LOG_IF_DIFFERENT_WITH_CAST(BlendMode, effectiveBlendMode);

    LOG_IF_DIFFERENT_WITH_CAST(Isolation, isolation);

    LOG_IF_DIFFERENT_WITH_CAST(InputSecurity, inputSecurity);

#if ENABLE(APPLE_PAY)
    LOG_IF_DIFFERENT_WITH_CAST(ApplePayButtonStyle, applePayButtonStyle);
    LOG_IF_DIFFERENT_WITH_CAST(ApplePayButtonType, applePayButtonType);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(BreakBetween, breakBefore);
    LOG_IF_DIFFERENT_WITH_CAST(BreakBetween, breakAfter);
    LOG_IF_DIFFERENT_WITH_CAST(BreakInside, breakInside);

    LOG_IF_DIFFERENT_WITH_CAST(ContainerType, containerType);
    LOG_IF_DIFFERENT_WITH_CAST(TextBoxTrim, textBoxTrim);
    LOG_IF_DIFFERENT_WITH_CAST(OverflowAnchor, overflowAnchor);
    LOG_IF_DIFFERENT_WITH_CAST(Style::PositionTryOrder, positionTryOrder);
    LOG_IF_DIFFERENT(fieldSizing);

    LOG_IF_DIFFERENT(nativeAppearanceDisabled);

#if HAVE(CORE_MATERIAL)
    LOG_IF_DIFFERENT(appleVisualEffect);
#endif

    LOG_IF_DIFFERENT(scrollbarWidth);

    LOG_IF_DIFFERENT_WITH_CAST(bool, usesAnchorFunctions);
    LOG_IF_DIFFERENT_WITH_CAST(bool, anchorFunctionScrollCompensatedAxes);
    LOG_IF_DIFFERENT_WITH_CAST(bool, isPopoverInvoker);
    LOG_IF_DIFFERENT_WITH_CAST(bool, useSVGZoomRulesForLength);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(Style::MarginTrim, marginTrim);
    LOG_IF_DIFFERENT_WITH_FROM_RAW(Style::Contain, contain);
}
#endif // !LOG_DISABLED

} // namespace WebCore
