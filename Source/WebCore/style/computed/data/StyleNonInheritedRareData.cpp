/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleNonInheritedRareData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NonInheritedRareData);

NonInheritedRareData::NonInheritedRareData()
    : containIntrinsicWidth(ComputedStyle::initialContainIntrinsicWidth())
    , containIntrinsicHeight(ComputedStyle::initialContainIntrinsicHeight())
    , lineClamp(ComputedStyle::initialLineClamp())
    , zoom(ComputedStyle::initialZoom())
    , maxLines(ComputedStyle::initialMaxLines())
    , touchAction(ComputedStyle::initialTouchAction())
    , initialLetter(ComputedStyle::initialInitialLetter())
    , marquee(MarqueeData::create())
    , backdropFilter(BackdropFilterData::create())
    , grid(GridData::create())
    , gridItem(GridItemData::create())
    , maskBorder(MaskBorderData::create())
    , clip(ComputedStyle::initialClip())
    , scrollMargin(0_css_px)
    , scrollPadding(CSS::Keyword::Auto { })
    , counterIncrement(ComputedStyle::initialCounterIncrement())
    , counterReset(ComputedStyle::initialCounterReset())
    , counterSet(ComputedStyle::initialCounterSet())
    , usedCounterDirectives { }
    , willChange(ComputedStyle::initialWillChange())
    , boxReflect(ComputedStyle::initialBoxReflect())
    , pageSize(ComputedStyle::initialPageSize())
    , shapeOutside(ComputedStyle::initialShapeOutside())
    , shapeMargin(ComputedStyle::initialShapeMargin())
    , shapeImageThreshold(ComputedStyle::initialShapeImageThreshold())
    , perspective(ComputedStyle::initialPerspective())
    , perspectiveOrigin({ ComputedStyle::initialPerspectiveOriginX(), ComputedStyle::initialPerspectiveOriginY() })
    , clipPath(ComputedStyle::initialClipPath())
    , customProperties(CustomPropertyData::create())
    // customPaintWatchedProperties
    , rotate(ComputedStyle::initialRotate())
    , scale(ComputedStyle::initialScale())
    , translate(ComputedStyle::initialTranslate())
    , containerNames(ComputedStyle::initialContainerNames())
    , viewTransitionClasses(ComputedStyle::initialViewTransitionClasses())
    , viewTransitionName(ComputedStyle::initialViewTransitionName())
    , columnGap(ComputedStyle::initialColumnGap())
    , rowGap(ComputedStyle::initialRowGap())
    , offsetPath(ComputedStyle::initialOffsetPath())
    , offsetDistance(ComputedStyle::initialOffsetDistance())
    , offsetPosition(ComputedStyle::initialOffsetPosition())
    , offsetAnchor(ComputedStyle::initialOffsetAnchor())
    , offsetRotate(ComputedStyle::initialOffsetRotate())
    , textDecorationColor(ComputedStyle::initialTextDecorationColor())
    , textDecorationThickness(ComputedStyle::initialTextDecorationThickness())
    // scrollTimelines
    , scrollTimelineAxes(ComputedStyle::initialScrollTimelineAxes())
    , scrollTimelineNames(ComputedStyle::initialScrollTimelineNames())
    // viewTimelines
    , viewTimelineInsets(ComputedStyle::initialViewTimelineInsets())
    , viewTimelineAxes(ComputedStyle::initialViewTimelineAxes())
    , viewTimelineNames(ComputedStyle::initialViewTimelineNames())
    , timelineScope(ComputedStyle::initialTimelineScope())
    , scrollbarGutter(ComputedStyle::initialScrollbarGutter())
    , scrollSnapType(ComputedStyle::initialScrollSnapType())
    , scrollSnapAlign(ComputedStyle::initialScrollSnapAlign())
    , pseudoElementNameArgument(nullAtom())
    , anchorNames(ComputedStyle::initialAnchorNames())
    , anchorScope(ComputedStyle::initialAnchorScope())
    , positionAnchor(ComputedStyle::initialPositionAnchor())
    , positionArea(ComputedStyle::initialPositionArea())
    , positionTryFallbacks(ComputedStyle::initialPositionTryFallbacks())
    , usedPositionOptionIndex()
    , blockStepSize(ComputedStyle::initialBlockStepSize())
    , blockStepAlign(static_cast<unsigned>(ComputedStyle::initialBlockStepAlign()))
    , blockStepInsert(static_cast<unsigned>(ComputedStyle::initialBlockStepInsert()))
    , blockStepRound(static_cast<unsigned>(ComputedStyle::initialBlockStepRound()))
    , overscrollBehaviorX(static_cast<unsigned>(ComputedStyle::initialOverscrollBehaviorX()))
    , overscrollBehaviorY(static_cast<unsigned>(ComputedStyle::initialOverscrollBehaviorY()))
    , transformStyle3D(static_cast<unsigned>(ComputedStyle::initialTransformStyle3D()))
    , transformStyleForcedToFlat(false)
    , backfaceVisibility(static_cast<unsigned>(ComputedStyle::initialBackfaceVisibility()))
    , scrollBehavior(static_cast<unsigned>(ComputedStyle::initialScrollBehavior()))
    , textDecorationStyle(static_cast<unsigned>(ComputedStyle::initialTextDecorationStyle()))
    , textGroupAlign(static_cast<unsigned>(ComputedStyle::initialTextGroupAlign()))
    , contentVisibility(static_cast<unsigned>(ComputedStyle::initialContentVisibility()))
    , effectiveBlendMode(static_cast<unsigned>(ComputedStyle::initialBlendMode()))
    , isolation(static_cast<unsigned>(ComputedStyle::initialIsolation()))
    , inputSecurity(static_cast<unsigned>(ComputedStyle::initialInputSecurity()))
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(static_cast<unsigned>(ComputedStyle::initialApplePayButtonStyle()))
    , applePayButtonType(static_cast<unsigned>(ComputedStyle::initialApplePayButtonType()))
#endif
    , breakBefore(static_cast<unsigned>(ComputedStyle::initialBreakBefore()))
    , breakAfter(static_cast<unsigned>(ComputedStyle::initialBreakAfter()))
    , breakInside(static_cast<unsigned>(ComputedStyle::initialBreakInside()))
    , containerType(static_cast<unsigned>(ComputedStyle::initialContainerType()))
    , textBoxTrim(static_cast<unsigned>(ComputedStyle::initialTextBoxTrim()))
    , overflowAnchor(static_cast<unsigned>(ComputedStyle::initialOverflowAnchor()))
    , positionTryOrder(static_cast<unsigned>(ComputedStyle::initialPositionTryOrder()))
    , positionVisibility(ComputedStyle::initialPositionVisibility().toRaw())
    , fieldSizing(static_cast<unsigned>(ComputedStyle::initialFieldSizing()))
    , nativeAppearanceDisabled(static_cast<unsigned>(false))
#if HAVE(CORE_MATERIAL)
    , appleVisualEffect(static_cast<unsigned>(ComputedStyle::initialAppleVisualEffect()))
#endif
    , scrollbarWidth(static_cast<unsigned>(ComputedStyle::initialScrollbarWidth()))
    , usesAnchorFunctions(false)
    , anchorFunctionScrollCompensatedAxes(0)
    , isPopoverInvoker(false)
    , useSVGZoomRulesForLength(false)
    , marginTrim(ComputedStyle::initialMarginTrim().toRaw())
    , contain(ComputedStyle::initialContain().toRaw())
    , overflowContinue(static_cast<unsigned>(ComputedStyle::initialOverflowContinue()))
    , scrollSnapStop(static_cast<unsigned>(ComputedStyle::initialScrollSnapStop()))
{
}

inline NonInheritedRareData::NonInheritedRareData(const NonInheritedRareData& o)
    : RefCounted<NonInheritedRareData>()
    , containIntrinsicWidth(o.containIntrinsicWidth)
    , containIntrinsicHeight(o.containIntrinsicHeight)
    , lineClamp(o.lineClamp)
    , zoom(o.zoom)
    , maxLines(o.maxLines)
    , touchAction(o.touchAction)
    , initialLetter(o.initialLetter)
    , marquee(o.marquee)
    , backdropFilter(o.backdropFilter)
    , grid(o.grid)
    , gridItem(o.gridItem)
    , maskBorder(o.maskBorder)
    , clip(o.clip)
    , scrollMargin(o.scrollMargin)
    , scrollPadding(o.scrollPadding)
    , counterIncrement(o.counterIncrement)
    , counterReset(o.counterReset)
    , counterSet(o.counterSet)
    , usedCounterDirectives(o.usedCounterDirectives)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
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
    , overflowContinue(o.overflowContinue)
    , scrollSnapStop(o.scrollSnapStop)
{
}

Ref<NonInheritedRareData> NonInheritedRareData::copy() const
{
    return adoptRef(*new NonInheritedRareData(*this));
}

NonInheritedRareData::~NonInheritedRareData() = default;

bool NonInheritedRareData::operator==(const NonInheritedRareData& o) const
{
    return containIntrinsicWidth == o.containIntrinsicWidth
        && containIntrinsicHeight == o.containIntrinsicHeight
        && lineClamp == o.lineClamp
        && zoom == o.zoom
        && maxLines == o.maxLines
        && touchAction == o.touchAction
        && initialLetter == o.initialLetter
        && marquee == o.marquee
        && backdropFilter == o.backdropFilter
        && grid == o.grid
        && gridItem == o.gridItem
        && maskBorder == o.maskBorder
        && clip == o.clip
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && counterIncrement == o.counterIncrement
        && counterReset == o.counterReset
        && counterSet == o.counterSet
        && usedCounterDirectives == o.usedCounterDirectives
        && willChange == o.willChange
        && boxReflect == o.boxReflect
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
        && contain == o.contain
        && overflowContinue == o.overflowContinue
        && scrollSnapStop == o.scrollSnapStop;
}

Contain NonInheritedRareData::usedContain() const
{
    auto result = Contain::fromRaw(contain);

    switch (static_cast<ContainerType>(containerType)) {
    case ContainerType::Normal:
        break;
    case ContainerType::Size:
        result.add({ ContainValue::Style, ContainValue::Size });
        break;
    case ContainerType::InlineSize:
        result.add({ ContainValue::Style, ContainValue::InlineSize });
        break;
    };

    return result;
}

#if !LOG_DISABLED
void NonInheritedRareData::dumpDifferences(TextStream& ts, const NonInheritedRareData& other) const
{
    marquee->dumpDifferences(ts, other.marquee);
    backdropFilter->dumpDifferences(ts, other.backdropFilter);
    grid->dumpDifferences(ts, other.grid);
    gridItem->dumpDifferences(ts, other.gridItem);
    maskBorder->dumpDifferences(ts, other.maskBorder);

    LOG_IF_DIFFERENT(containIntrinsicWidth);
    LOG_IF_DIFFERENT(containIntrinsicHeight);

    LOG_IF_DIFFERENT(lineClamp);

    LOG_IF_DIFFERENT(zoom);

    LOG_IF_DIFFERENT(maxLines);

    LOG_IF_DIFFERENT(touchAction);

    LOG_IF_DIFFERENT(initialLetter);

    LOG_IF_DIFFERENT(clip);
    LOG_IF_DIFFERENT(scrollMargin);
    LOG_IF_DIFFERENT(scrollPadding);

    LOG_IF_DIFFERENT(counterIncrement);
    LOG_IF_DIFFERENT(counterReset);
    LOG_IF_DIFFERENT(counterSet);
    LOG_IF_DIFFERENT(usedCounterDirectives);

    LOG_IF_DIFFERENT(willChange);
    LOG_IF_DIFFERENT(boxReflect);

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

    LOG_IF_DIFFERENT_WITH_CAST(ScrollBehavior, scrollBehavior);
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
    LOG_IF_DIFFERENT_WITH_CAST(PositionTryOrder, positionTryOrder);
    LOG_IF_DIFFERENT_WITH_CAST(FieldSizing, fieldSizing);

    LOG_IF_DIFFERENT_WITH_CAST(bool, nativeAppearanceDisabled);

#if HAVE(CORE_MATERIAL)
    LOG_IF_DIFFERENT_WITH_CAST(AppleVisualEffect, appleVisualEffect);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(ScrollbarWidth, scrollbarWidth);

    LOG_IF_DIFFERENT_WITH_CAST(bool, usesAnchorFunctions);
    LOG_IF_DIFFERENT_WITH_CAST(bool, anchorFunctionScrollCompensatedAxes);
    LOG_IF_DIFFERENT_WITH_CAST(bool, isPopoverInvoker);
    LOG_IF_DIFFERENT_WITH_CAST(bool, useSVGZoomRulesForLength);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(MarginTrim, marginTrim);
    LOG_IF_DIFFERENT_WITH_FROM_RAW(Contain, contain);

    LOG_IF_DIFFERENT_WITH_CAST(OverflowContinue, overflowContinue);
    LOG_IF_DIFFERENT_WITH_CAST(ScrollSnapStop, scrollSnapStop);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
