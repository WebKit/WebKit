/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc.
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
#include "config.h"
#include "CSSShadowValue.h"

#include "CSSPrimitiveValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// Used for text-shadow and box-shadow
CSSShadowValue::CSSShadowValue(RefPtr<CSSPrimitiveValue>&& x, RefPtr<CSSPrimitiveValue>&& y, RefPtr<CSSPrimitiveValue>&& blur, RefPtr<CSSPrimitiveValue>&& spread, RefPtr<CSSPrimitiveValue>&& style, RefPtr<CSSPrimitiveValue>&& color)
    : CSSValue(ShadowClass)
    , x(WTFMove(x))
    , y(WTFMove(y))
    , blur(WTFMove(blur))
    , spread(WTFMove(spread))
    , style(WTFMove(style))
    , color(WTFMove(color))
{
}

String CSSShadowValue::customCSSText() const
{
    StringBuilder text;

    if (color)
        text.append(color->cssText());
    if (x) {
        if (!text.isEmpty())
            text.append(' ');
        text.append(x->cssText());
    }
    if (y) {
        if (!text.isEmpty())
            text.append(' ');
        text.append(y->cssText());
    }
    if (blur) {
        if (!text.isEmpty())
            text.append(' ');
        text.append(blur->cssText());
    }
    if (spread) {
        if (!text.isEmpty())
            text.append(' ');
        text.append(spread->cssText());
    }
    if (style) {
        if (!text.isEmpty())
            text.append(' ');
        text.append(style->cssText());
    }

    return text.toString();
}

bool CSSShadowValue::equals(const CSSShadowValue& other) const
{
    return compareCSSValuePtr(color, other.color)
        && compareCSSValuePtr(x, other.x)
        && compareCSSValuePtr(y, other.y)
        && compareCSSValuePtr(blur, other.blur)
        && compareCSSValuePtr(spread, other.spread)
        && compareCSSValuePtr(style, other.style);
}

}
