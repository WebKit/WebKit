/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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
#include "LayoutPoint.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MouseRelatedEvent);

MouseRelatedEvent::MouseRelatedEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers, IsSimulated isSimulated, IsTrusted isTrusted)
    : UIEventWithKeyState(eventType, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail, modifiers, isTrusted)
    , m_screenLocation(screenLocation)
#if ENABLE(POINTER_LOCK)
    , m_movementDelta(movementDelta)
#endif
    , m_isSimulated(isSimulated == IsSimulated::Yes)
{
#if !ENABLE(POINTER_LOCK)
    UNUSED_PARAM(movementDelta);
#endif
    init(m_isSimulated, windowLocation);
}

MouseRelatedEvent::MouseRelatedEvent(const AtomString& type, IsCancelable isCancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, const IntPoint& globalLocation, OptionSet<Modifier> modifiers)
    : MouseRelatedEvent(type, CanBubble::Yes, isCancelable, IsComposed::Yes, timestamp,
        WTFMove(view), 0, globalLocation, globalLocation /* Converted in init */, { }, modifiers, IsSimulated::No)
{
}

MouseRelatedEvent::MouseRelatedEvent(const AtomString& eventType, const MouseRelatedEventInit& initializer, IsTrusted isTrusted)
    : UIEventWithKeyState(eventType, initializer)
    , m_screenLocation(IntPoint(initializer.screenX, initializer.screenY))
#if ENABLE(POINTER_LOCK)
    , m_movementDelta(IntPoint(0, 0))
#endif
{
    ASSERT_UNUSED(isTrusted, isTrusted == IsTrusted::No);
    init(false, IntPoint(0, 0));
}

void MouseRelatedEvent::init(bool isSimulated, const IntPoint& windowLocation)
{
    if (!isSimulated) {
        if (auto* frameView = frameViewFromWindowProxy(view())) {
            FloatPoint absolutePoint = frameView->windowToContents(windowLocation);
            FloatPoint documentPoint = frameView->absoluteToDocumentPoint(absolutePoint);
            m_pageLocation = flooredLayoutPoint(documentPoint);
            m_clientLocation = pagePointToClientPoint(m_pageLocation, frameView);
        }
    }

    initCoordinates();
}

void MouseRelatedEvent::initCoordinates()
{
    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.
    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

FrameView* MouseRelatedEvent::frameViewFromWindowProxy(WindowProxy* windowProxy)
{
    if (!windowProxy || !is<DOMWindow>(windowProxy->window()))
        return nullptr;

    auto* frame = downcast<DOMWindow>(*windowProxy->window()).frame();
    return frame ? frame->view() : nullptr;
}

LayoutPoint MouseRelatedEvent::pagePointToClientPoint(LayoutPoint pagePoint, FrameView* frameView)
{
    if (!frameView)
        return pagePoint;

    return flooredLayoutPoint(frameView->documentToClientPoint(pagePoint));
}

LayoutPoint MouseRelatedEvent::pagePointToAbsolutePoint(LayoutPoint pagePoint, FrameView* frameView)
{
    if (!frameView)
        return pagePoint;
    
    return pagePoint.scaled(frameView->documentToAbsoluteScaleFactor());
}

void MouseRelatedEvent::initCoordinates(const LayoutPoint& clientLocation)
{
    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.
    FloatSize documentToClientOffset;
    if (auto* frameView = frameViewFromWindowProxy(view()))
        documentToClientOffset = frameView->documentToClientOffset();

    m_clientLocation = clientLocation;
    m_pageLocation = clientLocation - LayoutSize(documentToClientOffset);

    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

float MouseRelatedEvent::documentToAbsoluteScaleFactor() const
{
    if (auto* frameView = frameViewFromWindowProxy(view()))
        return frameView->documentToAbsoluteScaleFactor();

    return 1;
}

void MouseRelatedEvent::computePageLocation()
{
    m_absoluteLocation = pagePointToAbsolutePoint(m_pageLocation, frameViewFromWindowProxy(view()));
}

void MouseRelatedEvent::receivedTarget()
{
    m_hasCachedRelativePosition = false;
}

void MouseRelatedEvent::computeRelativePosition()
{
    if (!is<Node>(target()))
        return;
    auto& targetNode = downcast<Node>(*target());

    // Compute coordinates that are based on the target.
    m_layerLocation = m_pageLocation;
    m_offsetLocation = m_pageLocation;

    // Must have an updated render tree for this math to work correctly.
    targetNode.document().updateLayoutIgnorePendingStylesheets();

    // Adjust offsetLocation to be relative to the target's position.
    if (RenderObject* r = targetNode.renderer()) {
        m_offsetLocation = LayoutPoint(r->absoluteToLocal(absoluteLocation(), UseTransforms));
        float scaleFactor = 1 / documentToAbsoluteScaleFactor();
        if (scaleFactor != 1.0f)
            m_offsetLocation.scale(scaleFactor);
    }

    // Adjust layerLocation to be relative to the layer.
    // FIXME: event.layerX and event.layerY are poorly defined,
    // and probably don't always correspond to RenderLayer offsets.
    // https://bugs.webkit.org/show_bug.cgi?id=21868
    Node* n = &targetNode;
    while (n && !n->renderer())
        n = n->parentNode();

    RenderLayer* layer;
    if (n && (layer = n->renderer()->enclosingLayer())) {
        for (; layer; layer = layer->parent()) {
            m_layerLocation -= toLayoutSize(layer->location());
        }
    }

    m_hasCachedRelativePosition = true;
}
    
FloatPoint MouseRelatedEvent::locationInRootViewCoordinates() const
{
    if (auto* frameView = frameViewFromWindowProxy(view()))
        return frameView->contentsToRootView(roundedIntPoint(m_absoluteLocation));

    return m_absoluteLocation;
}

int MouseRelatedEvent::layerX()
{
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return m_layerLocation.x();
}

int MouseRelatedEvent::layerY()
{
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return m_layerLocation.y();
}

int MouseRelatedEvent::offsetX()
{
    if (isSimulated())
        return 0;
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return roundToInt(m_offsetLocation.x());
}

int MouseRelatedEvent::offsetY()
{
    if (isSimulated())
        return 0;
    if (!m_hasCachedRelativePosition)
        computeRelativePosition();
    return roundToInt(m_offsetLocation.y());
}

int MouseRelatedEvent::pageX() const
{
    return m_pageLocation.x();
}

int MouseRelatedEvent::pageY() const
{
    return m_pageLocation.y();
}

const LayoutPoint& MouseRelatedEvent::pageLocation() const
{
    return m_pageLocation;
}

int MouseRelatedEvent::x() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientLocation.x();
}

int MouseRelatedEvent::y() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientLocation.y();
}

} // namespace WebCore
