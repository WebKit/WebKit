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

#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"
#include "StyleRule.h"
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

String CSSFontFaceRule::cssText() const
{
    String declarations = m_fontFaceRule->properties().asText();
    if (declarations.isEmpty())
        return "@font-face { }"_s;
    return makeString("@font-face { ", declarations, " }");
}

void CSSFontFaceRule::reattach(StyleRuleBase& rule)
{
    m_fontFaceRule = downcast<StyleRuleFontFace>(rule);
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->reattach(m_fontFaceRule->mutableProperties());
}

} // namespace WebCore
