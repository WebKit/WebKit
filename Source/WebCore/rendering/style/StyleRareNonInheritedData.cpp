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

namespace WebCore {

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : opacity(RenderStyle::initialOpacity())
    , m_aspectRatioDenominator(RenderStyle::initialAspectRatioDenominator())
    , m_aspectRatioNumerator(RenderStyle::initialAspectRatioNumerator())
    , m_perspective(RenderStyle::initialPerspective())
    , m_perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , m_perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
    , lineClamp(RenderStyle::initialLineClamp())
    , m_deprecatedFlexibleBox(StyleDeprecatedFlexibleBoxData::create())
    , m_flexibleBox(StyleFlexibleBoxData::create())
    , m_marquee(StyleMarqueeData::create())
    , m_multiCol(StyleMultiColData::create())
    , m_transform(StyleTransformData::create())
#if ENABLE(CSS_FILTERS)
    , m_filter(StyleFilterData::create())
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    , m_grid(StyleGridData::create())
    , m_gridItem(StyleGridItemData::create())
#endif
    , m_mask(FillLayer(MaskFillLayer))
    , m_pageSize()
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    , m_shapeInside(RenderStyle::initialShapeInside())
#endif
#if ENABLE(CSS_SHAPES)
    , m_shapeOutside(RenderStyle::initialShapeOutside())
    , m_shapeMargin(RenderStyle::initialShapeMargin())
    , m_shapePadding(RenderStyle::initialShapePadding())
    , m_shapeImageThreshold(RenderStyle::initialShapeImageThreshold())
#endif
    , m_clipPath(RenderStyle::initialClipPath())
    , m_visitedLinkBackgroundColor(RenderStyle::initialBackgroundColor())
    , m_order(RenderStyle::initialOrder())
    , m_flowThread(RenderStyle::initialFlowThread())
    , m_regionThread(RenderStyle::initialRegionThread())
    , m_regionFragment(RenderStyle::initialRegionFragment())
    , m_regionBreakAfter(RenderStyle::initialPageBreak())
    , m_regionBreakBefore(RenderStyle::initialPageBreak())
    , m_regionBreakInside(RenderStyle::initialPageBreak())
    , m_pageSizeType(PAGE_SIZE_AUTO)
    , m_transformStyle3D(RenderStyle::initialTransformStyle3D())
    , m_backfaceVisibility(RenderStyle::initialBackfaceVisibility())
    , m_alignContent(RenderStyle::initialAlignContent())
    , m_alignItems(RenderStyle::initialAlignItems())
    , m_alignSelf(RenderStyle::initialAlignSelf())
    , m_justifyContent(RenderStyle::initialJustifyContent())
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginBeforeCollapse(MCOLLAPSE)
    , marginAfterCollapse(MCOLLAPSE)
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
    , m_textCombine(RenderStyle::initialTextCombine())
    , m_textDecorationStyle(RenderStyle::initialTextDecorationStyle())
    , m_wrapFlow(RenderStyle::initialWrapFlow())
    , m_wrapThrough(RenderStyle::initialWrapThrough())
    , m_runningAcceleratedAnimation(false)
    , m_aspectRatioType(RenderStyle::initialAspectRatioType())
#if ENABLE(CSS_COMPOSITING)
    , m_effectiveBlendMode(RenderStyle::initialBlendMode())
    , m_isolation(RenderStyle::initialIsolation())
#endif
    , m_objectFit(RenderStyle::initialObjectFit())
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
    , m_deprecatedFlexibleBox(o.m_deprecatedFlexibleBox)
    , m_flexibleBox(o.m_flexibleBox)
    , m_marquee(o.m_marquee)
    , m_multiCol(o.m_multiCol)
    , m_transform(o.m_transform)
#if ENABLE(CSS_FILTERS)
    , m_filter(o.m_filter)
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    , m_grid(o.m_grid)
    , m_gridItem(o.m_gridItem)
#endif
    , m_content(o.m_content ? o.m_content->clone() : nullptr)
    , m_counterDirectives(o.m_counterDirectives ? clone(*o.m_counterDirectives) : nullptr)
    , m_boxShadow(o.m_boxShadow ? std::make_unique<ShadowData>(*o.m_boxShadow) : nullptr)
    , m_boxReflect(o.m_boxReflect)
    , m_animations(o.m_animations ? adoptPtr(new AnimationList(*o.m_animations)) : nullptr)
    , m_transitions(o.m_transitions ? adoptPtr(new AnimationList(*o.m_transitions)) : nullptr)
    , m_mask(o.m_mask)
    , m_maskBoxImage(o.m_maskBoxImage)
    , m_pageSize(o.m_pageSize)
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    , m_shapeInside(o.m_shapeInside)
#endif
#if ENABLE(CSS_SHAPES)
    , m_shapeOutside(o.m_shapeOutside)
    , m_shapeMargin(o.m_shapeMargin)
    , m_shapePadding(o.m_shapePadding)
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
    , m_regionFragment(o.m_regionFragment)
    , m_regionBreakAfter(o.m_regionBreakAfter)
    , m_regionBreakBefore(o.m_regionBreakBefore)
    , m_regionBreakInside(o.m_regionBreakInside)
    , m_pageSizeType(o.m_pageSizeType)
    , m_transformStyle3D(o.m_transformStyle3D)
    , m_backfaceVisibility(o.m_backfaceVisibility)
    , m_alignContent(o.m_alignContent)
    , m_alignItems(o.m_alignItems)
    , m_alignSelf(o.m_alignSelf)
    , m_justifyContent(o.m_justifyContent)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginBeforeCollapse(o.marginBeforeCollapse)
    , marginAfterCollapse(o.marginAfterCollapse)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
    , m_textCombine(o.m_textCombine)
    , m_textDecorationStyle(o.m_textDecorationStyle)
    , m_wrapFlow(o.m_wrapFlow)
    , m_wrapThrough(o.m_wrapThrough)
    , m_runningAcceleratedAnimation(o.m_runningAcceleratedAnimation)
    , m_aspectRatioType(o.m_aspectRatioType)
#if ENABLE(CSS_COMPOSITING)
    , m_effectiveBlendMode(o.m_effectiveBlendMode)
    , m_isolation(o.m_isolation)
#endif
    , m_objectFit(o.m_objectFit)
{
}

PassRef<StyleRareNonInheritedData> StyleRareNonInheritedData::copy() const
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
#if ENABLE(DASHBOARD_SUPPORT)
        && m_dashboardRegions == o.m_dashboardRegions
#endif
        && m_deprecatedFlexibleBox == o.m_deprecatedFlexibleBox
        && m_flexibleBox == o.m_flexibleBox
        && m_marquee == o.m_marquee
        && m_multiCol == o.m_multiCol
        && m_transform == o.m_transform
#if ENABLE(CSS_FILTERS)
        && m_filter == o.m_filter
#endif
#if ENABLE(CSS_GRID_LAYOUT)
        && m_grid == o.m_grid
        && m_gridItem == o.m_gridItem
#endif
        && contentDataEquivalent(o)
        && counterDataEquivalent(o)
        && shadowDataEquivalent(o)
        && reflectionDataEquivalent(o)
        && animationDataEquivalent(o)
        && transitionDataEquivalent(o)
        && m_mask == o.m_mask
        && m_maskBoxImage == o.m_maskBoxImage
        && m_pageSize == o.m_pageSize
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    && m_shapeInside == o.m_shapeInside
#endif
#if ENABLE(CSS_SHAPES)
        && m_shapeOutside == o.m_shapeOutside
        && m_shapeMargin == o.m_shapeMargin
        && m_shapePadding == o.m_shapePadding
        && m_shapeImageThreshold == o.m_shapeImageThreshold
#endif
        && m_clipPath == o.m_clipPath
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
        && m_regionThread == o.m_regionThread
        && m_regionFragment == o.m_regionFragment
        && m_regionBreakAfter == o.m_regionBreakAfter
        && m_regionBreakBefore == o.m_regionBreakBefore
        && m_regionBreakInside == o.m_regionBreakInside
        && m_pageSizeType == o.m_pageSizeType
        && m_transformStyle3D == o.m_transformStyle3D
        && m_backfaceVisibility == o.m_backfaceVisibility
        && m_alignContent == o.m_alignContent
        && m_alignItems == o.m_alignItems
        && m_alignSelf == o.m_alignSelf
        && m_justifyContent == o.m_justifyContent
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginBeforeCollapse == o.marginBeforeCollapse
        && marginAfterCollapse == o.marginAfterCollapse
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
        && m_textCombine == o.m_textCombine
        && m_textDecorationStyle == o.m_textDecorationStyle
        && m_wrapFlow == o.m_wrapFlow
        && m_wrapThrough == o.m_wrapThrough
        && !m_runningAcceleratedAnimation && !o.m_runningAcceleratedAnimation
#if ENABLE(CSS_COMPOSITING)
        && m_effectiveBlendMode == o.m_effectiveBlendMode
        && m_isolation == o.m_isolation
#endif
        && m_aspectRatioType == o.m_aspectRatioType
        && m_objectFit == o.m_objectFit;
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

bool StyleRareNonInheritedData::counterDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (m_counterDirectives.get() == o.m_counterDirectives.get())
        return true;
        
    if (m_counterDirectives && o.m_counterDirectives && *m_counterDirectives == *o.m_counterDirectives)
        return true;

    return false;
}

bool StyleRareNonInheritedData::shadowDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if ((!m_boxShadow && o.m_boxShadow) || (m_boxShadow && !o.m_boxShadow))
        return false;
    if (m_boxShadow && o.m_boxShadow && (*m_boxShadow != *o.m_boxShadow))
        return false;
    return true;
}

bool StyleRareNonInheritedData::reflectionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (m_boxReflect != o.m_boxReflect) {
        if (!m_boxReflect || !o.m_boxReflect)
            return false;
        return *m_boxReflect == *o.m_boxReflect;
    }
    return true;
}

bool StyleRareNonInheritedData::animationDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if ((!m_animations && o.m_animations) || (m_animations && !o.m_animations))
        return false;
    if (m_animations && o.m_animations && (*m_animations != *o.m_animations))
        return false;
    return true;
}

bool StyleRareNonInheritedData::transitionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if ((!m_transitions && o.m_transitions) || (m_transitions && !o.m_transitions))
        return false;
    if (m_transitions && o.m_transitions && (*m_transitions != *o.m_transitions))
        return false;
    return true;
}

bool StyleRareNonInheritedData::hasFilters() const
{
#if ENABLE(CSS_FILTERS)
    return m_filter.get() && !m_filter->m_operations.isEmpty();
#else
    return false;
#endif
}

} // namespace WebCore
