/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "SVGElementInstance.h"

#include "ContainerNodeAlgorithms.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameView.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, instanceCounter, ("WebCoreSVGElementInstance"));

SVGElementInstance::SVGElementInstance(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtr<SVGElement> originalElement)
    : m_parentInstance(0)
    , m_correspondingUseElement(correspondingUseElement)
    , m_directUseElement(directUseElement)
    , m_element(originalElement)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
{
    ASSERT(m_correspondingUseElement);
    ASSERT(m_element);

    // Register as instance for passed element.
    m_element->mapInstanceToElement(this);

#ifndef NDEBUG
    instanceCounter.increment();
#endif
}

SVGElementInstance::~SVGElementInstance()
{
    // Call detach because we may be deleted directly if we are a child of a detached instance.
    detach();

#ifndef NDEBUG
    instanceCounter.decrement();
#endif

    m_element = 0;
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete an SVGElementInstance at each deref call site.
void SVGElementInstance::removedLastRef()
{
#ifndef NDEBUG
    m_deletionHasBegun = true;
#endif
    delete this;
}

void SVGElementInstance::detach()
{
    // Clear all pointers. When the node is detached from the shadow DOM it should be removed but,
    // due to ref counting, it may not be. So clear everything to avoid dangling pointers.

    for (SVGElementInstance* node = firstChild(); node; node = node->nextSibling())
        node->detach();

    // Deregister as instance for passed element, if we haven't already.
    if (m_element->instancesForElement().contains(this))
        m_element->removeInstanceMapping(this);
    // DO NOT clear ref to m_element because JavaScriptCore uses it for garbage collection

    m_shadowTreeElement = 0;

    m_directUseElement = 0;
    m_correspondingUseElement = 0;

    removeDetachedChildrenInContainer<SVGElementInstance, SVGElementInstance>(this);
}

PassRefPtr<SVGElementInstanceList> SVGElementInstance::childNodes()
{
    return SVGElementInstanceList::create(this);
}

void SVGElementInstance::setShadowTreeElement(SVGElement* element)
{
    ASSERT(element);
    m_shadowTreeElement = element;
}

void SVGElementInstance::appendChild(PassRefPtr<SVGElementInstance> child)
{
    appendChildToContainer<SVGElementInstance, SVGElementInstance>(child.get(), this);
}

void SVGElementInstance::invalidateAllInstancesOfElement(SVGElement* element)
{
    if (!element || !element->inDocument())
        return;

    if (element->isSVGStyledElement() && toSVGStyledElement(element)->instanceUpdatesBlocked())
        return;

    const HashSet<SVGElementInstance*>& set = element->instancesForElement();
    if (set.isEmpty())
        return;

    // Mark all use elements referencing 'element' for rebuilding
    const HashSet<SVGElementInstance*>::const_iterator end = set.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = set.begin(); it != end; ++it) {
        ASSERT((*it)->shadowTreeElement());
        ASSERT((*it)->shadowTreeElement()->correspondingElement());
        ASSERT((*it)->shadowTreeElement()->correspondingElement() == (*it)->correspondingElement());
        ASSERT((*it)->correspondingElement() == element);
        (*it)->shadowTreeElement()->setCorrespondingElement(0);

        if (SVGUseElement* element = (*it)->correspondingUseElement()) {
            ASSERT(element->inDocument());
            element->invalidateShadowTree();
        }
    }

    element->document()->updateStyleIfNeeded();
}

const AtomicString& SVGElementInstance::interfaceName() const
{
    return eventNames().interfaceForSVGElementInstance;
}

ScriptExecutionContext* SVGElementInstance::scriptExecutionContext() const
{
    return m_element->document();
}

bool SVGElementInstance::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    return m_element->addEventListener(eventType, listener, useCapture);
}

bool SVGElementInstance::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    return m_element->removeEventListener(eventType, listener, useCapture);
}

void SVGElementInstance::removeAllEventListeners()
{
    m_element->removeAllEventListeners();
}

bool SVGElementInstance::dispatchEvent(PassRefPtr<Event> event)
{
    SVGElement* element = shadowTreeElement();
    if (!element)
        return false;

    return element->dispatchEvent(event);
}

EventTargetData* SVGElementInstance::eventTargetData()
{
    // Since no event listeners are added to an SVGElementInstance, we don't have eventTargetData.
    return 0;
}

EventTargetData* SVGElementInstance::ensureEventTargetData()
{
    // EventTarget would use these methods if we were actually using its add/removeEventListener logic.
    // As we're forwarding those calls to the correspondingElement(), no one should ever call this function.
    ASSERT_NOT_REACHED();
    return 0;
}

SVGElementInstance::InstanceUpdateBlocker::InstanceUpdateBlocker(SVGElement* targetElement)
    : m_targetElement(targetElement->isSVGStyledElement() ? toSVGStyledElement(targetElement) : 0)
{
    if (m_targetElement)
        m_targetElement->setInstanceUpdatesBlocked(true);
}

SVGElementInstance::InstanceUpdateBlocker::~InstanceUpdateBlocker()
{
    if (m_targetElement)
        m_targetElement->setInstanceUpdatesBlocked(false);
}
   
}

#endif
