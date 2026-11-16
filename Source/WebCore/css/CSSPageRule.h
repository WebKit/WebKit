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

#include <WebCore/CSSRule.h>

namespace WebCore {

class CSSPageDescriptors;
class CSSStyleSheet;
class StyleRulePage;

class CSSPageRule final : public CSSRule {
public:
    static Ref<CSSPageRule> create(StyleRulePage& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSPageRule(rule, sheet)); }

    virtual ~CSSPageRule();

    WEBCORE_EXPORT CSSPageDescriptors& style();

    WEBCORE_EXPORT String selectorText() const;
    WEBCORE_EXPORT void setSelectorText(const String&);

private:
    CSSPageRule(StyleRulePage&, CSSStyleSheet*);

    StyleRuleType styleRuleType() const final { return StyleRuleType::Page; }
    String cssText() const final;
    void reattach(StyleRuleBase&) final;

    Ref<StyleRulePage> m_pageRule;
    mutable RefPtr<CSSPageDescriptors> m_propertiesCSSOMWrapper;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSPageRule)
    static bool isType(const WebCore::CSSRule& rule) { return rule.styleRuleType() == WebCore::StyleRuleType::Page; }
SPECIALIZE_TYPE_TRAITS_END()
