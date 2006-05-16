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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef CSSStyleRule_H
#define CSSStyleRule_H

#include "CSSRule.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSMutableStyleDeclaration;
class CSSSelector;

class CSSStyleRule : public CSSRule
{
public:
    CSSStyleRule(StyleBase* parent);
    virtual ~CSSStyleRule();

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

    virtual bool isStyleRule() { return true; }
    virtual String cssText() const;

    String selectorText() const;
    void setSelectorText(String);

    virtual bool parseString(const String&, bool = false);

    void setSelector(CSSSelector* selector) { m_selector = selector; }
    void setDeclaration(PassRefPtr<CSSMutableStyleDeclaration>);

    CSSSelector* selector() { return m_selector; }
    CSSMutableStyleDeclaration* declaration() { return m_style.get(); }
 
protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
    CSSSelector* m_selector;
};

} // namespace

#endif
