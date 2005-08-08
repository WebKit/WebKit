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

#include <kurl.h>
#include <qdict.h>

#include <kdom/Shared.h>
#include <kdom/Helper.h>
#include <kdom/DocumentType.h>
#include <kdom/events/Event.h>
#include <kdom/impl/DocumentImpl.h>

#include "Ecma.h"
#include "SVGElement.h"
#include "SVGSVGElement.h"
#include "SVGScriptElement.h"
#include "SVGDocument.h"
#include "SVGDocumentImpl.h"

#include "SVGConstants.h"
#include "SVGDocument.lut.h"
using namespace KSVG;

/*
@begin SVGDocument::s_hashTable 7
 title			SVGDocumentConstants::Title			DontDelete|ReadOnly
 referrer		SVGDocumentConstants::Referrer		DontDelete|ReadOnly
 domain			SVGDocumentConstants::Domain		DontDelete|ReadOnly
 URL			SVGDocumentConstants::URL			DontDelete|ReadOnly
 rootElement	SVGDocumentConstants::RootElement	DontDelete|ReadOnly
@end
*/

ValueImp *SVGDocument::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case SVGDocumentConstants::Title:
			return getDOMString(title());
		case SVGDocumentConstants::Referrer:
			return getDOMString(referrer());
		case SVGDocumentConstants::Domain:
			return getDOMString(domain());
		case SVGDocumentConstants::URL:
			return getDOMString(URL());
		case SVGDocumentConstants::RootElement:
			return getDOMNode(exec, rootElement());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGDocumentImpl *>(EventTarget::d))

SVGDocument SVGDocument::null;

SVGDocument::SVGDocument() : KDOM::Document()
{
	// A document with d = 0, won't be able to create any elements!
}

SVGDocument::SVGDocument(SVGDocumentImpl *i) : KDOM::Document(i)
{
}

SVGDocument::SVGDocument(const SVGDocument &other) : KDOM::Document()
{
	(*this) = other;
}

SVGDocument::SVGDocument(const KDOM::Node &other) : KDOM::Document()
{
	(*this) = other;
}

SVGDocument::~SVGDocument()
{
}

SVGDocument &SVGDocument::operator=(const SVGDocument &other)
{
	KDOM::Document::operator=(other);
	return *this;
}

SVGDocument &SVGDocument::operator=(const KDOM::Node &other)
{
	KDOM::NodeImpl *ohandle = static_cast<KDOM::NodeImpl *>(other.handle());
	if(impl != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::DOCUMENT_NODE)
		{
			if(impl)
				impl->deref();

			Node::d = 0;
		}
		else
			KDOM::Document::operator=(other);
	}

	return *this;
}

KDOM::DOMString SVGDocument::title() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->title();
}

KDOM::DOMString SVGDocument::referrer() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->referrer();
}

KDOM::DOMString SVGDocument::domain() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->domain();
}

KDOM::DOMString SVGDocument::URL() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->URL();
}

SVGSVGElement SVGDocument::rootElement() const
{
	if(!impl)
		return SVGSVGElement::null;

	return SVGSVGElement(impl->rootElement());
}

// vim:ts=4:noet
