/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#include "CSSRule.h"

namespace WebCore {

class CSSStyleDeclaration;
class StyleRuleCSSStyleDeclaration;
class StyleRule;

class CSSStyleRule final : public CSSRule {
public:
    static Ref<CSSStyleRule> create(StyleRule& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSStyleRule(rule, sheet)); }

    virtual ~CSSStyleRule();

    WEBCORE_EXPORT String selectorText() const;
    WEBCORE_EXPORT void setSelectorText(const String&);

    WEBCORE_EXPORT CSSStyleDeclaration& style();

    // FIXME: Not CSSOM. Remove.
    StyleRule& styleRule() const { return m_styleRule.get(); }

private:
    CSSStyleRule(StyleRule&, CSSStyleSheet*);

    CSSRule::Type type() const final { return STYLE_RULE; }
    String cssText() const final;
    void reattach(StyleRuleBase&) final;

    String generateSelectorText() const;

    Ref<StyleRule> m_styleRule;
    RefPtr<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSStyleRule, CSSRule::STYLE_RULE)
