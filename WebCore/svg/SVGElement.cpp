/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
    Copyright (C) 2008 Apple Inc. All rights reserved.
    Copyright (C) 2008 Alp Toker <alp@atoker.com>
    Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>

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

#include "CSSCursorImageValue.h"
#include "DOMImplementation.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "RegisteredEventListener.h"
#include "RenderObject.h"
#include "SVGCursorElement.h"
#include "SVGElementInstance.h"
#include "SVGElementRareData.h"
#include "SVGNames.h"
#include "SVGResource.h"
#include "SVGSVGElement.h"
#include "SVGURIReference.h"
#include "SVGUseElement.h"
#include "ScriptEventListener.h"
#include "XMLNames.h"

namespace WebCore {

using namespace HTMLNames;

SVGElement::SVGElement(const QualifiedName& tagName, Document* document)
    : StyledElement(tagName, document, CreateElementZeroRefCount)
{
}

PassRefPtr<SVGElement> SVGElement::create(const QualifiedName& tagName, Document* document)
{
    return new SVGElement(tagName, document);
}

SVGElement::~SVGElement()
{
    if (!hasRareSVGData())
        ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    else {
        SVGElementRareData::SVGElementRareDataMap& rareDataMap = SVGElementRareData::rareDataMap();
        SVGElementRareData::SVGElementRareDataMap::iterator it = rareDataMap.find(this);
        ASSERT(it != rareDataMap.end());

        SVGElementRareData* rareData = it->second;
        if (SVGCursorElement* cursorElement = rareData->cursorElement())
            cursorElement->removeClient(this);
        if (CSSCursorImageValue* cursorImageValue = rareData->cursorImageValue())
            cursorImageValue->removeReferencedElement(this);

        delete rareData;
        rareDataMap.remove(it);
    }
}

SVGElementRareData* SVGElement::rareSVGData() const
{
    ASSERT(hasRareSVGData());
    return SVGElementRareData::rareDataFromMap(this);
}

SVGElementRareData* SVGElement::ensureRareSVGData()
{
    if (hasRareSVGData())
        return rareSVGData();

    ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    SVGElementRareData* data = new SVGElementRareData;
    SVGElementRareData::rareDataMap().set(this, data);
    m_hasRareSVGData = true;
    return data;
}

bool SVGElement::isSupported(StringImpl* feature, StringImpl* version) const
{
    return DOMImplementation::hasFeature(feature, version);
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
    Node* n = isShadowNode() ? const_cast<SVGElement*>(this)->shadowParentNode() : parentNode();
    while (n) {
        if (n->hasTagName(SVGNames::svgTag))
            return static_cast<SVGSVGElement*>(n);

        n = n->isShadowNode() ? n->shadowParentNode() : n->parentNode();
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

SVGDocumentExtensions* SVGElement::accessDocumentSVGExtensions() const
{
    // This function is provided for use by SVGAnimatedProperty to avoid
    // global inclusion of Document.h in SVG code.
    return document() ? document()->accessSVGExtensions() : 0;
}
 
void SVGElement::mapInstanceToElement(SVGElementInstance* instance)
{
    ASSERT(instance);

    HashSet<SVGElementInstance*>& instances = ensureRareSVGData()->elementInstances();
    ASSERT(!instances.contains(instance));

    instances.add(instance);
}
 
void SVGElement::removeInstanceMapping(SVGElementInstance* instance)
{
    ASSERT(instance);
    ASSERT(hasRareSVGData());

    HashSet<SVGElementInstance*>& instances = rareSVGData()->elementInstances();
    ASSERT(instances.contains(instance));

    instances.remove(instance);
}

const HashSet<SVGElementInstance*>& SVGElement::instancesForElement() const
{
    if (!hasRareSVGData()) {
        DEFINE_STATIC_LOCAL(HashSet<SVGElementInstance*>, emptyInstances, ());
        return emptyInstances;
    }
    return rareSVGData()->elementInstances();
}

void SVGElement::setCursorElement(SVGCursorElement* cursorElement)
{
    ensureRareSVGData()->setCursorElement(cursorElement);
}

void SVGElement::setCursorImageValue(CSSCursorImageValue* cursorImageValue)
{
    ensureRareSVGData()->setCursorImageValue(cursorImageValue);
}

void SVGElement::parseMappedAttribute(MappedAttribute* attr)
{
    // standard events
    if (attr->name() == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onclickAttr)
        setAttributeEventListener(eventNames().clickEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onmousedownAttr)
        setAttributeEventListener(eventNames().mousedownEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onmousemoveAttr)
        setAttributeEventListener(eventNames().mousemoveEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onmouseoutAttr)
        setAttributeEventListener(eventNames().mouseoutEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onmouseoverAttr)
        setAttributeEventListener(eventNames().mouseoverEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == onmouseupAttr)
        setAttributeEventListener(eventNames().mouseupEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == SVGNames::onfocusinAttr)
        setAttributeEventListener(eventNames().DOMFocusInEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == SVGNames::onfocusoutAttr)
        setAttributeEventListener(eventNames().DOMFocusOutEvent, createAttributeEventListener(this, attr));
    else if (attr->name() == SVGNames::onactivateAttr)
        setAttributeEventListener(eventNames().DOMActivateEvent, createAttributeEventListener(this, attr));
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

static bool hasLoadListener(Node* node)
{
    if (node->hasEventListeners(eventNames().loadEvent))
        return true;

    for (node = node->parentNode(); node && node->isElementNode(); node = node->parentNode()) {
        const EventListenerVector& entry = node->getEventListeners(eventNames().loadEvent);
        for (size_t i = 0; i < entry.size(); ++i) {
            if (entry[i].useCapture)
                return true;
        }
    }

    return false;
}

void SVGElement::sendSVGLoadEventIfPossible(bool sendParentLoadEvents)
{
    RefPtr<SVGElement> currentTarget = this;
    while (currentTarget && currentTarget->haveLoadedRequiredResources()) {
        RefPtr<Node> parent;
        if (sendParentLoadEvents)
            parent = currentTarget->parentNode(); // save the next parent to dispatch too incase dispatching the event changes the tree
        if (hasLoadListener(currentTarget.get())) {
            RefPtr<Event> event = Event::create(eventNames().loadEvent, false, false);
            event->setTarget(currentTarget);
            currentTarget->dispatchGenericEvent(event.release());
        }
        currentTarget = (parent && parent->isSVGElement()) ? static_pointer_cast<SVGElement>(parent) : 0;
    }
}

void SVGElement::finishParsingChildren()
{
    StyledElement::finishParsingChildren();

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

    String resourceId = SVGURIReference::getTarget(getAttribute(idAttributeName()));
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

void SVGElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    ASSERT(attr);
    if (!attr)
        return;

    StyledElement::attributeChanged(attr, preserveDecls);
    svgAttributeChanged(attr->name());
}

void SVGElement::updateAnimatedSVGAttribute(const QualifiedName& name) const
{
    ASSERT(!m_areSVGAttributesValid);

    if (m_synchronizingSVGAttributes)
        return;

    m_synchronizingSVGAttributes = true;

    const_cast<SVGElement*>(this)->synchronizeProperty(name);
    if (name == anyQName())
        m_areSVGAttributesValid = true;

    m_synchronizingSVGAttributes = false;
}

ContainerNode* SVGElement::eventParentNode()
{
    if (Node* shadowParent = shadowParentNode()) {
        ASSERT(shadowParent->isContainerNode());
        return static_cast<ContainerNode*>(shadowParent);
    }
    return StyledElement::eventParentNode();
}

}

#endif // ENABLE(SVG)
