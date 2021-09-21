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

String CSSFontPaletteValuesRule::fontFamily() const
{
    return m_fontPaletteValuesRule->fontFamily();
}

String CSSFontPaletteValuesRule::basePalette() const
{
    return WTF::switchOn(m_fontPaletteValuesRule->basePalette(), [&](int64_t index) {
        return makeString(index);
    }, [&](const AtomString& basePalette) -> String {
        if (!basePalette.isNull())
            return basePalette.string();
        return StringImpl::empty();
    });
}

void CSSFontPaletteValuesRule::initializeMapLike(DOMMapAdapter& map)
{
    for (auto& pair : m_fontPaletteValuesRule->overrideColor()) {
        if (!WTF::holds_alternative<int64_t>(pair.first))
            continue;
        map.set<IDLUnsignedLong, IDLUSVString>(WTF::get<int64_t>(pair.first), serializationForCSS(pair.second));
    }
}

String CSSFontPaletteValuesRule::cssText() const
{
    StringBuilder builder;
    builder.append("@font-palette-values ", m_fontPaletteValuesRule->name(), " { ");
    if (!m_fontPaletteValuesRule->fontFamily().isNull())
        builder.append("font-family: ", m_fontPaletteValuesRule->fontFamily(), "; ");
    WTF::switchOn(m_fontPaletteValuesRule->basePalette(), [&](int64_t index) {
        builder.append("base-palette: ", index, "; ");
    }, [&](const AtomString& basePalette) {
        if (!basePalette.isNull())
            builder.append("base-palette: ", serializeString(basePalette.string()), "; ");
    });
    if (!m_fontPaletteValuesRule->overrideColor().isEmpty()) {
        builder.append("override-color:");
        for (size_t i = 0; i < m_fontPaletteValuesRule->overrideColor().size(); ++i) {
            if (i)
                builder.append(',');
            builder.append(' ');
            WTF::switchOn(m_fontPaletteValuesRule->overrideColor()[i].first, [&](const AtomString& name) {
                builder.append(serializeString(name.string()));
            }, [&](int64_t index) {
                builder.append(index);
            });
            builder.append(' ', serializationForCSS(m_fontPaletteValuesRule->overrideColor()[i].second));
        }
        builder.append("; ");
    }
    builder.append('}');
    return builder.toString();
}

void CSSFontPaletteValuesRule::reattach(StyleRuleBase& rule)
{
    m_fontPaletteValuesRule = static_cast<StyleRuleFontPaletteValues&>(rule);
}

} // namespace WebCore
