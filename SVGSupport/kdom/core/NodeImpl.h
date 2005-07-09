/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
                  2004 Rob Buis           <buis@kde.org>

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

#ifndef KDOM_NodeImpl_H
#define KDOM_NodeImpl_H

#include <kdom/Element.h>
#include <kdom/Document.h>
#include <kdom/DocumentType.h>

#include <kdom/impl/DOMExceptionImpl.h>
#include <kdom/events/impl/EventTargetImpl.h>

#ifdef _DOM_NodeImpl_h_
#warning KDOM and DOM in same file!
#endif

#define NodeImpl_IdNSMask    0xffff0000
#define NodeImpl_IdLocalMask 0x0000ffff

// Helpers
const Q_UINT16 noNamespace = 0;
const Q_UINT16 anyNamespace = 0xffff;
const Q_UINT16 xhtmlNamespace = 1;
const Q_UINT16 anyLocalName = 0xffff;

inline Q_UINT16 localNamePart(Q_UINT32 id) { return id & NodeImpl_IdLocalMask; }
inline Q_UINT16 namespacePart(Q_UINT32 id) { return (((unsigned int)id) & NodeImpl_IdNSMask) >> 16; }
inline Q_UINT32 makeId(Q_UINT16 n, Q_UINT16 l) { return (n << 16) | l; }

const Q_UINT32 anyQName = makeId(anyNamespace, anyLocalName);

namespace KDOM
{
	class RenderStyle;
	class ElementImpl;
	class NodeListImpl;
	class DocumentImpl;
	class NamedAttrMapImpl;

	// Any layer-on-top, which offers any rendering functionality
	// needs to reimplement that (for instance. khtml2/ksvg2)
	class RenderObject { };

	class NodeImpl : public EventTargetImpl
	{
	public:
		NodeImpl(DocumentImpl *i);
		virtual ~NodeImpl();

		// Virtual functions
		virtual DOMString nodeName() const;

		virtual DOMString nodeValue() const;
		virtual void setNodeValue(const DOMString &nodeValue);

		virtual unsigned short nodeType() const; 

		virtual NamedAttrMapImpl *attributes(bool readonly = false) const;

		virtual NodeListImpl *childNodes() const;

		// Function  virtual unsigned long childNodeCount()  has been removed in favor
		// of childNodes()->length();
		// Function  virtual NodeImpl *childNode(unsigned long index)  has also been removed
		// in favor of childNodes()->item(index);

		NodeImpl *parentNode() const;
		NodeImpl *previousSibling() const;
		NodeImpl *nextSibling() const;

		virtual NodeImpl *firstChild() const;
		virtual NodeImpl *lastChild() const;

		virtual KDOM::DocumentImpl *ownerDocument() const;
		void setOwnerDocument(DocumentImpl *doc);

		virtual NodeImpl *insertBefore(NodeImpl *newChild, NodeImpl *refChild);
		virtual NodeImpl *replaceChild(NodeImpl *newChild, NodeImpl *oldChild);
		virtual NodeImpl *removeChild(NodeImpl *oldChild);
		virtual NodeImpl *appendChild(NodeImpl *newChild);

		virtual bool hasChildNodes() const;

		/**
		 * FIXME Why was 'doc' added?
		 */
		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc ) const;
		NodeImpl *cloneNode(bool deep) const;

		virtual void normalize();

		virtual DOMString namespaceURI() const;

		virtual DOMString prefix() const;
		virtual void setPrefix(const DOMString &prefix);

		virtual DOMString localName() const;

		virtual bool hasAttributes() const;

		virtual bool isSupported(const DOMString &feature, const DOMString &version) const;

		unsigned short compareDocumentPosition(NodeImpl *other) const; // DOM3

		virtual DOMString textContent() const; // DOM3
		void setTextContent(const DOMString &text); // DOM3

		bool isDefaultNamespace(const DOMString &namespaceURI) const; // DOM3
		bool isSameNode(NodeImpl *other) const; // DOM3
		bool isEqualNode(NodeImpl *arg) const; // DOM3

		DOMString lookupNamespaceURI(const DOMString &prefix) const; // DOM3
		DOMString lookupPrefix(const DOMString &namespaceURI) const; // DOM3

		// Event dispatching helpers
		void dispatchSubtreeModifiedEvent();
		void dispatchChildRemovalEvents(NodeImpl *child);
		void dispatchChildInsertedEvents(NodeImpl *child);
		void dispatchEventToSubTree(NodeImpl *node, EventImpl *event);

		// Internal
		virtual bool isReadOnly() const;

		virtual bool childAllowed(NodeImpl *newChild);
		virtual bool childTypeAllowed(unsigned short type) const;

		void setPreviousSibling(NodeImpl *previous);
		void setNextSibling(NodeImpl *next);

		DOMString toString() const;

		/**
		 * Count the number of previousSibling calls needed to find the start.
		 *
		 * @return The number of siblings before it.
		 */
		unsigned long nodeIndex() const;

		DOMString baseURI() const; // DOM3
		KURL baseKURI() const;

		// -----------------------------------------------------------------------------
		// Notification of document stucture changes

		/**
		 * Notifies the node that it has been inserted into the document.
		 * This is called during document parsing, and also
		 * when a node is added through the DOM methods insertBefore(),
		 * appendChild() or replaceChild(). Note that this only
		 * happens when the node becomes part of the document tree, i.e. only
		 * when the document is actually an ancestor of the node. The
		 * call happens _after_ the node has been added to the tree.
		 *
		 * This is similar to the DOMNodeInsertedIntoDocument DOM event, but
		 * does not require the overhead of event dispatching.
		 */
		virtual void insertedIntoDocument();

		/**
		 * Notifies the node that it is no longer part of the document tree, i.e.
		 * when the document is no longer an ancestor node.
		 *
		 * This is similar to the DOMNodeRemovedFromDocument DOM event, but does
		 * not require the overhead of event dispatching, and is called _after_
		 * the node is removed from the tree.
		 */
		virtual void removedFromDocument();

		/**
		 * Notifies the node that its list of children have changed (either by adding
		 * or removingchild nodes), or a child node that is of the type
		 * CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
		 */
		virtual void childrenChanged();

		virtual NodeImpl *addChild(NodeImpl *newChild);

		typedef Q_UINT32 Id;

		virtual Id id() const { return 0; }
		Id localId() const;

		enum IdType
		{
			AttributeId,
			ElementId,
			NamespaceId
		};

		NodeImpl *traverseNextNode(NodeImpl *stayWithin = 0) const;

		virtual void attach();
		virtual void detach();

		/**
		 * Notifies the node that no more children will be added.
		 */
		virtual void close();

		enum StyleChange { NoChange, NoInherit, Inherit, Force };
		virtual void recalcStyle(StyleChange = NoChange) { }

		StyleChange diff(RenderStyle *s1, RenderStyle *s2) const;

		// Rendering logic
		RenderObject *renderer() const { return m_render; }
		RenderObject *previousRenderer();
		RenderObject *nextRenderer();
		void setRenderer(RenderObject *o) { m_render = o; }

		// for <link> and <style>...
		virtual void sheetLoaded() { }

		// Helpers
		bool closed() const { return m_closed; }
		bool attached() const { return m_attached; }
		bool hasAnchor() const  { return m_hasAnchor; }

		bool focused() const { return m_focused; }
		virtual void setFocus(bool b = true) { m_focused = b; }

		bool active() const { return m_active; }
		virtual void setActive(bool b = true) { m_active = b; }

		bool changed() const { return m_changed; }
		virtual void setChanged(bool b = true, bool deep = false);

		bool hasChangedChild() const { return m_hasChangedChild; }
		void setHasChangedChild(bool b = true) { m_hasChangedChild = b; }

		bool hasId() const { return m_hasId; }
		void setHasId(bool b = true) { m_hasId = b; }

		bool hasStyle() const { return m_hasStyle; }
		void setHasStyle(bool b = true) { m_hasStyle = b; }

		bool inDocument() const { return m_inDocument; }
		void setInDocument(bool b = true) { m_inDocument = b; }

	protected:
		// Helper methods (They will eventually throw exceptions!)
		bool isAncestor(NodeImpl *current, NodeImpl *other);

		DOMString lookupNamespacePrefix(DOMString namespaceURI, const ElementImpl *originalElement) const;

	public:
		NodeImpl *next, *prev;

	protected:
		DocumentImpl *document;
		RenderObject *m_render;

		// TODO: pad to 32 bits soon...
		bool m_hasId : 1;
		bool m_hasStyle : 1;
		bool m_attached : 1;
		bool m_closed : 1;
		bool m_changed : 1;
		bool m_hasChangedChild : 1;
		bool m_inDocument : 1;
		bool m_hasAnchor : 1;

		bool m_specified : 1;
		bool m_focused : 1;
		bool m_active : 1;

		bool m_dom2 : 1;
		bool m_nullNSSpecified : 1;
	};

	class NodeBaseImpl : public NodeImpl
	{
	public:
		NodeBaseImpl(DocumentImpl *i);
		virtual ~NodeBaseImpl();

		// Virtual functions overridden from 'NodeImpl'
		virtual NodeImpl *firstChild() const;
		virtual NodeImpl *lastChild() const;
		virtual NodeImpl *insertBefore(NodeImpl *newChild, NodeImpl *refChild);
		virtual NodeImpl *replaceChild(NodeImpl *newChild, NodeImpl *oldChild);
		virtual NodeImpl *removeChild(NodeImpl *oldChild);
		virtual NodeImpl *appendChild(NodeImpl *newChild);
		virtual bool hasChildNodes() const;

		virtual NodeImpl *addChild(NodeImpl *newChild);
		virtual void attach();
		virtual void detach();

		// Internal
		void cloneChildNodes(NodeImpl *clone, DocumentImpl *doc) const;

		void removeChildren();

	protected:
		// Helper methods (They will eventually throw exceptions!)
		void checkAddChild(NodeImpl *current, NodeImpl *newChild);
		void checkDocumentAddChild(NodeImpl *current, NodeImpl *newChild);

	protected:
		NodeImpl *first, *last;
	};
};

#endif

// vim:ts=4:noet
