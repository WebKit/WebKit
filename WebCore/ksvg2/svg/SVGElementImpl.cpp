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
#include "SVGElementImpl.h"

#include "DocumentImpl.h"
#include "EventNames.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGNames.h"
#include "SVGSVGElementImpl.h"
#include "css_stylesheetimpl.h"
#include "dom2_events.h"
#include "htmlnames.h"
#include "ksvg.h"
#include "PlatformString.h"
#include <kdom/Namespace.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/domattrs.h>
#include <kdom/events/EventListenerImpl.h>
#include <kdom/kdom.h>
#include "SVGDocumentExtensions.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

SVGElementImpl::SVGElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : XMLElementImpl(tagName, doc), m_closed(false)
{
}

SVGElementImpl::~SVGElementImpl()
{
}

bool SVGElementImpl::isSupported(DOMStringImpl *feature, DOMStringImpl *version) const
{
    if(SVGDOMImplementationImpl::instance()->hasFeature(feature, version))
        return true;

    return DOMImplementationImpl::instance()->hasFeature(feature, version);
}

SVGSVGElementImpl *SVGElementImpl::ownerSVGElement() const
{
    NodeImpl *n = parentNode();
    while(n)
    {
        if(n->nodeType() == ELEMENT_NODE && n->hasTagName(SVGNames::svgTag))
            return static_cast<SVGSVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElementImpl *SVGElementImpl::viewportElement() const
{
    NodeImpl *n = parentNode();
    while (n) {
        if (n->isElementNode() &&
            (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::symbolTag)))
            return static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

AtomicString SVGElementImpl::tryGetAttribute(const DOMString& name, AtomicString defaultVal) const
{
    if(hasAttribute(name))
        return getAttribute(name);

    return defaultVal;
}

AtomicString SVGElementImpl::tryGetAttributeNS(const DOMString& namespaceURI, const DOMString& localName, AtomicString defaultVal) const
{
    if(hasAttributeNS(namespaceURI, localName))
        return getAttributeNS(namespaceURI, localName);

    return defaultVal;
}

void SVGElementImpl::addSVGEventListener(const AtomicString& eventType, const AttributeImpl* attr)
{
    ElementImpl::setHTMLEventListener(eventType, getDocument()->accessSVGExtensions()->createSVGEventListener(attr->value(), this));
}

void SVGElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        StyledElementImpl::parseMappedAttribute(attr);
}

bool SVGElementImpl::childShouldCreateRenderer(DOM::NodeImpl *child) const
{
    if (child->isSVGElement())
        return static_cast<SVGElementImpl *>(child)->isValid();
    return false;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
