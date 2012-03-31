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

#ifndef CSSPageRule_h
#define CSSPageRule_h

#include "CSSRule.h"
#include "CSSSelectorList.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertySet.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSSelector;
class CSSSelectorList;
class StyleRuleCSSStyleDeclaration;

class CSSPageRule : public CSSRule {
public:
    static PassRefPtr<CSSPageRule> create(CSSStyleSheet* parent)
    {
        return adoptRef(new CSSPageRule(parent));
    }
    ~CSSPageRule();

    CSSStyleDeclaration* style() const;

    String selectorText() const;
    void setSelectorText(const String&);

    String cssText() const;
    
    const CSSSelector* selector() const { return m_selectorList.first(); }
    StylePropertySet* properties() const { return m_style.get(); }
    
    void adoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void setDeclaration(PassRefPtr<StylePropertySet> style) { m_style = style; }

private:
    CSSPageRule(CSSStyleSheet* parent);

    RefPtr<StylePropertySet> m_style;
    CSSSelectorList m_selectorList;

    mutable RefPtr<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
};

} // namespace WebCore

#endif // CSSPageRule_h
