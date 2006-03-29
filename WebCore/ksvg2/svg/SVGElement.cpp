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
#include "SVGElement.h"

#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "SVGDOMImplementation.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "css_stylesheetimpl.h"
#include "HTMLNames.h"
#include "ksvg.h"
#include "PlatformString.h"
#include <kdom/Namespace.h>
#include "Attr.h"
#include <kdom/core/domattrs.h>
#include <kdom/events/EventListener.h>
#include <kdom/kdom.h>
#include "SVGDocumentExtensions.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

SVGElement::SVGElement(const QualifiedName& tagName, Document *doc) : StyledElement(tagName, doc), m_closed(false)
{
}

SVGElement::~SVGElement()
{
}

bool SVGElement::isSupported(StringImpl *feature, StringImpl *version) const
{
    if(SVGDOMImplementation::instance()->hasFeature(feature, version))
        return true;

    return DOMImplementation::instance()->hasFeature(feature, version);
}

SVGSVGElement *SVGElement::ownerSVGElement() const
{
    Node *n = parentNode();
    while(n)
    {
        if(n->nodeType() == ELEMENT_NODE && n->hasTagName(SVGNames::svgTag))
            return static_cast<SVGSVGElement *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElement *SVGElement::viewportElement() const
{
    Node *n = parentNode();
    while (n) {
        if (n->isElementNode() &&
            (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::symbolTag)))
            return static_cast<SVGElement *>(n);

        n = n->parentNode();
    }

    return 0;
}

AtomicString SVGElement::tryGetAttribute(const String& name, AtomicString defaultVal) const
{
    if(hasAttribute(name))
        return getAttribute(name);

    return defaultVal;
}

AtomicString SVGElement::tryGetAttributeNS(const String& namespaceURI, const String& localName, AtomicString defaultVal) const
{
    if(hasAttributeNS(namespaceURI, localName))
        return getAttributeNS(namespaceURI, localName);

    return defaultVal;
}

void SVGElement::addSVGEventListener(const AtomicString& eventType, const Attribute* attr)
{
    Element::setHTMLEventListener(eventType, document()->accessSVGExtensions()->
        createSVGEventListener(attr->localName().domString(), attr->value(), this));
}

void SVGElement::parseMappedAttribute(MappedAttribute *attr)
{
    // standard events
    if (attr->name() == onclickAttr)
        addSVGEventListener(clickEvent, attr);
    else if (attr->name() == onmousedownAttr)
        addSVGEventListener(mousedownEvent, attr);
    else if (attr->name() == onmousemoveAttr)
        addSVGEventListener(mousemoveEvent, attr);
    else if (attr->name() == onmouseoutAttr)
        addSVGEventListener(mouseoutEvent, attr);
    else if (attr->name() == onmouseoverAttr)
        addSVGEventListener(mouseoverEvent, attr);
    else if (attr->name() == onmouseupAttr)
        addSVGEventListener(mouseupEvent, attr);
    else if (attr->name() == onfocusAttr)
        addSVGEventListener(DOMFocusInEvent, attr);
    else if (attr->name() == onblurAttr)
        addSVGEventListener(DOMFocusOutEvent, attr);
    else
        StyledElement::parseMappedAttribute(attr);
}

bool SVGElement::childShouldCreateRenderer(WebCore::Node *child) const
{
    if (child->isSVGElement())
        return static_cast<SVGElement *>(child)->isValid();
    return false;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
