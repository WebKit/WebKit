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
#include "StyleSurroundData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SurroundData);

SurroundData::SurroundData()
    : hasExplicitlySetBorderBottomLeftRadius(false)
    , hasExplicitlySetBorderBottomRightRadius(false)
    , hasExplicitlySetBorderTopLeftRadius(false)
    , hasExplicitlySetBorderTopRightRadius(false)
    , hasExplicitlySetPaddingBottom(false)
    , hasExplicitlySetPaddingLeft(false)
    , hasExplicitlySetPaddingRight(false)
    , hasExplicitlySetPaddingTop(false)
    , inset(CSS::Keyword::Auto { })
    , margin(0_css_px)
    , padding(0_css_px)
{
}

inline SurroundData::SurroundData(const SurroundData& o)
    : RefCounted<SurroundData>()
    , hasExplicitlySetBorderBottomLeftRadius(o.hasExplicitlySetBorderBottomLeftRadius)
    , hasExplicitlySetBorderBottomRightRadius(o.hasExplicitlySetBorderBottomRightRadius)
    , hasExplicitlySetBorderTopLeftRadius(o.hasExplicitlySetBorderTopLeftRadius)
    , hasExplicitlySetBorderTopRightRadius(o.hasExplicitlySetBorderTopRightRadius)
    , hasExplicitlySetPaddingBottom(o.hasExplicitlySetPaddingBottom)
    , hasExplicitlySetPaddingLeft(o.hasExplicitlySetPaddingLeft)
    , hasExplicitlySetPaddingRight(o.hasExplicitlySetPaddingRight)
    , hasExplicitlySetPaddingTop(o.hasExplicitlySetPaddingTop)
    , inset(o.inset)
    , margin(o.margin)
    , padding(o.padding)
    , border(o.border)
{
}

Ref<SurroundData> SurroundData::copy() const
{
    return adoptRef(*new SurroundData(*this));
}

bool SurroundData::operator==(const SurroundData& o) const
{
    return inset == o.inset
        && margin == o.margin
        && padding == o.padding
        && border == o.border
        && hasExplicitlySetBorderBottomLeftRadius == o.hasExplicitlySetBorderBottomLeftRadius
        && hasExplicitlySetBorderBottomRightRadius == o.hasExplicitlySetBorderBottomRightRadius
        && hasExplicitlySetBorderTopLeftRadius == o.hasExplicitlySetBorderTopLeftRadius
        && hasExplicitlySetBorderTopRightRadius == o.hasExplicitlySetBorderTopRightRadius
        && hasExplicitlySetPaddingBottom == o.hasExplicitlySetPaddingBottom
        && hasExplicitlySetPaddingLeft == o.hasExplicitlySetPaddingLeft
        && hasExplicitlySetPaddingRight == o.hasExplicitlySetPaddingRight
        && hasExplicitlySetPaddingTop == o.hasExplicitlySetPaddingTop;
}

#if !LOG_DISABLED
void SurroundData::dumpDifferences(TextStream& ts, const SurroundData& other) const
{
    LOG_IF_DIFFERENT(hasExplicitlySetBorderBottomLeftRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderBottomRightRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderTopLeftRadius);
    LOG_IF_DIFFERENT(hasExplicitlySetBorderTopRightRadius);

    LOG_IF_DIFFERENT(hasExplicitlySetPaddingBottom);
    LOG_IF_DIFFERENT(hasExplicitlySetPaddingLeft);
    LOG_IF_DIFFERENT(hasExplicitlySetPaddingRight);
    LOG_IF_DIFFERENT(hasExplicitlySetPaddingTop);

    LOG_IF_DIFFERENT(inset);
    LOG_IF_DIFFERENT(margin);
    LOG_IF_DIFFERENT(padding);
    border.dumpDifferences(ts, other.border);
}
#endif

} // namespace Style
} // namespace WebCore
