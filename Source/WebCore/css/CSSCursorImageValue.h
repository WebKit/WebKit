/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2025 Apple Inc. All right reserved.
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

#include "CSSPrimitiveNumericTypes.h"
#include "CSSURL.h"
#include "CSSValue.h"
#include "CSSValuePair.h"

namespace WebCore {

namespace Style {
class BuilderState;
class CursorImage;
class Image;
}

class CSSCursorImageValue final : public CSSValue {
public:
    using HotSpot = SpaceSeparatedPoint<CSS::Number<>>;

    static Ref<CSSCursorImageValue> create(Ref<CSSValue>&& imageValue, std::optional<HotSpot>&&);
    static Ref<CSSCursorImageValue> NODELETE create(Ref<CSSValue>&& imageValue, std::optional<HotSpot>&&, CSS::URL&&);
    ~CSSCursorImageValue();

    const CSS::URL& originalURL() const LIFETIME_BOUND { return m_originalURL; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSCursorImageValue&) const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
    {
        return func(m_imageValue.get());
    }

    RefPtr<Style::CursorImage> createStyleImage(const Style::BuilderState&) const;

private:
    CSSCursorImageValue(Ref<CSSValue>&& imageValue, std::optional<HotSpot>&&, CSS::URL&&);

    const Ref<CSSValue> m_imageValue;
    const std::optional<HotSpot> m_hotSpot;
    CSS::URL m_originalURL;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCursorImageValue, isCursorImageValue())
