/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002-2020 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class CSSStyleSheet;
class StyleRuleBase;

struct CSSParserContext;

class CSSRule : public RefCounted<CSSRule> {
public:
    virtual ~CSSRule() = default;

    // WebIDL enum
    enum Type {
        UNKNOWN_RULE,
        STYLE_RULE,
        CHARSET_RULE,
        IMPORT_RULE,
        MEDIA_RULE,
        FONT_FACE_RULE,
        PAGE_RULE,
        KEYFRAMES_RULE,
        KEYFRAME_RULE,
        MARGIN_RULE,
        NAMESPACE_RULE,
        COUNTER_STYLE_RULE,
        SUPPORTS_RULE
    };

    enum DeprecatedType {
        WEBKIT_KEYFRAMES_RULE = KEYFRAMES_RULE,
        WEBKIT_KEYFRAME_RULE = KEYFRAME_RULE
    };

    virtual Type type() const = 0;
    virtual String cssText() const = 0;
    virtual void reattach(StyleRuleBase&) = 0;

    void setParentStyleSheet(CSSStyleSheet*);
    void setParentRule(CSSRule*);
    CSSStyleSheet* parentStyleSheet() const;
    CSSRule* parentRule() const { return m_parentIsRule ? m_parentRule : nullptr; }

    WEBCORE_EXPORT ExceptionOr<void> setCssText(const String&);

protected:
    explicit CSSRule(CSSStyleSheet*);

    bool hasCachedSelectorText() const { return m_hasCachedSelectorText; }
    void setHasCachedSelectorText(bool hasCachedSelectorText) const { m_hasCachedSelectorText = hasCachedSelectorText; }

    const CSSParserContext& parserContext() const;

private:
    mutable unsigned char m_hasCachedSelectorText : 1;
    unsigned char m_parentIsRule : 1;
    union {
        CSSRule* m_parentRule;
        CSSStyleSheet* m_parentStyleSheet;
    };
};

inline CSSRule::CSSRule(CSSStyleSheet* parent)
    : m_hasCachedSelectorText(false)
    , m_parentIsRule(false)
    , m_parentStyleSheet(parent)
{
}

inline void CSSRule::setParentStyleSheet(CSSStyleSheet* styleSheet)
{
    m_parentIsRule = false;
    m_parentStyleSheet = styleSheet;
}

inline void CSSRule::setParentRule(CSSRule* rule)
{
    m_parentIsRule = true;
    m_parentRule = rule;
}

inline CSSStyleSheet* CSSRule::parentStyleSheet() const
{
    if (m_parentIsRule)
        return m_parentRule ? m_parentRule->parentStyleSheet() : nullptr;
    return m_parentStyleSheet;
}

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_RULE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSRule& rule) { return rule.type() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
