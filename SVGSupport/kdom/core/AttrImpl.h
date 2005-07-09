/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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
		AttrImpl(DocumentImpl *doc, NodeImpl::Id id);
		AttrImpl(DocumentImpl *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified = false);
		AttrImpl(DocumentImpl *doc, NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *prefix, bool nullNSSpecified = false);
		virtual ~AttrImpl();

		// 'Attr' functions
		DOMString name() const;
		DOMString value() const;
		virtual void setValue(const DOMString &value);

		virtual DOMString nodeName() const;
		virtual DOMString localName() const;

		virtual DOMString nodeValue() const;
		virtual void setNodeValue(const DOMString &nodeValue);

		virtual unsigned short nodeType() const; 

		virtual DOMString namespaceURI() const;

		virtual DOMString prefix() const;
		virtual void setPrefix(const DOMString &prefix);

		virtual bool specified() const;
		virtual bool isId() const; // DOM3
		virtual void setIsId(bool isId); // DOM3

		virtual ElementImpl *ownerElement() const;

		// Internal
		virtual bool isReadOnly() const;

		virtual bool childAllowed(NodeImpl *newChild);
		virtual bool childTypeAllowed(unsigned short type) const;

		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

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
		DOMStringImpl *val() const { return m_attrId ? m_data.value : m_data.attr->val(); }
		DOMString value() const { return DOMString(val()); }
		AttrImpl *attr() const { return m_attrId ? 0 : m_data.attr; }
		DOMString namespaceURI() { return m_attrId ? DOMString() : m_data.attr->namespaceURI(); }
		DOMString prefix() { return m_attrId ? DOMString() : m_data.attr->prefix(); }
		DOMString localName() { return m_attrId ? DOMString() : m_data.attr->localName(); }
		DOMString name() { return m_attrId ? DOMString() : m_data.attr->name(); }

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
