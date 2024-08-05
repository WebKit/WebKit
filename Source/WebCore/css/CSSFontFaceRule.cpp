/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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
#include "CSSFontFaceRule.h"

#include "MutableStyleProperties.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSFontFaceRule::CSSFontFaceRule(StyleRuleFontFace& fontFaceRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_fontFaceRule(fontFaceRule)
{
}

CSSFontFaceRule::~CSSFontFaceRule()
{
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();
}

CSSStyleDeclaration& CSSFontFaceRule::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_fontFaceRule->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

template<typename DeclarationsFunctor> void CSSFontFaceRule::cssTextInternal(StringBuilder& builder, bool hasDeclarations, DeclarationsFunctor&& declarations) const
{
    builder.append("@font-face { "_s);
    if (!hasDeclarations) {
        builder.append('}');
        return;
    }

    declarations(builder);

    builder.append(" }"_s);
}

void CSSFontFaceRule::cssText(StringBuilder& builder) const
{
    return cssTextInternal(builder, !m_fontFaceRule->properties().isEmpty(), [&](auto& builder) {
        m_fontFaceRule->properties().asText(builder);
    });
}

void CSSFontFaceRule::cssTextWithReplacementURLs(StringBuilder& builder, const HashMap<String, String>& replacementURLStrings, const HashMap<RefPtr<CSSStyleSheet>, String>&) const
{
    auto mutableStyleProperties = m_fontFaceRule->properties().mutableCopy();

    return cssTextInternal(builder, !mutableStyleProperties->isEmpty(), [&](auto& builder) {
        mutableStyleProperties->setReplacementURLForSubresources(replacementURLStrings);
        mutableStyleProperties->asText(builder);
        mutableStyleProperties->clearReplacementURLForSubresources();
    });
}

void CSSFontFaceRule::reattach(StyleRuleBase& rule)
{
    m_fontFaceRule = downcast<StyleRuleFontFace>(rule);
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_fontFaceRule->mutableProperties());
}

} // namespace WebCore
