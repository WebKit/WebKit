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

#include <WebCore/CSSValue.h>

namespace WebCore {

class RectBase {
public:
    const CSSValue& top() const { return m_top.get(); }
    const CSSValue& right() const { return m_right.get(); }
    const CSSValue& bottom() const { return m_bottom.get(); }
    const CSSValue& left() const { return m_left.get(); }

    bool equals(const RectBase& other) const
    {
        return compareCSSValue(m_top, other.m_top)
            && compareCSSValue(m_right, other.m_right)
            && compareCSSValue(m_left, other.m_left)
            && compareCSSValue(m_bottom, other.m_bottom);
    }

protected:
    explicit RectBase(Ref<CSSValue> value)
        : m_top(value)
        , m_right(value)
        , m_bottom(value)
        , m_left(WTFMove(value))
    { }
    RectBase(Ref<CSSValue> top, Ref<CSSValue> right, Ref<CSSValue> bottom, Ref<CSSValue> left)
        : m_top(WTFMove(top))
        , m_right(WTFMove(right))
        , m_bottom(WTFMove(bottom))
        , m_left(WTFMove(left))
    { }
    ~RectBase() = default;

private:
    const Ref<const CSSValue> m_top;
    const Ref<const CSSValue> m_right;
    const Ref<const CSSValue> m_bottom;
    const Ref<const CSSValue> m_left;
};

} // namespace WebCore
