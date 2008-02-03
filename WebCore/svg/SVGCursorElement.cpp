/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
#include "SVGCursorElement.h"

#include "Attr.h"
#include "CachedImage.h"
#include "Document.h"
#include "DocLoader.h"
#include "SVGNames.h"
#include "SVGLength.h"

namespace WebCore {

SVGCursorElement::SVGCursorElement(const QualifiedName& tagName, Document *doc)
    : SVGElement(tagName, doc)
    , SVGTests()
    , SVGExternalResourcesRequired()
    , SVGURIReference()
    , CachedResourceClient()
    , m_x(0, LengthModeWidth)
    , m_y(0, LengthModeHeight)
{
    m_cachedImage = 0;
}

SVGCursorElement::~SVGCursorElement()
{
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

ANIMATED_PROPERTY_DEFINITIONS(SVGCursorElement, SVGLength, Length, length, X, x, SVGNames::xAttr, m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGCursorElement, SVGLength, Length, length, Y, y, SVGNames::yAttr, m_y)

void SVGCursorElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(0, LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(0, LengthModeHeight, attr->value()));
    else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGURIReference::parseMappedAttribute(attr)) {
            if (m_cachedImage)
                m_cachedImage->deref(this);
            m_cachedImage = ownerDocument()->docLoader()->requestImage(href());
            if (m_cachedImage)
                m_cachedImage->ref(this);
            return;
        }

        SVGElement::parseMappedAttribute(attr);
    }
}

}

#endif // ENABLE(SVG)
