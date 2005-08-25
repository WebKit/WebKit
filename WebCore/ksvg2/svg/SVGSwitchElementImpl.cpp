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

#include <kdom/impl/AttrImpl.h>

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGTestsImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSwitchElementImpl.h"
#include "SVGAnimatedLengthImpl.h"

using namespace KSVG;

SVGSwitchElementImpl::SVGSwitchElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGStyledElementImpl(doc, id, prefix), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl()
{
}

SVGSwitchElementImpl::~SVGSwitchElementImpl()
{
}

bool SVGSwitchElementImpl::allowAttachChildren(KDOM::ElementImpl *child) const
{
	for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
	{
		SVGTestsImpl *element = dynamic_cast<SVGTestsImpl *>(n);

		// Lookup first valid item...
		if(element && element->isValid())
		{
			// Only render first valid child!
			if(n == child)
				return true;

			return false;
		}
	}

	return false;
}

KCanvasItem *SVGSwitchElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
	return KCanvasCreator::self()->createContainer(canvas, style);
}

// vim:ts=4:noet
