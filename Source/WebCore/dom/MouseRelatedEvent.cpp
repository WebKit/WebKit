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

#include "DocumentView.h"
#include "EventNames.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MouseEventInit.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderLayerInlines.h"
#include "RenderObject.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MouseRelatedEvent);

// FIXME: Remove this variant.
MouseRelatedEvent::MouseRelatedEvent()
    : UIEventWithKeyState(EventInterfaceType::Invalid)
{
}

MouseRelatedEvent::MouseRelatedEvent(enum EventInterfaceType eventInterface)
    : UIEventWithKeyState(eventInterface)
{
}

MouseRelatedEvent::MouseRelatedEvent(enum EventInterfaceType eventInterface, const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const DoublePoint& screenLocation, const DoublePoint& windowLocation, double movementX, double movementY, OptionSet<Modifier> modifiers, IsSimulated isSimulated, IsTrusted isTrusted)
    : UIEventWithKeyState(eventInterface, eventType, canBubble, isCancelable, isComposed, timestamp, WTF::move(view), detail, modifiers, isTrusted)
        , m_movementX(movementX)
        , m_movementY(movementY)
        , m_windowLocation(windowLocation)
        , m_screenLocation(screenLocation)
        , m_isSimulated(isSimulated == IsSimulated::Yes)
{
    init(m_isSimulated, windowLocation);
}

MouseRelatedEvent::MouseRelatedEvent(enum EventInterfaceType eventInterface, const AtomString& type, IsCancelable isCancelable, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, const DoublePoint& globalLocation, OptionSet<Modifier> modifiers)
    : MouseRelatedEvent(eventInterface, type, CanBubble::Yes, isCancelable, IsComposed::Yes, timestamp,
        WTF::move(view), 0, globalLocation, globalLocation /* Converted in init */, 0, 0, modifiers, IsSimulated::No)
{
}

MouseRelatedEvent::MouseRelatedEvent(enum EventInterfaceType eventInterface, const AtomString& eventType, const MouseEventInit& initializer, IsTrusted isTrusted)
    : UIEventWithKeyState(eventInterface, eventType, initializer, isTrusted)
        , m_movementX(initializer.movementX)
        , m_movementY(initializer.movementY)
        , m_screenLocation(DoublePoint(initializer.screenX, initializer.screenY))
{
    auto screenX = initializer.screenX;
    auto screenY = initializer.screenY;

    if (!std::isfinite(screenX))
        screenX = 0;
    if (!std::isfinite(screenY))
        screenY = 0;

    setScreenLocation({ screenX, screenY });

    init(false, DoublePoint::zero());
}

MouseRelatedEvent::MouseRelatedEvent(enum EventInterfaceType eventInterface, const AtomString& eventType, const EventModifierInit& initializer, IsTrusted isTrusted)
    : UIEventWithKeyState(eventInterface, eventType, initializer, isTrusted)
{
    init(false, DoublePoint::zero());
}

static inline bool isMoveEventType(const AtomString& eventType)
{
    auto& eventNames = WebCore::eventNames();
    return eventType == eventNames.mousemoveEvent
        || eventType == eventNames.pointermoveEvent
        || eventType == eventNames.touchmoveEvent;
}

void MouseRelatedEvent::init(bool isSimulated, const DoublePoint& windowLocation)
{
    if (!isSimulated) {
        if (RefPtr frameView = frameViewFromWindowProxy(view())) {
            DoublePoint absolutePoint = frameView->windowToContents(windowLocation);
            DoublePoint documentPoint = frameView->absoluteToDocumentPoint(absolutePoint);
            m_pageLocation = WTF::move(documentPoint);
            m_clientLocation = pagePointToClientPoint(m_pageLocation, frameView.get());
        }
    }

    initCoordinates();

    if (!isConstructedFromInitializer() && !isMoveEventType(type())) {
        m_movementX = 0;
        m_movementY = 0;
    }
}

void MouseRelatedEvent::initCoordinates()
{
    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.
    m_layerLocation = LayoutPoint(m_pageLocation);
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

LocalFrameView* MouseRelatedEvent::frameViewFromWindowProxy(WindowProxy* windowProxy)
{
    if (!windowProxy)
        return nullptr;

    RefPtr window = dynamicDowncast<LocalDOMWindow>(windowProxy->window());
    if (!window)
        return nullptr;

    RefPtr frame = window->localFrame();
    return frame ? frame->view() : nullptr;
}

DoublePoint MouseRelatedEvent::pagePointToClientPoint(DoublePoint pagePoint, LocalFrameView* frameView)
{
    if (!frameView)
        return pagePoint;

    return frameView->documentToClientPoint(pagePoint);
}

DoublePoint MouseRelatedEvent::pagePointToAbsolutePoint(DoublePoint pagePoint, LocalFrameView* frameView)
{
    if (!frameView)
        return pagePoint;
    
    return pagePoint.scaled(frameView->documentToAbsoluteScaleFactor());
}

void MouseRelatedEvent::initCoordinates(const DoublePoint& clientLocation)
{
    // Set up initial values for coordinates.
    // Correct values are computed lazily, see computeRelativePosition.

    auto documentToClientOffset = [&] -> DoubleSize {
        if (RefPtr frameView = frameViewFromWindowProxy(view()))
            return frameView->documentToClientOffset();
        return { };
    };

    m_clientLocation = clientLocation;
    m_pageLocation = clientLocation - documentToClientOffset();

    m_layerLocation = LayoutPoint(m_pageLocation);
    m_offsetLocation = m_pageLocation;

    computePageLocation();
    m_hasCachedRelativePosition = false;
}

float MouseRelatedEvent::documentToAbsoluteScaleFactor() const
{
    if (RefPtr frameView = frameViewFromWindowProxy(view()))
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
    RefPtr targetNode = dynamicDowncast<Node>(target());
    if (!targetNode)
        return;

    // Find the target renderer, adjusting for SVG elements.
    auto findTargetRendererAndAdjustedNode = [](const RefPtr<Node>& node) {
        CheckedPtr renderer = node->renderer();
        if (!renderer || !renderer->isSVGRenderer())
            return std::pair { renderer, node };

        // If this is an SVG node, compute the offset to the padding box of the
        // outermost SVG root (== the closest ancestor that has a CSS layout box.).
        while (!renderer->isRenderOrLegacyRenderSVGRoot())
            renderer = renderer->parent();

        // Update the target node to point to the SVG root.
        return std::pair { renderer, renderer->protectedNode() };
    };

    // Compute coordinates that are based on the target.
    m_layerLocation = LayoutPoint(m_pageLocation);
    m_offsetLocation = m_pageLocation;

    // Must have an updated render tree for this math to work correctly.
    protect(targetNode->document())->updateLayoutIgnorePendingStylesheets();

    // Adjust offsetLocation to be relative to the target's padding box.
    auto [renderer, adjustedNode] = findTargetRendererAndAdjustedNode(targetNode);
    if (targetNode != adjustedNode)
        targetNode = WTF::move(adjustedNode);

    if (renderer) {
        m_offsetLocation = renderer->absoluteToLocal(absoluteLocation(), UseTransforms);

        if (CheckedPtr boxModel = dynamicDowncast<RenderBoxModelObject>(renderer.get()))
            m_offsetLocation.move(-boxModel->borderLeft(), -boxModel->borderTop());

        float scaleFactor = 1 / documentToAbsoluteScaleFactor();
        if (scaleFactor != 1.0f)
            m_offsetLocation.scale(scaleFactor);
    }

    // Adjust layerLocation to be relative to the layer.
    // FIXME: event.layerX and event.layerY are poorly defined,
    // and probably don't always correspond to RenderLayer offsets.
    // https://bugs.webkit.org/show_bug.cgi?id=21868
    RefPtr node = WTF::move(targetNode);
    while (node && !node->renderer())
        node = node->parentNode();

    if (node) {
        CheckedPtr layer = node->renderer()->enclosingLayer();
        while (layer && !layer->isSelfPaintingLayer())
            layer = layer->parent();

        if (layer) {
            // Start with absoluteLocation which is already in absolute coordinates
            // (has zoom factor applied via documentToAbsoluteScaleFactor).
            auto layerLocationInAbsoluteCoords = absoluteLocation();

            // Convert layer position to absolute coordinates accounting for transforms.
            auto layerAbsolutePosition = layer->renderer().localToAbsolute(FloatPoint(), UseTransforms);

            // Subtract the layer's absolute position from the mouse absolute position.
            layerLocationInAbsoluteCoords.moveBy(-layerAbsolutePosition);

            // Scale back to page coordinates (remove zoom factor).
            auto inverseScaleFactor = 1 / documentToAbsoluteScaleFactor();
            m_layerLocation = LayoutPoint(layerLocationInAbsoluteCoords.scaled(inverseScaleFactor));
        }
    }

    m_hasCachedRelativePosition = true;
}

DoublePoint MouseRelatedEvent::locationInRootViewCoordinates() const
{
    if (RefPtr frameView = frameViewFromWindowProxy(view()))
        return frameView->contentsToRootView(m_absoluteLocation);

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

double MouseRelatedEvent::offsetX()
{
    if (isSimulated())
        return 0;

    if (!m_hasCachedRelativePosition)
        computeRelativePosition();

    return std::round(m_offsetLocation.x());
}

double MouseRelatedEvent::offsetY()
{
    if (isSimulated())
        return 0;

    if (!m_hasCachedRelativePosition)
        computeRelativePosition();

    return std::round(m_offsetLocation.y());
}

double MouseRelatedEvent::pageX() const
{
    return std::floor(m_pageLocation.x());
}

double MouseRelatedEvent::pageY() const
{
    return std::floor(m_pageLocation.y());
}

static double finiteValue(double value)
{
    return std::isfinite(value) ? value : 0;
}

double MouseRelatedEvent::screenX() const
{
    // FIXME: Determine whether or not we should restrict the range of this value
    // now that screenX is a double instead of an integer.
    return std::floor(finiteValue(m_screenLocation.x()));
}

double MouseRelatedEvent::screenY() const
{
    // FIXME: Determine whether or not we should restrict the range of this value
    // now that screenY is a double instead of an integer.
    return std::floor(finiteValue(m_screenLocation.y()));
}

double MouseRelatedEvent::clientX() const
{
    // FIXME: Determine whether or not we should restrict the range of this value
    // now that clientX is a double instead of an integer.
    return std::floor(finiteValue(m_clientLocation.x()));
}

double MouseRelatedEvent::clientY() const
{
    // FIXME: Determine whether or not we should restrict the range of this value
    // now that clientY is a double instead of an integer.
    return std::floor(finiteValue(m_clientLocation.y()));
}

} // namespace WebCore
