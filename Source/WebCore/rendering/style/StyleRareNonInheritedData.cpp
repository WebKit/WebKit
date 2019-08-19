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
#include "StyleScrollSnapPoints.h"
#include <wtf/PointerComparison.h>
#include <wtf/RefPtr.h>

namespace WebCore {

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : opacity(RenderStyle::initialOpacity())
    , aspectRatioDenominator(RenderStyle::initialAspectRatioDenominator())
    , aspectRatioNumerator(RenderStyle::initialAspectRatioNumerator())
    , perspective(RenderStyle::initialPerspective())
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
#if ENABLE(CSS_SCROLL_SNAP)
    , scrollSnapPort(StyleScrollSnapPort::create())
    , scrollSnapArea(StyleScrollSnapArea::create())
#endif
    , willChange(RenderStyle::initialWillChange())
    , mask(FillLayerType::Mask)
    , objectPosition(RenderStyle::initialObjectPosition())
    , shapeOutside(RenderStyle::initialShapeOutside())
    , shapeMargin(RenderStyle::initialShapeMargin())
    , shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
    , clipPath(RenderStyle::initialClipPath())
    , visitedLinkBackgroundColor(RenderStyle::initialBackgroundColor())
    , order(RenderStyle::initialOrder())
    , alignContent(RenderStyle::initialContentAlignment())
    , alignItems(RenderStyle::initialDefaultAlignment())
    , alignSelf(RenderStyle::initialSelfAlignment())
    , justifyContent(RenderStyle::initialContentAlignment())
    , justifyItems(RenderStyle::initialJustifyItems())
    , justifySelf(RenderStyle::initialSelfAlignment())
    , customProperties(StyleCustomPropertyData::create())
#if ENABLE(POINTER_EVENTS)
    , touchActions(static_cast<unsigned>(RenderStyle::initialTouchActions()))
#endif
    , pageSizeType(PAGE_SIZE_AUTO)
    , transformStyle3D(static_cast<unsigned>(RenderStyle::initialTransformStyle3D()))
    , backfaceVisibility(static_cast<unsigned>(RenderStyle::initialBackfaceVisibility()))
    , userDrag(static_cast<unsigned>(RenderStyle::initialUserDrag()))
    , textOverflow(static_cast<unsigned>(RenderStyle::initialTextOverflow()))
    , marginBeforeCollapse(static_cast<unsigned>(MarginCollapse::Collapse))
    , marginAfterCollapse(static_cast<unsigned>(MarginCollapse::Collapse))
    , appearance(static_cast<unsigned>(RenderStyle::initialAppearance()))
    , borderFit(static_cast<unsigned>(RenderStyle::initialBorderFit()))
    , textCombine(static_cast<unsigned>(RenderStyle::initialTextCombine()))
    , textDecorationStyle(static_cast<unsigned>(RenderStyle::initialTextDecorationStyle()))
    , aspectRatioType(static_cast<unsigned>(RenderStyle::initialAspectRatioType()))
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
    , hasAttrContent(false)
    , isNotFinal(false)
    , columnGap(RenderStyle::initialColumnGap())
    , rowGap(RenderStyle::initialRowGap())
{
    maskBoxImage.setMaskDefaults();
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , opacity(o.opacity)
    , aspectRatioDenominator(o.aspectRatioDenominator)
    , aspectRatioNumerator(o.aspectRatioNumerator)
    , perspective(o.perspective)
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
#if ENABLE(CSS_SCROLL_SNAP)
    , scrollSnapPort(o.scrollSnapPort)
    , scrollSnapArea(o.scrollSnapArea)
#endif
    , content(o.content ? o.content->clone() : nullptr)
    , counterDirectives(o.counterDirectives ? makeUnique<CounterDirectiveMap>(*o.counterDirectives) : nullptr)
    , altText(o.altText)
    , boxShadow(o.boxShadow ? makeUnique<ShadowData>(*o.boxShadow) : nullptr)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
    , animations(o.animations ? makeUnique<AnimationList>(*o.animations) : nullptr)
    , transitions(o.transitions ? makeUnique<AnimationList>(*o.transitions) : nullptr)
    , mask(o.mask)
    , maskBoxImage(o.maskBoxImage)
    , pageSize(o.pageSize)
    , objectPosition(o.objectPosition)
    , shapeOutside(o.shapeOutside)
    , shapeMargin(o.shapeMargin)
    , shapeImageThreshold(o.shapeImageThreshold)
    , clipPath(o.clipPath)
    , textDecorationColor(o.textDecorationColor)
    , visitedLinkTextDecorationColor(o.visitedLinkTextDecorationColor)
    , visitedLinkBackgroundColor(o.visitedLinkBackgroundColor)
    , visitedLinkOutlineColor(o.visitedLinkOutlineColor)
    , visitedLinkBorderLeftColor(o.visitedLinkBorderLeftColor)
    , visitedLinkBorderRightColor(o.visitedLinkBorderRightColor)
    , visitedLinkBorderTopColor(o.visitedLinkBorderTopColor)
    , visitedLinkBorderBottomColor(o.visitedLinkBorderBottomColor)
    , order(o.order)
    , alignContent(o.alignContent)
    , alignItems(o.alignItems)
    , alignSelf(o.alignSelf)
    , justifyContent(o.justifyContent)
    , justifyItems(o.justifyItems)
    , justifySelf(o.justifySelf)
    , customProperties(o.customProperties)
    , customPaintWatchedProperties(o.customPaintWatchedProperties ? makeUnique<HashSet<String>>(*o.customPaintWatchedProperties) : nullptr)
#if ENABLE(POINTER_EVENTS)
    , touchActions(o.touchActions)
#endif
    , pageSizeType(o.pageSizeType)
    , transformStyle3D(o.transformStyle3D)
    , backfaceVisibility(o.backfaceVisibility)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginBeforeCollapse(o.marginBeforeCollapse)
    , marginAfterCollapse(o.marginAfterCollapse)
    , appearance(o.appearance)
    , borderFit(o.borderFit)
    , textCombine(o.textCombine)
    , textDecorationStyle(o.textDecorationStyle)
    , aspectRatioType(o.aspectRatioType)
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
    , hasAttrContent(o.hasAttrContent)
    , isNotFinal(o.isNotFinal)
    , columnGap(o.columnGap)
    , rowGap(o.rowGap)
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
        && aspectRatioDenominator == o.aspectRatioDenominator
        && aspectRatioNumerator == o.aspectRatioNumerator
        && perspective == o.perspective
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
#if ENABLE(CSS_SCROLL_SNAP)
        && scrollSnapPort == o.scrollSnapPort
        && scrollSnapArea == o.scrollSnapArea
#endif
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
        && arePointingToEqualData(clipPath, o.clipPath)
        && textDecorationColor == o.textDecorationColor
        && visitedLinkTextDecorationColor == o.visitedLinkTextDecorationColor
        && visitedLinkBackgroundColor == o.visitedLinkBackgroundColor
        && visitedLinkOutlineColor == o.visitedLinkOutlineColor
        && visitedLinkBorderLeftColor == o.visitedLinkBorderLeftColor
        && visitedLinkBorderRightColor == o.visitedLinkBorderRightColor
        && visitedLinkBorderTopColor == o.visitedLinkBorderTopColor
        && visitedLinkBorderBottomColor == o.visitedLinkBorderBottomColor
        && order == o.order
        && alignContent == o.alignContent
        && alignItems == o.alignItems
        && alignSelf == o.alignSelf
        && justifyContent == o.justifyContent
        && justifyItems == o.justifyItems
        && justifySelf == o.justifySelf
        && customProperties == o.customProperties
        && ((customPaintWatchedProperties && o.customPaintWatchedProperties && *customPaintWatchedProperties == *o.customPaintWatchedProperties)
            || (!customPaintWatchedProperties && !o.customPaintWatchedProperties))
        && pageSizeType == o.pageSizeType
        && transformStyle3D == o.transformStyle3D
        && backfaceVisibility == o.backfaceVisibility
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginBeforeCollapse == o.marginBeforeCollapse
        && marginAfterCollapse == o.marginAfterCollapse
        && appearance == o.appearance
        && borderFit == o.borderFit
        && textCombine == o.textCombine
        && textDecorationStyle == o.textDecorationStyle
#if ENABLE(POINTER_EVENTS)
        && touchActions == o.touchActions
#endif
#if ENABLE(CSS_COMPOSITING)
        && effectiveBlendMode == o.effectiveBlendMode
        && isolation == o.isolation
#endif
#if ENABLE(APPLE_PAY)
        && applePayButtonStyle == o.applePayButtonStyle
        && applePayButtonType == o.applePayButtonType
#endif
        && aspectRatioType == o.aspectRatioType
        && objectFit == o.objectFit
        && breakAfter == o.breakAfter
        && breakBefore == o.breakBefore
        && breakInside == o.breakInside
        && resize == o.resize
        && hasAttrContent == o.hasAttrContent
        && isNotFinal == o.isNotFinal
        && columnGap == o.columnGap
        && rowGap == o.rowGap;
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

#if ENABLE(FILTERS_LEVEL_2)

bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return !backdropFilter->operations.isEmpty();
}

#endif

} // namespace WebCore
