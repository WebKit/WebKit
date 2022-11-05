/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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
#include "CSSFontValue.h"

#include "CSSFontStyleWithAngleValue.h"
#include "CSSValueList.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

String CSSFontValue::customCSSText() const
{
    // font variant weight size / line-height family
    StringBuilder result;
    if (style)
        result.append(style->cssText());
    if (variant)
        result.append(result.isEmpty() ? "" : " ", variant->cssText());
    if (weight)
        result.append(result.isEmpty() ? "" : " ", weight->cssText());
    if (stretch)
        result.append(result.isEmpty() ? "" : " ", stretch->cssText());
    if (size)
        result.append(result.isEmpty() ? "" : " ", size->cssText());
    if (lineHeight)
        result.append(size ? " / " : result.isEmpty() ? "" : " ", lineHeight->cssText());
    if (family)
        result.append(result.isEmpty() ? "" : " ", family->cssText());
    return result.toString();
}

bool CSSFontValue::equals(const CSSFontValue& other) const
{
    return compareCSSValuePtr(style, other.style)
        && compareCSSValuePtr(variant, other.variant)
        && compareCSSValuePtr(weight, other.weight)
        && compareCSSValuePtr(stretch, other.stretch)
        && compareCSSValuePtr(size, other.size)
        && compareCSSValuePtr(lineHeight, other.lineHeight)
        && compareCSSValuePtr(family, other.family);
}

}
