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

#include "config.h"
#if SVG_SUPPORT
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEMergeElementImpl.h"
#include "SVGFEMergeNodeElementImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEMergeElementImpl::SVGFEMergeElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFilterPrimitiveStandardAttributesImpl(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEMergeElementImpl::~SVGFEMergeElementImpl()
{
    delete m_filterEffect;
}

KCanvasFEMerge *SVGFEMergeElementImpl::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEMerge *>(QPainter::renderingDevice()->createFilterEffect(FE_MERGE));
    if (!m_filterEffect)
        return 0;
    setStandardAttributes(m_filterEffect);

    QStringList mergeInputs;
    for(KDOM::NodeImpl *n = firstChild(); n != 0; n = n->nextSibling())
    {
        if(n->hasTagName(SVGNames::feMergeNodeTag))
        {
            KDOM::DOMString mergeInput = static_cast<SVGFEMergeNodeElementImpl *>(n)->in1()->baseVal();
            mergeInputs.append(mergeInput.qstring());
        }
    }

    m_filterEffect->setMergeInputs(mergeInputs);

    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

