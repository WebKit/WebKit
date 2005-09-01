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

#ifndef KDOM_AttrImpl_H
#define KDOM_AttrImpl_H

#include <kdom/impl/NodeImpl.h>

namespace KDOM
{
    class TypeInfoImpl;

    class AttrImpl : public NodeBaseImpl
    {
    public:
        AttrImpl(DocumentPtr *doc, NodeImpl::Id id);
        AttrImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified = false);
        AttrImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *prefix, bool nullNSSpecified = false);
        virtual ~AttrImpl();

        // 'Attr' functions
        DOMStringImpl *name() const;

        DOMStringImpl *value() const;
        virtual void setValue(DOMStringImpl *value);

        virtual DOMStringImpl *nodeName() const;
        virtual DOMStringImpl *localName() const;

        virtual DOMStringImpl *nodeValue() const;
        virtual void setNodeValue(DOMStringImpl *nodeValue);

        virtual unsigned short nodeType() const; 

        virtual DOMStringImpl *namespaceURI() const;

        virtual DOMStringImpl *prefix() const;
        virtual void setPrefix(DOMStringImpl *prefix);

        virtual bool specified() const;
        virtual bool isId() const; // DOM3
        virtual void setIsId(bool isId); // DOM3

        virtual ElementImpl *ownerElement() const;

        // Internal
        virtual bool isReadOnly() const;

        virtual bool childAllowed(NodeImpl *newChild);
        virtual bool childTypeAllowed(unsigned short type) const;

        virtual NodeImpl *cloneNode(bool deep, DocumentPtr *doc) const;

        void setOwnerElement(ElementImpl* impl);

        virtual NodeImpl::Id id() const { return m_id; }
        bool hasNullNSSpecified() const { return m_nullNSSpecified; }

        DOMStringImpl *val();

        void setSpecified(bool specified);

        TypeInfoImpl *schemaTypeInfo() const;

    protected:
        ElementImpl *m_ownerElement;

        DOMStringImpl *m_prefix;
        DOMStringImpl *m_value;

        NodeImpl::Id m_id;
        bool m_isId : 1;
    };

    // Mini version of AttrImpl internal to NamedAttrMapImpl.
    // Stores either the id and value of an attribute
    // (in the case of m_attrId != 0), or a pointer to an AttrImpl (if m_attrId == 0)
    // The latter case only happens when the Attr node is requested by some DOM
    // code or is an XML attribute.
    // In most cases the id and value is all we need to store, which is more
    // memory efficient.
    struct AttributeImpl
    {
        NodeImpl::Id id() const { return m_attrId ? m_attrId : m_data.attr->id(); }
        DOMStringImpl *value() const { return m_attrId ? m_data.value : m_data.attr->val(); }
        AttrImpl *attr() const { return m_attrId ? 0 : m_data.attr; }
        DOMStringImpl *namespaceURI() { return m_attrId ? 0 : m_data.attr->namespaceURI(); }
        DOMStringImpl *prefix() { return m_attrId ? 0 : m_data.attr->prefix(); }
        DOMStringImpl *localName() { return m_attrId ? 0 : m_data.attr->localName(); }
        DOMStringImpl *name() { return m_attrId ? 0 : m_data.attr->name(); }

        void setValue(DOMStringImpl *value, ElementImpl *element);
        AttrImpl *createAttr(ElementImpl *element);
        void free();

        NodeImpl::Id m_attrId;
        union
        {
            DOMStringImpl *value;
            AttrImpl *attr;
        } m_data;
    };
};

#endif

// vim:ts=4:noet
