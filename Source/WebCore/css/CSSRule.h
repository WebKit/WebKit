/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef CSSRule_h
#define CSSRule_h

#include "KURLHash.h"
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CSSStyleSheet;
class StyleRuleBase;
struct CSSParserContext;
typedef int ExceptionCode;

class CSSRule : public RefCounted<CSSRule> {
public:
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref()
    {
        if (derefBase())
            destroy();
    }

    enum Type {
        UNKNOWN_RULE,
        STYLE_RULE,
        CHARSET_RULE,
        IMPORT_RULE,
        MEDIA_RULE,
        FONT_FACE_RULE,
        PAGE_RULE,
        // 7 was VARIABLES_RULE; we now match other browsers with 7 as
        // KEYFRAMES_RULE:
        // <https://bugs.webkit.org/show_bug.cgi?id=71293>.
        WEBKIT_KEYFRAMES_RULE,
        WEBKIT_KEYFRAME_RULE,
#if ENABLE(CSS_REGIONS)
        WEBKIT_REGION_RULE = 10
#endif
    };

    Type type() const { return static_cast<Type>(m_type); }

    bool isCharsetRule() const { return type() == CHARSET_RULE; }
    bool isFontFaceRule() const { return type() == FONT_FACE_RULE; }
    bool isKeyframeRule() const { return type() == WEBKIT_KEYFRAME_RULE; }
    bool isKeyframesRule() const { return type() == WEBKIT_KEYFRAMES_RULE; }
    bool isMediaRule() const { return type() == MEDIA_RULE; }
    bool isPageRule() const { return type() == PAGE_RULE; }
    bool isStyleRule() const { return type() == STYLE_RULE; }
    bool isImportRule() const { return type() == IMPORT_RULE; }

#if ENABLE(CSS_REGIONS)
    bool isRegionRule() const { return type() == WEBKIT_REGION_RULE; }
#endif

    void setParentStyleSheet(CSSStyleSheet* styleSheet)
    {
        m_parentIsRule = false;
        m_parentStyleSheet = styleSheet;
    }

    void setParentRule(CSSRule* rule)
    {
        m_parentIsRule = true;
        m_parentRule = rule;
    }

    CSSStyleSheet* parentStyleSheet() const
    {
        if (m_parentIsRule)
            return m_parentRule ? m_parentRule->parentStyleSheet() : 0;
        return m_parentStyleSheet;
    }

    CSSRule* parentRule() const { return m_parentIsRule ? m_parentRule : 0; }

    String cssText() const;
    void setCssText(const String&, ExceptionCode&);

    void reattach(StyleRuleBase*);

protected:
    CSSRule(CSSStyleSheet* parent, Type type)
        : m_hasCachedSelectorText(false)
        , m_parentIsRule(false)
        , m_type(type)
        , m_parentStyleSheet(parent)
    {
    }

    // NOTE: This class is non-virtual for memory and performance reasons.
    // Don't go making it virtual again unless you know exactly what you're doing!

    ~CSSRule() { }

    bool hasCachedSelectorText() const { return m_hasCachedSelectorText; }
    void setHasCachedSelectorText(bool hasCachedSelectorText) const { m_hasCachedSelectorText = hasCachedSelectorText; }

    const CSSParserContext& parserContext() const;

private:
    mutable unsigned m_hasCachedSelectorText : 1;
    unsigned m_parentIsRule : 1;
    unsigned m_type : 4;
    union {
        CSSRule* m_parentRule;
        CSSStyleSheet* m_parentStyleSheet;
    };

    void destroy();
};

} // namespace WebCore

#endif // CSSRule_h
