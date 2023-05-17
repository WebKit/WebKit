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

#include "LegacyInlineFlowBox.h"
#include "RenderBoxModelObjectInlines.h"

namespace WebCore {

inline LayoutUnit LegacyInlineFlowBox::marginBorderPaddingLogicalLeft() const { return LayoutUnit(marginLogicalLeft() + borderLogicalLeft() + paddingLogicalLeft()); }
inline LayoutUnit LegacyInlineFlowBox::marginBorderPaddingLogicalRight() const { return LayoutUnit(marginLogicalRight() + borderLogicalRight() + paddingLogicalRight()); }

inline float LegacyInlineFlowBox::borderLogicalLeft() const
{
    if (!includeLogicalLeftEdge())
        return 0;
    return isHorizontal() ? lineStyle().borderLeftWidth() : lineStyle().borderTopWidth();
}

inline float LegacyInlineFlowBox::borderLogicalRight() const
{
    if (!includeLogicalRightEdge())
        return 0;
    return isHorizontal() ? lineStyle().borderRightWidth() : lineStyle().borderBottomWidth();
}

inline float LegacyInlineFlowBox::paddingLogicalLeft() const
{
    if (!includeLogicalLeftEdge())
        return 0;
    return isHorizontal() ? renderer().paddingLeft() : renderer().paddingTop();
}

inline float LegacyInlineFlowBox::paddingLogicalRight() const
{
    if (!includeLogicalRightEdge())
        return 0;
    return isHorizontal() ? renderer().paddingRight() : renderer().paddingBottom();
}

} // namespace WebCore
