/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGElement.h"

#include "DOMImplementation.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "PlatformString.h"
#include "RenderObject.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "SVGUseElement.h"
#include "XMLNames.h"

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

SVGElement::SVGElement(const QualifiedName& tagName, Document* doc)
    : StyledElement(tagName, doc)
    , m_shadowParent(0)
{
}

SVGElement::~SVGElement()
{
}

bool SVGElement::isSupported(StringImpl* feature, StringImpl* version) const
{
    if (DOMImplementation::instance()->hasFeature(feature, version))
        return true;

    return DOMImplementation::instance()->hasFeature(feature, version);
}

String SVGElement::id() const
{
    return getAttribute(idAttr);
}

void SVGElement::setId(const String& value, ExceptionCode&)
{
    setAttribute(idAttr, value);
}

String SVGElement::xmlbase() const
{
    return getAttribute(XMLNames::baseAttr);
}

void SVGElement::setXmlbase(const String& value, ExceptionCode&)
{
    setAttribute(XMLNames::baseAttr, value);
}

SVGSVGElement* SVGElement::ownerSVGElement() const
{
    Node* n = parentNode();
    while (n) {
        if (n->hasTagName(SVGNames::svgTag))
            return static_cast<SVGSVGElement*>(n);

        n = n->parentNode();
    }

    return 0;
}

SVGElement* SVGElement::viewportElement() const
{
    // This function needs shadow tree support - as RenderSVGContainer uses this function
    // to determine the "overflow" property. <use> on <symbol> wouldn't work otherwhise.
    Node* n = isShadowNode() ? const_cast<SVGElement*>(this)->shadowParentNode() : parentNode();
    while (n) {
        if (n->hasTagName(SVGNames::svgTag) || n->hasTagName(SVGNames::imageTag) || n->hasTagName(SVGNames::symbolTag))
            return static_cast<SVGElement*>(n);

        n = n->isShadowNode() ? n->shadowParentNode() : n->parentNode();
    }

    return 0;
}

void SVGElement::addSVGEventListener(const AtomicString& eventType, const Attribute* attr)
{
    Element::setHTMLEventListener(eventType, document()->accessSVGExtensions()->
        createSVGEventListener(attr->localName().domString(), attr->value(), this));
}

void SVGElement::parseMappedAttribute(MappedAttribute* attr)
{
    // standard events
    if (attr->name() == onloadAttr)
        addSVGEventListener(loadEvent, attr);
    else if (attr->name() == onclickAttr)
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
    else if (attr->name() == SVGNames::onfocusinAttr)
        addSVGEventListener(DOMFocusInEvent, attr);
    else if (attr->name() == SVGNames::onfocusoutAttr)
        addSVGEventListener(DOMFocusOutEvent, attr);
    else if (attr->name() == SVGNames::onactivateAttr)
        addSVGEventListener(DOMActivateEvent, attr);
    else
        StyledElement::parseMappedAttribute(attr);
}

bool SVGElement::haveLoadedRequiredResources()
{
    Node* child = firstChild();
    while (child) {
        if (child->isSVGElement() && !static_cast<SVGElement*>(child)->haveLoadedRequiredResources())
            return false;
        child = child->nextSibling();
    }
    return true;
}

void SVGElement::sendSVGLoadEventIfPossible(bool sendParentLoadEvents)
{
    RefPtr<SVGElement> currentTarget = this;
    while (currentTarget && currentTarget->haveLoadedRequiredResources()) {
        RefPtr<Node> parent;
        if (sendParentLoadEvents)
            parent = currentTarget->parentNode(); // save the next parent to dispatch too incase dispatching the event changes the tree
        
        // FIXME: This malloc could be avoided by walking the tree first to check if any listeners are present: http://bugs.webkit.org/show_bug.cgi?id=10264
        RefPtr<Event> event = new Event(loadEvent, false, false);
        event->setTarget(currentTarget);
        ExceptionCode ignored = 0;
        dispatchGenericEvent(this, event.release(), ignored, false);
        currentTarget = (parent && parent->isSVGElement()) ? static_pointer_cast<SVGElement>(parent) : 0;
    }
}

void SVGElement::finishParsingChildren()
{
    // finishParsingChildren() is called when the close tag is reached for an element (e.g. </svg>)
    // we send SVGLoad events here if we can, otherwise they'll be sent when any required loads finish
    sendSVGLoadEventIfPossible();
}

bool SVGElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isSVGElement())
        return static_cast<SVGElement*>(child)->isValid();
    return false;
}

void SVGElement::insertedIntoDocument()
{
    StyledElement::insertedIntoDocument();
    SVGDocumentExtensions* extensions = document()->accessSVGExtensions();

    String resourceId = SVGURIReference::getTarget(id());
    if (extensions->isPendingResource(resourceId)) {
        std::auto_ptr<HashSet<SVGStyledElement*> > clients(extensions->removePendingResource(resourceId));
        if (clients->isEmpty())
            return;

        HashSet<SVGStyledElement*>::const_iterator it = clients->begin();
        const HashSet<SVGStyledElement*>::const_iterator end = clients->end();

        for (; it != end; ++it)
            (*it)->buildPendingResource();

        SVGResource::invalidateClients(*clients);
    }
}

static Node* shadowTreeParentElementForShadowTreeElement(Node* node)
{
    for (Node* n = node; n; n = n->parentNode()) {
        if (n->isShadowNode())
            return n->shadowParentNode();
    }

    return 0;
}

bool SVGElement::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    // TODO: This function will be removed in a follow-up patch!

    EventTarget* target = this;
    Node* useNode = shadowTreeParentElementForShadowTreeElement(this);

    // If we are a hidden shadow tree element, the target must
    // point to our corresponding SVGElementInstance object
    if (useNode) {
        ASSERT(useNode->hasTagName(SVGNames::useTag));
        SVGUseElement* use = static_cast<SVGUseElement*>(useNode);

        SVGElementInstance* instance = use->instanceForShadowTreeElement(this);

        if (instance)
            target = instance;
    }

    e->setTarget(target);

    RefPtr<FrameView> view = document()->view();
    return EventTargetNode::dispatchGenericEvent(this, e, ec, tempEvent);
}

void SVGElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    ASSERT(attr);
    if (!attr)
        return;

    StyledElement::attributeChanged(attr, preserveDecls);
    svgAttributeChanged(attr->name());
}

}

#endif // ENABLE(SVG)
