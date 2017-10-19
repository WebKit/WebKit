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
    , mask(FillLayer(MaskFillLayer))
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
    , justifyItems(RenderStyle::initialSelfAlignment())
    , justifySelf(RenderStyle::initialSelfAlignment())
#if ENABLE(TOUCH_EVENTS)
    , touchAction(static_cast<unsigned>(RenderStyle::initialTouchAction()))
#endif
    , pageSizeType(PAGE_SIZE_AUTO)
    , transformStyle3D(RenderStyle::initialTransformStyle3D())
    , backfaceVisibility(RenderStyle::initialBackfaceVisibility())
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginBeforeCollapse(MCOLLAPSE)
    , marginAfterCollapse(MCOLLAPSE)
    , appearance(RenderStyle::initialAppearance())
    , borderFit(RenderStyle::initialBorderFit())
    , textCombine(RenderStyle::initialTextCombine())
    , textDecorationStyle(RenderStyle::initialTextDecorationStyle())
    , aspectRatioType(RenderStyle::initialAspectRatioType())
#if ENABLE(CSS_COMPOSITING)
    , effectiveBlendMode(RenderStyle::initialBlendMode())
    , isolation(RenderStyle::initialIsolation())
#endif
#if ENABLE(APPLE_PAY)
    , applePayButtonStyle(static_cast<unsigned>(RenderStyle::initialApplePayButtonStyle()))
    , applePayButtonType(static_cast<unsigned>(RenderStyle::initialApplePayButtonType()))
#endif
    , objectFit(RenderStyle::initialObjectFit())
    , breakBefore(RenderStyle::initialBreakBetween())
    , breakAfter(RenderStyle::initialBreakBetween())
    , breakInside(RenderStyle::initialBreakInside())
    , resize(RenderStyle::initialResize())
    , hasAttrContent(false)
    , isNotFinal(false)
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
    , counterDirectives(o.counterDirectives ? clone(*o.counterDirectives) : nullptr)
    , altText(o.altText)
    , boxShadow(o.boxShadow ? std::make_unique<ShadowData>(*o.boxShadow) : nullptr)
    , willChange(o.willChange)
    , boxReflect(o.boxReflect)
    , animations(o.animations ? std::make_unique<AnimationList>(*o.animations) : nullptr)
    , transitions(o.transitions ? std::make_unique<AnimationList>(*o.transitions) : nullptr)
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
#if ENABLE(TOUCH_EVENTS)
    , touchAction(o.touchAction)
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
#if ENABLE(DASHBOARD_SUPPORT)
        && dashboardRegions == o.dashboardRegions
#endif
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
#if ENABLE(TOUCH_EVENTS)
        && touchAction == o.touchAction
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
        && isNotFinal == o.isNotFinal;
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
