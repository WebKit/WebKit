/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFontPaletteValuesRule.h"

#include "CSSMarkup.h"
#include "ColorSerialization.h"
#include "JSDOMMapLike.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontPaletteValuesRule::CSSFontPaletteValuesRule(StyleRuleFontPaletteValues& fontPaletteValuesRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_fontPaletteValuesRule(fontPaletteValuesRule)
{
}

CSSFontPaletteValuesRule::~CSSFontPaletteValuesRule()
{
}

String CSSFontPaletteValuesRule::name() const
{
    return m_fontPaletteValuesRule->name();
}

String CSSFontPaletteValuesRule::fontFamily() const
{
    return m_fontPaletteValuesRule->fontFamily();
}

String CSSFontPaletteValuesRule::basePalette() const
{
    if (!m_fontPaletteValuesRule->basePalette())
        return StringImpl::empty();

    switch (m_fontPaletteValuesRule->basePalette()->type) {
    case FontPaletteIndex::Type::Light:
        return "light"_s;
    case FontPaletteIndex::Type::Dark:
        return "dark"_s;
    case FontPaletteIndex::Type::Integer:
        return makeString(m_fontPaletteValuesRule->basePalette()->integer);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String CSSFontPaletteValuesRule::overrideColors() const
{     
    StringBuilder result;
    for (size_t i = 0; i < m_fontPaletteValuesRule->overrideColors().size(); ++i) {
        if (i)
            result.append(", ");
        const auto& item = m_fontPaletteValuesRule->overrideColors()[i];
        result.append(item.first, ' ', serializationForCSS(item.second));
    }
    return result.toString();
}

String CSSFontPaletteValuesRule::cssText() const
{
    StringBuilder builder;
    builder.append("@font-palette-values ", m_fontPaletteValuesRule->name(), " { ");
    if (!m_fontPaletteValuesRule->fontFamily().isNull())
        builder.append("font-family: ", m_fontPaletteValuesRule->fontFamily(), "; ");

    if (m_fontPaletteValuesRule->basePalette()) {
        switch (m_fontPaletteValuesRule->basePalette()->type) {
        case FontPaletteIndex::Type::Light:
            builder.append("base-palette: light; ");
            break;
        case FontPaletteIndex::Type::Dark:
            builder.append("base-palette: dark; ");
            break;
        case FontPaletteIndex::Type::Integer:
            builder.append("base-palette: ", m_fontPaletteValuesRule->basePalette()->integer, "; ");
            break;
        }
    }

    if (!m_fontPaletteValuesRule->overrideColors().isEmpty()) {
        builder.append("override-colors:");
        for (size_t i = 0; i < m_fontPaletteValuesRule->overrideColors().size(); ++i) {
            if (i)
                builder.append(',');
            builder.append(' ', m_fontPaletteValuesRule->overrideColors()[i].first, ' ', serializationForCSS(m_fontPaletteValuesRule->overrideColors()[i].second));
        }
        builder.append("; ");
    }
    builder.append('}');
    return builder.toString();
}

void CSSFontPaletteValuesRule::reattach(StyleRuleBase& rule)
{
    m_fontPaletteValuesRule = downcast<StyleRuleFontPaletteValues>(rule);
}

} // namespace WebCore
