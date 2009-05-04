/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG)
#include "SVGViewElement.h"

#include "Attr.h"
#include "MappedAttribute.h"
#include "PlatformString.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

SVGViewElement::SVGViewElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , SVGZoomAndPan()
{
}

SVGViewElement::~SVGViewElement()
{
}

SVGStringList* SVGViewElement::viewTarget() const
{
    if (!m_viewTarget)
        m_viewTarget = SVGStringList::create(SVGNames::viewTargetAttr);

    return m_viewTarget.get();
}

void SVGViewElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::viewTargetAttr)
        viewTarget()->reset(attr->value());
    else {
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr)
           || SVGFitToViewBox::parseMappedAttribute(attr)
           || SVGZoomAndPan::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

}

#endif // ENABLE(SVG)
