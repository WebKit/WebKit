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

#include <kurl.h>
#include <kdebug.h>

#include <kdom/Helper.h>
#include <kdom/Namespace.h>

#include <kcanvas/KCanvasView.h>

#include "ksvg.h"
#include "svgtags.h"
#include <ksvg2/KSVGView.h>
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "KSVGDocumentBuilder.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

DocumentBuilder::DocumentBuilder(KSVGView *view) : KDOM::DocumentBuilder()
{
	m_view = view;
}

DocumentBuilder::~DocumentBuilder()
{
}

bool DocumentBuilder::startDocument(const KURL &uri)
{
	kdDebug(26001) << "KSVG::DocumentBuilder::startDocument, uri = " << uri.prettyURL() << endl;

	SVGDOMImplementationImpl *factory = SVGDOMImplementationImpl::self();
	SVGDocumentImpl *docImpl = static_cast<SVGDocumentImpl *>(factory->createDocument(KDOM::NS_SVG.handle(), KDOM::DOMString("svg:svg").handle(),
															  factory->defaultDocumentType(), false, m_view));

	if(!docImpl)
		return false;

	DocumentBuilder::linkDocumentToCanvas(docImpl, m_view);

	setDocument(docImpl); // Register in kdom...
	return KDOM::DocumentBuilder::startDocument(uri);
}
#if 0
void DocumentBuilder::linkDocumentToCanvas(const SVGDocument &doc, KSVGView *view)
{
	SVGDocumentImpl *docImpl = static_cast<SVGDocumentImpl *>(doc.handle());
	if(!docImpl)
		return;

	DocumentBuilder::linkDocumentToCanvas(docImpl, view);
}
#endif
void DocumentBuilder::linkDocumentToCanvas(SVGDocumentImpl *docImpl, KSVGView *view)
{
	docImpl->ref();

	docImpl->setCanvasView((view ? view->canvasView() : 0));
	docImpl->attach();
}

void DocumentBuilder::finishedDocument(SVGDocumentImpl *docImpl)
{
	if(docImpl)
		docImpl->finishedParsing();
}

bool DocumentBuilder::endDocument()
{
	kdDebug(26001) << "KSVG::DocumentBuilder::endDocument" << endl;

	KDOM::DocumentBuilder::endDocument();
	DocumentBuilder::finishedDocument(static_cast<SVGDocumentImpl *>(document()));

	return true;
}

// vim:ts=4:noet
