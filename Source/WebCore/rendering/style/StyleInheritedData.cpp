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
#include "StyleInheritedData.h"

#include "RenderStyleInlines.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleInheritedData);

StyleInheritedData::StyleInheritedData()
    : horizontalBorderSpacing(RenderStyle::initialHorizontalBorderSpacing())
    , verticalBorderSpacing(RenderStyle::initialVerticalBorderSpacing())
    , lineHeight(RenderStyle::initialLineHeight())
#if ENABLE(TEXT_AUTOSIZING)
    , specifiedLineHeight(RenderStyle::initialLineHeight())
#endif
    , color(RenderStyle::initialColor())
    , visitedLinkColor(RenderStyle::initialColor())
{
}

inline StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>()
    , horizontalBorderSpacing(o.horizontalBorderSpacing)
    , verticalBorderSpacing(o.verticalBorderSpacing)
    , lineHeight(o.lineHeight)
#if ENABLE(TEXT_AUTOSIZING)
    , specifiedLineHeight(o.specifiedLineHeight)
#endif
    , fontCascade(o.fontCascade)
    , color(o.color)
    , visitedLinkColor(o.visitedLinkColor)
{
}

Ref<StyleInheritedData> StyleInheritedData::copy() const
{
    return adoptRef(*new StyleInheritedData(*this));
}

bool StyleInheritedData::operator==(const StyleInheritedData& other) const
{
    return fastPathInheritedEqual(other) && nonFastPathInheritedEqual(other);
}

bool StyleInheritedData::fastPathInheritedEqual(const StyleInheritedData& other) const
{
    // These properties also need to have "fast-path-inherited" codegen property set.
    // Cases where other properties depend on these values need to disallow the fast path (via RenderStyle::setDisallowsFastPathInheritance).
    return color == other.color
        && visitedLinkColor == other.visitedLinkColor;
}

bool StyleInheritedData::nonFastPathInheritedEqual(const StyleInheritedData& other) const
{
    return lineHeight == other.lineHeight
#if ENABLE(TEXT_AUTOSIZING)
        && specifiedLineHeight == other.specifiedLineHeight
#endif
        && fontCascade == other.fontCascade
        && horizontalBorderSpacing == other.horizontalBorderSpacing
        && verticalBorderSpacing == other.verticalBorderSpacing;
}

void StyleInheritedData::fastPathInheritFrom(const StyleInheritedData& inheritParent)
{
    color = inheritParent.color;
    visitedLinkColor = inheritParent.visitedLinkColor;
}

} // namespace WebCore
