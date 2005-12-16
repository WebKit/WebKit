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
#include <kdom/kdom.h>
#include <kdom/Namespace.h>
#include <kdom/DOMString.h>
#include <kdom/core/domattrs.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/css/CSSStyleSheetImpl.h>
#include <kdom/events/EventListenerImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "EventNames.h"
#include "htmlnames.h"
#include "SVGElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;
using namespace DOM::HTMLNames;
using namespace DOM::EventNames;

SVGElementImpl::SVGElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : KDOM::XMLElementImpl(tagName, doc), m_closed(false)
{
}

SVGElementImpl::~SVGElementImpl()
{
}

bool SVGElementImpl::isSupported(KDOM::DOMStringImpl *feature, KDOM::DOMStringImpl *version) const
{
    if(SVGDOMImplementationImpl::instance()->hasFeature(feature, version))
        return true;

    return KDOM::DOMImplementationImpl::instance()->hasFeature(feature, version);
}

SVGSVGElementImpl *SVGElementImpl::ownerSVGElement() const
{
    NodeImpl *n = parentNode();
    while(n)
    {
        if(n->nodeType() == KDOM::ELEMENT_NODE && n->hasTagName(SVGNames::svgTag))
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

KDOM::AtomicString SVGElementImpl::tryGetAttribute(const KDOM::DOMString& name, KDOM::AtomicString defaultVal) const
{
    if(hasAttribute(name))
        return getAttribute(name);

    return defaultVal;
}

KDOM::AtomicString SVGElementImpl::tryGetAttributeNS(const KDOM::DOMString& namespaceURI, const KDOM::DOMString& localName, KDOM::AtomicString defaultVal) const
{
    if(hasAttributeNS(namespaceURI, localName))
        return getAttributeNS(namespaceURI, localName);

    return defaultVal;
}

void SVGElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    // standard events
    if (attr->name() == onclickAttr)
        setHTMLEventListener(clickEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onmousedownAttr)
        setHTMLEventListener(mousedownEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onmousemoveAttr)
        setHTMLEventListener(mousemoveEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onmouseoutAttr)
        setHTMLEventListener(mouseoutEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onmouseoverAttr)
        setHTMLEventListener(mouseoverEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onmouseupAttr)
        setHTMLEventListener(mouseupEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onfocusAttr)
        setHTMLEventListener(DOMFocusInEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else if (attr->name() == onblurAttr)
        setHTMLEventListener(DOMFocusOutEvent, getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    else
        KDOM::StyledElementImpl::parseMappedAttribute(attr);
}

bool SVGElementImpl::childShouldCreateRenderer(DOM::NodeImpl *child) const
{
    if (child->isSVGElement())
        return static_cast<SVGElementImpl *>(child)->isValid();
    return false;
}

// vim:ts=4:noet
