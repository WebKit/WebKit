/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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

#include <WebCore/CSSParserEnum.h>
#include <WebCore/StyleRuleType.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompactVariant.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CSSStyleSheet;
class StyleRuleBase;
class StyleRule;
class StyleRuleWithNesting;

struct CSSParserContext;

template<typename> class ExceptionOr;

namespace CSS {
struct SerializationContext;
}

class CSSRule : public RefCountedAndCanMakeWeakPtr<CSSRule>, public CanMakeCheckedPtr<CSSRule> {
    WTF_MAKE_TZONE_ALLOCATED(CSSRule);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CSSRule);
public:
    virtual ~CSSRule() = default;

    WEBCORE_EXPORT unsigned short typeForCSSOM() const;

    virtual StyleRuleType styleRuleType() const = 0;
    virtual bool isGroupingRule() const { return false; }
    virtual String cssText() const = 0;
    virtual String cssText(const CSS::SerializationContext&) const { return cssText(); }
    virtual void reattach(StyleRuleBase&) = 0;

    void setParentStyleSheet(CheckedPtr<CSSStyleSheet>&&);
    void setParentRule(CheckedPtr<CSSRule>&&);
    CSSStyleSheet* parentStyleSheet() const;
    CheckedPtr<CSSStyleSheet> checkedParentStyleSheet() const;
    CSSRule* parentRule() const;
    CheckedPtr<CSSRule> checkedParentRule() const;
    bool hasStyleRuleAncestor() const;
    CSSParserEnum::NestedContext nestedContext() const;
    virtual RefPtr<StyleRuleWithNesting> prepareChildStyleRuleForNesting(StyleRule&);
    virtual void getChildStyleSheets(HashSet<RefPtr<CSSStyleSheet>>&) { }

    WEBCORE_EXPORT ExceptionOr<void> setCssText(const String&);

protected:
    explicit CSSRule(CheckedPtr<CSSStyleSheet>&&);

    bool hasCachedSelectorText() const { return m_hasCachedSelectorText; }
    void setHasCachedSelectorText(bool hasCachedSelectorText) const { m_hasCachedSelectorText = hasCachedSelectorText; }

    const CSSParserContext& parserContext() const;

private:
    mutable unsigned char m_hasCachedSelectorText : 1;

    using ParentRuleOrStyleSheet = WTF::CompactVariant<CheckedPtr<CSSRule>, CheckedPtr<CSSStyleSheet>>;
    ParentRuleOrStyleSheet m_parentRuleOrStyleSheet;
};

inline CSSRule::CSSRule(CheckedPtr<CSSStyleSheet>&& parent)
    : m_hasCachedSelectorText(false)
    , m_parentRuleOrStyleSheet(WTFMove(parent))
{
}

inline void CSSRule::setParentStyleSheet(CheckedPtr<CSSStyleSheet>&& checkedStyleSheet)
{
    m_parentRuleOrStyleSheet = WTFMove(checkedStyleSheet);
}

inline void CSSRule::setParentRule(CheckedPtr<CSSRule>&& checkedRule)
{
    m_parentRuleOrStyleSheet = WTFMove(checkedRule);
}

inline CSSStyleSheet* CSSRule::parentStyleSheet() const
{
    return WTF::switchOn(m_parentRuleOrStyleSheet,
        [&](const CheckedPtr<CSSStyleSheet>& stylesheet) { return stylesheet.get(); },
        [&](const CheckedPtr<CSSRule>& rule) { return rule ? rule->parentStyleSheet() : nullptr; }
    );
}

inline CheckedPtr<CSSStyleSheet> CSSRule::checkedParentStyleSheet() const
{
    return WTF::switchOn(m_parentRuleOrStyleSheet,
        [&](const CheckedPtr<CSSStyleSheet>& stylesheet) { return stylesheet; },
        [&](const CheckedPtr<CSSRule>& rule) { return CheckedPtr<CSSStyleSheet>(rule->parentStyleSheet()); }
    );
}

inline CSSRule* CSSRule::parentRule() const
{
    return WTF::switchOn(m_parentRuleOrStyleSheet,
        [&](const CheckedPtr<CSSStyleSheet>&) { return static_cast<CSSRule*>(nullptr); },
        [&](const CheckedPtr<CSSRule>& rule) { return rule.get(); }
    );
}

inline CheckedPtr<CSSRule> CSSRule::checkedParentRule() const
{
    return WTF::switchOn(m_parentRuleOrStyleSheet,
        [&](const CheckedPtr<CSSStyleSheet>&) { return CheckedPtr<CSSRule>(); },
        [&](const CheckedPtr<CSSRule>& rule) { return rule; }
    );
}


} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_RULE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSRule& rule) { return rule.styleRuleType() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
