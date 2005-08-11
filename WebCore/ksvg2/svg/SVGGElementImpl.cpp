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

#include "SVGGElementImpl.h"
#include "SVGTransformableImpl.h"

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace KSVG;

SVGGElementImpl::SVGGElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
}

SVGGElementImpl::~SVGGElementImpl()
{
}

void SVGGElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	if(SVGTestsImpl::parseAttribute(attr)) return;
	if(SVGLangSpaceImpl::parseAttribute(attr)) return;
	if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
	if(SVGTransformableImpl::parseAttribute(attr)) return;

	SVGStyledElementImpl::parseAttribute(attr);
}

void SVGGElementImpl::setChanged(bool b, bool)
{
	// FIXME: this is waaay to slow & wrong!
	KDOM::NodeImpl::setChanged(b, true);
	//KDOM::NodeImpl::setChanged(b, false);
}

KCanvasItem *SVGGElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	return KCanvasCreator::self()->createContainer(canvas, style);
}

// Helper class for <use> support
SVGDummyElementImpl::SVGDummyElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id, const KDOM::DOMString &prefix) : SVGGElementImpl(doc, 0, prefix)
{
}

SVGDummyElementImpl::~SVGDummyElementImpl()
{
}

KDOM::DOMString SVGDummyElementImpl::localName() const
{
	return "dummy";
}

// vim:ts=4:noet
