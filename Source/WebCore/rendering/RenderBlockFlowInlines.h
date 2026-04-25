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

#include "CaretRectComputation.h"
#include "FloatingObjects.h"
#include "LegacyLineLayout.h"
#include "LegacyRootInlineBox.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"

namespace WebCore {

inline bool RenderBlockFlow::hasSvgTextLayout() const
{
    return std::holds_alternative<std::unique_ptr<LegacyLineLayout>>(m_lineLayout);
}

inline const LegacyLineLayout* RenderBlockFlow::svgTextLayout() const
{
    return hasSvgTextLayout() ? std::get<std::unique_ptr<LegacyLineLayout>>(m_lineLayout).get() : nullptr;
}

inline LegacyLineLayout* RenderBlockFlow::svgTextLayout()
{
    return hasSvgTextLayout() ? std::get<std::unique_ptr<LegacyLineLayout>>(m_lineLayout).get() : nullptr;
}

inline bool RenderBlockFlow::hasInlineLayout() const
{
    return std::holds_alternative<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout);
}

inline const LayoutIntegration::LineLayout* RenderBlockFlow::inlineLayout() const
{
    return hasInlineLayout() ? std::get<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout).get() : nullptr;
}

inline LayoutIntegration::LineLayout* RenderBlockFlow::inlineLayout()
{
    return hasInlineLayout() ? std::get<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout).get() : nullptr;
}

inline LegacyRootInlineBox* RenderBlockFlow::legacyRootBox() const
{
    return svgTextLayout() ? svgTextLayout()->legacyRootBox() : nullptr;
}

inline bool RenderBlockFlow::hasOverhangingFloats() const { return parent() && containsFloats() && lowestFloatLogicalBottom() > logicalHeight(); }

inline LayoutUnit RenderBlockFlow::endPaddingWidthForCaret() const
{
    RefPtr protectedElement = element();
    if (protectedElement && protectedElement->isRootEditableElement() && hasNonVisibleOverflow() && style().writingMode().deprecatedIsLeftToRightDirection() && !paddingEnd())
        return caretWidth();
    return { };
}

inline const FloatingObjectSet* RenderBlockFlow::floatingObjectSet() const
{
    return m_floatingObjects ? &m_floatingObjects->set() : nullptr;
}

inline LayoutUnit RenderBlockFlow::logicalTopForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.y() : floatingObject.x();
}

inline LayoutUnit RenderBlockFlow::logicalBottomForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.maxY() : floatingObject.maxX();
}

inline LayoutUnit RenderBlockFlow::logicalLeftForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.x() : floatingObject.y();
}

inline LayoutUnit RenderBlockFlow::logicalRightForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.maxX() : floatingObject.maxY();
}

inline LayoutUnit RenderBlockFlow::logicalWidthForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.width() : floatingObject.height();
}

inline LayoutUnit RenderBlockFlow::logicalHeightForFloat(const FloatingObject& floatingObject) const
{
    return isHorizontalWritingMode() ? floatingObject.height() : floatingObject.width();
}

inline void RenderBlockFlow::setLogicalTopForFloat(FloatingObject& floatingObject, LayoutUnit logicalTop)
{
    if (isHorizontalWritingMode())
        floatingObject.setY(logicalTop);
    else
        floatingObject.setX(logicalTop);
}

inline void RenderBlockFlow::setLogicalLeftForFloat(FloatingObject& floatingObject, LayoutUnit logicalLeft)
{
    if (isHorizontalWritingMode())
        floatingObject.setX(logicalLeft);
    else
        floatingObject.setY(logicalLeft);
}

inline void RenderBlockFlow::setLogicalHeightForFloat(FloatingObject& floatingObject, LayoutUnit logicalHeight)
{
    if (isHorizontalWritingMode())
        floatingObject.setHeight(logicalHeight);
    else
        floatingObject.setWidth(logicalHeight);
}

inline void RenderBlockFlow::setLogicalWidthForFloat(FloatingObject& floatingObject, LayoutUnit logicalWidth)
{
    if (isHorizontalWritingMode())
        floatingObject.setWidth(logicalWidth);
    else
        floatingObject.setHeight(logicalWidth);
}

inline void RenderBlockFlow::setLogicalMarginsForFloat(FloatingObject& floatingObject, LayoutUnit logicalLeftMargin, LayoutUnit logicalBeforeMargin)
{
    if (isHorizontalWritingMode())
        floatingObject.setMarginOffset(LayoutSize(logicalLeftMargin, logicalBeforeMargin));
    else
        floatingObject.setMarginOffset(LayoutSize(logicalBeforeMargin, logicalLeftMargin));
}

} // namespace WebCore
