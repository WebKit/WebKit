/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "DeprecatedCSSOMPrimitiveValue.h"
#include "RGBColor.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DeprecatedCSSOMRGBColor final : public RefCounted<DeprecatedCSSOMRGBColor> {
public:
    static Ref<DeprecatedCSSOMRGBColor> create(const RGBColor& color, CSSStyleDeclaration& owner)
    {
        return adoptRef(*new DeprecatedCSSOMRGBColor(color, owner));
    }

    DeprecatedCSSOMPrimitiveValue* red() { return m_red.get(); }
    DeprecatedCSSOMPrimitiveValue* green() { return m_green.get(); }
    DeprecatedCSSOMPrimitiveValue* blue() { return m_blue.get(); }
    DeprecatedCSSOMPrimitiveValue* alpha() { return m_alpha.get(); }
    
    Color color() const { return Color(m_rgbColor); }

private:
    DeprecatedCSSOMRGBColor(const RGBColor& color, CSSStyleDeclaration& owner)
        : m_rgbColor(color.rgbColor())
    {
        // Red
        unsigned value = (m_rgbColor >> 16) & 0xFF;
        auto result = CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER);
        m_red = result->createDeprecatedCSSOMPrimitiveWrapper(owner);
        
        // Green
        value = (m_rgbColor >> 8) & 0xFF;
        result = CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER);
        m_green = result->createDeprecatedCSSOMPrimitiveWrapper(owner);

        // Blue
        value = m_rgbColor & 0xFF;
        result = CSSPrimitiveValue::create(value, CSSPrimitiveValue::CSS_NUMBER);
        m_blue = result->createDeprecatedCSSOMPrimitiveWrapper(owner);
        
        // Alpha
        float alphaValue = static_cast<float>((m_rgbColor >> 24) & 0xFF) / 0xFF;
        result = CSSPrimitiveValue::create(alphaValue, CSSPrimitiveValue::CSS_NUMBER);
        m_alpha = result->createDeprecatedCSSOMPrimitiveWrapper(owner);
    }
    
    RGBA32 m_rgbColor;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_red;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_green;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_blue;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_alpha;
};

} // namespace WebCore
