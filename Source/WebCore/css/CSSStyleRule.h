/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSStyleRule_h
#define CSSStyleRule_h

#include "CSSRule.h"
#include "CSSSelectorList.h"
#include "StylePropertySet.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSSelector;
class CSSStyleRule;
    
class StyleRule {
    WTF_MAKE_NONCOPYABLE(StyleRule); WTF_MAKE_FAST_ALLOCATED;
public:
    StyleRule(int sourceLine, CSSStyleRule*);
    ~StyleRule();
            
    const CSSSelectorList& selectorList() const { return m_selectorList; }
    StylePropertySet* properties() const { return m_properties.get(); }
    
    void addSubresourceStyleURLs(ListHashSet<KURL>& urls, CSSStyleSheet*);

    int sourceLine() const { return m_sourceLine; }
    
    CSSStyleRule* ensureCSSStyleRule() const;
    
    void adoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void adoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet> properties) { m_properties = properties; }
    
private:
    RefPtr<StylePropertySet> m_properties;
    CSSSelectorList m_selectorList;
    signed m_sourceLine;

    CSSStyleRule* m_cssomWrapper;
};

class CSSStyleRule : public CSSRule {
public:
    static PassRefPtr<CSSStyleRule> create(CSSStyleSheet* parent, int line)
    {
        return adoptRef(new CSSStyleRule(parent, line));
    }
    ~CSSStyleRule();

    String selectorText() const;
    void setSelectorText(const String&);

    CSSStyleDeclaration* style() const { return m_styleRule->properties()->ensureRuleCSSStyleDeclaration(this); }

    String cssText() const;
    
    StyleRule* styleRule() const { return m_styleRule.get(); }

private:
    CSSStyleRule(CSSStyleSheet* parent, int sourceLine);

    void cleanup();
    String generateSelectorText() const;

    OwnPtr<StyleRule> m_styleRule;
};

} // namespace WebCore

#endif // CSSStyleRule_h
