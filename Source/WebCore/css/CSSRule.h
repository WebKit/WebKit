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

#include "CSSStyleSheet.h"
#include "KURLHash.h"
#include <wtf/ListHashSet.h>

namespace WebCore {

typedef int ExceptionCode;

class CSSRule : public RefCounted<CSSRule> {
public:
    virtual ~CSSRule() { }

    // FIXME: Change name to Type.
    enum CSSRuleType {
        UNKNOWN_RULE,
        STYLE_RULE,
        CHARSET_RULE,
        IMPORT_RULE,
        MEDIA_RULE,
        FONT_FACE_RULE,
        PAGE_RULE,
        // 7 used to be VARIABLES_RULE
        WEBKIT_KEYFRAMES_RULE = 8,
        WEBKIT_KEYFRAME_RULE,
        WEBKIT_REGION_STYLE_RULE
    };

    virtual CSSRuleType type() const = 0;

    virtual bool isCharsetRule() const { return false; }
    virtual bool isFontFaceRule() const { return false; }
    virtual bool isKeyframeRule() const { return false; }
    virtual bool isKeyframesRule() const { return false; }
    virtual bool isMediaRule() const { return false; }
    virtual bool isPageRule() const { return false; }
    virtual bool isStyleRule() const { return false; }
    virtual bool isRegionStyleRule() const { return false; }
    virtual bool isImportRule() const { return false; }

    bool useStrictParsing() const
    {
        if (parentRule())
            return parentRule()->useStrictParsing();
        if (parentStyleSheet())
            return parentStyleSheet()->useStrictParsing();
        return true;
    }

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

    virtual String cssText() const = 0;
    void setCssText(const String&, ExceptionCode&);

    virtual void addSubresourceStyleURLs(ListHashSet<KURL>&) { }

    virtual void insertedIntoParent() { }

    KURL baseURL() const
    {
        if (CSSStyleSheet* parentSheet = parentStyleSheet())
            return parentSheet->baseURL();
        return KURL();
    }

protected:
    CSSRule(CSSStyleSheet* parent)
        : m_parentIsRule(false)
        , m_parentStyleSheet(parent)
    {
    }

private:
    bool m_parentIsRule;
    union {
        CSSRule* m_parentRule;
        CSSStyleSheet* m_parentStyleSheet;
    };
};

} // namespace WebCore

#endif // CSSRule_h
