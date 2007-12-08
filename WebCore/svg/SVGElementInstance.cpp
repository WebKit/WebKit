/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

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
#include "SVGElementInstance.h"

#include "Event.h"
#include "EventListener.h"
#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/Assertions.h>

namespace WebCore {

SVGElementInstance::SVGElementInstance(SVGUseElement* useElement, PassRefPtr<SVGElement> originalElement)
    : m_refCount(0)
    , m_parent(0)
    , m_useElement(useElement)
    , m_element(originalElement)
    , m_shadowTreeElement(0)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
{
    ASSERT(m_useElement);
    ASSERT(m_element);

    // Register as instance for passed element.
    m_element->document()->accessSVGExtensions()->mapInstanceToElement(this, m_element.get());
}

SVGElementInstance::~SVGElementInstance()
{
    for (RefPtr<SVGElementInstance> child = m_firstChild; child; child = child->m_nextSibling)
        child->setParent(0);

    // Deregister as instance for passed element.
    m_element->document()->accessSVGExtensions()->removeInstanceMapping(this, m_element.get());
}

SVGElement* SVGElementInstance::correspondingElement() const
{
    return m_element.get();
}

SVGUseElement* SVGElementInstance::correspondingUseElement() const
{
    return m_useElement;
}

SVGElementInstance* SVGElementInstance::parentNode() const
{
    return parent();
}

PassRefPtr<SVGElementInstanceList> SVGElementInstance::childNodes()
{
    return new SVGElementInstanceList(this);
}

SVGElementInstance* SVGElementInstance::previousSibling() const
{
    return m_previousSibling;
}

SVGElementInstance* SVGElementInstance::nextSibling() const
{
    return m_nextSibling;
}

SVGElementInstance* SVGElementInstance::firstChild() const
{
    return m_firstChild;
}

SVGElementInstance* SVGElementInstance::lastChild() const
{
    return m_lastChild;
}

SVGElement* SVGElementInstance::shadowTreeElement() const
{
    return m_shadowTreeElement;
}

void SVGElementInstance::setShadowTreeElement(SVGElement* element)
{
    ASSERT(element);
    m_shadowTreeElement = element;
}

void SVGElementInstance::appendChild(PassRefPtr<SVGElementInstance> child)
{
    child->setParent(this);

    if (m_lastChild) {
        child->m_previousSibling = m_lastChild;
        m_lastChild->m_nextSibling = child.get();
    } else
        m_firstChild = child.get();

    m_lastChild = child.get();
}

// Helper function for updateInstance
static bool containsUseChildNode(Node* start)
{
    if (start->hasTagName(SVGNames::useTag))
        return true;

    for (Node* current = start->firstChild(); current; current = current->nextSibling()) {
        if (containsUseChildNode(current))
            return true;
    }

    return false;
}

void SVGElementInstance::updateInstance(SVGElement* element)
{
    ASSERT(element == m_element);
    ASSERT(m_shadowTreeElement);

    // TODO: Eventually come up with a more optimized updating logic for the cases below:
    //
    // <symbol>: We can't just clone the original element, we need to apply
    // the same "replace by generated content" logic that SVGUseElement does.
    //
    // <svg>: <use> on <svg> is too rare to actually implement it faster.
    // If someone still wants to do it: recloning, adjusting width/height attributes is enough.
    //
    // <use>: Too hard to get it right in a fast way. Recloning seems the only option.

    if (m_element->hasTagName(SVGNames::symbolTag) ||
        m_element->hasTagName(SVGNames::svgTag) ||
        containsUseChildNode(m_element.get())) {
        m_useElement->buildPendingResource();
        return;
    }

    // For all other nodes this logic is sufficient.
    RefPtr<Node> clone = m_element->cloneNode(true);
    SVGElement* svgClone = 0;
    if (clone && clone->isSVGElement())
        svgClone = static_cast<SVGElement*>(clone.get());
    ASSERT(svgClone);

    // Replace node in the <use> shadow tree
    ExceptionCode ec = 0;
    m_shadowTreeElement->parentNode()->replaceChild(clone.release(), m_shadowTreeElement, ec);
    ASSERT(ec == 0);

    m_shadowTreeElement = svgClone;
}

SVGElementInstance* SVGElementInstance::toSVGElementInstance()
{
    return this;
}

EventTargetNode* SVGElementInstance::toNode()
{
    return m_element.get();
}

void SVGElementInstance::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool useCapture)
{
    // FIXME!
}

void SVGElementInstance::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool useCapture)
{
    // FIXME!
}

bool SVGElementInstance::dispatchEvent(PassRefPtr<Event>, ExceptionCode& ec, bool tempEvent)
{
    // FIXME!
    return false;
}
 
}

#endif // ENABLE(SVG)

// vim:ts=4:noet
