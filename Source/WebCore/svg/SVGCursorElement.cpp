/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGCursorElement.h"

#include "Attr.h"
#include "Document.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGCursorElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGCursorElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_STRING(SVGCursorElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGCursorElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

inline SVGCursorElement::SVGCursorElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
{
    ASSERT(hasTagName(SVGNames::cursorTag));
}

PassRefPtr<SVGCursorElement> SVGCursorElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGCursorElement(tagName, document));
}

SVGCursorElement::~SVGCursorElement()
{
    HashSet<SVGElement*>::iterator end = m_clients.end();
    for (HashSet<SVGElement*>::iterator it = m_clients.begin(); it != end; ++it)
        (*it)->cursorElementRemoved();
}

bool SVGCursorElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
    }
    return supportedAttributes.contains(attrName);
}

void SVGCursorElement::parseMappedAttribute(Attribute* attr)
{
    if (!isSupportedAttribute(attr->name())) {
        SVGElement::parseMappedAttribute(attr);
        return;
    }

    if (attr->name() == SVGNames::xAttr) {
        setXBaseValue(SVGLength(LengthModeWidth, attr->value()));
        return;
    }

    if (attr->name() == SVGNames::yAttr) {
        setYBaseValue(SVGLength(LengthModeHeight, attr->value()));
        return;
    }
    
    if (SVGTests::parseMappedAttribute(attr))
        return;
    if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
        return;
    if (SVGURIReference::parseMappedAttribute(attr))
        return;

    ASSERT_NOT_REACHED();
}

AttributeToPropertyTypeMap& SVGCursorElement::attributeToPropertyTypeMap()
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, s_attributeToPropertyTypeMap, ());
    return s_attributeToPropertyTypeMap;
}

void SVGCursorElement::fillAttributeToPropertyTypeMap()
{
    AttributeToPropertyTypeMap& attributeToPropertyTypeMap = this->attributeToPropertyTypeMap();
    attributeToPropertyTypeMap.set(SVGNames::xAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(SVGNames::yAttr, AnimatedNumber);
    attributeToPropertyTypeMap.set(XLinkNames::hrefAttr, AnimatedString);
}

void SVGCursorElement::addClient(SVGElement* element)
{
    m_clients.add(element);
    element->setCursorElement(this);
}

void SVGCursorElement::removeClient(SVGElement* element)
{
    HashSet<SVGElement*>::iterator it = m_clients.find(element);
    if (it != m_clients.end()) {
        m_clients.remove(it);
        element->cursorElementRemoved();
    }
}

void SVGCursorElement::removeReferencedElement(SVGElement* element)
{
    m_clients.remove(element);
}

void SVGCursorElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    // Any change of a cursor specific attribute triggers this recalc.
    HashSet<SVGElement*>::const_iterator it = m_clients.begin();
    HashSet<SVGElement*>::const_iterator end = m_clients.end();

    for (; it != end; ++it)
        (*it)->setNeedsStyleRecalc();
}

void SVGCursorElement::synchronizeProperty(const QualifiedName& attrName)
{
    if (attrName == anyQName()) {
        synchronizeX();
        synchronizeY();
        synchronizeExternalResourcesRequired();
        synchronizeHref();
        SVGTests::synchronizeProperties(this, attrName);
        SVGElement::synchronizeProperty(attrName);
        return;
    }

    if (!isSupportedAttribute(attrName)) {
        SVGElement::synchronizeProperty(attrName);
        return;
    }

    if (attrName == SVGNames::xAttr) {
        synchronizeX();
        return;
    }

    if (attrName == SVGNames::yAttr) {
        synchronizeY();
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        synchronizeExternalResourcesRequired();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        synchronizeHref();
        return;
    }

    if (SVGTests::isKnownAttribute(attrName)) {
        SVGTests::synchronizeProperties(this, attrName);
        return;
    }

    ASSERT_NOT_REACHED();
}

void SVGCursorElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(href()));
}

}

#endif // ENABLE(SVG)
