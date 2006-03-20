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
#include <DeprecatedStringList.h>

#include <kdom/core/Attr.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEMergeElement.h"
#include "SVGFEMergeNodeElement.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEMergeElement::SVGFEMergeElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEMergeElement::~SVGFEMergeElement()
{
    delete m_filterEffect;
}

KCanvasFEMerge *SVGFEMergeElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEMerge *>(renderingDevice()->createFilterEffect(FE_MERGE));
    if (!m_filterEffect)
        return 0;
    setStandardAttributes(m_filterEffect);

    DeprecatedStringList mergeInputs;
    for(Node *n = firstChild(); n != 0; n = n->nextSibling())
    {
        if(n->hasTagName(SVGNames::feMergeNodeTag))
        {
            String mergeInput = static_cast<SVGFEMergeNodeElement *>(n)->in1()->baseVal();
            mergeInputs.append(mergeInput.deprecatedString());
        }
    }

    m_filterEffect->setMergeInputs(mergeInputs);

    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

