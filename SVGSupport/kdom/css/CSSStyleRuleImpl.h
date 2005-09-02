/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_CSSStyleRuleImpl_H
#define KDOM_CSSStyleRuleImpl_H

#include <kdom/css/CSSRuleImpl.h>

#include <q3ptrlist.h>

namespace KDOM
{
    class CSSStyleDeclarationImpl;
    class CSSStyleRuleImpl : public CSSRuleImpl
    {
    public:
        CSSStyleRuleImpl(StyleBaseImpl *parent);
        virtual ~CSSStyleRuleImpl();

        // 'CSSStyleRule' functions
        DOMStringImpl *selectorText() const;
        void setSelectorText(DOMStringImpl *selectorText);

        virtual bool parseString(const DOMString &string, bool = false);

        CSSStyleDeclarationImpl *style() const;

        // 'CSSRule' functions
        virtual bool isStyleRule() const { return true; }

        void setSelector(Q3PtrList<CSSSelector> *selector);
        void setDeclaration(CSSStyleDeclarationImpl *style);

        Q3PtrList<CSSSelector> *selector() { return m_selector; }
        CSSStyleDeclarationImpl *declaration() { return m_style; }

        void setNonCSSHints();

    protected:
        CSSStyleDeclarationImpl *m_style;
        Q3PtrList<CSSSelector> *m_selector;
    };
};

#endif

// vim:ts=4:noet
