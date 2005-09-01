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

#include <kdom/kdom.h>
#include <kdom/Namespace.h>
#include <kdom/DOMString.h>
#include <kdom/impl/domattrs.h>
#include <kdom/impl/AttrImpl.h>
#include <kdom/css/impl/CSSStyleSheetImpl.h>
#include <kdom/events/impl/EventListenerImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
//#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGCSSStyleDeclarationImpl.h"

using namespace KSVG;

SVGElementImpl::SVGElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : KDOM::XMLElementImpl(doc, id, prefix)
{
}

SVGElementImpl::~SVGElementImpl()
{
}

bool SVGElementImpl::isSupported(KDOM::DOMStringImpl *feature, KDOM::DOMStringImpl *version) const
{
    if(SVGDOMImplementationImpl::self()->hasFeature(feature, version))
        return true;

    return KDOM::DOMImplementationImpl::self()->hasFeature(feature, version);
}

KDOM::DOMStringImpl *SVGElementImpl::getId() const
{
    KDOM::DOMString name("id");
    if(hasAttribute(name.handle()))
        return getAttribute(name.handle());

    return NULL;
}

void SVGElementImpl::setGetId(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

KDOM::DOMStringImpl *SVGElementImpl::xmlbase() const
{
    KDOM::DOMString name("xml:base");
    if(hasAttribute(name.handle()))
        return getAttribute(name.handle());

    return NULL;
}

void SVGElementImpl::setXmlbase(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

SVGSVGElementImpl *SVGElementImpl::ownerSVGElement() const
{
    NodeImpl *n = parentNode();
    while(n)
    {
        if(n->nodeType() == KDOM::ELEMENT_NODE && n->id() == ID_SVG)
            return static_cast<SVGSVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElementImpl *SVGElementImpl::viewportElement() const
{
    NodeImpl *n = parentNode();
    while(n)
    {
        if(n->nodeType() == KDOM::ELEMENT_NODE &&
            (n->id() == ID_SVG || n->id() == ID_IMAGE || n->id() == ID_SYMBOL))
            return static_cast<SVGElementImpl *>(n);

        n = n->parentNode();
    }

    return 0;
}

void SVGElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    if(!ownerDocument())
        return;
    //const KDOM::DocumentImpl *doc = ownerDocument();

    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_ONLOAD:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("load"), value);
            break;
        }
        case ATTR_ONUNLOAD:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("unload"), value);
            break;
        }
        case ATTR_ONABORT:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("abort"), value);
            break;
        }
        case ATTR_ONERROR:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("error"), value);
            break;
        }
        case ATTR_ONRESIZE:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("resize"), value);
            break;
        }
        case ATTR_ONSCROLL:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("scroll"), value);
            break;
        }
        case ATTR_ONZOOM:
        {
//            addSVGEventListener(doc->ecmaEngine(), KDOM::DOMString("zoom"), value);
            break;
        }
        case ATTR_ID:
        {
            KDOM::DOMString svg(KDOM::NS_SVG);
            KDOM::DOMString id("id");

            KDOM::AttrImpl *attr = getAttributeNodeNS(svg.handle(), id.handle());
            if(attr)
                attr->setIsId(true);

            break;
        }
        default:
            KDOM::ElementImpl::parseAttribute(attr);
    };
}
#if 0
void SVGElementImpl::addSVGEventListener(KDOM::Ecma *ecmaEngine, const KDOM::DOMString &type, const KDOM::DOMString &value)
{
    if(!ecmaEngine)
        return;
    // FIXME
    KDOM::EventListenerImpl *listener = 0;//ecmaEngine->createEventListener(type, value);
    if(listener)
    {
        listener->ref();
        addEventListener(type.handle(), listener, false);
    }
}
#endif
void SVGElementImpl::createStyleDeclaration() const
{
    m_styleDeclarations = new SVGCSSStyleDeclarationImpl(ownerDocument()->implementation()->cdfInterface(), 0);
    m_styleDeclarations->ref();

    m_styleDeclarations->setNode(const_cast<SVGElementImpl *>(this));
    m_styleDeclarations->setParent(ownerDocument()->elementSheet());
}

SVGDocumentImpl *SVGElementImpl::getDocument() const
{
    return static_cast<SVGDocumentImpl *>(ownerDocument());
}

// vim:ts=4:noet
