/**
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "RectBase.h"
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

class Quad final : public RectBase {
public:
    Quad(Ref<CSSPrimitiveValue> value)
        : RectBase(WTFMove(value))
    { }
    Quad(Ref<CSSPrimitiveValue> top, Ref<CSSPrimitiveValue> right, Ref<CSSPrimitiveValue> bottom, Ref<CSSPrimitiveValue> left)
        : RectBase(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left))
    { }

    String cssText() const
    {
        return serialize(top().cssText(), right().cssText(), bottom().cssText(), left().cssText());
    }

    static String serialize(const String& top, const String& right, const String& bottom, const String& left)
    {
        if (left != right)
            return makeString(top, ' ', right, ' ', bottom, ' ', left);
        if (bottom != top)
            return makeString(top, ' ', right, ' ', bottom);
        if (right != top)
            return makeString(top, ' ', right);
        return top;
    }
};

} // namespace WebCore
