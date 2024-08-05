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

void CSSFontValue::customCSSText(StringBuilder& builder) const
{
    auto initialLengthOfBuilder = builder.length();

    auto append = [&](auto& arg) {
        if (!arg)
            return;
        if (initialLengthOfBuilder != builder.length())
            builder.append(' ');
        arg->cssText(builder);
    };

    append(style);
    append(variant);
    append(weight);
    append(stretch);
    append(size);

    if (lineHeight) {
        if (size)
            builder.append(" / "_s);
        else if (initialLengthOfBuilder != builder.length())
            builder.append(' ');
        lineHeight->cssText(builder);
    }

    append(family);
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

IterationStatus CSSFontValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    if (style) {
        if (func(*style) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (variant) {
        if (func(*variant) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (weight) {
        if (func(*weight) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (stretch) {
        if (func(*stretch) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (size) {
        if (func(*size) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (lineHeight) {
        if (func(*lineHeight) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (family) {
        if (func(*family) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

}
