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

#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/Assertions.h>

namespace WebCore {

SVGElementInstance::SVGElementInstance(SVGUseElement* useElement, PassRefPtr<SVGElement> originalElement)
    : m_useElement(useElement)
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
    m_element->mapInstanceToElement(this);
}

SVGElementInstance::~SVGElementInstance()
{
    for (RefPtr<SVGElementInstance> child = m_firstChild; child; child = child->m_nextSibling)
        child->setParent(0);

    // Deregister as instance for passed element.
    m_element->removeInstanceMapping(this);
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
    return SVGElementInstanceList::create(this);
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

SVGElementInstance* SVGElementInstance::toSVGElementInstance()
{
    return this;
}

EventTargetNode* SVGElementInstance::toNode()
{
    return m_element.get();
}

 
void SVGElementInstance::updateAllInstancesOfElement(SVGElement* element)
{
    if (!element)
        return;

    HashSet<SVGElementInstance*> set = element->instancesForElement();
    if (set.isEmpty())
        return;

    // Find all use elements referencing the instances - ask them _once_ to rebuild.
    HashSet<SVGElementInstance*>::const_iterator it = set.begin();
    const HashSet<SVGElementInstance*>::const_iterator end = set.end();

    HashSet<SVGUseElement*> useHash;
    for (; it != end; ++it)
        useHash.add((*it)->correspondingUseElement());

    HashSet<SVGUseElement*>::const_iterator itUse = useHash.begin();
    const HashSet<SVGUseElement*>::const_iterator endUse = useHash.end();
    for (; itUse != endUse; ++itUse)
        (*itUse)->buildPendingResource();
}

}

#endif // ENABLE(SVG)
