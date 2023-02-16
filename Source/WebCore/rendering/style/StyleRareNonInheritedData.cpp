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
 *
 */

#include "config.h"
#include "StyleRareNonInheritedData.h"

#include "RenderCounter.h"
#include "RenderStyle.h"
#include "ShadowData.h"
#include "StyleCustomPropertyData.h"
#include "StyleFilterData.h"
#include "StyleImage.h"
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
#if ENABLE(FILTERS_LEVEL_2)
    , backdropFilter(StyleFilterData::create())
#endif
    , grid(StyleGridData::create())
    , gridItem(StyleGridItemData::create())
    // clip
    // scrollMargin
    // scrollPadding
    // counterDirectives
    , willChange(RenderStyle::initialWillChange())
    // boxReflect
    , maskBoxImage(NinePieceImage::Type::Mask)
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
    , zoom(RenderStyle::initialZoom())
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
#if ENABLE(CSS_COMPOSITING)
    , effectiveBlendMode(static_cast<unsigned>(RenderStyle::initialBlendMode()))
    , isolation(static_cast<unsigned>(RenderStyle::initialIsolation()))
#endif
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
    , leadingTrim(static_cast<unsigned>(RenderStyle::initialLeadingTrim()))
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
#if ENABLE(FILTERS_LEVEL_2)
    , backdropFilter(o.backdropFilter)
#endif
    , grid(o.grid)
    , gridItem(o.gridItem)
    , clip(o.clip)
    , scrollMargin(o.scrollMargin)
    , scrollPadding(o.scrollPadding)
    , counterDirectives(o.counterDirectives ? makeUnique<CounterDirectiveMap>(*o.counterDirectives) : nullptr)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
    , maskBoxImage(o.maskBoxImage)
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
    , zoom(o.zoom)
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
#if ENABLE(CSS_COMPOSITING)
    , effectiveBlendMode(o.effectiveBlendMode)
    , isolation(o.isolation)
#endif
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
    , leadingTrim(o.leadingTrim)
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
#if ENABLE(FILTERS_LEVEL_2)
        && backdropFilter == o.backdropFilter
#endif
        && grid == o.grid
        && gridItem == o.gridItem
        && clip == o.clip
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && arePointingToEqualData(counterDirectives, o.counterDirectives)
        && arePointingToEqualData(willChange, o.willChange)
        && arePointingToEqualData(boxReflect, o.boxReflect)
        && maskBoxImage == o.maskBoxImage
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
        && zoom == o.zoom
        && overscrollBehaviorX == o.overscrollBehaviorX
        && overscrollBehaviorY == o.overscrollBehaviorY
        && pageSizeType == o.pageSizeType
        && transformStyle3D == o.transformStyle3D
        && transformStyleForcedToFlat == o.transformStyleForcedToFlat
        && backfaceVisibility == o.backfaceVisibility
        && useSmoothScrolling == o.useSmoothScrolling
        && textDecorationStyle == o.textDecorationStyle
        && textGroupAlign == o.textGroupAlign
#if ENABLE(CSS_COMPOSITING)
        && effectiveBlendMode == o.effectiveBlendMode
        && isolation == o.isolation
#endif
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
        && leadingTrim == o.leadingTrim
        && overflowAnchor == o.overflowAnchor
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

#if ENABLE(FILTERS_LEVEL_2)

bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return !backdropFilter->operations.isEmpty();
}

#endif

} // namespace WebCore
