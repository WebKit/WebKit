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

#include <kstaticdeleter.h>

#include "kdomls.h"
#include <kdom/Helper.h>
#include "Namespace.h"
#include "KDOMCache.h"
#include "RenderStyle.h"
#include "ElementImpl.h"
#include "LSInputImpl.h"
#include "LSOutputImpl.h"
#include "LSParserImpl.h"
#include "DocumentImpl.h"
#include "CDFInterface.h"
#include "CSSStyleSheet.h"
#include "MediaListImpl.h"
#include "DocumentTypeImpl.h"
#include "kdom/css/impl/CSSStyleSelector.h"
#include "CSSStyleSheetImpl.h"
#include "LSSerializerImpl.h"
#include "DOMImplementationLS.h"
#include "DOMImplementationImpl.h"


using namespace KDOM;

static KStaticDeleter<DOMImplementationImpl> instanceDeleter;
DOMImplementationImpl *DOMImplementationImpl::s_instance = 0;

DOMImplementationImpl::DOMImplementationImpl()
{
	m_cdfInterface = 0;

	Cache::init();
}

DOMImplementationImpl::~DOMImplementationImpl()
{
	// clean up static data
	CSSStyleSelector::clear();
	RenderStyle::cleanup();

	delete m_cdfInterface;
}

DOMImplementationImpl *DOMImplementationImpl::self()
{
	if(!s_instance)
		s_instance = instanceDeleter.setObject(s_instance, new DOMImplementationImpl());

	return s_instance;
}

CDFInterface *DOMImplementationImpl::cdfInterface() const
{
	if(!m_cdfInterface)
		m_cdfInterface = createCDFInterface();
	
	return m_cdfInterface;
}

bool DOMImplementationImpl::hasFeature(const DOMString &feature, const DOMString &version) const
{
	// TODO: Update to the correct values!
	DOMString upFeature = feature.upper();
	if(upFeature[0] == '+')
		upFeature.remove(0, 1);
	if((upFeature == "XML" || upFeature == "CORE" ||
		upFeature == "EVENTS" || upFeature == "UIEVENTS" ||
		upFeature == "MOUSEEVENTS" || upFeature == "MUTATIONEVENTS" ||
		upFeature == "LS-ASYNC" || upFeature == "LS" || upFeature == "XPATH" ||
		upFeature == "RANGE" || upFeature == "TRAVERSAL") &&
	   (version == "1.0" || version == "2.0" || version == "3.0" ||
	    version.isEmpty()))
		return true;

	return false;
}

DocumentTypeImpl *DOMImplementationImpl::createDocumentType(const DOMString &qualifiedName, const DOMString &publicId, const DOMString &systemId) const
{
	// INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
	if(qualifiedName.isEmpty() || !Helper::ValidateAttributeName(qualifiedName.implementation()))
		throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

	// NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!)
	if(qualifiedName.isEmpty())
		throw new DOMExceptionImpl(NAMESPACE_ERR);

	// NAMESPACE_ERR: Raised if the qualifiedName is malformed.
	Helper::CheckMalformedQualifiedName(qualifiedName);

	return new DocumentTypeImpl(0, qualifiedName, publicId, systemId);
}

DocumentImpl *DOMImplementationImpl::createDocument(const DOMString &namespaceURI, const DOMString &qualifiedName, const DocumentType &doctype, bool createDocElement, KDOMView *view) const
{
	int temp;
	Helper::CheckQualifiedName(qualifiedName, namespaceURI, temp, 
							   true /*nameCanBeNull*/,
							   true /*nameCanBeEmpty, see #61650*/);

	DocumentTypeImpl *docTypeImpl = static_cast<DocumentTypeImpl *>(doctype.handle());

	// WRONG_DOCUMENT_ERR: Raised if docType has already been used with a different
	//					   document or was created from a different implementation.
	if(docTypeImpl != 0 && docTypeImpl->ownerDocument() != 0)
		throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

	DocumentImpl *doc = new DocumentImpl(const_cast<DOMImplementationImpl *>(this), 0 /* default: no view! */);
	if(docTypeImpl)
		doc->setDocType(docTypeImpl);

	/* isEmpty instead if isNull, for the reason given in #61650 */
	if(!qualifiedName.isEmpty() && !namespaceURI.isEmpty() && createDocElement)
		doc->appendChild(doc->createElementNS(namespaceURI, qualifiedName));

	return doc;
}

CSSStyleSheetImpl *DOMImplementationImpl::createCSSStyleSheet(const DOMString &title, const DOMString &media) const
{
	// TODO : check whether media is valid
	CSSStyleSheetImpl *parent = 0;
	CSSStyleSheetImpl *sheet = new CSSStyleSheetImpl(parent, DOMString());
	sheet->setTitle(title);
	sheet->setMedia(new MediaListImpl(sheet, media));
	return sheet;
}

LSParserImpl *DOMImplementationImpl::createLSParser(unsigned short mode, const DOMString &schemaType) const
{
	// NOT_SUPPORTED_ERR: Raised if the requested mode or schema type is not supported.
	if((mode == 0 || mode > MODE_ASYNCHRONOUS) ||
		!(schemaType == NS_SCHEMATYPE_DTD || schemaType == NS_SCHEMATYPE_WXS || schemaType.isEmpty()))
		throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

	LSParserImpl *ret = new LSParserImpl();
	ret->setASync(mode == MODE_ASYNCHRONOUS);
	return ret;
}

LSInputImpl *DOMImplementationImpl::createLSInput() const
{
	return new LSInputImpl();
}

LSOutputImpl *DOMImplementationImpl::createLSOutput() const
{
	return new LSOutputImpl();
}

LSSerializerImpl *DOMImplementationImpl::createLSSerializer() const
{
	return new LSSerializerImpl();
}

KDOM::DocumentType DOMImplementationImpl::defaultDocumentType() const
{
	return KDOM::DocumentType(createDocumentType("name", "public", "system"));
}

int DOMImplementationImpl::typeToId(const DOMString &type)
{
	if(type == "DOMFocusIn") return DOMFOCUSIN_EVENT;
	else if(type == "DOMFocusOut") return DOMFOCUSOUT_EVENT;
	else if(type == "DOMActivate") return DOMACTIVATE_EVENT;
	else if(type == "click") return CLICK_EVENT;
	else if(type == "mousedown") return MOUSEDOWN_EVENT;
	else if(type == "mouseup") return MOUSEUP_EVENT;
	else if(type == "mouseover") return MOUSEOVER_EVENT;
	else if(type == "mousemove") return MOUSEMOVE_EVENT;
	else if(type == "mouseout") return MOUSEOUT_EVENT;
	else if(type == "keydown") return KEYDOWN_EVENT;
	else if(type == "keyup") return KEYUP_EVENT;
	else if(type == "DOMSubtreeModified") return DOMSUBTREEMODIFIED_EVENT;
	else if(type == "DOMNodeInserted") return DOMNODEINSERTED_EVENT;
	else if(type == "DOMNodeRemoved") return DOMNODEREMOVED_EVENT;
	else if(type == "DOMNodeRemovedFromDocument") return DOMNODEREMOVEDFROMDOCUMENT_EVENT;
	else if(type == "DOMNodeInsertedIntoDocument") return DOMNODEINSERTEDINTODOCUMENT_EVENT;
	else if(type == "DOMAttrModified") return DOMATTRMODIFIED_EVENT;
	else if(type == "DOMCharacterDataModified")	return DOMCHARACTERDATAMODIFIED_EVENT;

	return UNKNOWN_EVENT;
}

DOMString DOMImplementationImpl::idToType(int id)
{
	switch(id)
	{
		case DOMFOCUSIN_EVENT: return "DOMFocusIn";
		case DOMFOCUSOUT_EVENT: return "DOMFocusOut";
		case DOMACTIVATE_EVENT: return "DOMActivate";
		case CLICK_EVENT: return "click";
		case MOUSEDOWN_EVENT: return "mousedown";
		case MOUSEUP_EVENT: return "mouseup";
		case MOUSEOVER_EVENT: return "mouseover";
		case MOUSEMOVE_EVENT: return "mousemove";
		case MOUSEOUT_EVENT: return "mouseout";
		case KEYDOWN_EVENT: return "keydown";
		case KEYUP_EVENT: return "keyup";
		case DOMSUBTREEMODIFIED_EVENT: return "DOMSubtreeModified";
		case DOMNODEINSERTED_EVENT: return "DOMNodeInserted";
		case DOMNODEREMOVED_EVENT: return "DOMNodeRemoved";
		case DOMNODEREMOVEDFROMDOCUMENT_EVENT: return "DOMNodeRemovedFromDocument";
		case DOMNODEINSERTEDINTODOCUMENT_EVENT: return "DOMNodeInsertedIntoDocument";
		case DOMATTRMODIFIED_EVENT: return "DOMAttrModified";
		case DOMCHARACTERDATAMODIFIED_EVENT: return "DOMCharacterDataModified";
		default: return DOMString();
	}
}

CDFInterface *DOMImplementationImpl::createCDFInterface() const
{
	return new KDOM::CDFInterface();
}

// vim:ts=4:noet
