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

#include "CSSStyleSelector.h"
#include "ContentData.h"
#include "RenderCounter.h"
#include "RenderStyle.h"
#include "ShadowData.h"
#include "StyleFilterData.h"
#include "StyleTransformData.h"
#include "StyleImage.h"

namespace WebCore {

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : lineClamp(RenderStyle::initialLineClamp())
    , opacity(RenderStyle::initialOpacity())
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginBeforeCollapse(MCOLLAPSE)
    , marginAfterCollapse(MCOLLAPSE)
    , matchNearestMailBlockquoteColor(RenderStyle::initialMatchNearestMailBlockquoteColor())
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
    , m_textCombine(RenderStyle::initialTextCombine())
    , m_counterIncrement(0)
    , m_counterReset(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_runningAcceleratedAnimation(false)
#endif
    , m_mask(FillLayer(MaskFillLayer))
    , m_transformStyle3D(RenderStyle::initialTransformStyle3D())
    , m_backfaceVisibility(RenderStyle::initialBackfaceVisibility())
    , m_perspective(RenderStyle::initialPerspective())
    , m_perspectiveOriginX(RenderStyle::initialPerspectiveOriginX())
    , m_perspectiveOriginY(RenderStyle::initialPerspectiveOriginY())
    , m_pageSize()
    , m_pageSizeType(PAGE_SIZE_AUTO)
    , m_flowThread(RenderStyle::initialFlowThread())
    , m_regionThread(RenderStyle::initialRegionThread())
    , m_regionIndex(RenderStyle::initialRegionIndex())
    , m_regionOverflow(RenderStyle::initialRegionOverflow())
    , m_wrapShape(RenderStyle::initialWrapShape())
    , m_regionBreakAfter(RenderStyle::initialPageBreak())
    , m_regionBreakBefore(RenderStyle::initialPageBreak())
    , m_regionBreakInside(RenderStyle::initialPageBreak())
{
    m_maskBoxImage.setMaskDefaults();
}

StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , lineClamp(o.lineClamp)
    , opacity(o.opacity)
    , m_deprecatedFlexibleBox(o.m_deprecatedFlexibleBox)
#if ENABLE(CSS3_FLEXBOX)
    , m_flexibleBox(o.m_flexibleBox)
#endif
    , m_marquee(o.m_marquee)
    , m_multiCol(o.m_multiCol)
    , m_transform(o.m_transform)
#if ENABLE(CSS_FILTERS)
    , m_filter(o.m_filter)
#endif
    , m_content(o.m_content ? o.m_content->clone() : nullptr)
    , m_counterDirectives(o.m_counterDirectives ? clone(*o.m_counterDirectives) : nullptr)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginBeforeCollapse(o.marginBeforeCollapse)
    , marginAfterCollapse(o.marginAfterCollapse)
    , matchNearestMailBlockquoteColor(o.matchNearestMailBlockquoteColor)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
    , m_textCombine(o.m_textCombine)
    , m_counterIncrement(o.m_counterIncrement)
    , m_counterReset(o.m_counterReset)
#if USE(ACCELERATED_COMPOSITING)
    , m_runningAcceleratedAnimation(o.m_runningAcceleratedAnimation)
#endif
    , m_boxShadow(o.m_boxShadow ? adoptPtr(new ShadowData(*o.m_boxShadow)) : nullptr)
    , m_boxReflect(o.m_boxReflect)
    , m_animations(o.m_animations ? adoptPtr(new AnimationList(*o.m_animations)) : nullptr)
    , m_transitions(o.m_transitions ? adoptPtr(new AnimationList(*o.m_transitions)) : nullptr)
    , m_mask(o.m_mask)
    , m_maskBoxImage(o.m_maskBoxImage)
    , m_transformStyle3D(o.m_transformStyle3D)
    , m_backfaceVisibility(o.m_backfaceVisibility)
    , m_perspective(o.m_perspective)
    , m_perspectiveOriginX(o.m_perspectiveOriginX)
    , m_perspectiveOriginY(o.m_perspectiveOriginY)
    , m_pageSize(o.m_pageSize)
    , m_pageSizeType(o.m_pageSizeType)
    , m_flowThread(o.m_flowThread)
    , m_regionThread(o.m_regionThread)
    , m_regionIndex(o.m_regionIndex)
    , m_regionOverflow(o.m_regionOverflow)
    , m_wrapShape(o.m_wrapShape)
    , m_regionBreakAfter(o.m_regionBreakAfter)
    , m_regionBreakBefore(o.m_regionBreakBefore)
    , m_regionBreakInside(o.m_regionBreakInside)
{
}

StyleRareNonInheritedData::~StyleRareNonInheritedData()
{
}

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return lineClamp == o.lineClamp
#if ENABLE(DASHBOARD_SUPPORT)
        && m_dashboardRegions == o.m_dashboardRegions
#endif
        && opacity == o.opacity
        && m_deprecatedFlexibleBox == o.m_deprecatedFlexibleBox
#if ENABLE(CSS3_FLEXBOX)
        && m_flexibleBox == o.m_flexibleBox
#endif
        && m_marquee == o.m_marquee
        && m_multiCol == o.m_multiCol
        && m_transform == o.m_transform
#if ENABLE(CSS_FILTERS)
        && m_filter == o.m_filter
#endif
        && contentDataEquivalent(o)
        && counterDataEquivalent(o)
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginBeforeCollapse == o.marginBeforeCollapse
        && marginAfterCollapse == o.marginAfterCollapse
        && matchNearestMailBlockquoteColor == o.matchNearestMailBlockquoteColor
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
        && m_textCombine == o.m_textCombine
        && m_counterIncrement == o.m_counterIncrement
        && m_counterReset == o.m_counterReset
#if USE(ACCELERATED_COMPOSITING)
        && !m_runningAcceleratedAnimation && !o.m_runningAcceleratedAnimation
#endif
        && shadowDataEquivalent(o)
        && reflectionDataEquivalent(o)
        && animationDataEquivalent(o)
        && transitionDataEquivalent(o)
        && m_mask == o.m_mask
        && m_maskBoxImage == o.m_maskBoxImage
        && (m_transformStyle3D == o.m_transformStyle3D)
        && (m_backfaceVisibility == o.m_backfaceVisibility)
        && (m_perspective == o.m_perspective)
        && (m_perspectiveOriginX == o.m_perspectiveOriginX)
        && (m_perspectiveOriginY == o.m_perspectiveOriginY)
        && (m_pageSize == o.m_pageSize)
        && (m_pageSizeType == o.m_pageSizeType)
        && (m_flowThread == o.m_flowThread)
        && (m_regionThread == o.m_regionThread)
        && (m_regionIndex == o.m_regionIndex)
        && (m_regionOverflow == o.m_regionOverflow)
        && (m_wrapShape == o.m_wrapShape)
        && (m_regionBreakAfter == o.m_regionBreakAfter)
        && (m_regionBreakBefore == o.m_regionBreakBefore)
        && (m_regionBreakInside == o.m_regionBreakInside);
}

bool StyleRareNonInheritedData::contentDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (m_content.get() == o.m_content.get())
        return true;
        
    if (m_content && o.m_content && *m_content == *o.m_content)
        return true;

    return false;
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

} // namespace WebCore
