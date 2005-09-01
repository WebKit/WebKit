/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Peter Kelly (pmk@post.com)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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

#ifndef KDOM_ElementImpl_H
#define KDOM_ElementImpl_H

#include <kdom/core/NodeImpl.h>

namespace KDOM
{
    struct AttributeImpl;

    class AttrImpl;
    class RenderStyle;
    class NodeListImpl;
    class TypeInfoImpl;
    class NamedAttrMapImpl;
    class NamedNodeMapImpl;
    class CSSStyleDeclarationImpl;
    class ElementImpl : public NodeBaseImpl
    {
    public:
        ElementImpl(DocumentPtr *doc);
        ElementImpl(DocumentPtr *doc, DOMStringImpl *prefix, bool nullNSSpecified = false);
        virtual ~ElementImpl();

        virtual DOMStringImpl *nodeName() const;
        virtual unsigned short nodeType() const;
        virtual DOMStringImpl *tagName() const = 0;

        virtual DOMStringImpl *prefix() const;
        virtual void setPrefix(DOMStringImpl *prefix);

        virtual bool hasAttributes() const;
        
        bool hasAttribute(DOMStringImpl *name) const;
        bool hasAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;
        
        virtual NamedAttrMapImpl *attributes(bool readonly = false) const;

        DOMStringImpl *getAttribute(NodeImpl::Id id, bool nsAware = 0, DOMStringImpl *qName = 0) const;
        DOMStringImpl *getAttribute(DOMStringImpl *name) const;
        DOMStringImpl *getAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;
        
        void setAttribute(NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *qName);
        void setAttribute(DOMStringImpl *name, DOMStringImpl *value);
        void setAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedName, DOMStringImpl *value);
    
        void removeAttribute(DOMStringImpl *name);
        void removeAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);
        
        AttrImpl *getAttributeNode(DOMStringImpl *name) const;
        AttrImpl *setAttributeNode(AttrImpl *newAttr);
        AttrImpl *removeAttributeNode(AttrImpl *oldAttr);
        
        virtual AttrImpl *getAttributeNodeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;
        virtual AttrImpl *setAttributeNodeNS(AttrImpl *newAttr);

        virtual NodeListImpl *getElementsByTagName(DOMStringImpl *name) const;
        virtual NodeListImpl *getElementsByTagNameNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;

        virtual DOMStringImpl *namespaceURI() const;

        void setIdAttribute(DOMStringImpl *name, bool isId); // DOM3
        void setIdAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName, bool isId); // DOM3
        void setIdAttributeNode(AttrImpl *idAttr, bool isId); // DOM3

        // Internal
        virtual bool childAllowed(NodeImpl *newChild);
        virtual bool childTypeAllowed(unsigned short type) const;

        virtual bool checkChild(unsigned short /* tagID */, unsigned short /* childID */) { return true; }

        virtual NodeImpl *cloneNode(bool deep, DocumentPtr *doc) const;

        AttrImpl *getIdAttribute(DOMStringImpl *name) const;

        virtual void parseAttribute(AttributeImpl *);
        void parseAttribute(Id attrId, DOMStringImpl *value);

        bool hasNullNSSpecified() const { return m_nullNSSpecified; }

        virtual bool hasListenerType(int eventId) const;

        virtual TypeInfoImpl *schemaTypeInfo() const;

        // CSS Stuff
        virtual RenderStyle *renderStyle() const { return 0; }

        CSSStyleDeclarationImpl *styleRules() const;
        virtual void createStyleDeclaration() const;

        // Helpers
        bool restyleLate() { return m_restyleLate; }

        void setRestyleLate(bool b = true) { m_restyleLate = b; }
        void setRestyleSelfLate() { m_restyleSelfLate = true; }
        void setRestyleChildrenLate() { m_restyleChildrenLate = true; }

        void setAttributeMap(NamedAttrMapImpl *list);

//    private:
//        void addDOMEventListener(Ecma *ecmaEngine, DOMStringImpl *type, DOMStringImpl *value);

    protected:
        mutable NamedAttrMapImpl *m_attributes;
        mutable CSSStyleDeclarationImpl *m_styleDeclarations;

        DOMStringImpl *m_prefix;

        bool m_restyleLate : 1;
        bool m_restyleSelfLate : 1;
        bool m_restyleChildrenLate : 1;
    };
};

#endif

// vim:ts=4:noet
