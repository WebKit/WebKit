/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "RenderCounter.h"
#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "ShadowData.h"
#include "StyleImage.h"
#include "StyleReflection.h"
#include "StyleResolver.h"
#include "StyleTextEdge.h"
#include <wtf/PointerComparison.h>
#include <wtf/RefPtr.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : containIntrinsicWidth(RenderStyle::initialContainIntrinsicWidth())
    , containIntrinsicHeight(RenderStyle::initialContainIntrinsicHeight())
    , perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
    , lineClamp(RenderStyle::initialLineClamp())
    , maxLines(RenderStyle::initialMaxLines())
    , overflowContinue(RenderStyle::initialOverflowContinue())
    , initialLetter(RenderStyle::initialInitialLetter())
    , marquee(StyleMarqueeData::create())
    , backdropFilter(StyleFilterData::create())
    , grid(StyleGridData::create())
    , gridItem(StyleGridItemData::create())
    // clip
    // scrollMargin
    // scrollPadding
    // counterDirectives
    , willChange(RenderStyle::initialWillChange())
    // boxReflect
    , maskBorder(NinePieceImage::Type::Mask)
    // pageSize
    , shapeOutside(RenderStyle::initialShapeOutside())
    , shapeMargin(RenderStyle::initialShapeMargin())
    , shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
    , perspective(RenderStyle::initialPerspective())
    , clipPath(RenderStyle::initialClipPath())
    , textDecorationColor(RenderStyle::initialTextDecorationColor())
    , customProperties(StyleCustomPropertyData::create())
    // customPaintWatchedProperties
    , rotate(RenderStyle::initialRotate())
    , scale(RenderStyle::initialScale())
    , translate(RenderStyle::initialTranslate())
    , offsetPath(RenderStyle::initialOffsetPath())
    // containerNames
    , viewTransitionClasses(RenderStyle::initialViewTransitionClasses())
    , viewTransitionName(RenderStyle::initialViewTransitionName())
    , columnGap(RenderStyle::initialColumnGap())
    , rowGap(RenderStyle::initialRowGap())
    , offsetDistance(RenderStyle::initialOffsetDistance())
    , offsetPosition(RenderStyle::initialOffsetPosition())
    , offsetAnchor(RenderStyle::initialOffsetAnchor())
    , offsetRotate(RenderStyle::initialOffsetRotate())
    , textDecorationThickness(RenderStyle::initialTextDecorationThickness())
    , touchActions(RenderStyle::initialTouchActions())
    , marginTrim(RenderStyle::initialMarginTrim())
    , contain(RenderStyle::initialContainment())
    // scrollSnapType
    // scrollSnapAlign
    // scrollSnapStop
    // scrollTimelines
    // scrollTimelineAxes
    // scrollTimelineNames
    // viewTimelines
    // viewTimelineAxes
    // viewTimelineInsets
    // viewTimelineNames
    // timelineScope
    // scrollbarGutter
    , scrollbarWidth(RenderStyle::initialScrollbarWidth())
    , zoom(RenderStyle::initialZoom())
    , pseudoElementNameArgument(nullAtom())
    , anchorNames(RenderStyle::initialAnchorNames())
    , positionAnchor(RenderStyle::initialPositionAnchor())
    , blockStepSize(RenderStyle::initialBlockStepSize())
    , blockStepInsert(static_cast<unsigned>(RenderStyle::initialBlockStepInsert()))
    , overscrollBehaviorX(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorX()))
    , overscrollBehaviorY(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorY()))
    , pageSizeType(static_cast<unsigned>(PageSizeType::Auto))
    , transformStyle3D(static_cast<unsigned>(RenderStyle::initialTransformStyle3D()))
    , transformStyleForcedToFlat(false)
    , backfaceVisibility(static_cast<unsigned>(RenderStyle::initialBackfaceVisibility()))
    , useSmoothScrolling(static_cast<unsigned>(RenderStyle::initialUseSmoothScrolling()))
    , textDecorationStyle(static_cast<unsigned>(RenderStyle::initialTextDecorationStyle()))
    , textGroupAlign(static_cast<unsigned>(RenderStyle::initialTextGroupAlign()))
    , contentVisibility(static_cast<unsigned>(RenderStyle::initialContentVisibility()))
    , effectiveBlendMode(static_cast<unsigned>(RenderStyle::initialBlendMode()))
    , isolation(static_cast<unsigned>(RenderStyle::initialIsolation()))
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(static_cast<unsigned>(RenderStyle::initialApplePayButtonStyle()))
    , applePayButtonType(static_cast<unsigned>(RenderStyle::initialApplePayButtonType()))
#endif
    , breakBefore(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakAfter(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakInside(static_cast<unsigned>(RenderStyle::initialBreakInside()))
    , inputSecurity(static_cast<unsigned>(RenderStyle::initialInputSecurity()))
    , containIntrinsicWidthType(static_cast<unsigned>(RenderStyle::initialContainIntrinsicWidthType()))
    , containIntrinsicHeightType(static_cast<unsigned>(RenderStyle::initialContainIntrinsicHeightType()))
    , containerType(static_cast<unsigned>(RenderStyle::initialContainerType()))
    , textBoxTrim(static_cast<unsigned>(RenderStyle::initialTextBoxTrim()))
    , overflowAnchor(static_cast<unsigned>(RenderStyle::initialOverflowAnchor()))
    , hasClip(false)
    , positionTryOrder(static_cast<unsigned>(RenderStyle::initialPositionTryOrder()))
    , fieldSizing(RenderStyle::initialFieldSizing())
{
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , containIntrinsicWidth(o.containIntrinsicWidth)
    , containIntrinsicHeight(o.containIntrinsicHeight)
    , perspectiveOriginX(o.perspectiveOriginX)
    , perspectiveOriginY(o.perspectiveOriginY)
    , lineClamp(o.lineClamp)
    , maxLines(o.maxLines)
    , overflowContinue(o.overflowContinue)
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
    , clipPath(o.clipPath)
    , textDecorationColor(o.textDecorationColor)
    , customProperties(o.customProperties)
    , customPaintWatchedProperties(o.customPaintWatchedProperties)
    , rotate(o.rotate)
    , scale(o.scale)
    , translate(o.translate)
    , offsetPath(o.offsetPath)
    , containerNames(o.containerNames)
    , viewTransitionClasses(o.viewTransitionClasses)
    , viewTransitionName(o.viewTransitionName)
    , columnGap(o.columnGap)
    , rowGap(o.rowGap)
    , offsetDistance(o.offsetDistance)
    , offsetPosition(o.offsetPosition)
    , offsetAnchor(o.offsetAnchor)
    , offsetRotate(o.offsetRotate)
    , textDecorationThickness(o.textDecorationThickness)
    , touchActions(o.touchActions)
    , marginTrim(o.marginTrim)
    , contain(o.contain)
    , scrollSnapType(o.scrollSnapType)
    , scrollSnapAlign(o.scrollSnapAlign)
    , scrollSnapStop(o.scrollSnapStop)
    , scrollTimelines(o.scrollTimelines)
    , scrollTimelineAxes(o.scrollTimelineAxes)
    , scrollTimelineNames(o.scrollTimelineNames)
    , viewTimelines(o.viewTimelines)
    , viewTimelineAxes(o.viewTimelineAxes)
    , viewTimelineInsets(o.viewTimelineInsets)
    , viewTimelineNames(o.viewTimelineNames)
    , timelineScope(o.timelineScope)
    , scrollbarGutter(o.scrollbarGutter)
    , scrollbarWidth(o.scrollbarWidth)
    , zoom(o.zoom)
    , pseudoElementNameArgument(o.pseudoElementNameArgument)
    , anchorNames(o.anchorNames)
    , positionAnchor(o.positionAnchor)
    , blockStepSize(o.blockStepSize)
    , blockStepInsert(o.blockStepInsert)
    , overscrollBehaviorX(o.overscrollBehaviorX)
    , overscrollBehaviorY(o.overscrollBehaviorY)
    , pageSizeType(o.pageSizeType)
    , transformStyle3D(o.transformStyle3D)
    , transformStyleForcedToFlat(o.transformStyleForcedToFlat)
    , backfaceVisibility(o.backfaceVisibility)
    , useSmoothScrolling(o.useSmoothScrolling)
    , textDecorationStyle(o.textDecorationStyle)
    , textGroupAlign(o.textGroupAlign)
    , contentVisibility(o.contentVisibility)
    , effectiveBlendMode(o.effectiveBlendMode)
    , isolation(o.isolation)
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(o.applePayButtonStyle)
    , applePayButtonType(o.applePayButtonType)
#endif
    , breakBefore(o.breakBefore)
    , breakAfter(o.breakAfter)
    , breakInside(o.breakInside)
    , inputSecurity(o.inputSecurity)
    , containIntrinsicWidthType(o.containIntrinsicWidthType)
    , containIntrinsicHeightType(o.containIntrinsicHeightType)
    , containerType(o.containerType)
    , textBoxTrim(o.textBoxTrim)
    , overflowAnchor(o.overflowAnchor)
    , hasClip(o.hasClip)
    , positionTryOrder(o.positionTryOrder)
    , fieldSizing(o.fieldSizing)
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
        && perspectiveOriginX == o.perspectiveOriginX
        && perspectiveOriginY == o.perspectiveOriginY
        && lineClamp == o.lineClamp
        && maxLines == o.maxLines
        && overflowContinue == o.overflowContinue
        && initialLetter == o.initialLetter
        && marquee == o.marquee
        && backdropFilter == o.backdropFilter
        && grid == o.grid
        && gridItem == o.gridItem
        && clip == o.clip
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && counterDirectives == o.counterDirectives
        && arePointingToEqualData(willChange, o.willChange)
        && arePointingToEqualData(boxReflect, o.boxReflect)
        && maskBorder == o.maskBorder
        && pageSize == o.pageSize
        && arePointingToEqualData(shapeOutside, o.shapeOutside)
        && shapeMargin == o.shapeMargin
        && shapeImageThreshold == o.shapeImageThreshold
        && perspective == o.perspective
        && arePointingToEqualData(clipPath, o.clipPath)
        && textDecorationColor == o.textDecorationColor
        && customProperties == o.customProperties
        && customPaintWatchedProperties == o.customPaintWatchedProperties
        && arePointingToEqualData(rotate, o.rotate)
        && arePointingToEqualData(scale, o.scale)
        && arePointingToEqualData(translate, o.translate)
        && arePointingToEqualData(offsetPath, o.offsetPath)
        && containerNames == o.containerNames
        && columnGap == o.columnGap
        && rowGap == o.rowGap
        && offsetDistance == o.offsetDistance
        && offsetPosition == o.offsetPosition
        && offsetAnchor == o.offsetAnchor
        && offsetRotate == o.offsetRotate
        && textDecorationThickness == o.textDecorationThickness
        && touchActions == o.touchActions
        && marginTrim == o.marginTrim
        && contain == o.contain
        && scrollSnapType == o.scrollSnapType
        && scrollSnapAlign == o.scrollSnapAlign
        && scrollSnapStop == o.scrollSnapStop
        && scrollTimelines == o.scrollTimelines
        && scrollTimelineAxes == o.scrollTimelineAxes
        && scrollTimelineNames == o.scrollTimelineNames
        && viewTimelines == o.viewTimelines
        && viewTimelineAxes == o.viewTimelineAxes
        && viewTimelineInsets == o.viewTimelineInsets
        && viewTimelineNames == o.viewTimelineNames
        && timelineScope == o.timelineScope
        && scrollbarGutter == o.scrollbarGutter
        && scrollbarWidth == o.scrollbarWidth
        && zoom == o.zoom
        && pseudoElementNameArgument == o.pseudoElementNameArgument
        && anchorNames == o.anchorNames
        && positionAnchor == o.positionAnchor
        && blockStepSize == o.blockStepSize
        && blockStepInsert == o.blockStepInsert
        && overscrollBehaviorX == o.overscrollBehaviorX
        && overscrollBehaviorY == o.overscrollBehaviorY
        && pageSizeType == o.pageSizeType
        && transformStyle3D == o.transformStyle3D
        && transformStyleForcedToFlat == o.transformStyleForcedToFlat
        && backfaceVisibility == o.backfaceVisibility
        && useSmoothScrolling == o.useSmoothScrolling
        && textDecorationStyle == o.textDecorationStyle
        && textGroupAlign == o.textGroupAlign
        && effectiveBlendMode == o.effectiveBlendMode
        && isolation == o.isolation
#if ENABLE(APPLE_PAY)
        && applePayButtonStyle == o.applePayButtonStyle
        && applePayButtonType == o.applePayButtonType
#endif
        && contentVisibility == o.contentVisibility
        && breakAfter == o.breakAfter
        && breakBefore == o.breakBefore
        && breakInside == o.breakInside
        && inputSecurity == o.inputSecurity
        && containIntrinsicWidthType == o.containIntrinsicWidthType
        && containIntrinsicHeightType == o.containIntrinsicHeightType
        && containerType == o.containerType
        && textBoxTrim == o.textBoxTrim
        && overflowAnchor == o.overflowAnchor
        && viewTransitionClasses == o.viewTransitionClasses
        && viewTransitionName == o.viewTransitionName
        && hasClip == o.hasClip
        && positionTryOrder == o.positionTryOrder
        && fieldSizing == o.fieldSizing;
}

OptionSet<Containment> StyleRareNonInheritedData::usedContain() const
{
    auto containment = contain;

    switch (static_cast<ContainerType>(containerType)) {
    case ContainerType::Normal:
        break;
    case ContainerType::Size:
        containment.add({ Containment::Style, Containment::Size });
        break;
    case ContainerType::InlineSize:
        containment.add({ Containment::Style, Containment::InlineSize });
        break;
    };

    return containment;
}

bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return !backdropFilter->operations.isEmpty();
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

    LOG_IF_DIFFERENT(perspectiveOriginX);
    LOG_IF_DIFFERENT(perspectiveOriginY);

    LOG_IF_DIFFERENT(lineClamp);
    LOG_IF_DIFFERENT(maxLines);
    LOG_IF_DIFFERENT(overflowContinue);

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

    LOG_IF_DIFFERENT(clipPath);

    LOG_IF_DIFFERENT(textDecorationColor);

    customProperties->dumpDifferences(ts, other.customProperties);
    LOG_IF_DIFFERENT(customPaintWatchedProperties);

    LOG_IF_DIFFERENT(rotate);
    LOG_IF_DIFFERENT(scale);
    LOG_IF_DIFFERENT(translate);
    LOG_IF_DIFFERENT(offsetPath);

    LOG_IF_DIFFERENT(containerNames);

    LOG_IF_DIFFERENT(viewTransitionClasses);
    LOG_IF_DIFFERENT(viewTransitionName);

    LOG_IF_DIFFERENT(columnGap);
    LOG_IF_DIFFERENT(rowGap);

    LOG_IF_DIFFERENT(offsetDistance);
    LOG_IF_DIFFERENT(offsetPosition);
    LOG_IF_DIFFERENT(offsetAnchor);
    LOG_IF_DIFFERENT(offsetRotate);

    LOG_IF_DIFFERENT(textDecorationThickness);

    LOG_IF_DIFFERENT(touchActions);
    LOG_IF_DIFFERENT(marginTrim);
    LOG_IF_DIFFERENT(contain);

    LOG_IF_DIFFERENT(scrollSnapType);
    LOG_IF_DIFFERENT(scrollSnapAlign);
    LOG_IF_DIFFERENT(scrollSnapStop);

    LOG_IF_DIFFERENT(scrollTimelines);
    LOG_IF_DIFFERENT(scrollTimelineAxes);
    LOG_IF_DIFFERENT(scrollTimelineNames);

    LOG_IF_DIFFERENT(viewTimelines);
    LOG_IF_DIFFERENT(viewTimelineAxes);
    LOG_IF_DIFFERENT(viewTimelineInsets);
    LOG_IF_DIFFERENT(viewTimelineNames);

    LOG_IF_DIFFERENT(timelineScope);

    LOG_IF_DIFFERENT(scrollbarGutter);
    LOG_IF_DIFFERENT(scrollbarWidth);

    LOG_IF_DIFFERENT(zoom);
    LOG_IF_DIFFERENT(pseudoElementNameArgument);

    LOG_IF_DIFFERENT(anchorNames);
    LOG_IF_DIFFERENT(positionAnchor);

    LOG_IF_DIFFERENT(blockStepSize);

    LOG_IF_DIFFERENT_WITH_CAST(BlockStepInsert, blockStepInsert);

    LOG_IF_DIFFERENT_WITH_CAST(OverscrollBehavior, overscrollBehaviorX);
    LOG_IF_DIFFERENT_WITH_CAST(BlockStepInsert, overscrollBehaviorY);

    LOG_IF_DIFFERENT_WITH_CAST(PageSizeType, pageSizeType);

    LOG_IF_DIFFERENT_WITH_CAST(TransformStyle3D, transformStyle3D);
    LOG_IF_DIFFERENT_WITH_CAST(bool, transformStyleForcedToFlat);
    LOG_IF_DIFFERENT_WITH_CAST(BackfaceVisibility, backfaceVisibility);

    LOG_IF_DIFFERENT_WITH_CAST(ScrollBehavior, useSmoothScrolling);
    LOG_IF_DIFFERENT_WITH_CAST(TextDecorationStyle, textDecorationStyle);
    LOG_IF_DIFFERENT_WITH_CAST(TextGroupAlign, textGroupAlign);

    LOG_IF_DIFFERENT_WITH_CAST(ContentVisibility, contentVisibility);
    LOG_IF_DIFFERENT_WITH_CAST(BlendMode, effectiveBlendMode);

    LOG_IF_DIFFERENT_WITH_CAST(Isolation, isolation);

#if ENABLE(APPLE_PAY)
    LOG_IF_DIFFERENT_WITH_CAST(ApplePayButtonStyle, applePayButtonStyle);
    LOG_IF_DIFFERENT_WITH_CAST(ApplePayButtonType, applePayButtonType);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(BreakBetween, breakBefore);
    LOG_IF_DIFFERENT_WITH_CAST(BreakBetween, breakAfter);
    LOG_IF_DIFFERENT_WITH_CAST(BreakInside, breakInside);

    LOG_IF_DIFFERENT_WITH_CAST(InputSecurity, inputSecurity);

    LOG_IF_DIFFERENT_WITH_CAST(ContainIntrinsicSizeType, containIntrinsicWidthType);
    LOG_IF_DIFFERENT_WITH_CAST(ContainIntrinsicSizeType, containIntrinsicHeightType);

    LOG_IF_DIFFERENT_WITH_CAST(ContainerType, containerType);
    LOG_IF_DIFFERENT_WITH_CAST(TextBoxTrim, textBoxTrim);
    LOG_IF_DIFFERENT_WITH_CAST(OverflowAnchor, overflowAnchor);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasClip);
    LOG_IF_DIFFERENT_WITH_CAST(Style::PositionTryOrder, positionTryOrder);
    LOG_IF_DIFFERENT(fieldSizing);
}
#endif // !LOG_DISABLED

} // namespace WebCore
