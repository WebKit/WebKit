/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
    , m_aspectRatioDenominator(RenderStyle::initialAspectRatioDenominator())
    , m_aspectRatioNumerator(RenderStyle::initialAspectRatioNumerator())
    , m_perspective(RenderStyle::initialPerspective())
    , m_perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , m_perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
    , lineClamp(RenderStyle::initialLineClamp())
    , m_initialLetter(RenderStyle::initialInitialLetter())
    , m_deprecatedFlexibleBox(StyleDeprecatedFlexibleBoxData::create())
    , m_flexibleBox(StyleFlexibleBoxData::create())
    , m_marquee(StyleMarqueeData::create())
    , m_multiCol(StyleMultiColData::create())
    , m_transform(StyleTransformData::create())
    , m_filter(StyleFilterData::create())
#if ENABLE(FILTERS_LEVEL_2)
    , m_backdropFilter(StyleFilterData::create())
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    , m_grid(StyleGridData::create())
    , m_gridItem(StyleGridItemData::create())
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    , m_scrollSnapPoints(StyleScrollSnapPoints::create())
#endif
    , m_willChange(RenderStyle::initialWillChange())
    , m_mask(FillLayer(MaskFillLayer))
    , m_objectPosition(RenderStyle::initialObjectPosition())
#if ENABLE(CSS_SHAPES)
    , m_shapeOutside(RenderStyle::initialShapeOutside())
    , m_shapeMargin(RenderStyle::initialShapeMargin())
    , m_shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
#endif
    , m_clipPath(RenderStyle::initialClipPath())
    , m_visitedLinkBackgroundColor(RenderStyle::initialBackgroundColor())
    , m_order(RenderStyle::initialOrder())
    , m_flowThread(RenderStyle::initialFlowThread())
    , m_regionThread(RenderStyle::initialRegionThread())
    , m_alignContent(RenderStyle::initialContentAlignment())
    , m_alignItems(RenderStyle::initialSelfAlignment())
    , m_alignSelf(RenderStyle::initialSelfAlignment())
    , m_justifyContent(RenderStyle::initialContentAlignment())
    , m_justifyItems(RenderStyle::initialSelfAlignment())
    , m_justifySelf(RenderStyle::initialSelfAlignment())
#if ENABLE(TOUCH_EVENTS)
    , m_touchAction(static_cast<unsigned>(RenderStyle::initialTouchAction()))
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    , m_scrollSnapType(static_cast<unsigned>(RenderStyle::initialScrollSnapType()))
#endif
    , m_regionFragment(RenderStyle::initialRegionFragment())
    , m_pageSizeType(PAGE_SIZE_AUTO)
    , m_transformStyle3D(RenderStyle::initialTransformStyle3D())
    , m_backfaceVisibility(RenderStyle::initialBackfaceVisibility())
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginBeforeCollapse(MCOLLAPSE)
    , marginAfterCollapse(MCOLLAPSE)
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
    , m_textCombine(RenderStyle::initialTextCombine())
    , m_textDecorationStyle(RenderStyle::initialTextDecorationStyle())
    , m_runningAcceleratedAnimation(false)
    , m_aspectRatioType(RenderStyle::initialAspectRatioType())
#if ENABLE(CSS_COMPOSITING)
    , m_effectiveBlendMode(RenderStyle::initialBlendMode())
    , m_isolation(RenderStyle::initialIsolation())
#endif
    , m_objectFit(RenderStyle::initialObjectFit())
    , m_breakBefore(RenderStyle::initialBreakBetween())
    , m_breakAfter(RenderStyle::initialBreakBetween())
    , m_breakInside(RenderStyle::initialBreakInside())
    , m_resize(RenderStyle::initialResize())
{
    m_maskBoxImage.setMaskDefaults();
}

inline StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , opacity(o.opacity)
    , m_aspectRatioDenominator(o.m_aspectRatioDenominator)
    , m_aspectRatioNumerator(o.m_aspectRatioNumerator)
    , m_perspective(o.m_perspective)
    , m_perspectiveOriginX(o.m_perspectiveOriginX)
    , m_perspectiveOriginY(o.m_perspectiveOriginY)
    , lineClamp(o.lineClamp)
    , m_initialLetter(o.m_initialLetter)
    , m_deprecatedFlexibleBox(o.m_deprecatedFlexibleBox)
    , m_flexibleBox(o.m_flexibleBox)
    , m_marquee(o.m_marquee)
    , m_multiCol(o.m_multiCol)
    , m_transform(o.m_transform)
    , m_filter(o.m_filter)
#if ENABLE(FILTERS_LEVEL_2)
    , m_backdropFilter(o.m_backdropFilter)
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    , m_grid(o.m_grid)
    , m_gridItem(o.m_gridItem)
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    , m_scrollSnapPoints(o.m_scrollSnapPoints)
#endif
    , m_content(o.m_content ? o.m_content->clone() : nullptr)
    , m_counterDirectives(o.m_counterDirectives ? clone(*o.m_counterDirectives) : nullptr)
    , m_altText(o.m_altText)
    , m_boxShadow(o.m_boxShadow ? std::make_unique<ShadowData>(*o.m_boxShadow) : nullptr)
    , m_willChange(o.m_willChange)
    , m_boxReflect(o.m_boxReflect)
    , m_animations(o.m_animations ? std::make_unique<AnimationList>(*o.m_animations) : nullptr)
    , m_transitions(o.m_transitions ? std::make_unique<AnimationList>(*o.m_transitions) : nullptr)
    , m_mask(o.m_mask)
    , m_maskBoxImage(o.m_maskBoxImage)
    , m_pageSize(o.m_pageSize)
    , m_objectPosition(o.m_objectPosition)
#if ENABLE(CSS_SHAPES)
    , m_shapeOutside(o.m_shapeOutside)
    , m_shapeMargin(o.m_shapeMargin)
    , m_shapeImageThreshold(o.m_shapeImageThreshold)
#endif
    , m_clipPath(o.m_clipPath)
    , m_textDecorationColor(o.m_textDecorationColor)
    , m_visitedLinkTextDecorationColor(o.m_visitedLinkTextDecorationColor)
    , m_visitedLinkBackgroundColor(o.m_visitedLinkBackgroundColor)
    , m_visitedLinkOutlineColor(o.m_visitedLinkOutlineColor)
    , m_visitedLinkBorderLeftColor(o.m_visitedLinkBorderLeftColor)
    , m_visitedLinkBorderRightColor(o.m_visitedLinkBorderRightColor)
    , m_visitedLinkBorderTopColor(o.m_visitedLinkBorderTopColor)
    , m_visitedLinkBorderBottomColor(o.m_visitedLinkBorderBottomColor)
    , m_order(o.m_order)
    , m_flowThread(o.m_flowThread)
    , m_regionThread(o.m_regionThread)
    , m_alignContent(o.m_alignContent)
    , m_alignItems(o.m_alignItems)
    , m_alignSelf(o.m_alignSelf)
    , m_justifyContent(o.m_justifyContent)
    , m_justifyItems(o.m_justifyItems)
    , m_justifySelf(o.m_justifySelf)
#if ENABLE(TOUCH_EVENTS)
    , m_touchAction(o.m_touchAction)
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    , m_scrollSnapType(o.m_scrollSnapType)
#endif
    , m_regionFragment(o.m_regionFragment)
    , m_pageSizeType(o.m_pageSizeType)
    , m_transformStyle3D(o.m_transformStyle3D)
    , m_backfaceVisibility(o.m_backfaceVisibility)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginBeforeCollapse(o.marginBeforeCollapse)
    , marginAfterCollapse(o.marginAfterCollapse)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
    , m_textCombine(o.m_textCombine)
    , m_textDecorationStyle(o.m_textDecorationStyle)
    , m_runningAcceleratedAnimation(o.m_runningAcceleratedAnimation)
    , m_aspectRatioType(o.m_aspectRatioType)
#if ENABLE(CSS_COMPOSITING)
    , m_effectiveBlendMode(o.m_effectiveBlendMode)
    , m_isolation(o.m_isolation)
#endif
    , m_objectFit(o.m_objectFit)
    , m_breakBefore(o.m_breakBefore)
    , m_breakAfter(o.m_breakAfter)
    , m_breakInside(o.m_breakInside)
    , m_resize(o.m_resize)
{
}

Ref<StyleRareNonInheritedData> StyleRareNonInheritedData::copy() const
{
    return adoptRef(*new StyleRareNonInheritedData(*this));
}

StyleRareNonInheritedData::~StyleRareNonInheritedData()
{
}

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return opacity == o.opacity
        && m_aspectRatioDenominator == o.m_aspectRatioDenominator
        && m_aspectRatioNumerator == o.m_aspectRatioNumerator
        && m_perspective == o.m_perspective
        && m_perspectiveOriginX == o.m_perspectiveOriginX
        && m_perspectiveOriginY == o.m_perspectiveOriginY
        && lineClamp == o.lineClamp
        && m_initialLetter == o.m_initialLetter
#if ENABLE(DASHBOARD_SUPPORT)
        && m_dashboardRegions == o.m_dashboardRegions
#endif
        && m_deprecatedFlexibleBox == o.m_deprecatedFlexibleBox
        && m_flexibleBox == o.m_flexibleBox
        && m_marquee == o.m_marquee
        && m_multiCol == o.m_multiCol
        && m_transform == o.m_transform
        && m_filter == o.m_filter
#if ENABLE(FILTERS_LEVEL_2)
        && m_backdropFilter == o.m_backdropFilter
#endif
#if ENABLE(CSS_GRID_LAYOUT)
        && m_grid == o.m_grid
        && m_gridItem == o.m_gridItem
#endif
#if ENABLE(CSS_SCROLL_SNAP)
        && m_scrollSnapPoints == o.m_scrollSnapPoints
#endif
        && contentDataEquivalent(o)
        && arePointingToEqualData(m_counterDirectives, o.m_counterDirectives)
        && m_altText == o.m_altText
        && arePointingToEqualData(m_boxShadow, o.m_boxShadow)
        && arePointingToEqualData(m_willChange, o.m_willChange)
        && arePointingToEqualData(m_boxReflect, o.m_boxReflect)
        && arePointingToEqualData(m_animations, o.m_animations)
        && arePointingToEqualData(m_transitions, o.m_transitions)
        && m_mask == o.m_mask
        && m_maskBoxImage == o.m_maskBoxImage
        && m_pageSize == o.m_pageSize
        && m_objectPosition == o.m_objectPosition
#if ENABLE(CSS_SHAPES)
        && arePointingToEqualData(m_shapeOutside, o.m_shapeOutside)
        && m_shapeMargin == o.m_shapeMargin
        && m_shapeImageThreshold == o.m_shapeImageThreshold
#endif
        && arePointingToEqualData(m_clipPath, o.m_clipPath)
        && m_textDecorationColor == o.m_textDecorationColor
        && m_visitedLinkTextDecorationColor == o.m_visitedLinkTextDecorationColor
        && m_visitedLinkBackgroundColor == o.m_visitedLinkBackgroundColor
        && m_visitedLinkOutlineColor == o.m_visitedLinkOutlineColor
        && m_visitedLinkBorderLeftColor == o.m_visitedLinkBorderLeftColor
        && m_visitedLinkBorderRightColor == o.m_visitedLinkBorderRightColor
        && m_visitedLinkBorderTopColor == o.m_visitedLinkBorderTopColor
        && m_visitedLinkBorderBottomColor == o.m_visitedLinkBorderBottomColor
        && m_order == o.m_order
        && m_flowThread == o.m_flowThread
        && m_alignContent == o.m_alignContent
        && m_alignItems == o.m_alignItems
        && m_alignSelf == o.m_alignSelf
        && m_justifyContent == o.m_justifyContent
        && m_justifyItems == o.m_justifyItems
        && m_justifySelf == o.m_justifySelf
        && m_regionThread == o.m_regionThread
        && m_regionFragment == o.m_regionFragment
        && m_pageSizeType == o.m_pageSizeType
        && m_transformStyle3D == o.m_transformStyle3D
        && m_backfaceVisibility == o.m_backfaceVisibility
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginBeforeCollapse == o.marginBeforeCollapse
        && marginAfterCollapse == o.marginAfterCollapse
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
        && m_textCombine == o.m_textCombine
        && m_textDecorationStyle == o.m_textDecorationStyle
#if ENABLE(TOUCH_EVENTS)
        && m_touchAction == o.m_touchAction
#endif
#if ENABLE(CSS_SCROLL_SNAP)
        && m_scrollSnapType == o.m_scrollSnapType
#endif
        && !m_runningAcceleratedAnimation && !o.m_runningAcceleratedAnimation
#if ENABLE(CSS_COMPOSITING)
        && m_effectiveBlendMode == o.m_effectiveBlendMode
        && m_isolation == o.m_isolation
#endif
        && m_aspectRatioType == o.m_aspectRatioType
        && m_objectFit == o.m_objectFit
        && m_breakAfter == o.m_breakAfter
        && m_breakBefore == o.m_breakBefore
        && m_breakInside == o.m_breakInside
        && m_resize == o.m_resize;
}

bool StyleRareNonInheritedData::contentDataEquivalent(const StyleRareNonInheritedData& o) const
{
    ContentData* a = m_content.get();
    ContentData* b = o.m_content.get();

    while (a && b && *a == *b) {
        a = a->next();
        b = b->next();
    }

    return !a && !b;
}

bool StyleRareNonInheritedData::hasFilters() const
{
    return m_filter.get() && !m_filter->m_operations.isEmpty();
}

#if ENABLE(FILTERS_LEVEL_2)
bool StyleRareNonInheritedData::hasBackdropFilters() const
{
    return m_backdropFilter.get() && !m_backdropFilter->m_operations.isEmpty();
}
#endif

} // namespace WebCore
