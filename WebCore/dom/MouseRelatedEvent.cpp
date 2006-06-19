/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "MouseRelatedEvent.h"

#include "AtomicString.h"
#include "Document.h"
#include "Node.h"
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

MouseRelatedEvent::MouseRelatedEvent(const AtomicString& eventType, bool canBubble, bool cancelable, AbstractView* view,
                                     int detail, int screenX, int screenY, int clientX, int clientY,
                                     bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool isSimulated)
    : UIEventWithKeyState(eventType, canBubble, cancelable, view, detail, ctrlKey, altKey, shiftKey, metaKey)
    , m_screenX(screenX)
    , m_screenY(screenY)
    , m_clientX(clientX)
    , m_clientY(clientY)
    , m_isSimulated(isSimulated)
{
    initCoordinates();
}

void MouseRelatedEvent::initCoordinates()
{
    // Set up initial values for coordinates.
    // Correct values can't be computed until we have at target, so receivedTarget
    // does the "real" computation.
    m_pageX = m_clientX;
    m_pageY = m_clientY;
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;
}

void MouseRelatedEvent::receivedTarget()
{
    // Compute coordinates that are based on the target.
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;
    m_layerX = m_pageX;    
    m_layerY = m_pageY;    

    Node* targ = target();
    ASSERT(targ);

    // Must have an updated render tree for this math to work correctly.
    targ->document()->updateRendering();

    // FIXME: clientX/Y should not be the same as pageX/Y!
    // Currently the passed-in clientX and clientY are incorrectly actually
    // pageX and pageY values, so we don't have any work to do here, but if
    // we started passing in correct clientX and clientY, we'd want to compute
    // pageX and pageY here.

    // Adjust offsetX/Y to be relative to the target's position.
    if (!isSimulated()) {
        if (RenderObject* r = targ->renderer()) {
            int rx, ry;
            if (r->absolutePosition(rx, ry)) {
                m_offsetX -= rx;
                m_offsetY -= ry;
            }
        }
    }

    // Adjust layerX/Y to be relative to the layer.
    // FIXME: We're pretty sure this is the wrong defintion of "layer."
    // Our RenderLayer is a more modern concept, and layerX/Y is some
    // other notion about groups of elements; we should test and fix this.
    Node* n = targ;
    while (n && !n->renderer())
        n = n->parent();
    if (n) {
        RenderLayer* layer = n->renderer()->enclosingLayer();
        layer->updateLayerPosition();
        for (; layer; layer = layer->parent()) {
            m_layerX -= layer->xPos();
            m_layerY -= layer->yPos();
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
