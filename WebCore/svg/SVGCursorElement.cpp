/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "Document.h"
#include "MappedAttribute.h"
#include "SVGLength.h"
#include "SVGNames.h"

namespace WebCore {

SVGCursorElement::SVGCursorElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , SVGTests()
    , SVGExternalResourcesRequired()
    , SVGURIReference()
    , m_x(this, SVGNames::xAttr, LengthModeWidth)
    , m_y(this, SVGNames::yAttr, LengthModeHeight)
    , m_href(this, XLinkNames::hrefAttr)
    , m_externalResourcesRequired(this, SVGNames::externalResourcesRequiredAttr, false)
{
}

SVGCursorElement::~SVGCursorElement()
{
    HashSet<SVGElement*>::iterator end = m_clients.end();
    for (HashSet<SVGElement*>::iterator it = m_clients.begin(); it != end; ++it)
        (*it)->setCursorElement(0);
}

void SVGCursorElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
    else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGURIReference::parseMappedAttribute(attr))
            return;

        SVGElement::parseMappedAttribute(attr);
    }
}

void SVGCursorElement::addClient(SVGElement* element)
{
    m_clients.add(element);
    element->setCursorElement(this);
}

void SVGCursorElement::removeClient(SVGElement* element)
{
    m_clients.remove(element);
    element->setCursorElement(0);
}

void SVGCursorElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGElement::svgAttributeChanged(attrName);

    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
        SVGTests::isKnownAttribute(attrName) ||
        SVGExternalResourcesRequired::isKnownAttribute(attrName) ||
        SVGURIReference::isKnownAttribute(attrName)) {
        HashSet<SVGElement*>::const_iterator it = m_clients.begin();
        HashSet<SVGElement*>::const_iterator end = m_clients.end();

        for (; it != end; ++it)
            (*it)->setNeedsStyleRecalc();
    }
}

void SVGCursorElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(href()));
}

}

#endif // ENABLE(SVG)
