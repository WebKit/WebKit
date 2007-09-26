/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006 Apple Computer, Inc.
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
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSMutableStyleDeclaration;
class CSSSelector;

typedef int ExceptionCode;

class CSSStyleRule : public CSSRule {
public:
    CSSStyleRule(StyleBase* parent);
    virtual ~CSSStyleRule();

    virtual bool isStyleRule() { return true; }

    String selectorText() const;
    void setSelectorText(const String&, ExceptionCode&);

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

    // Inherited from CSSRule
    virtual unsigned short type() const { return STYLE_RULE; }

    virtual String cssText() const;

    // Not part of the CSSOM
    virtual bool parseString(const String&, bool = false);

    void setSelector(CSSSelector* selector) { m_selector = selector; }
    void setDeclaration(PassRefPtr<CSSMutableStyleDeclaration>);

    CSSSelector* selector() { return m_selector; }
    CSSMutableStyleDeclaration* declaration() { return m_style.get(); }
 
protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
    CSSSelector* m_selector;
};

} // namespace WebCore

#endif // CSSStyleRule_h
