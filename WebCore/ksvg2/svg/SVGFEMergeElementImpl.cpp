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

#include <qstringlist.h>

#include <kdom/impl/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasRegistry.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEMergeElementImpl.h"
#include "SVGFEMergeNodeElementImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEMergeElementImpl::SVGFEMergeElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix) : 
SVGFilterPrimitiveStandardAttributesImpl(doc, id, prefix)
{
	m_filterEffect = 0;
}

SVGFEMergeElementImpl::~SVGFEMergeElementImpl()
{
}

KCanvasItem *SVGFEMergeElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *) const
{
	m_filterEffect = static_cast<KCanvasFEMerge *>(canvas->renderingDevice()->createFilterEffect(FE_MERGE));
	setStandardAttributes(m_filterEffect);
	return 0;
}

KCanvasFilterEffect *SVGFEMergeElementImpl::filterEffect() const
{
	return m_filterEffect;
}

void SVGFEMergeElementImpl::close()
{
	QStringList mergeInputs;
	for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
	{
		if(n->id() == ID_FEMERGENODE)
		{
			SVGFEMergeNodeElementImpl *mergeNode = static_cast<SVGFEMergeNodeElementImpl *>(n);
			mergeInputs.append(KDOM::DOMString(mergeNode->in1()->baseVal()).string());
		}
	}

	if(m_filterEffect) // we may not be attached...
		m_filterEffect->setMergeInputs(mergeInputs);
}

// vim:ts=4:noet
