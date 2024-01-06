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
 *
 */

#pragma once

#include "RenderBoxModelObject.h"
#include "RenderStyleInlines.h"

namespace WebCore {

inline LayoutUnit RenderBoxModelObject::borderAfter() const { return LayoutUnit(style().borderAfterWidth()); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingAfter() const { return borderAfter() + paddingAfter(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingBefore() const { return borderBefore() + paddingBefore(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingLogicalHeight() const { return borderAndPaddingBefore() + borderAndPaddingAfter(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingLogicalWidth() const { return borderStart() + borderEnd() + paddingStart() + paddingEnd(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingLogicalLeft() const { return style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingLogicalRight() const { return style().isHorizontalWritingMode() ? borderRight() + paddingRight() : borderBottom() + paddingBottom(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingStart() const { return borderStart() + paddingStart(); }
inline LayoutUnit RenderBoxModelObject::borderAndPaddingEnd() const { return borderEnd() + paddingEnd(); }
inline LayoutUnit RenderBoxModelObject::borderBefore() const { return LayoutUnit(style().borderBeforeWidth()); }
inline LayoutUnit RenderBoxModelObject::borderBottom() const { return LayoutUnit(style().borderBottomWidth()); }
inline LayoutUnit RenderBoxModelObject::borderEnd() const { return LayoutUnit(style().borderEndWidth()); }
inline LayoutUnit RenderBoxModelObject::borderLeft() const { return LayoutUnit(style().borderLeftWidth()); }
inline LayoutUnit RenderBoxModelObject::borderLogicalHeight() const { return borderBefore() + borderAfter(); }
inline LayoutUnit RenderBoxModelObject::borderLogicalLeft() const { return style().isHorizontalWritingMode() ? borderLeft() : borderTop(); }
inline LayoutUnit RenderBoxModelObject::borderLogicalRight() const { return style().isHorizontalWritingMode() ? borderRight() : borderBottom(); }
inline LayoutUnit RenderBoxModelObject::borderLogicalWidth() const { return borderStart() + borderEnd(); }
inline LayoutUnit RenderBoxModelObject::borderRight() const { return LayoutUnit(style().borderRightWidth()); }
inline LayoutUnit RenderBoxModelObject::borderStart() const { return LayoutUnit(style().borderStartWidth()); }
inline LayoutUnit RenderBoxModelObject::borderTop() const { return LayoutUnit(style().borderTopWidth()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingAfter() const { return computedCSSPadding(style().paddingAfter()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingBefore() const { return computedCSSPadding(style().paddingBefore()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingBottom() const { return computedCSSPadding(style().paddingBottom()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingEnd() const { return computedCSSPadding(style().paddingEnd()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingLeft() const { return computedCSSPadding(style().paddingLeft()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingRight() const { return computedCSSPadding(style().paddingRight()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingStart() const { return computedCSSPadding(style().paddingStart()); }
inline LayoutUnit RenderBoxModelObject::computedCSSPaddingTop() const { return computedCSSPadding(style().paddingTop()); }
inline bool RenderBoxModelObject::hasInlineDirectionBordersOrPadding() const { return borderStart() || borderEnd() || paddingStart() || paddingEnd(); }
inline bool RenderBoxModelObject::hasInlineDirectionBordersPaddingOrMargin() const { return hasInlineDirectionBordersOrPadding() || marginStart() || marginEnd(); }
inline LayoutUnit RenderBoxModelObject::horizontalBorderAndPaddingExtent() const { return borderLeft() + borderRight() + paddingLeft() + paddingRight(); }
inline LayoutUnit RenderBoxModelObject::horizontalBorderExtent() const { return borderLeft() + borderRight(); }
inline LayoutUnit RenderBoxModelObject::marginAndBorderAndPaddingAfter() const { return marginAfter() + borderAfter() + paddingAfter(); }
inline LayoutUnit RenderBoxModelObject::marginAndBorderAndPaddingBefore() const { return marginBefore() + borderBefore() + paddingBefore(); }
inline LayoutUnit RenderBoxModelObject::marginAndBorderAndPaddingEnd() const { return marginEnd() + borderEnd() + paddingEnd(); }
inline LayoutUnit RenderBoxModelObject::marginAndBorderAndPaddingStart() const { return marginStart() + borderStart() + paddingStart(); }
inline LayoutUnit RenderBoxModelObject::paddingAfter() const { return computedCSSPaddingAfter(); }
inline LayoutUnit RenderBoxModelObject::paddingBefore() const { return computedCSSPaddingBefore(); }
inline LayoutUnit RenderBoxModelObject::paddingBottom() const { return computedCSSPaddingBottom(); }
inline LayoutUnit RenderBoxModelObject::paddingEnd() const { return computedCSSPaddingEnd(); }
inline LayoutUnit RenderBoxModelObject::paddingLeft() const { return computedCSSPaddingLeft(); }
inline LayoutUnit RenderBoxModelObject::paddingLogicalHeight() const { return paddingBefore() + paddingAfter(); }
inline LayoutUnit RenderBoxModelObject::paddingLogicalLeft() const { return style().isHorizontalWritingMode() ? paddingLeft() : paddingTop(); }
inline LayoutUnit RenderBoxModelObject::paddingLogicalRight() const { return style().isHorizontalWritingMode() ? paddingRight() : paddingBottom(); }
inline LayoutUnit RenderBoxModelObject::paddingLogicalWidth() const { return paddingStart() + paddingEnd(); }
inline LayoutUnit RenderBoxModelObject::paddingRight() const { return computedCSSPaddingRight(); }
inline LayoutUnit RenderBoxModelObject::paddingStart() const { return computedCSSPaddingStart(); }
inline LayoutUnit RenderBoxModelObject::paddingTop() const { return computedCSSPaddingTop(); }
inline LayoutSize RenderBoxModelObject::relativePositionLogicalOffset() const { return style().isHorizontalWritingMode() ? relativePositionOffset() : relativePositionOffset().transposedSize(); }
inline LayoutSize RenderBoxModelObject::stickyPositionLogicalOffset() const { return style().isHorizontalWritingMode() ? stickyPositionOffset() : stickyPositionOffset().transposedSize(); }
inline LayoutUnit RenderBoxModelObject::verticalBorderAndPaddingExtent() const { return borderTop() + borderBottom() + paddingTop() + paddingBottom(); }
inline LayoutUnit RenderBoxModelObject::verticalBorderExtent() const { return borderTop() + borderBottom(); }

inline LayoutUnit RenderBoxModelObject::computedCSSPadding(const Length& padding) const
{
    LayoutUnit containerWidth;
    if (padding.isPercentOrCalculated())
        containerWidth = containingBlockLogicalWidthForContent();
    return minimumValueForLength(padding, containerWidth);
}

} // namespace WebCore
