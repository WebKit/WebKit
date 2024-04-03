/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderBox.h"
#include "RenderBoxModelObjectInlines.h"

namespace WebCore {

inline LayoutUnit RenderBox::availableHeight() const { return style().isHorizontalWritingMode() ? availableLogicalHeight(IncludeMarginBorderPadding) : availableLogicalWidth(); }
inline LayoutUnit RenderBox::availableLogicalWidth() const { return contentLogicalWidth(); }
inline LayoutUnit RenderBox::availableWidth() const { return style().isHorizontalWritingMode() ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding); }
inline LayoutSize RenderBox::borderBoxLogicalSize() const { return logicalSize(); }
inline LayoutRect RenderBox::clientBoxRect() const { return LayoutRect(clientLeft(), clientTop(), clientWidth(), clientHeight()); }
inline LayoutUnit RenderBox::clientLeft() const { return borderLeft(); }
inline LayoutUnit RenderBox::clientLogicalBottom() const { return borderBefore() + clientLogicalHeight(); }
inline LayoutUnit RenderBox::clientLogicalHeight() const { return style().isHorizontalWritingMode() ? clientHeight() : clientWidth(); }
inline LayoutUnit RenderBox::clientLogicalWidth() const { return style().isHorizontalWritingMode() ? clientWidth() : clientHeight(); }
inline LayoutUnit RenderBox::clientTop() const { return borderTop(); }
inline LayoutRect RenderBox::computedCSSContentBoxRect() const { return LayoutRect(borderLeft() + computedCSSPaddingLeft(), borderTop() + computedCSSPaddingTop(), paddingBoxWidth() - computedCSSPaddingLeft() - computedCSSPaddingRight()  - (style().scrollbarGutter().bothEdges ? verticalScrollbarWidth() : 0), paddingBoxHeight() - computedCSSPaddingTop() - computedCSSPaddingBottom() - (style().scrollbarGutter().bothEdges ? horizontalScrollbarHeight() : 0)); }
inline LayoutRect RenderBox::contentBoxRect() const { return { contentBoxLocation(), contentSize() }; }
inline LayoutUnit RenderBox::contentHeight() const { return std::max(0_lu, paddingBoxHeight() - paddingTop() - paddingBottom() - (style().scrollbarGutter().bothEdges ? horizontalScrollbarHeight() : 0)); }
inline LayoutUnit RenderBox::contentLogicalHeight() const { return style().isHorizontalWritingMode() ? contentHeight() : contentWidth(); }
inline LayoutSize RenderBox::contentLogicalSize() const { return style().isHorizontalWritingMode() ? contentSize() : contentSize().transposedSize(); }
inline LayoutUnit RenderBox::contentLogicalWidth() const { return style().isHorizontalWritingMode() ? contentWidth() : contentHeight(); }
inline LayoutSize RenderBox::contentSize() const { return { contentWidth(), contentHeight() }; }
inline LayoutUnit RenderBox::contentWidth() const { return std::max(0_lu, paddingBoxWidth() - paddingLeft() - paddingRight() - (style().scrollbarGutter().bothEdges ? verticalScrollbarWidth() : 0)); }
inline std::optional<LayoutUnit> RenderBox::explicitIntrinsicInnerLogicalHeight() const { return style().isHorizontalWritingMode() ? explicitIntrinsicInnerHeight() : explicitIntrinsicInnerWidth(); }
inline std::optional<LayoutUnit> RenderBox::explicitIntrinsicInnerLogicalWidth() const { return style().isHorizontalWritingMode() ? explicitIntrinsicInnerWidth() : explicitIntrinsicInnerHeight(); }
inline bool RenderBox::hasHorizontalOverflow() const { return scrollWidth() != roundToInt(paddingBoxWidth()); }
inline bool RenderBox::hasScrollableOverflowX() const { return scrollsOverflowX() && hasHorizontalOverflow(); }
inline bool RenderBox::hasScrollableOverflowY() const { return scrollsOverflowY() && hasVerticalOverflow(); }
inline bool RenderBox::hasVerticalOverflow() const { return scrollHeight() != roundToInt(paddingBoxHeight()); }
inline LayoutUnit RenderBox::intrinsicLogicalHeight() const { return style().isHorizontalWritingMode() ? intrinsicSize().height() : intrinsicSize().width(); }
inline LayoutUnit RenderBox::logicalBottom() const { return logicalTop() + logicalHeight(); }
inline LayoutUnit RenderBox::logicalHeight() const { return style().isHorizontalWritingMode() ? height() : width(); }
inline LayoutUnit RenderBox::logicalLeft() const { return style().isHorizontalWritingMode() ? x() : y(); }
inline LayoutUnit RenderBox::logicalLeftLayoutOverflow() const { return style().isHorizontalWritingMode() ? layoutOverflowRect().x() : layoutOverflowRect().y(); }
inline LayoutUnit RenderBox::logicalLeftVisualOverflow() const { return style().isHorizontalWritingMode() ? visualOverflowRect().x() : visualOverflowRect().y(); }
inline LayoutUnit RenderBox::logicalRight() const { return logicalLeft() + logicalWidth(); }
inline LayoutUnit RenderBox::logicalRightLayoutOverflow() const { return style().isHorizontalWritingMode() ? layoutOverflowRect().maxX() : layoutOverflowRect().maxY(); }
inline LayoutUnit RenderBox::logicalRightVisualOverflow() const { return style().isHorizontalWritingMode() ? visualOverflowRect().maxX() : visualOverflowRect().maxY(); }
inline LayoutSize RenderBox::logicalSize() const { return style().isHorizontalWritingMode() ? m_frameRect.size() : m_frameRect.size().transposedSize(); }
inline LayoutUnit RenderBox::logicalTop() const { return style().isHorizontalWritingMode() ? y() : x(); }
inline LayoutUnit RenderBox::logicalWidth() const { return style().isHorizontalWritingMode() ? width() : height(); }
inline LayoutUnit RenderBox::overridingContentLogicalHeight() const { return std::max(LayoutUnit(), overridingLogicalHeight() - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight() - (style().scrollbarGutter().bothEdges ? scrollbarLogicalHeight() : 0)); }
inline LayoutUnit RenderBox::overridingContentLogicalWidth() const { return std::max(LayoutUnit(), overridingLogicalWidth() - borderAndPaddingLogicalWidth() - scrollbarLogicalWidth() - (style().scrollbarGutter().bothEdges ? scrollbarLogicalWidth() : 0)); }
inline LayoutUnit RenderBox::paddingBoxHeight() const { return std::max(0_lu, height() - borderTop() - borderBottom() - horizontalScrollbarHeight()); }
inline LayoutRect RenderBox::paddingBoxRectIncludingScrollbar() const { return LayoutRect(borderLeft(), borderTop(), width() - borderLeft() - borderRight(), height() - borderTop() - borderBottom()); }
inline LayoutUnit RenderBox::paddingBoxWidth() const { return std::max(0_lu, width() - borderLeft() - borderRight() - verticalScrollbarWidth()); }
inline int RenderBox::scrollbarLogicalHeight() const { return style().isHorizontalWritingMode() ? horizontalScrollbarHeight() : verticalScrollbarWidth(); }
inline int RenderBox::scrollbarLogicalWidth() const { return style().isHorizontalWritingMode() ? verticalScrollbarWidth() : horizontalScrollbarHeight(); }
inline void RenderBox::setLogicalLocation(LayoutPoint location) { setLocation(style().isHorizontalWritingMode() ? location : location.transposedPoint()); }
inline void RenderBox::setLogicalSize(LayoutSize size) { setSize(style().isHorizontalWritingMode() ? size : size.transposedSize()); }
inline bool RenderBox::shouldTrimChildMargin(MarginTrimType type, const RenderBox& child) const { return style().marginTrim().contains(type) && isChildEligibleForMarginTrim(type, child); }
inline bool RenderBox::stretchesToViewport() const { return document().inQuirksMode() && style().logicalHeight().isAuto() && !isFloatingOrOutOfFlowPositioned() && (isDocumentElementRenderer() || isBody()) && !shouldComputeLogicalHeightFromAspectRatio() && !isInline(); }

inline LayoutRect RenderBox::marginBoxRect() const
{
    auto left = computedCSSPadding(style().marginLeft());
    auto right = computedCSSPadding(style().marginRight());
    auto top = computedCSSPadding(style().marginTop());
    auto bottom = computedCSSPadding(style().marginBottom());
    return { -left, -top, size().width() + left + right, size().height() + top + bottom };
}

inline void RenderBox::setLogicalHeight(LayoutUnit size)
{
    if (style().isHorizontalWritingMode())
        setHeight(size);
    else
        setWidth(size);
}

inline void RenderBox::setLogicalLeft(LayoutUnit left)
{
    if (style().isHorizontalWritingMode())
        setX(left);
    else
        setY(left);
}

inline void RenderBox::setLogicalTop(LayoutUnit top)
{
    if (style().isHorizontalWritingMode())
        setY(top);
    else
        setX(top);
}

inline void RenderBox::setLogicalWidth(LayoutUnit size)
{
    if (style().isHorizontalWritingMode())
        setWidth(size);
    else
        setHeight(size);
}

} // namespace WebCore
