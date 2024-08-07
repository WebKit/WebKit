/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValue.h"
#include "CSSValue.h"
#include <wtf/Function.h>
#include <wtf/RefPtr.h>

namespace WebCore {

// Used for text-shadow and box-shadow
class CSSShadowValue final : public CSSValue {
public:
    static Ref<CSSShadowValue> create(RefPtr<CSSPrimitiveValue>&& x,
        RefPtr<CSSPrimitiveValue>&& y,
        RefPtr<CSSPrimitiveValue>&& blur,
        RefPtr<CSSPrimitiveValue>&& spread,
        RefPtr<CSSPrimitiveValue>&& style,
        RefPtr<CSSPrimitiveValue>&& color,
        bool isWebkitBoxShadow = false)
    {
        return adoptRef(*new CSSShadowValue(WTFMove(x), WTFMove(y), WTFMove(blur), WTFMove(spread), WTFMove(style), WTFMove(color), isWebkitBoxShadow));
    }

    String customCSSText() const;

    bool equals(const CSSShadowValue&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (x) {
            if (func(*x) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (y) {
            if (func(*y) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (blur) {
            if (func(*blur) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (spread) {
            if (func(*spread) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (style) {
            if (func(*style) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (color) {
            if (func(*color) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

    RefPtr<CSSPrimitiveValue> x;
    RefPtr<CSSPrimitiveValue> y;
    RefPtr<CSSPrimitiveValue> blur;
    RefPtr<CSSPrimitiveValue> spread;
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;
    bool isWebkitBoxShadow { false };

private:
    CSSShadowValue(RefPtr<CSSPrimitiveValue>&& x,
        RefPtr<CSSPrimitiveValue>&& y,
        RefPtr<CSSPrimitiveValue>&& blur,
        RefPtr<CSSPrimitiveValue>&& spread,
        RefPtr<CSSPrimitiveValue>&& style,
        RefPtr<CSSPrimitiveValue>&& color,
        bool isWebkitBoxShadow);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSShadowValue, isShadowValue())
