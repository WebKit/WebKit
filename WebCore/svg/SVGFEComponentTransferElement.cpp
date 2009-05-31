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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEComponentTransferElement.h"

#include "Attr.h"
#include "MappedAttribute.h"
#include "SVGFEFuncAElement.h"
#include "SVGFEFuncBElement.h"
#include "SVGFEFuncGElement.h"
#include "SVGFEFuncRElement.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEComponentTransferElement::SVGFEComponentTransferElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_in1(this, SVGNames::inAttr)
{
}

SVGFEComponentTransferElement::~SVGFEComponentTransferElement()
{
}

void SVGFEComponentTransferElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::inAttr)
        setIn1BaseValue(value);
    else
        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
}

bool SVGFEComponentTransferElement::build(SVGResourceFilter* filterResource)
{
    FilterEffect* input1 = filterResource->builder()->getEffectById(in1());
    
    if(!input1)
        return false;

    ComponentTransferFunction red;
    ComponentTransferFunction green;
    ComponentTransferFunction blue;
    ComponentTransferFunction alpha;
    
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feFuncRTag))
            red = static_cast<SVGFEFuncRElement*>(n)->transferFunction();
        else if (n->hasTagName(SVGNames::feFuncGTag))
            green = static_cast<SVGFEFuncGElement*>(n)->transferFunction();
        else if (n->hasTagName(SVGNames::feFuncBTag))
           blue = static_cast<SVGFEFuncBElement*>(n)->transferFunction();
        else if (n->hasTagName(SVGNames::feFuncATag))
            alpha = static_cast<SVGFEFuncAElement*>(n)->transferFunction();
    }
    
    RefPtr<FilterEffect> effect = FEComponentTransfer::create(input1, red, green, blue, alpha);
    filterResource->addFilterEffect(this, effect.release());
    
    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
