/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "MouseRelatedEvent.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "RenderLayer.h"
#include "RenderObject.h"

namespace WebCore {

MouseRelatedEvent::MouseRelatedEvent()
    : m_screenX(0)
    , m_screenY(0)
    , m_clientX(0)
    , m_clientY(0)
    , m_pageX(0)
    , m_pageY(0)
    , m_layerX(0)
    , m_layerY(0)
    , m_offsetX(0)
    , m_offsetY(0)
    , m_isSimulated(false)
{
}

static int contentsX(AbstractView* abstractView)
{
    if (!abstractView)
        return 0;
    Frame* frame = abstractView->frame();
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    return frameView->scrollX() / frame->pageZoomFactor();
}

static int contentsY(AbstractView* abstractView)
{
    if (!abstractView)
        return 0;
    Frame* frame = abstractView->frame();
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    return frameView->scrollY() / frame->pageZoomFactor();
}

MouseRelatedEvent::MouseRelatedEvent(const AtomicString& eventType, bool canBubble, bool cancelable, PassRefPtr<AbstractView> viewArg,
                                     int detail, int screenX, int screenY, int pageX, int pageY,
                                     bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool isSimulated)
    : UIEventWithKeyState(eventType, canBubble, cancelable, viewArg, detail, ctrlKey, altKey, shiftKey, metaKey)
    , m_screenX(screenX)
    , m_screenY(screenY)
    , m_clientX(pageX - contentsX(view()))
    , m_clientY(pageY - contentsY(view()))
    , m_pageX(pageX)
    , m_pageY(pageY)
    , m_isSimulated(isSimulated)
{
    initCoordinates();
}

void MouseRelatedEvent::initCoordinates()
{
    // Set up initial values for coordinates.
    // Correct values can't be computed until we have at target, so receivedTarget
    // does the "real" computation.
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;

    computePageLocation();
}

void MouseRelatedEvent::initCoordinates(int clientX, int clientY)
{
    // Set up initial values for coordinates.
    // Correct values can't be computed until we have at target, so receivedTarget
    // does the "real" computation.
    m_clientX = clientX;
    m_clientY = clientY;
    m_pageX = clientX + contentsX(view());
    m_pageY = clientY + contentsY(view());
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;

    computePageLocation();
}

static float pageZoomFactor(UIEvent* event)
{
    DOMWindow* window = event->view();
    if (!window)
        return 1;
    Frame* frame = window->frame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void MouseRelatedEvent::computePageLocation()
{
    float zoomFactor = pageZoomFactor(this);
    setAbsoluteLocation(roundedIntPoint(FloatPoint(pageX() * zoomFactor, pageY() * zoomFactor)));
}

void MouseRelatedEvent::receivedTarget()
{
    ASSERT(target());
    Node* targ = target()->toNode();
    if (!targ)
        return;

    // Compute coordinates that are based on the target.
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;

    // Must have an updated render tree for this math to work correctly.
    targ->document()->updateStyleIfNeeded();

    // Adjust offsetX/Y to be relative to the target's position.
    if (!isSimulated()) {
        if (RenderObject* r = targ->renderer()) {
            FloatPoint localPos = r->absoluteToLocal(absoluteLocation(), false, true);
            float zoomFactor = pageZoomFactor(this);
            m_offsetX = lroundf(localPos.x() / zoomFactor);
            m_offsetY = lroundf(localPos.y() / zoomFactor);
        }
    }

    // Adjust layerX/Y to be relative to the layer.
    // FIXME: We're pretty sure this is the wrong definition of "layer."
    // Our RenderLayer is a more modern concept, and layerX/Y is some
    // other notion about groups of elements (left over from the Netscape 4 days?);
    // we should test and fix this.
    Node* n = targ;
    while (n && !n->renderer())
        n = n->parentNode();
    if (n) {
        RenderLayer* layer = n->renderer()->enclosingLayer();
        layer->updateLayerPosition();
        for (; layer; layer = layer->parent()) {
            m_layerX -= layer->x();
            m_layerY -= layer->y();
        }
    }
}

int MouseRelatedEvent::pageX() const
{
    return m_pageX;
}

int MouseRelatedEvent::pageY() const
{
    return m_pageY;
}

int MouseRelatedEvent::x() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientX;
}

int MouseRelatedEvent::y() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientY;
}

} // namespace WebCore
