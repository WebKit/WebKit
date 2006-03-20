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
#include <kcanvas/KCanvasResources.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGFEComponentTransferElement.h"
#include "SVGFEFuncRElement.h"
#include "SVGFEFuncGElement.h"
#include "SVGFEFuncBElement.h"
#include "SVGFEFuncAElement.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGFEComponentTransferElement::SVGFEComponentTransferElement(const QualifiedName& tagName, Document *doc) : 
SVGFilterPrimitiveStandardAttributes(tagName, doc)
{
    m_filterEffect = 0;
}

SVGFEComponentTransferElement::~SVGFEComponentTransferElement()
{
    delete m_filterEffect;
}

SVGAnimatedString *SVGFEComponentTransferElement::in1() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedString>(m_in1, dummy);
}

void SVGFEComponentTransferElement::parseMappedAttribute(MappedAttribute *attr)
{
    String value(attr->value());
    if (attr->name() == SVGNames::inAttr)
        in1()->setBaseVal(value.impl());
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

KCanvasFEComponentTransfer *SVGFEComponentTransferElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEComponentTransfer *>(renderingDevice()->createFilterEffect(FE_COMPONENT_TRANSFER));
    if (!m_filterEffect)
        return 0;
    
    m_filterEffect->setIn(String(in1()->baseVal()).deprecatedString());
    setStandardAttributes(m_filterEffect);
    
    for (Node *n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feFuncRTag))
            m_filterEffect->setRedFunction(static_cast<SVGFEFuncRElement *>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncGTag))
            m_filterEffect->setGreenFunction(static_cast<SVGFEFuncGElement *>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncBTag))
            m_filterEffect->setBlueFunction(static_cast<SVGFEFuncBElement *>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncATag))
            m_filterEffect->setAlphaFunction(static_cast<SVGFEFuncAElement *>(n)->transferFunction());
    }

    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

