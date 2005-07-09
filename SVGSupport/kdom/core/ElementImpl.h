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

#ifndef KDOM_ElementImpl_H
#define KDOM_ElementImpl_H

#include <kdom/impl/NodeImpl.h>

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
		ElementImpl(DocumentImpl *doc);
		ElementImpl(DocumentImpl *doc, const DOMString &prefix, bool nullNSSpecified = false);
		virtual ~ElementImpl();

		virtual DOMString nodeName() const;
		virtual unsigned short nodeType() const;
		virtual DOMString tagName() const = 0;

		virtual DOMString prefix() const;
		virtual void setPrefix(const DOMString &prefix);

		virtual bool hasAttributes() const;
		
		bool hasAttribute(const DOMString &name) const;
		bool hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;
		
		virtual NamedAttrMapImpl *attributes(bool readonly = false) const;

		DOMString getAttribute(NodeImpl::Id id, bool nsAware = 0, DOMStringImpl *qName = 0) const;
		DOMString getAttribute(const DOMString &name) const;
		DOMString getAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;
		
		void setAttribute(NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *qName);
		void setAttribute(DOMStringImpl *name, DOMStringImpl *value);
		
		void removeAttribute(DOMStringImpl *name);
		void removeAttributeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);
		
		AttrImpl *getAttributeNode(DOMStringImpl *name) const;
		AttrImpl *setAttributeNode(AttrImpl *newAttr);
		AttrImpl *removeAttributeNode(AttrImpl *oldAttr);
		
		void setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value);
		virtual AttrImpl *getAttributeNodeNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;
		virtual AttrImpl *setAttributeNodeNS(AttrImpl *newAttr);
		virtual NodeListImpl *getElementsByTagName(const DOMString &name) const;

		virtual NodeListImpl *getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName) const;

		virtual DOMString namespaceURI() const;

		void setIdAttribute(const DOMString &name, bool isId); // DOM3
		void setIdAttributeNS(const DOMString &namespaceURI, const DOMString &localName, bool isId); // DOM3
		void setIdAttributeNode(AttrImpl *idAttr, bool isId); // DOM3

		// Internal
		virtual bool childAllowed(NodeImpl *newChild);
		virtual bool childTypeAllowed(unsigned short type) const;

		virtual bool checkChild(unsigned short /* tagID */, unsigned short /* childID */) { return true; }

		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

		AttrImpl *getIdAttribute(const DOMString &name) const;

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

	private:
		void addDOMEventListener(Ecma *ecmaEngine, const DOMString &type, const DOMString &value);

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
