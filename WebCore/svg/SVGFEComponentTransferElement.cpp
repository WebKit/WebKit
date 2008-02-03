/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEComponentTransferElement.h"

#include "Attr.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGFEFuncRElement.h"
#include "SVGFEFuncGElement.h"
#include "SVGFEFuncBElement.h"
#include "SVGFEFuncAElement.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEComponentTransferElement::SVGFEComponentTransferElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_filterEffect(0)
{
}

SVGFEComponentTransferElement::~SVGFEComponentTransferElement()
{
    delete m_filterEffect;
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEComponentTransferElement, String, String, string, In1, in1, SVGNames::inAttr, m_in1)

void SVGFEComponentTransferElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

SVGFEComponentTransfer* SVGFEComponentTransferElement::filterEffect(SVGResourceFilter* filter) const
{
    if (!m_filterEffect)
        m_filterEffect = new SVGFEComponentTransfer(filter);
    
    m_filterEffect->setIn(in1());
    setStandardAttributes(m_filterEffect);
    
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feFuncRTag))
            m_filterEffect->setRedFunction(static_cast<SVGFEFuncRElement*>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncGTag))
            m_filterEffect->setGreenFunction(static_cast<SVGFEFuncGElement*>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncBTag))
            m_filterEffect->setBlueFunction(static_cast<SVGFEFuncBElement*>(n)->transferFunction());
        else if (n->hasTagName(SVGNames::feFuncATag))
            m_filterEffect->setAlphaFunction(static_cast<SVGFEFuncAElement*>(n)->transferFunction());
    }

    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
