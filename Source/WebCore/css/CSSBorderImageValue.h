/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "CSSBorderImage.h"
#include "CSSValue.h"

namespace WebCore {

class CSSBorderImageValue final : public CSSValue {
public:
    static Ref<CSSBorderImageValue> create(CSS::BorderImage&&);
    ~CSSBorderImageValue();

    const CSS::BorderImage& borderImage() const { return m_borderImage; }

    String customCSSText(const CSS::SerializationContext&) const;
    bool equals(const CSSBorderImageValue&) const;
    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>&) const;

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

private:
    CSSBorderImageValue(CSS::BorderImage&&);

    CSS::BorderImage m_borderImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSBorderImageValue, isBorderImageValue())
