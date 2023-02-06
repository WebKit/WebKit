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

#include "ContentData.h"
#include "RenderCounter.h"
#include "RenderStyle.h"
#include "ShadowData.h"
#include "StyleCustomPropertyData.h"
#include "StyleFilterData.h"
#include "StyleTransformData.h"
#include "StyleImage.h"
#include "StyleResolver.h"
#include <wtf/PointerComparison.h>
#include <wtf/RefPtr.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : opacity(RenderStyle::initialOpacity())
    , aspectRatioWidth(RenderStyle::initialAspectRatioWidth())
    , aspectRatioHeight(RenderStyle::initialAspectRatioHeight())
    , containIntrinsicWidth(RenderStyle::initialContainIntrinsicWidth())
    , containIntrinsicHeight(RenderStyle::initialContainIntrinsicHeight())
    , perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
    , lineClamp(RenderStyle::initialLineClamp())
    , initialLetter(RenderStyle::initialInitialLetter())
    , deprecatedFlexibleBox(StyleDeprecatedFlexibleBoxData::create())
    , flexibleBox(StyleFlexibleBoxData::create())
    , marquee(StyleMarqueeData::create())
    , multiCol(StyleMultiColData::create())
    , transform(StyleTransformData::create())
    , filter(StyleFilterData::create())
#if ENABLE(FILTERS_LEVEL_2)
    , backdropFilter(StyleFilterData::create())
#endif
    , grid(StyleGridData::create())
    , gridItem(StyleGridItemData::create())
    // scrollMargin
    // scrollPadding
    // content
    // counterDirectives
    // altText
    // boxShadow
    , willChange(RenderStyle::initialWillChange())
    // boxReflect
    // animations
    // transitions
    , mask(FillLayer::create(FillLayerType::Mask))
    , maskBoxImage(NinePieceImage::Type::Mask)
    // pageSize
    , objectPosition(RenderStyle::initialObjectPosition())
    , shapeOutside(RenderStyle::initialShapeOutside())
    , shapeMargin(RenderStyle::initialShapeMargin())
    , shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
    , perspective(RenderStyle::initialPerspective())
    , order(RenderStyle::initialOrder())
    , clipPath(RenderStyle::initialClipPath())
    , textDecorationColor(RenderStyle::initialTextDecorationColor())
    // visitedLinkTextDecorationColor
    , visitedLinkBackgroundColor(RenderStyle::initialBackgroundColor())
    // visitedLinkOutlineColor;
    // visitedLinkBorderLeftColor
    // visitedLinkBorderRightColor
    // visitedLinkBorderTopColor
    // visitedLinkBorderBottomColor
    , alignContent(RenderStyle::initialContentAlignment())
    , justifyContent(RenderStyle::initialContentAlignment())
    , alignItems(RenderStyle::initialDefaultAlignment())
    , alignSelf(RenderStyle::initialSelfAlignment())
    , justifyItems(RenderStyle::initialJustifyItems())
    , justifySelf(RenderStyle::initialSelfAlignment())
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
    , overscrollBehaviorX(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorX()))
    , overscrollBehaviorY(static_cast<unsigned>(RenderStyle::initialOverscrollBehaviorY()))
    , pageSizeType(PAGE_SIZE_AUTO)
    , transformStyle3D(static_cast<unsigned>(RenderStyle::initialTransformStyle3D()))
    , transformStyleForcedToFlat(false)
    , backfaceVisibility(static_cast<unsigned>(RenderStyle::initialBackfaceVisibility()))
    , userDrag(static_cast<unsigned>(RenderStyle::initialUserDrag()))
    , textOverflow(static_cast<unsigned>(RenderStyle::initialTextOverflow()))
    , useSmoothScrolling(static_cast<unsigned>(RenderStyle::initialUseSmoothScrolling()))
    , appearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , effectiveAppearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , textDecorationStyle(static_cast<unsigned>(RenderStyle::initialTextDecorationStyle()))
    , textGroupAlign(static_cast<unsigned>(RenderStyle::initialTextGroupAlign()))
    , aspectRatioType(static_cast<unsigned>(RenderStyle::initialAspectRatioType()))
    , contentVisibility(static_cast<unsigned>(RenderStyle::initialContentVisibility()))
#if ENABLE(CSS_COMPOSITING)
    , effectiveBlendMode(static_cast<unsigned>(RenderStyle::initialBlendMode()))
    , isolation(static_cast<unsigned>(RenderStyle::initialIsolation()))
#endif
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(static_cast<unsigned>(RenderStyle::initialApplePayButtonStyle()))
    , applePayButtonType(static_cast<unsigned>(RenderStyle::initialApplePayButtonType()))
#endif
    , objectFit(static_cast<unsigned>(RenderStyle::initialObjectFit()))
    , breakBefore(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakAfter(static_cast<unsigned>(RenderStyle::initialBreakBetween()))
    , breakInside(static_cast<unsigned>(RenderStyle::initialBreakInside()))
    , resize(static_cast<unsigned>(RenderStyle::initialResize()))
    , inputSecurity(static_cast<unsigned>(RenderStyle::initialInputSecurity()))
    , hasAttrContent(false)
    , isNotFinal(false)
    , containIntrinsicWidthType(static_cast<unsigned>(RenderStyle::initialContainIntrinsicWidthType()))
    , containIntrinsicHeightType(static_cast<unsigned>(RenderStyle::initialContainIntrinsicHeightType()))
    , containerType(static_cast<unsigned>(RenderStyle::initialContainerType()))
    , leadingTrim(static_cast<unsigned>(RenderStyle::initialLeadingTrim()))
    , overflowAnchor(static_cast<unsigned>(RenderStyle::initialOverflowAnchor()))
{
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , opacity(o.opacity)
    , aspectRatioWidth(o.aspectRatioWidth)
    , aspectRatioHeight(o.aspectRatioHeight)
    , containIntrinsicWidth(o.containIntrinsicWidth)
    , containIntrinsicHeight(o.containIntrinsicHeight)
    , perspectiveOriginX(o.perspectiveOriginX)
    , perspectiveOriginY(o.perspectiveOriginY)
    , lineClamp(o.lineClamp)
    , initialLetter(o.initialLetter)
    , deprecatedFlexibleBox(o.deprecatedFlexibleBox)
    , flexibleBox(o.flexibleBox)
    , marquee(o.marquee)
    , multiCol(o.multiCol)
    , transform(o.transform)
    , filter(o.filter)
#if ENABLE(FILTERS_LEVEL_2)
    , backdropFilter(o.backdropFilter)
#endif
    , grid(o.grid)
    , gridItem(o.gridItem)
    , scrollMargin(o.scrollMargin)
    , scrollPadding(o.scrollPadding)
    , content(o.content ? o.content->clone() : nullptr)
    , counterDirectives(o.counterDirectives ? makeUnique<CounterDirectiveMap>(*o.counterDirectives) : nullptr)
    , altText(o.altText)
    , boxShadow(o.boxShadow ? makeUnique<ShadowData>(*o.boxShadow) : nullptr)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
    , animations(o.animations ? o.animations->copy() : o.animations)
    , transitions(o.transitions ? o.transitions->copy() : o.transitions)
    , mask(o.mask)
    , maskBoxImage(o.maskBoxImage)
    , pageSize(o.pageSize)
    , objectPosition(o.objectPosition)
    , shapeOutside(o.shapeOutside)
    , shapeMargin(o.shapeMargin)
    , shapeImageThreshold(o.shapeImageThreshold)
    , perspective(o.perspective)
    , order(o.order)
    , clipPath(o.clipPath)
    , textDecorationColor(o.textDecorationColor)
    , visitedLinkTextDecorationColor(o.visitedLinkTextDecorationColor)
    , visitedLinkBackgroundColor(o.visitedLinkBackgroundColor)
    , visitedLinkOutlineColor(o.visitedLinkOutlineColor)
    , visitedLinkBorderLeftColor(o.visitedLinkBorderLeftColor)
    , visitedLinkBorderRightColor(o.visitedLinkBorderRightColor)
    , visitedLinkBorderTopColor(o.visitedLinkBorderTopColor)
    , visitedLinkBorderBottomColor(o.visitedLinkBorderBottomColor)
    , alignContent(o.alignContent)
    , justifyContent(o.justifyContent)
    , alignItems(o.alignItems)
    , alignSelf(o.alignSelf)
    , justifyItems(o.justifyItems)
    , justifySelf(o.justifySelf)
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
    , overscrollBehaviorX(o.overscrollBehaviorX)
    , overscrollBehaviorY(o.overscrollBehaviorY)
    , pageSizeType(o.pageSizeType)
    , transformStyle3D(o.transformStyle3D)
    , transformStyleForcedToFlat(o.transformStyleForcedToFlat)
    , backfaceVisibility(o.backfaceVisibility)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , useSmoothScrolling(o.useSmoothScrolling)
    , appearance(o.appearance)
    , effectiveAppearance(o.effectiveAppearance)
    , textDecorationStyle(o.textDecorationStyle)
    , textGroupAlign(o.textGroupAlign)
    , aspectRatioType(o.aspectRatioType)
    , contentVisibility(o.contentVisibility)
#if ENABLE(CSS_COMPOSITING)
    , effectiveBlendMode(o.effectiveBlendMode)
    , isolation(o.isolation)
#endif
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(o.applePayButtonStyle)
    , applePayButtonType(o.applePayButtonType)
#endif
    , objectFit(o.objectFit)
    , breakBefore(o.breakBefore)
    , breakAfter(o.breakAfter)
    , breakInside(o.breakInside)
    , resize(o.resize)
    , inputSecurity(o.inputSecurity)
    , hasAttrContent(o.hasAttrContent)
    , isNotFinal(o.isNotFinal)
    , containIntrinsicWidthType(o.containIntrinsicWidthType)
    , containIntrinsicHeightType(o.containIntrinsicHeightType)
    , containerType(o.containerType)
    , leadingTrim(o.leadingTrim)
    , overflowAnchor(o.overflowAnchor)
{
}

Ref<StyleRareNonInheritedData> StyleRareNonInheritedData::copy() const
{
    return adoptRef(*new StyleRareNonInheritedData(*this));
}

StyleRareNonInheritedData::~StyleRareNonInheritedData() = default;

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return opacity == o.opacity
        && aspectRatioWidth == o.aspectRatioWidth
        && aspectRatioHeight == o.aspectRatioHeight
        && containIntrinsicWidth == o.containIntrinsicWidth
        && containIntrinsicHeight == o.containIntrinsicHeight
        && perspectiveOriginX == o.perspectiveOriginX
        && perspectiveOriginY == o.perspectiveOriginY
        && lineClamp == o.lineClamp
        && initialLetter == o.initialLetter
        && deprecatedFlexibleBox == o.deprecatedFlexibleBox
        && flexibleBox == o.flexibleBox
        && marquee == o.marquee
        && multiCol == o.multiCol
        && transform == o.transform
        && filter == o.filter
#if ENABLE(FILTERS_LEVEL_2)
        && backdropFilter == o.backdropFilter
#endif
        && grid == o.grid
        && gridItem == o.gridItem
        && scrollMargin == o.scrollMargin
        && scrollPadding == o.scrollPadding
        && contentDataEquivalent(o)
        && arePointingToEqualData(counterDirectives, o.counterDirectives)
        && altText == o.altText
        && arePointingToEqualData(boxShadow, o.boxShadow)
        && arePointingToEqualData(willChange, o.willChange)
        && arePointingToEqualData(boxReflect, o.boxReflect)
        && arePointingToEqualData(animations, o.animations)
        && arePointingToEqualData(transitions, o.transitions)
        && mask == o.mask
        && maskBoxImage == o.maskBoxImage
        && pageSize == o.pageSize
        && objectPosition == o.objectPosition
        && arePointingToEqualData(shapeOutside, o.shapeOutside)
        && shapeMargin == o.shapeMargin
        && shapeImageThreshold == o.shapeImageThreshold
        && perspective == o.perspective
        && order == o.order
        && arePointingToEqualData(clipPath, o.clipPath)
        && textDecorationColor == o.textDecorationColor
        && visitedLinkTextDecorationColor == o.visitedLinkTextDecorationColor
        && visitedLinkBackgroundColor == o.visitedLinkBackgroundColor
        && visitedLinkOutlineColor == o.visitedLinkOutlineColor
        && visitedLinkBorderLeftColor == o.visitedLinkBorderLeftColor
        && visitedLinkBorderRightColor == o.visitedLinkBorderRightColor
        && visitedLinkBorderTopColor == o.visitedLinkBorderTopColor
        && visitedLinkBorderBottomColor == o.visitedLinkBorderBottomColor
        && alignContent == o.alignContent
        && justifyContent == o.justifyContent
        && alignItems == o.alignItems
        && alignSelf == o.alignSelf
        && justifyItems == o.justifyItems
        && justifySelf == o.justifySelf
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
        && overscrollBehaviorX == o.overscrollBehaviorX
        && overscrollBehaviorY == o.overscrollBehaviorY
        && pageSizeType == o.pageSizeType
        && transformStyle3D == o.transformStyle3D
        && transformStyleForcedToFlat == o.transformStyleForcedToFlat
        && backfaceVisibility == o.backfaceVisibility
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && useSmoothScrolling == o.useSmoothScrolling
        && appearance == o.appearance
        && effectiveAppearance == o.effectiveAppearance
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
        && aspectRatioType == o.aspectRatioType
        && contentVisibility == o.contentVisibility
        && objectFit == o.objectFit
        && breakAfter == o.breakAfter
        && breakBefore == o.breakBefore
        && breakInside == o.breakInside
        && resize == o.resize
        && inputSecurity == o.inputSecurity
        && hasAttrContent == o.hasAttrContent
        && isNotFinal == o.isNotFinal
        && containIntrinsicWidthType == o.containIntrinsicWidthType
        && containIntrinsicHeightType == o.containIntrinsicHeightType
        && containerType == o.containerType
        && leadingTrim == o.leadingTrim
        && overflowAnchor == o.overflowAnchor;
}

bool StyleRareNonInheritedData::contentDataEquivalent(const StyleRareNonInheritedData& other) const
{
    auto* a = content.get();
    auto* b = other.content.get();
    while (a && b && *a == *b) {
        a = a->next();
        b = b->next();
    }
    return !a && !b;
}

bool StyleRareNonInheritedData::hasFilters() const
{
    return !filter->operations.isEmpty();
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
