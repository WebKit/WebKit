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
#include "RenderStyleInlines.h"
#include "ShadowData.h"
#include "StyleImage.h"
#include "StyleReflection.h"
#include "StyleResolver.h"
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
    // viewTransitionName
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
    // scrollbarGutter
    , scrollbarWidth(RenderStyle::initialScrollbarWidth())
    , zoom(RenderStyle::initialZoom())
    , pseudoElementNameArgument(nullAtom())
    , blockStepSize(RenderStyle::initialBlockStepSize())
    , blockStepInsert(static_cast<unsigned>(RenderStyle::initialBlockStepInsert()))
    , overscrollBehaviorX(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorX()))
    , overscrollBehaviorY(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorY()))
    , pageSizeType(PAGE_SIZE_AUTO)
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
{
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , containIntrinsicWidth(o.containIntrinsicWidth)
    , containIntrinsicHeight(o.containIntrinsicHeight)
    , perspectiveOriginX(o.perspectiveOriginX)
    , perspectiveOriginY(o.perspectiveOriginY)
    , lineClamp(o.lineClamp)
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
    , scrollbarGutter(o.scrollbarGutter)
    , scrollbarWidth(o.scrollbarWidth)
    , zoom(o.zoom)
    , pseudoElementNameArgument(o.pseudoElementNameArgument)
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
        && initialLetter == o.initialLetter
        && marquee == o.marquee
        && backdropFilter == o.backdropFilter
        && grid == o.grid
        && gridItem == o.gridItem
        && clip == o.clip
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && counterDirectives.map == o.counterDirectives.map
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
        && scrollbarGutter == o.scrollbarGutter
        && scrollbarWidth == o.scrollbarWidth
        && zoom == o.zoom
        && pseudoElementNameArgument == o.pseudoElementNameArgument
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
        && viewTransitionName == o.viewTransitionName
        && hasClip == o.hasClip;
}

OptionSet<Containment> StyleRareNonInheritedData::effectiveContainment() const
{
    auto containment = contain;

    switch (static_cast<ContainerType>(containerType)) {
    case ContainerType::Normal:
        break;
    case ContainerType::Size:
        containment.add({ Containment::Layout, Containment::Style, Containment::Size });
        break;
    case ContainerType::InlineSize:
        containment.add({ Containment::Layout, Containment::Style, Containment::InlineSize });
        break;
    };

    return containment;
}

bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return !backdropFilter->operations.isEmpty();
}

} // namespace WebCore
