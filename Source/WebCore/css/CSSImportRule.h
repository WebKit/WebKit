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

#ifndef CSSImportRule_h
#define CSSImportRule_h

#include "CSSRule.h"

namespace WebCore {

class CachedCSSStyleSheet;
class MediaList;
class MediaQuerySet;
class StyleRuleImport;

class CSSImportRule final : public CSSRule {
public:
    static Ref<CSSImportRule> create(StyleRuleImport& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSImportRule(rule, sheet)); }
    
    virtual ~CSSImportRule();

    String cssText() const override;
    void reattach(StyleRuleBase&) override;

    String href() const;
    MediaList& media() const;
    CSSStyleSheet* styleSheet() const;

private:
    CSSImportRule(StyleRuleImport&, CSSStyleSheet*);

    CSSRule::Type type() const override { return IMPORT_RULE; }

    Ref<StyleRuleImport> m_importRule;
    mutable RefPtr<MediaList> m_mediaCSSOMWrapper;
    mutable RefPtr<CSSStyleSheet> m_styleSheetCSSOMWrapper;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSImportRule, CSSRule::IMPORT_RULE)

#endif // CSSImportRule_h
