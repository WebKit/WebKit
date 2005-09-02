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

#ifndef KDOM_CSSStyleDeclarationImpl_H
#define KDOM_CSSStyleDeclarationImpl_H

#include <q3ptrlist.h>

#include <kdom/css/StyleBaseImpl.h>

namespace KDOM
{
    class NodeImpl;
    class DOMString;
    class CSSProperty;
    class CSSRuleImpl;
    class CSSValueImpl;
    class CDFInterface;
    class CSSStyleDeclarationImpl : public StyleBaseImpl
    {
    public:
        CSSStyleDeclarationImpl(CDFInterface *interface, CSSRuleImpl *parentRule);
        CSSStyleDeclarationImpl(CDFInterface *interface, CSSRuleImpl *parentRule, Q3PtrList<CSSProperty> *lstValues);
        virtual ~CSSStyleDeclarationImpl();

        CSSStyleDeclarationImpl& operator=(const CSSStyleDeclarationImpl &);

        // 'CSSStyleDeclaration' functions
        void setCssText(DOMStringImpl *cssText);
        DOMStringImpl *cssText() const;

        DOMStringImpl *getPropertyValue(int propertyID) const;
        DOMStringImpl *getPropertyValue(DOMStringImpl *propertyName) const;

        CSSValueImpl *getPropertyCSSValue(int propertyID) const;
        CSSValueImpl *getPropertyCSSValue(DOMStringImpl *propertyName) const;

        bool getPropertyPriority(int propertyID) const;
        DOMStringImpl *getPropertyPriority(DOMStringImpl *propertyName) const;

        void setProperty(int propertyId, int value, bool important = false, bool nonCSSHint = false);
        bool setProperty(int propertyID, DOMStringImpl *value, bool important = false, bool nonCSSHint = false);

        void setProperty(DOMStringImpl *propertyName, DOMStringImpl *value, DOMStringImpl *priority);
        void setLengthProperty(int id, DOMStringImpl *value, bool important, bool nonCSSHint = true, bool multiLength = false);

        // add a whole, unparsed property
        void setProperty(DOMStringImpl *propertyString);
        unsigned long length() const;
        DOMStringImpl *item(unsigned long index) const;

        CSSRuleImpl *parentRule() const;

        DOMStringImpl *removeProperty(DOMStringImpl *propertyName);
        DOMStringImpl *removeProperty(int propertyID, bool NonCSSHints = false);

        void setNode(NodeImpl *node) { m_node = node; }
        
        CDFInterface *interface() const { return m_interface; }
        Q3PtrList<CSSProperty> *values() const { return m_lstValues; }

        virtual void setChanged();

        virtual bool isStyleDeclaration() const { return true; }
        virtual bool parseString(DOMStringImpl *string, bool = false);

        // Helper
        void removeCSSHints();

    protected:
        DOMStringImpl *getShortHandValue(const int *properties, int number) const;
        DOMStringImpl *get4Values(const int *properties) const;

    protected:
        Q3PtrList<CSSProperty> *m_lstValues;
        CDFInterface *m_interface;
        NodeImpl *m_node;
    };

    class DOMImplementationCSS;
    class CSSProperty
    {
    public:
        CSSProperty();
        CSSProperty(const CSSProperty &other);
        ~CSSProperty();

        void setValue(CSSValueImpl *val);

        CSSValueImpl *value() const;
        DOMStringImpl *cssText(const CSSStyleDeclarationImpl &decl) const;

        // make sure the following fits in 4 bytes.
        signed int m_id : 29;
        bool m_important : 1;
        bool m_nonCSSHint : 1;

    protected:
        CSSValueImpl *m_value;
    };
};

#endif

// vim:ts=4:noet
