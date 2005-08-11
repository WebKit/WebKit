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

#include "Text.h"
#include "Attr.h"
#include "Ecma.h"
#include "Event.h"
#include <kdom/Helper.h>
#include "Entity.h"
#include "Element.h"
#include "Comment.h"
#include "Document.h"
#include "Notation.h"
#include "NodeList.h"
#include "DOMObject.h"
#include "XPathResult.h"
#include "NodeListImpl.h"
#include "DocumentImpl.h"
#include "DOMException.h"
#include "AbstractView.h"
#include "DocumentType.h"
#include "CDATASection.h"
#include "XPathNSResolver.h"
#include "XPathExpression.h"
#include "EntityReference.h"
#include "DocumentFragment.h"
#include "DOMConfiguration.h"
#include "DOMImplementation.h"
#include "XPathEvaluatorImpl.h"
#include "DOMImplementationImpl.h"
#include "ProcessingInstruction.h"
#include "XPointerEvaluatorImpl.h"

#include "DOMConstants.h"
#include "Document.lut.h"

using namespace KDOM::XPointer;

namespace KDOM
{
    using namespace KJS;
/*
@begin Document::s_hashTable 11
 doctype				DocumentConstants::Doctype						DontDelete|ReadOnly
 implementation			DocumentConstants::Implementation				DontDelete|ReadOnly
 documentElement		DocumentConstants::DocumentElement				DontDelete|ReadOnly
 inputEncoding			DocumentConstants::InputEncoding				DontDelete|ReadOnly
 xmlEncoding			DocumentConstants::XmlEncoding		 			DontDelete|ReadOnly
 xmlStandalone			DocumentConstants::XmlStandalone	 			DontDelete
 xmlVersion				DocumentConstants::XmlVersion		 			DontDelete
 strictErrorChecking	DocumentConstants::StrictErrorChecking 			DontDelete
 documentURI			DocumentConstants::DocumentURI		 			DontDelete
 domConfig				DocumentConstants::DomConfig		 			DontDelete|ReadOnly
@end
@begin DocumentProto::s_hashTable 17
 createElement					DocumentConstants::CreateElement				DontDelete|Function 1
 createDocumentFragment			DocumentConstants::CreateDocumentFragment		DontDelete|Function 0
 createTextNode					DocumentConstants::CreateTextNode				DontDelete|Function 1
 createComment					DocumentConstants::CreateComment				DontDelete|Function 1
 createCDATASection				DocumentConstants::CreateCDATASection			DontDelete|Function 1
 createProcessingInstruction	DocumentConstants::CreateProcessingInstruction	DontDelete|Function 2
 createAttribute				DocumentConstants::CreateAttribute				DontDelete|Function 1
 createEntityReference			DocumentConstants::CreateEntityReference		DontDelete|Function 1
 getElementsByTagName			DocumentConstants::GetElementsByTagName			DontDelete|Function 1
 importNode						DocumentConstants::ImportNode					DontDelete|Function 2
 normalizeDocument				DocumentConstants::NormalizeDocument			DontDelete|Function 0
 createElementNS				DocumentConstants::CreateElementNS				DontDelete|Function 2
 createAttributeNS				DocumentConstants::CreateAttributeNS			DontDelete|Function 2
 getElementsByTagNameNS			DocumentConstants::GetElementsByTagNameNS		DontDelete|Function 2
 getElementById					DocumentConstants::GetElementById				DontDelete|Function 1
 adoptNode						DocumentConstants::AdoptNode					DontDelete|Function 1
 renameNode						DocumentConstants::RenameNode					DontDelete|Function 3
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Document", DocumentProto, DocumentProtoFunc)

ValueImp *Document::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DocumentConstants::Doctype:
			return safe_cache<DocumentType>(exec, doctype());
		case DocumentConstants::Implementation:
			return implementation().cache(exec); // Can't be 'null'
		case DocumentConstants::DocumentElement:
			return getDOMNode(exec, documentElement());
		case DocumentConstants::InputEncoding:
			return getDOMString(inputEncoding());
		case DocumentConstants::XmlEncoding:
			return getDOMString(xmlEncoding());
		case DocumentConstants::XmlStandalone:
			return KJS::Boolean(xmlStandalone());
		case DocumentConstants::XmlVersion:
			return getDOMString(xmlVersion());
		case DocumentConstants::StrictErrorChecking:
			return KJS::Boolean(strictErrorChecking());
		case DocumentConstants::DocumentURI:
			return getDOMString(documentURI());
		case DocumentConstants::DomConfig:
			return safe_cache<DOMConfiguration>(exec, domConfig());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
};

void Document::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case DocumentConstants::XmlStandalone:
			setXmlStandalone(value->toBoolean(exec));
			break;
		case DocumentConstants::XmlVersion:
			setXmlVersion(toDOMString(exec, value));
			break;
		case DocumentConstants::StrictErrorChecking:
			setStrictErrorChecking(value->toBoolean(exec));
			break;
		case DocumentConstants::DocumentURI:
			setDocumentURI(toDOMString(exec, value));
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *DocumentProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	KDOM_CHECK_THIS(Document)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case DocumentConstants::CreateElement:
		{
			DOMString tagName = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createElement(tagName));
		}
		case DocumentConstants::CreateDocumentFragment:
			return getDOMNode(exec, obj.createDocumentFragment());
		case DocumentConstants::CreateTextNode:
		{
			DOMString data = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createTextNode(data));
		}
		case DocumentConstants::CreateComment:
		{
			DOMString data = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createComment(data));
		}
		case DocumentConstants::CreateCDATASection:
		{
			DOMString data = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createCDATASection(data));
		}
		case DocumentConstants::CreateProcessingInstruction:
		{
			DOMString target = toDOMString(exec, args[0]);
			DOMString data = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.createProcessingInstruction(target, data));
		}
		case DocumentConstants::CreateAttribute:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createAttribute(name));
		}
		case DocumentConstants::CreateEntityReference:
		{
			DOMString name = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.createEntityReference(name));
		}
		case DocumentConstants::GetElementsByTagName:
		{
			DOMString tagName = toDOMString(exec, args[0]);
			return safe_cache<NodeList>(exec, obj.getElementsByTagName(tagName));
		}
		case DocumentConstants::ImportNode:
		{
			Node importedNode = ecma_cast<Node>(exec, args[0], &toNode);
			bool deep = args[1]->toBoolean(exec);
			return getDOMNode(exec, obj.importNode(importedNode, deep));
		}
		case DocumentConstants::NormalizeDocument:
		{
			obj.normalizeDocument();
			return Undefined();
		}
		case DocumentConstants::CreateElementNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString qualifiedName = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.createElementNS(namespaceURI, qualifiedName));
		}
		case DocumentConstants::CreateAttributeNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString qualifiedName = toDOMString(exec, args[1]);
			return getDOMNode(exec, obj.createAttributeNS(namespaceURI, qualifiedName));
		}
		case DocumentConstants::GetElementsByTagNameNS:
		{
			DOMString namespaceURI = toDOMString(exec, args[0]);
			DOMString localName = toDOMString(exec, args[1]);
			return safe_cache<NodeList>(exec, obj.getElementsByTagNameNS(namespaceURI, localName));
		}
		case DocumentConstants::GetElementById:
		{
			DOMString elementId = toDOMString(exec, args[0]);
			return getDOMNode(exec, obj.getElementById(elementId));
		}
		case DocumentConstants::AdoptNode:
		{
			Node source = ecma_cast<Node>(exec, args[0], &toNode);
			return getDOMNode(exec, obj.adoptNode(source));
		}
		case DocumentConstants::RenameNode:
		{
			Node n = ecma_cast<Node>(exec, args[0], &toNode);
			DOMString namespaceURI = toDOMString(exec, args[1]);
			DOMString qualifiedName = toDOMString(exec, args[2]);
			return getDOMNode(exec, obj.renameNode(n, namespaceURI, qualifiedName));
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<DocumentImpl *>(Node::d))

Document Document::null;

Document::Document()
: Node(), DocumentView(), DocumentEvent(), DocumentStyle(),
  DocumentTraversal(), DocumentRange(),
  XPointerEvaluator(), XPathEvaluator()
{
	// A document with d = 0, won't be able to create any elements!
}

Document::Document(DocumentImpl *i)
: Node(i), DocumentView(i), DocumentEvent(i), DocumentStyle(i),
  DocumentTraversal(i), DocumentRange(i), XPointerEvaluator(i), XPathEvaluator(i)
{
}

Document::Document(const Document &other)
: Node(), DocumentView(), DocumentEvent(), DocumentStyle(),
  DocumentTraversal(), DocumentRange(), XPointerEvaluator(), XPathEvaluator()
{
	(*this) = other;
}

Document::Document(const Node &other)
: Node(), DocumentView(), DocumentEvent(), DocumentStyle(),
  DocumentTraversal(), DocumentRange(), XPointerEvaluator(), XPathEvaluator()
{
	(*this) = other;
}

Document::~Document()
{
}

Document &Document::operator=(const Document &other)
{
	Node::operator=(other);
	DocumentView::operator=(other);
	DocumentEvent::operator=(other);
	DocumentStyle::operator=(other);
	DocumentTraversal::operator=(other);
	DocumentRange::operator=(other);
	XPointerEvaluator::operator=(other);
	XPathEvaluator::operator=(other);
	return *this;
}

Document &Document::operator=(const Node &other)
{
	DocumentImpl *ohandle = static_cast<DocumentImpl *>(other.handle());
	if(impl != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != DOCUMENT_NODE)
		{
			if(impl)
				impl->deref();
			// Laurent gcc-3.4.1 "Document.cc:262: error: ISO C++ forbids cast to non-reference type used as lvalue"
			//impl = 0;
			//#define impl (static_cast<DocumentImpl *>(Node::d))
			//fix compile.
			Node::d = 0;
		}
		else
		{
			Node::operator=(other);
			DocumentView::operator=(static_cast<DocumentViewImpl *>(ohandle));
			DocumentEvent::operator=(static_cast<DocumentEventImpl *>(ohandle));
			DocumentStyle::operator=(static_cast<DocumentStyleImpl *>(ohandle));
			DocumentTraversal::operator=(static_cast<DocumentTraversalImpl *>(ohandle));
			DocumentRange::operator=(static_cast<DocumentRangeImpl *>(ohandle));
			XPointerEvaluator::operator=(static_cast<XPointerEvaluatorImpl *>(ohandle));
			XPathEvaluator::operator=(static_cast<XPathEvaluatorImpl *>(ohandle));
		}
	}

	return *this;
}

DocumentType Document::doctype() const
{
	if(!impl)
		return DocumentType::null;

	return DocumentType(impl->doctype());
}

DOMImplementation Document::implementation() const
{
	if(!impl)
		return DOMImplementation::null;

	return DOMImplementation(impl->implementation());
}

Element Document::documentElement() const
{
	if(!impl)
		return Element::null;

	return Element(impl->documentElement());
}

Element Document::createElement(const DOMString &tagName)
{
	if(!impl)
		return Element::null;

	return Element(impl->createElement(tagName));
}

Element Document::createElementNS(const DOMString &namespaceURI, const DOMString &qualifiedName)
{
	if(!impl)
		return Element::null;

	return Element(impl->createElementNS(namespaceURI, qualifiedName));
}

DocumentFragment Document::createDocumentFragment()
{
	if(!impl)
		return DocumentFragment::null;

	return DocumentFragment(impl->createDocumentFragment());
}

Text Document::createTextNode(const DOMString &data)
{
	if(!impl)
		return Text::null;

	return Text(impl->createTextNode(data));
}

KDOM::Comment Document::createComment(const DOMString &data)
{
	if(!impl)
		return Comment::null;

	return Comment(impl->createComment(data));
}

CDATASection Document::createCDATASection(const DOMString &data)
{
	if(!impl)
		return CDATASection::null;

	return CDATASection(impl->createCDATASection(data));
}

ProcessingInstruction Document::createProcessingInstruction(const DOMString &target, const DOMString &data)
{
	if(!impl)
		return ProcessingInstruction::null;

	return ProcessingInstruction(impl->createProcessingInstruction(target, data));
}

Attr Document::createAttribute(const DOMString &name)
{
	if(!impl)
		return Attr::null;

	return Attr(impl->createAttribute(name));
}

Attr Document::createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName)
{
	if(!impl)
		return Attr::null;

	return Attr(impl->createAttributeNS(namespaceURI, qualifiedName));
}

EntityReference Document::createEntityReference(const DOMString &name)
{
	if(!impl)
		return EntityReference::null;

	return EntityReference(impl->createEntityReference(name));
}

NodeList Document::getElementsByTagName(const DOMString &tagName)
{
	if(!impl)
		return NodeList::null;

	return NodeList(impl->getElementsByTagName(tagName));
}

NodeList Document::getElementsByTagNameNS(const DOMString &namespaceURI, const DOMString &localName)
{
	if(!impl)
		return NodeList::null;

	return NodeList(impl->getElementsByTagNameNS(namespaceURI, localName));
}

Element Document::getElementById(const DOMString &elementId)
{
	if(!impl)
		return Element::null;

	return Element(impl->getElementById(elementId));
}

Node Document::importNode(const Node &importedNode, bool deep)
{
	if(!impl)
		return Node::null;

	return Node(impl->importNode(static_cast<NodeImpl *>(importedNode.handle()), deep));
}

void Document::normalizeDocument()
{
	if(impl)
		impl->normalizeDocument();
}

Node Document::renameNode(const Node &n, const DOMString &namespaceURI, const DOMString &qualifiedName)
{
	if(!impl)
		return Node::null;

	return Node(impl->renameNode(static_cast<NodeImpl *>(n.handle()), namespaceURI, qualifiedName));
}

DOMString Document::documentURI() const
{
	if(!impl)
		return DOMString();

	return impl->documentURI();
}

void Document::setDocumentURI(const DOMString &uri)
{
	if(impl)
		impl->setDocumentKURI(KURL(uri.string()));
}

void Document::setDocumentKURI(const KURL &url)
{
	if(impl)
		impl->setDocumentKURI(url);
}

KURL Document::documentKURI() const
{
	if(!impl)
		return KURL();

	return impl->documentKURI();
}

DOMConfiguration Document::domConfig() const
{
	if(!impl)
		return DOMConfiguration::null;

	return DOMConfiguration(impl->domConfig());
}

DOMString Document::inputEncoding() const
{
	if(!impl)
		return DOMString();

	return impl->inputEncoding();
}

DOMString Document::xmlEncoding() const
{
	if(!impl)
		return DOMString();

	return impl->xmlEncoding();
}

bool Document::xmlStandalone() const
{
	if(!impl)
		return false;

	return impl->standalone();
}

void Document::setXmlStandalone(bool standalone) const
{
	if(impl)
		impl->setStandalone(standalone);
}

DOMString Document::xmlVersion() const
{
	if(!impl)
		return DOMString();

	return impl->xmlVersion();
}

void Document::setXmlVersion(const DOMString &xmlVersion)
{
	if(impl)
	{
		if(xmlVersion != "1.0" && xmlVersion != "1.1")
			throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

		impl->setXmlVersion(xmlVersion);
	}
}

bool Document::strictErrorChecking() const
{
	if(!impl)
		return false;

	return impl->strictErrorChecking();
}

void Document::setStrictErrorChecking(bool strictErrorChecking)
{
	if(impl)
		impl->setStrictErrorChecking(strictErrorChecking);
}

Node Document::adoptNode(const Node &source) const
{
	if(!impl)
		return Node::null;

	return Node(impl->adoptNode(static_cast<NodeImpl *>(source.handle())));
}

}

// vim:ts=4:noet
