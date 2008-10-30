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
#include "GCController.h"
#include "SVGElementInstanceList.h"
#include "SVGUseElement.h"

#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

using namespace EventNames;

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
    // FIXME: This is fragile, as discussed with Sam. Need to find a better solution.
    // Think about the case where JS explicitely holds "var root = useElement.instanceRoot;".
    // We still have to recreate this wrapper somehow. The gc collection below, won't catch it.

    // If the use shadow tree has been rebuilt, just the JSSVGElementInstance objects
    // are still holding RefPtrs of SVGElementInstance objects, which prevent us to
    // be deleted (and the shadow tree is not destructed as well). Force JS GC.
    gcController().garbageCollectNow();    
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
        correspondingUseElement()->setChanged();
}

Frame* SVGElementInstance::associatedFrame() const
{
    if (SVGElement* element = correspondingElement())
        return element->associatedFrame();
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
    return element->dispatchGenericEvent(evt.release(), ec);
}

EventListener* SVGElementInstance::onabort() const
{
    return correspondingElement()->inlineEventListenerForType(abortEvent);
}

void SVGElementInstance::setOnabort(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(abortEvent, eventListener);
}

EventListener* SVGElementInstance::onblur() const
{
    return correspondingElement()->inlineEventListenerForType(blurEvent);
}

void SVGElementInstance::setOnblur(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(blurEvent, eventListener);
}

EventListener* SVGElementInstance::onchange() const
{
    return correspondingElement()->inlineEventListenerForType(changeEvent);
}

void SVGElementInstance::setOnchange(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(changeEvent, eventListener);
}

EventListener* SVGElementInstance::onclick() const
{
    return correspondingElement()->inlineEventListenerForType(clickEvent);
}

void SVGElementInstance::setOnclick(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(clickEvent, eventListener);
}

EventListener* SVGElementInstance::oncontextmenu() const
{
    return correspondingElement()->inlineEventListenerForType(contextmenuEvent);
}

void SVGElementInstance::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(contextmenuEvent, eventListener);
}

EventListener* SVGElementInstance::ondblclick() const
{
    return correspondingElement()->inlineEventListenerForType(dblclickEvent);
}

void SVGElementInstance::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dblclickEvent, eventListener);
}

EventListener* SVGElementInstance::onerror() const
{
    return correspondingElement()->inlineEventListenerForType(errorEvent);
}

void SVGElementInstance::setOnerror(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(errorEvent, eventListener);
}

EventListener* SVGElementInstance::onfocus() const
{
    return correspondingElement()->inlineEventListenerForType(focusEvent);
}

void SVGElementInstance::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(focusEvent, eventListener);
}

EventListener* SVGElementInstance::oninput() const
{
    return correspondingElement()->inlineEventListenerForType(inputEvent);
}

void SVGElementInstance::setOninput(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(inputEvent, eventListener);
}

EventListener* SVGElementInstance::onkeydown() const
{
    return correspondingElement()->inlineEventListenerForType(keydownEvent);
}

void SVGElementInstance::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(keydownEvent, eventListener);
}

EventListener* SVGElementInstance::onkeypress() const
{
    return correspondingElement()->inlineEventListenerForType(keypressEvent);
}

void SVGElementInstance::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(keypressEvent, eventListener);
}

EventListener* SVGElementInstance::onkeyup() const
{
    return correspondingElement()->inlineEventListenerForType(keyupEvent);
}

void SVGElementInstance::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(keyupEvent, eventListener);
}

EventListener* SVGElementInstance::onload() const
{
    return correspondingElement()->inlineEventListenerForType(loadEvent);
}

void SVGElementInstance::setOnload(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(loadEvent, eventListener);
}

EventListener* SVGElementInstance::onmousedown() const
{
    return correspondingElement()->inlineEventListenerForType(mousedownEvent);
}

void SVGElementInstance::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mousedownEvent, eventListener);
}

EventListener* SVGElementInstance::onmousemove() const
{
    return correspondingElement()->inlineEventListenerForType(mousemoveEvent);
}

void SVGElementInstance::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mousemoveEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseout() const
{
    return correspondingElement()->inlineEventListenerForType(mouseoutEvent);
}

void SVGElementInstance::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mouseoutEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseover() const
{
    return correspondingElement()->inlineEventListenerForType(mouseoverEvent);
}

void SVGElementInstance::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mouseoverEvent, eventListener);
}

EventListener* SVGElementInstance::onmouseup() const
{
    return correspondingElement()->inlineEventListenerForType(mouseupEvent);
}

void SVGElementInstance::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mouseupEvent, eventListener);
}

EventListener* SVGElementInstance::onmousewheel() const
{
    return correspondingElement()->inlineEventListenerForType(mousewheelEvent);
}

void SVGElementInstance::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(mousewheelEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforecut() const
{
    return correspondingElement()->inlineEventListenerForType(beforecutEvent);
}

void SVGElementInstance::setOnbeforecut(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(beforecutEvent, eventListener);
}

EventListener* SVGElementInstance::oncut() const
{
    return correspondingElement()->inlineEventListenerForType(cutEvent);
}

void SVGElementInstance::setOncut(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(cutEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforecopy() const
{
    return correspondingElement()->inlineEventListenerForType(beforecopyEvent);
}

void SVGElementInstance::setOnbeforecopy(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(beforecopyEvent, eventListener);
}

EventListener* SVGElementInstance::oncopy() const
{
    return correspondingElement()->inlineEventListenerForType(copyEvent);
}

void SVGElementInstance::setOncopy(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(copyEvent, eventListener);
}

EventListener* SVGElementInstance::onbeforepaste() const
{
    return correspondingElement()->inlineEventListenerForType(beforepasteEvent);
}

void SVGElementInstance::setOnbeforepaste(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(beforepasteEvent, eventListener);
}

EventListener* SVGElementInstance::onpaste() const
{
    return correspondingElement()->inlineEventListenerForType(pasteEvent);
}

void SVGElementInstance::setOnpaste(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(pasteEvent, eventListener);
}

EventListener* SVGElementInstance::ondragenter() const
{
    return correspondingElement()->inlineEventListenerForType(dragenterEvent);
}

void SVGElementInstance::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragenterEvent, eventListener);
}

EventListener* SVGElementInstance::ondragover() const
{
    return correspondingElement()->inlineEventListenerForType(dragoverEvent);
}

void SVGElementInstance::setOndragover(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragoverEvent, eventListener);
}

EventListener* SVGElementInstance::ondragleave() const
{
    return correspondingElement()->inlineEventListenerForType(dragleaveEvent);
}

void SVGElementInstance::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragleaveEvent, eventListener);
}

EventListener* SVGElementInstance::ondrop() const
{
    return correspondingElement()->inlineEventListenerForType(dropEvent);
}

void SVGElementInstance::setOndrop(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dropEvent, eventListener);
}

EventListener* SVGElementInstance::ondragstart() const
{
    return correspondingElement()->inlineEventListenerForType(dragstartEvent);
}

void SVGElementInstance::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragstartEvent, eventListener);
}

EventListener* SVGElementInstance::ondrag() const
{
    return correspondingElement()->inlineEventListenerForType(dragEvent);
}

void SVGElementInstance::setOndrag(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragEvent, eventListener);
}

EventListener* SVGElementInstance::ondragend() const
{
    return correspondingElement()->inlineEventListenerForType(dragendEvent);
}

void SVGElementInstance::setOndragend(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(dragendEvent, eventListener);
}

EventListener* SVGElementInstance::onreset() const
{
    return correspondingElement()->inlineEventListenerForType(resetEvent);
}

void SVGElementInstance::setOnreset(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(resetEvent, eventListener);
}

EventListener* SVGElementInstance::onresize() const
{
    return correspondingElement()->inlineEventListenerForType(resizeEvent);
}

void SVGElementInstance::setOnresize(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(resizeEvent, eventListener);
}

EventListener* SVGElementInstance::onscroll() const
{
    return correspondingElement()->inlineEventListenerForType(scrollEvent);
}

void SVGElementInstance::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(scrollEvent, eventListener);
}

EventListener* SVGElementInstance::onsearch() const
{
    return correspondingElement()->inlineEventListenerForType(searchEvent);
}

void SVGElementInstance::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(searchEvent, eventListener);
}

EventListener* SVGElementInstance::onselect() const
{
    return correspondingElement()->inlineEventListenerForType(selectEvent);
}

void SVGElementInstance::setOnselect(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(selectEvent, eventListener);
}

EventListener* SVGElementInstance::onselectstart() const
{
    return correspondingElement()->inlineEventListenerForType(selectstartEvent);
}

void SVGElementInstance::setOnselectstart(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(selectstartEvent, eventListener);
}

EventListener* SVGElementInstance::onsubmit() const
{
    return correspondingElement()->inlineEventListenerForType(submitEvent);
}

void SVGElementInstance::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(submitEvent, eventListener);
}

EventListener* SVGElementInstance::onunload() const
{
    return correspondingElement()->inlineEventListenerForType(unloadEvent);
}

void SVGElementInstance::setOnunload(PassRefPtr<EventListener> eventListener)
{
    correspondingElement()->setInlineEventListenerForType(unloadEvent, eventListener);
}

}

#endif // ENABLE(SVG)
