/*
    Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>

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

#include "ContainerNodeAlgorithms.h"
#include "Document.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameView.h"
#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/RefCountedLeakCounter.h>

#if USE(JSC)
#include "GCController.h"
#endif

namespace WebCore {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter instanceCounter("WebCoreSVGElementInstance");
#endif

SVGElementInstance::SVGElementInstance(SVGUseElement* useElement, SVGElement* originalElement)
    : m_needsUpdate(false)
    , m_useElement(useElement)
    , m_element(originalElement)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
{
    ASSERT(m_useElement);
    ASSERT(m_element);

    // Register as instance for passed element.
    m_element->mapInstanceToElement(this);

#ifndef NDEBUG
    instanceCounter.increment();
#endif
}

SVGElementInstance::~SVGElementInstance()
{
#ifndef NDEBUG
    instanceCounter.decrement();
#endif

    // Deregister as instance for passed element.
    m_element->removeInstanceMapping(this);

    removeAllChildrenInContainer<SVGElementInstance, SVGElementInstance>(this);
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

void SVGElementInstance::forgetWrapper()
{
#if USE(JSC)
    // FIXME: This is fragile, as discussed with Sam. Need to find a better solution.
    // Think about the case where JS explicitely holds "var root = useElement.instanceRoot;".
    // We still have to recreate this wrapper somehow. The gc collection below, won't catch it.

    // If the use shadow tree has been rebuilt, just the JSSVGElementInstance objects
    // are still holding RefPtrs of SVGElementInstance objects, which prevent us to
    // be deleted (and the shadow tree is not destructed as well). Force JS GC.
    gcController().garbageCollectNow();
#endif
}

void SVGElementInstance::appendChild(PassRefPtr<SVGElementInstance> child)
{
    appendChildToContainer<SVGElementInstance, SVGElementInstance>(child.get(), this);
}

void SVGElementInstance::invalidateAllInstancesOfElement(SVGElement* element)
{
    if (!element)
        return;

    HashSet<SVGElementInstance*> set = element->instancesForElement();
    if (set.isEmpty())
        return;

    // Find all use elements referencing the instances - ask them _once_ to rebuild.
    HashSet<SVGElementInstance*>::const_iterator it = set.begin();
    const HashSet<SVGElementInstance*>::const_iterator end = set.end();

    for (; it != end; ++it)
        (*it)->setNeedsUpdate(true);
}

void SVGElementInstance::setNeedsUpdate(bool value)
{
    m_needsUpdate = value;

    if (m_needsUpdate)
        correspondingUseElement()->setNeedsStyleRecalc();
}

ScriptExecutionContext* SVGElementInstance::scriptExecutionContext() const
{
    if (SVGElement* element = correspondingElement())
        return element->scriptExecutionContext();
    return 0;
}

void SVGElementInstance::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (SVGElement* element = correspondingElement())
        element->addEventListener(eventType, listener, useCapture);
}

void SVGElementInstance::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (SVGElement* element = correspondingElement())
        element->removeEventListener(eventType, listener, useCapture);
}

bool SVGElementInstance::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    // The event has to be dispatched to the shadowTreeElement(), not the correspondingElement()!
    SVGElement* element = shadowTreeElement();
    if (!element)
        return false;

    evt->setTarget(this);

    RefPtr<FrameView> view = element->document()->view();
    return element->dispatchGenericEvent(evt.release());
}

EventListener* SVGElementInstance::onabort() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().abortEvent);
}

void SVGElementInstance::setOnabort(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().abortEvent, eventListener);
}

EventListener* SVGElementInstance::onblur() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().blurEvent);
}

void SVGElementInstance::setOnblur(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().blurEvent, eventListener);
}

EventListener* SVGElementInstance::onchange() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().changeEvent);
}

void SVGElementInstance::setOnchange(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().changeEvent, eventListener);
}

EventListener* SVGElementInstance::onclick() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().clickEvent);
}

void SVGElementInstance::setOnclick(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().clickEvent, eventListener);
}

EventListener* SVGElementInstance::oncontextmenu() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().contextmenuEvent);
}

void SVGElementInstance::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().contextmenuEvent, eventListener);
}

EventListener* SVGElementInstance::ondblclick() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dblclickEvent);
}

void SVGElementInstance::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dblclickEvent, eventListener);
}

EventListener* SVGElementInstance::onerror() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().errorEvent);
}

void SVGElementInstance::setOnerror(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().errorEvent, eventListener);
}

EventListener* SVGElementInstance::onfocus() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().focusEvent);
}

void SVGElementInstance::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().focusEvent, eventListener);
}

EventListener* SVGElementInstance::oninput() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().inputEvent);
}

void SVGElementInstance::setOninput(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().inputEvent, eventListener);
}

EventListener* SVGElementInstance::onkeydown() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().keydownEvent);
}

void SVGElementInstance::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().keydownEvent, eventListener);
}

EventListener* SVGElementInstance::onkeypress() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().keypressEvent);
}

void SVGElementInstance::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().keypressEvent, eventListener);
}

EventListener* SVGElementInstance::onkeyup() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().keyupEvent);
}

void SVGElementInstance::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().keyupEvent, eventListener);
}

EventListener* SVGElementInstance::onload() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().loadEvent);
}

void SVGElementInstance::setOnload(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().loadEvent, eventListener);
}

EventListener* SVGElementInstance::onmousedown() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mousedownEvent);
}

void SVGElementInstance::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mousedownEvent, eventListener);
}

EventListener* SVGElementInstance::onmousemove() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mousemoveEvent);
}

void SVGElementInstance::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mousemoveEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseout() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mouseoutEvent);
}

void SVGElementInstance::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mouseoutEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseover() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mouseoverEvent);
}

void SVGElementInstance::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mouseoverEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseup() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mouseupEvent);
}

void SVGElementInstance::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mouseupEvent, eventListener);
}

EventListener* SVGElementInstance::onmousewheel() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().mousewheelEvent);
}

void SVGElementInstance::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().mousewheelEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforecut() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().beforecutEvent);
}

void SVGElementInstance::setOnbeforecut(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().beforecutEvent, eventListener);
}

EventListener* SVGElementInstance::oncut() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().cutEvent);
}

void SVGElementInstance::setOncut(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().cutEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforecopy() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().beforecopyEvent);
}

void SVGElementInstance::setOnbeforecopy(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().beforecopyEvent, eventListener);
}

EventListener* SVGElementInstance::oncopy() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().copyEvent);
}

void SVGElementInstance::setOncopy(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().copyEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforepaste() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().beforepasteEvent);
}

void SVGElementInstance::setOnbeforepaste(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().beforepasteEvent, eventListener);
}

EventListener* SVGElementInstance::onpaste() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().pasteEvent);
}

void SVGElementInstance::setOnpaste(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().pasteEvent, eventListener);
}

EventListener* SVGElementInstance::ondragenter() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragenterEvent);
}

void SVGElementInstance::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragenterEvent, eventListener);
}

EventListener* SVGElementInstance::ondragover() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragoverEvent);
}

void SVGElementInstance::setOndragover(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragoverEvent, eventListener);
}

EventListener* SVGElementInstance::ondragleave() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragleaveEvent);
}

void SVGElementInstance::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragleaveEvent, eventListener);
}

EventListener* SVGElementInstance::ondrop() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dropEvent);
}

void SVGElementInstance::setOndrop(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dropEvent, eventListener);
}

EventListener* SVGElementInstance::ondragstart() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragstartEvent);
}

void SVGElementInstance::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragstartEvent, eventListener);
}

EventListener* SVGElementInstance::ondrag() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragEvent);
}

void SVGElementInstance::setOndrag(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragEvent, eventListener);
}

EventListener* SVGElementInstance::ondragend() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().dragendEvent);
}

void SVGElementInstance::setOndragend(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().dragendEvent, eventListener);
}

EventListener* SVGElementInstance::onreset() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().resetEvent);
}

void SVGElementInstance::setOnreset(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().resetEvent, eventListener);
}

EventListener* SVGElementInstance::onresize() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().resizeEvent);
}

void SVGElementInstance::setOnresize(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().resizeEvent, eventListener);
}

EventListener* SVGElementInstance::onscroll() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().scrollEvent);
}

void SVGElementInstance::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().scrollEvent, eventListener);
}

EventListener* SVGElementInstance::onsearch() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().searchEvent);
}

void SVGElementInstance::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().searchEvent, eventListener);
}

EventListener* SVGElementInstance::onselect() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().selectEvent);
}

void SVGElementInstance::setOnselect(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().selectEvent, eventListener);
}

EventListener* SVGElementInstance::onselectstart() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().selectstartEvent);
}

void SVGElementInstance::setOnselectstart(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().selectstartEvent, eventListener);
}

EventListener* SVGElementInstance::onsubmit() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().submitEvent);
}

void SVGElementInstance::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().submitEvent, eventListener);
}

EventListener* SVGElementInstance::onunload() const
{
    return correspondingElement()->getAttributeEventListener(eventNames().unloadEvent);
}

void SVGElementInstance::setOnunload(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setAttributeEventListener(eventNames().unloadEvent, eventListener);
}

}

#endif // ENABLE(SVG)
