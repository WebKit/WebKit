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

String CSSFontValue::customCSSText(const CSS::SerializationContext& context) const
{
    // font variant weight size / line-height family
    StringBuilder result;
    if (style)
        result.append(protect(style)->cssText(context));
    if (variant)
        result.append(result.isEmpty() ? ""_s : " "_s, protect(variant)->cssText(context));
    if (weight)
        result.append(result.isEmpty() ? ""_s : " "_s, protect(weight)->cssText(context));
    if (width)
        result.append(result.isEmpty() ? ""_s : " "_s, protect(width)->cssText(context));
    if (size)
        result.append(result.isEmpty() ? ""_s : " "_s, protect(size)->cssText(context));
    if (lineHeight)
        result.append(size ? " / "_s : result.isEmpty() ? ""_s : " "_s, protect(lineHeight)->cssText(context));
    if (family)
        result.append(result.isEmpty() ? ""_s : " "_s, protect(family)->cssText(context));
    return result.toString();
}

bool CSSFontValue::equals(const CSSFontValue& other) const
{
    return compareCSSValuePtr(style, other.style)
        && compareCSSValuePtr(variant, other.variant)
        && compareCSSValuePtr(weight, other.weight)
        && compareCSSValuePtr(width, other.width)
        && compareCSSValuePtr(size, other.size)
        && compareCSSValuePtr(lineHeight, other.lineHeight)
        && compareCSSValuePtr(family, other.family);
}

IterationStatus CSSFontValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    if (style) {
        if (func(protect(*style)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (variant) {
        if (func(protect(*variant)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (weight) {
        if (func(protect(*weight)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (width) {
        if (func(protect(*width)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (size) {
        if (func(protect(*size)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (lineHeight) {
        if (func(protect(*lineHeight)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    if (family) {
        if (func(protect(*family)) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

}
