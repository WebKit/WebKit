/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutRect.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore
{
// RenderOverflow is a class for tracking content that spills out of a box.
// This class is used by RenderBox and LegacyInlineFlowBox.
//
// There are three types of overflow:
// * layout overflow (which is expected to be reachable via scrolling mechanisms)
// * visual overflow (which is not expected to be reachable via scrolling mechanisms)
// * content overflow (non-recursive overflow of the in-flow content edge, a subset of layout overflow)
//
// Layout overflow examples include other boxes that spill out of our box (recursively).
// For example, in the inline case a tall image could spill out of a line box.
//
// Examples of visual overflow are shadows, text stroke, outline, border-image.
//
// Examples of content overflow are a grid larger than its container's content box,
// line boxes that extend past a block's explicit height, etc. This content area
// is the rectangle that gets aligned by content alignment, that gets wrapped by
// padding when calculating a scroll container's scrollable area, and that defines
// the "scrollable containing block" for absolutely positioned boxes.

// This object is allocated only when some of these fields have non-default values in the owning box.
class RenderOverflow : public CanMakeCheckedPtr<RenderOverflow> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RenderOverflow);
    WTF_MAKE_NONCOPYABLE(RenderOverflow);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderOverflow);
public:
    RenderOverflow(const LayoutRect& layoutRect, const LayoutRect& visualRect, const LayoutRect& contentRect)
        : m_contentArea(contentRect)
        , m_layoutOverflow(layoutRect)
        , m_visualOverflow(visualRect)
    {
    }
   
    const LayoutRect layoutOverflowRect() const { return m_layoutOverflow; }
    const LayoutRect visualOverflowRect() const { return m_visualOverflow; }
    const LayoutRect contentArea() const { return m_contentArea; }

    void move(LayoutUnit dx, LayoutUnit dy);
    
    void addLayoutOverflow(const LayoutRect&);
    void addVisualOverflow(const LayoutRect&);
    void addContentOverflow(const LayoutRect& rect) { m_contentArea.uniteEvenIfEmpty(rect); }
    void addContentOverflowX(const LayoutRect& rect) { m_contentArea.uniteXEvenIfEmpty(rect); }
    void addContentOverflowY(const LayoutRect& rect) { m_contentArea.uniteYEvenIfEmpty(rect); }

    void setLayoutOverflow(const LayoutRect& rect) { m_layoutOverflow = rect; }
    void setVisualOverflow(const LayoutRect& rect) { m_visualOverflow = rect; }
    void setContentArea(const LayoutRect& rect) { m_contentArea = rect; }

private:
    LayoutRect m_contentArea;
    LayoutRect m_layoutOverflow;
    LayoutRect m_visualOverflow;
};

inline void RenderOverflow::move(LayoutUnit dx, LayoutUnit dy)
{
    m_contentArea.move(dx, dy);
    m_layoutOverflow.move(dx, dy);
    m_visualOverflow.move(dx, dy);
}

inline void RenderOverflow::addLayoutOverflow(const LayoutRect& rect)
{
    LayoutUnit maxX = std::max(rect.maxX(), m_layoutOverflow.maxX());
    LayoutUnit maxY = std::max(rect.maxY(), m_layoutOverflow.maxY());
    LayoutUnit minX = std::min(rect.x(), m_layoutOverflow.x());
    LayoutUnit minY = std::min(rect.y(), m_layoutOverflow.y());
    // In case the width/height is larger than LayoutUnit can represent, fix the right/bottom edge and shift the top/left ones
    m_layoutOverflow.setWidth(maxX - minX);
    m_layoutOverflow.setHeight(maxY - minY);
    m_layoutOverflow.setX(maxX - m_layoutOverflow.width());
    m_layoutOverflow.setY(maxY - m_layoutOverflow.height());
}

inline void RenderOverflow::addVisualOverflow(const LayoutRect& rect)
{
    LayoutUnit maxX = std::max(rect.maxX(), m_visualOverflow.maxX());
    LayoutUnit maxY = std::max(rect.maxY(), m_visualOverflow.maxY());
    m_visualOverflow.setX(std::min(rect.x(), m_visualOverflow.x()));
    m_visualOverflow.setY(std::min(rect.y(), m_visualOverflow.y()));
    m_visualOverflow.setWidth(maxX - m_visualOverflow.x());
    m_visualOverflow.setHeight(maxY - m_visualOverflow.y());
}

} // namespace WebCore
