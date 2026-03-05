/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InheritedData);

InheritedData::InheritedData()
    : borderHorizontalSpacing(ComputedStyle::initialBorderHorizontalSpacing())
    , borderVerticalSpacing(ComputedStyle::initialBorderVerticalSpacing())
    , lineHeight(ComputedStyle::initialLineHeight())
#if ENABLE(TEXT_AUTOSIZING)
    , specifiedLineHeight(ComputedStyle::initialLineHeight())
#endif
    , fontData(FontData::create())
    , color(ComputedStyle::initialColor())
    , visitedLinkColor(ComputedStyle::initialColor())
{
}

inline InheritedData::InheritedData(const InheritedData& o)
    : RefCounted<InheritedData>()
    , borderHorizontalSpacing(o.borderHorizontalSpacing)
    , borderVerticalSpacing(o.borderVerticalSpacing)
    , lineHeight(o.lineHeight)
#if ENABLE(TEXT_AUTOSIZING)
    , specifiedLineHeight(o.specifiedLineHeight)
#endif
    , fontData(o.fontData)
    , color(o.color)
    , visitedLinkColor(o.visitedLinkColor)
{
    ASSERT(o == *this, "InheritedData should be properly copied.");
}

Ref<InheritedData> InheritedData::copy() const
{
    return adoptRef(*new InheritedData(*this));
}

InheritedData::~InheritedData() = default;

bool InheritedData::operator==(const InheritedData& other) const
{
    return fastPathInheritedEqual(other) && nonFastPathInheritedEqual(other);
}

bool InheritedData::fastPathInheritedEqual(const InheritedData& other) const
{
    // These properties also need to have "fast-path-inherited" codegen property set.
    // Cases where other properties depend on these values need to disallow the fast path (via RenderStyle::setDisallowsFastPathInheritance).
    return color == other.color
        && visitedLinkColor == other.visitedLinkColor;
}

bool InheritedData::nonFastPathInheritedEqual(const InheritedData& other) const
{
    return lineHeight == other.lineHeight
#if ENABLE(TEXT_AUTOSIZING)
        && specifiedLineHeight == other.specifiedLineHeight
#endif
        && fontData == other.fontData
        && borderHorizontalSpacing == other.borderHorizontalSpacing
        && borderVerticalSpacing == other.borderVerticalSpacing;
}

void InheritedData::fastPathInheritFrom(const InheritedData& inheritParent)
{
    color = inheritParent.color;
    visitedLinkColor = inheritParent.visitedLinkColor;
}

#if !LOG_DISABLED
void InheritedData::dumpDifferences(TextStream& ts, const InheritedData& other) const
{
    fontData->dumpDifferences(ts, *other.fontData);

    LOG_IF_DIFFERENT(borderHorizontalSpacing);
    LOG_IF_DIFFERENT(borderVerticalSpacing);
    LOG_IF_DIFFERENT(lineHeight);

#if ENABLE(TEXT_AUTOSIZING)
    LOG_IF_DIFFERENT(specifiedLineHeight);
#endif

    LOG_IF_DIFFERENT(color);
    LOG_IF_DIFFERENT(visitedLinkColor);
}
#endif

} // namespace Style
} // namespace WebCore
