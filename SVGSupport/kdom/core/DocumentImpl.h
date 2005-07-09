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

#ifndef KDOM_DocumentImpl_H
#define KDOM_DocumentImpl_H

#include <kurl.h>

#include <qdict.h>
#include <qintdict.h>
#include <qstringlist.h>

#include <kdom/impl/NodeImpl.h>
#include <kdom/cache/KDOMLoader.h>
#include <kdom/impl/DOMStringImpl.h>
#include <kdom/css/impl/DocumentStyleImpl.h>
#include <kdom/views/impl/DocumentViewImpl.h>
#include <kdom/range/impl/DocumentRangeImpl.h>
#include <kdom/events/impl/DocumentEventImpl.h>
#include <kdom/xpath/impl/XPathEvaluatorImpl.h>
#include <kdom/traversal/impl/DocumentTraversalImpl.h>
#include <kdom/xpointer/impl/XPointerEvaluatorImpl.h>

class QPaintDevice;
class QPaintDeviceMetrics;

namespace KDOM
{
	class KDOMView;
	class KDOMPart;
	class AttrImpl;
	class TextImpl;
	class ElementImpl;
	class CommentImpl;
	class CSSRuleImpl;
	class MouseEventImpl;
	class StyleSheetImpl;
	class DocumentLoader;
	class CSSStyleSelector;
	class DocumentTypeImpl;
	class CDATASectionImpl;
	class NodeIteratorImpl;
	class CSSStyleSheetImpl;
	class EntityReferenceImpl;
	class DocumentFragmentImpl;
	class DOMConfigurationImpl;
	class DOMImplementationImpl;
	class ProcessingInstructionImpl;
	typedef enum
	{
		KDOM_DOC_GENERIC,
		KDOM_DOC_XHTML,
		KDOM_DOC_SVG,
		KDOM_DOC_MATHML,
		KDOM_DOC_CDF
	} KDOMDocumentType;

	class DocumentImpl : public NodeBaseImpl,
						 public DocumentEventImpl,
						 public DocumentViewImpl,
						 public DocumentStyleImpl,
						 public DocumentTraversalImpl,
						 public DocumentRangeImpl,
						 public XPointer::XPointerEvaluatorImpl,
						 public XPathEvaluatorImpl
	{
	public:
		DocumentImpl(DOMImplementationImpl *i, KDOMView *view, int nrTags = -1, int nrAttrs = -1);
		virtual ~DocumentImpl();

		virtual DOMString nodeName() const;
		virtual unsigned short nodeType() const;
		virtual DOMString textContent() const; // DOM3

		DOMImplementationImpl *implementation() const;
		
		// 'Document' functions
		virtual DocumentTypeImpl *doctype() const;
		virtual ElementImpl *documentElement() const;
		virtual ElementImpl *createElement(const DOMString &tagName);
		virtual ElementImpl *createElementNS(const DOMString &namespaceURI, const DOMString &qualifiedName);

		virtual AttrImpl *createAttribute(const DOMString &name);
		virtual AttrImpl *createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName);

		virtual DocumentFragmentImpl *createDocumentFragment();
		virtual TextImpl *createTextNode(const DOMString &data);
		virtual CommentImpl *createComment(const DOMString &data);
		virtual CDATASectionImpl *createCDATASection(const DOMString &data);
		virtual EntityReferenceImpl *createEntityReference(const DOMString &data);
		virtual ProcessingInstructionImpl *createProcessingInstruction(const DOMString &target, const DOMString &data);

		virtual NodeListImpl *getElementsByTagName(const DOMString &name);
		virtual NodeListImpl *getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName);

		virtual NodeImpl *importNode(NodeImpl *importedNode, bool deep);
		virtual void normalizeDocument();
		
		virtual NodeImpl *renameNode(NodeImpl *n, const DOMString &namespaceURI, const DOMString &qualifiedName);
		virtual ElementImpl *getElementById(const DOMString &elementId);

		DOMString documentURI() const;

		KURL documentKURI() const;
		void setDocumentKURI(const KURL &url);

		bool standalone() const;
		void setStandalone(bool standalone);

		DOMString inputEncoding() const;
		void setInputEncoding(const DOMString &inputEncoding);

		DOMString xmlEncoding() const;
		void setXmlEncoding(const DOMString &encoding);

		DOMString xmlVersion() const;
		void setXmlVersion(const DOMString &version);

		bool strictErrorChecking() const;
		void setStrictErrorChecking(bool strictErrorChecking);

		DOMConfigurationImpl *domConfig() const;

		// Internal
		virtual bool childAllowed(NodeImpl *newChild);
		virtual bool childTypeAllowed(unsigned short type) const;

		virtual NodeImpl *adoptNode(NodeImpl *source) const;
		virtual NodeImpl *cloneNode(bool deep, DocumentImpl *doc) const;

		void setParsing(bool b) { m_parsingMode = b; }
		bool parsing() const { return m_parsingMode; }

		virtual NodeImpl::Id getId(NodeImpl::IdType type, DOMStringImpl *namspaceURI,  DOMStringImpl *prefix, DOMStringImpl *name, bool readonly) const;
		virtual NodeImpl::Id getId(NodeImpl::IdType type, DOMStringImpl *nodeName, bool readonly) const;
		virtual DOMString getName(NodeImpl::IdType type, NodeImpl::Id id) const;

		// Keep track of listener types
		int addListenerType(const DOMString &type);
		int removeListenerType(const DOMString &type);
		bool hasListenerType(const DOMString &type) const;

		void addListenerType(int eventId);
		void removeListenerType(int eventId);
		bool hasListenerType(int eventId) const;

		void attachNodeIterator(NodeIteratorImpl *ni);
		void detachNodeIterator(NodeIteratorImpl *ni);

		virtual Ecma *ecmaEngine() const;

		// Default element style sheet
		virtual CSSStyleSheetImpl *elementSheet() const;

		void setDocType(DocumentTypeImpl *doctype);

		virtual CSSStyleSheetImpl *createCSSStyleSheet(NodeImpl *parent, const DOMString &url) const;
		virtual CSSStyleSheetImpl *createCSSStyleSheet(CSSRuleImpl *ownerRule, const DOMString &url) const;
		
		// Style selecting
		CSSStyleSelector *styleSelector() { return m_styleSelector; }
		virtual void updateRendering();
		
		void updateStyleSelector();
		virtual void recalcStyleSelector();
		
		void styleSheetLoaded();
		bool haveStylesheetsLoaded() const;

		KDOMView *view() const { return m_view; }
		void setView(KDOMView *view) { m_view = view; }
		KDOMPart *part() const;

		QPaintDeviceMetrics *paintDeviceMetrics() { return m_paintDeviceMetrics; }
		QPaintDevice *paintDevice() const { return m_paintDevice; }
		void setPaintDevice(QPaintDevice *dev);

		// External fetching (images, scripts, stylesheets..)
		DocumentLoader *docLoader() { return m_docLoader; }

		virtual bool prepareMouseEvent(bool readonly, int x, int y, MouseEventImpl *event);

		/**
		 * Increments the number of pending sheets.  The <link> elements
		 * invoke this to add themselves to the loading list.
		 */
		void addPendingSheet() { m_pendingStylesheets++; }

		/**
		 * Returns true if the document has pending stylesheets
		 * loading.
		 */
		bool hasPendingSheets() const { return m_pendingStylesheets; }

		// Used by the css parsing engine
		bool usesDescendantRules() { return m_usesDescendantRules; }
		void setUsesDescendantRules(bool b) { m_usesDescendantRules = b; }

		QString userStyleSheet() const { return m_userSheet; }
		QString printStyleSheet() const { return m_printSheet; }

		void setUserStyleSheet(const QString &sheet);
		void setPrintStyleSheet(const QString &sheet);

		NodeImpl* hoverNode() const { return m_hoverNode; }
		void setHoverNode(NodeImpl *newHoverNode);
		NodeImpl *focusNode() const { return m_focusNode; }
		virtual void setFocusNode(NodeImpl *newFocusNode);

		// Updates for :target (CSS3 selector).
		void setCSSTarget(NodeImpl *n);
		NodeImpl *getCSSTarget() { return m_cssTarget; }
		
		/**
		 * Returns the type of this document - e.g. HTML, XHTML, MATHML etc
		 * If you inherit from this class make sure you set m_kdomDocType;
		 */
		KDOMDocumentType kdomDocumentType();


	protected:
		// Important function for users of this library
		// who want to create specialized Nodes with namespaces
		struct IdNameMapping
		{
			IdNameMapping(unsigned short start) : idStart(start), count(0) { }
			~IdNameMapping()
			{
				QIntDictIterator<DOMStringImpl> it(names);
				for(; it.current() ; ++it)
					(it.current())->deref();
			}

			unsigned short idStart;
			unsigned short count;
			QIntDict<DOMStringImpl> names;
			QDict<NodeImpl::Id> ids;
		};

		IdNameMapping *m_attrMap;
		IdNameMapping *m_elementMap;
		IdNameMapping *m_namespaceMap;

		QPtrList<NodeIteratorImpl> m_nodeIterators;

		virtual DOMString defaultNS() const;

		virtual StyleSheetImpl *checkForStyleSheet(NodeImpl *n, QString &title);
		virtual CSSStyleSelector *createStyleSelector(const QString &userSheet);

		NodeImpl *normalizeNode(NodeImpl *node);

	protected:
		DocumentLoader *m_docLoader;
		KDOMDocumentType m_kdomDocType;

		mutable Ecma *m_ecmaEngine;
		mutable CSSStyleSheetImpl *m_elementSheet;

		bool m_standalone : 1;
		bool m_strictErrorChecking : 1;
		bool m_usesDescendantRules : 1;
		bool m_parsingMode : 1;
		bool m_ignorePendingStylesheets : 1;

		int m_listenerTypes;

		KURL m_url;
		DOMString m_inputEncoding;
		DOMString m_xmlEncoding;
		DOMString m_xmlVersion;
		DOMImplementationImpl *m_implementation;
		mutable DOMConfigurationImpl *m_configuration;

		CSSStyleSelector *m_styleSelector;

		QString m_userSheet;
		QString m_printSheet;
		QStringList m_availableSheets;

		KDOMView *m_view;

		QPaintDevice *m_paintDevice;
		QPaintDeviceMetrics *m_paintDeviceMetrics;

		// Track the number of currently loading top-level stylesheets.  Sheets
		// loaded using the @import directive are not included in this count.
		// We use this count of pending sheets to detect when we can begin attaching
		// elements.
		int m_pendingStylesheets;

		NodeImpl *m_hoverNode;
		NodeImpl *m_focusNode;
		NodeImpl *m_cssTarget;
	};
};

#endif

// vim:ts=4:noet
