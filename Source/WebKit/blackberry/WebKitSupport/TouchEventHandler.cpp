/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "TouchEventHandler.h"

#include "DOMSupport.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "InRegionScroller_p.h"
#include "InputHandler.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Node.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SelectionHandler.h"
#include "WebPage_p.h"
#include "WebTapHighlight.h"

#include <wtf/MathExtras.h>

using namespace WebCore;
using namespace WTF;

namespace BlackBerry {
namespace WebKit {

static bool hasMouseMoveListener(Element* element)
{
    ASSERT(element);
    return element->hasEventListeners(eventNames().mousemoveEvent) || element->document()->hasEventListeners(eventNames().mousemoveEvent);
}

static bool hasTouchListener(Element* element)
{
    ASSERT(element);
    return element->hasEventListeners(eventNames().touchstartEvent)
        || element->hasEventListeners(eventNames().touchmoveEvent)
        || element->hasEventListeners(eventNames().touchcancelEvent)
        || element->hasEventListeners(eventNames().touchendEvent);
}

static bool isRangeControlElement(Element* element)
{
    ASSERT(element);
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
    return inputElement->isRangeControl();
}

static bool shouldConvertTouchToMouse(Element* element)
{
    if (!element)
        return false;

    if ((element->hasTagName(HTMLNames::objectTag) || element->hasTagName(HTMLNames::embedTag)) && static_cast<HTMLPlugInElement*>(element))
        return true;

    // Input Range element is a special case that requires natural mouse events
    // in order to allow dragging of the slider handle.
    // Input Range element might appear in the webpage, or it might appear in the shadow tree,
    // like the timeline and volume media controls all use Input Range.
    // won't operate on the shadow node of other element type, because the webpages
    // aren't able to attach listeners to shadow content.
    do {
        if (isRangeControlElement(element))
            return true;
        element = toElement(element->shadowAncestorNode()); // If an element is not in shadow tree, shadowAncestorNode returns itself.
    } while (element->isInShadowTree());

    // Check if the element has a mouse listener and no touch listener. If so,
    // the field will require touch events be converted to mouse events to function properly.
    return hasMouseMoveListener(element) && !hasTouchListener(element);

}

TouchEventHandler::TouchEventHandler(WebPagePrivate* webpage)
    : m_webPage(webpage)
    , m_didCancelTouch(false)
    , m_convertTouchToMouse(false)
    , m_existingTouchMode(ProcessedTouchEvents)
{
}

TouchEventHandler::~TouchEventHandler()
{
}

bool TouchEventHandler::shouldSuppressMouseDownOnTouchDown() const
{
    return m_lastFatFingersResult.isTextInput() || m_webPage->m_inputHandler->isInputMode() || m_webPage->m_selectionHandler->isSelectionActive();
}

void TouchEventHandler::touchEventCancel()
{
    m_webPage->m_inputHandler->processPendingKeyboardVisibilityChange();

    // Input elements delay mouse down and do not need to be released on touch cancel.
    if (!shouldSuppressMouseDownOnTouchDown())
        m_webPage->m_page->focusController()->focusedOrMainFrame()->eventHandler()->setMousePressed(false);

    m_convertTouchToMouse = m_webPage->m_touchEventMode == PureTouchEventsWithMouseConversion;
    m_didCancelTouch = true;

    // If we cancel a single touch event, we need to also clean up any hover
    // state we get into by synthetically moving the mouse to the m_fingerPoint.
    Element* elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
    if (elementUnderFatFinger && elementUnderFatFinger->renderer()) {

        HitTestRequest request(HitTestRequest::FingerUp);
        // The HitTestResult point is not actually needed.
        HitTestResult result(IntPoint::zero());
        result.setInnerNode(elementUnderFatFinger);

        Document* document = elementUnderFatFinger->document();
        ASSERT(document);
        document->renderView()->layer()->updateHoverActiveState(request, result);
        document->updateStyleIfNeeded();
        // Updating the document style may destroy the renderer.
        if (elementUnderFatFinger->renderer())
            elementUnderFatFinger->renderer()->repaint();
        ASSERT(!elementUnderFatFinger->hovered());
    }

    m_lastFatFingersResult.reset();
}

void TouchEventHandler::touchHoldEvent()
{
    // This is a hack for our hack that converts the touch pressed event that we've delayed because the user has focused a input field
    // to the page as a mouse pressed event.
    if (shouldSuppressMouseDownOnTouchDown())
        handleFatFingerPressed();

    // Clear the focus ring indication if tap-and-hold'ing on a link.
    if (m_lastFatFingersResult.node() && m_lastFatFingersResult.node()->isLink())
        m_webPage->clearFocusNode();
}

bool TouchEventHandler::handleTouchPoint(Platform::TouchPoint& point, bool useFatFingers)
{
    // Enable input mode on any touch event.
    m_webPage->m_inputHandler->setInputModeEnabled();
    bool pureWithMouseConversion = m_webPage->m_touchEventMode == PureTouchEventsWithMouseConversion;

    switch (point.m_state) {
    case Platform::TouchPoint::TouchPressed:
        {
            // FIXME: bypass FatFingers if useFatFingers is false
            m_lastFatFingersResult.reset(); // Theoretically this shouldn't be required. Keep it just in case states get mangled.
            m_didCancelTouch = false;
            m_lastScreenPoint = point.m_screenPos;

            IntPoint contentPos(m_webPage->mapFromViewportToContents(point.m_pos));

            m_lastFatFingersResult = FatFingers(m_webPage, contentPos, FatFingers::ClickableElement).findBestPoint();

            Element* elementUnderFatFinger = 0;
            if (m_lastFatFingersResult.positionWasAdjusted() && m_lastFatFingersResult.node()) {
                ASSERT(m_lastFatFingersResult.node()->isElementNode());
                elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
            }

            // Set or reset the touch mode.
            Element* possibleTargetNodeForMouseMoveEvents = static_cast<Element*>(m_lastFatFingersResult.positionWasAdjusted() ? elementUnderFatFinger : m_lastFatFingersResult.node());
            m_convertTouchToMouse = pureWithMouseConversion ? true : shouldConvertTouchToMouse(possibleTargetNodeForMouseMoveEvents);

            if (elementUnderFatFinger)
                drawTapHighlight();

            // Lets be conservative here: since we have problems on major website having
            // mousemove listener for no good reason (e.g. google.com, desktop edition),
            // let only delay client notifications when there is not input text node involved.
            if (m_convertTouchToMouse
                && (m_webPage->m_inputHandler->isInputMode() && !m_lastFatFingersResult.isTextInput())) {
                m_webPage->m_inputHandler->setDelayKeyboardVisibilityChange(true);
                handleFatFingerPressed();
            } else if (!shouldSuppressMouseDownOnTouchDown())
                handleFatFingerPressed();

            return true;
        }
    case Platform::TouchPoint::TouchReleased:
        {
            imf_sp_text_t spellCheckOptionRequest;
            bool shouldRequestSpellCheckOptions = false;

            if (m_lastFatFingersResult.isTextInput())
                shouldRequestSpellCheckOptions = m_webPage->m_inputHandler->shouldRequestSpellCheckingOptionsForPoint(point.m_pos, m_lastFatFingersResult.nodeAsElementIfApplicable(), spellCheckOptionRequest);

            // Apply any suppressed changes. This does not eliminate the need
            // for the show after the handling of fat finger pressed as it may
            // have triggered a state change.
            m_webPage->m_inputHandler->processPendingKeyboardVisibilityChange();

            if (shouldSuppressMouseDownOnTouchDown())
                handleFatFingerPressed();

            // The rebase has eliminated a necessary event when the mouse does not
            // trigger an actual selection change preventing re-showing of the
            // keyboard. If input mode is active, call showVirtualKeyboard which
            // will update the state and display keyboard if needed.
            if (m_webPage->m_inputHandler->isInputMode())
                m_webPage->m_inputHandler->notifyClientOfKeyboardVisibilityChange(true);

            IntPoint adjustedPoint;
            if (m_convertTouchToMouse || !useFatFingers) {
                adjustedPoint = point.m_pos;
                m_convertTouchToMouse = pureWithMouseConversion;
            } else // Fat finger point in viewport coordinates.
                adjustedPoint = m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition());

            PlatformMouseEvent mouseEvent(adjustedPoint, m_lastScreenPoint, PlatformEvent::MouseReleased, 1, LeftButton, TouchScreen);
            m_webPage->handleMouseEvent(mouseEvent);
            m_lastFatFingersResult.reset(); // Reset the fat finger result as its no longer valid when a user's finger is not on the screen.
            if (shouldRequestSpellCheckOptions)
                m_webPage->m_inputHandler->requestSpellingCheckingOptions(spellCheckOptionRequest);
            return true;
        }
    case Platform::TouchPoint::TouchMoved:
        if (m_convertTouchToMouse) {
            PlatformMouseEvent mouseEvent(point.m_pos, m_lastScreenPoint, PlatformEvent::MouseMoved, 1, LeftButton, TouchScreen);
            m_lastScreenPoint = point.m_screenPos;
            if (!m_webPage->handleMouseEvent(mouseEvent)) {
                m_convertTouchToMouse = pureWithMouseConversion;
                return false;
            }
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

void TouchEventHandler::handleFatFingerPressed()
{
    if (!m_didCancelTouch) {
        // First update the mouse position with a MouseMoved event.
        PlatformMouseEvent mouseMoveEvent(m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition()), m_lastScreenPoint, PlatformEvent::MouseMoved, 0, LeftButton, TouchScreen);
        m_webPage->handleMouseEvent(mouseMoveEvent);

        // Then send the MousePressed event.
        PlatformMouseEvent mousePressedEvent(m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition()), m_lastScreenPoint, PlatformEvent::MousePressed, 1, LeftButton, TouchScreen);
        m_webPage->handleMouseEvent(mousePressedEvent);
    }
}

// This method filters what element will get tap-highlight'ed or not. To start with,
// we are going to highlight links (anchors with a valid href element), and elements
// whose tap highlight color value is different than the default value.
static Element* elementForTapHighlight(Element* elementUnderFatFinger)
{
    // Do not bail out right way here if there element does not have a renderer.
    // It is the casefor <map> (descendent of <area>) elements. The associated <image>
    // element actually has the renderer.
    if (elementUnderFatFinger->renderer()) {
        Color tapHighlightColor = elementUnderFatFinger->renderStyle()->tapHighlightColor();
        if (tapHighlightColor != RenderTheme::defaultTheme()->platformTapHighlightColor())
            return elementUnderFatFinger;
    }

    bool isArea = elementUnderFatFinger->hasTagName(HTMLNames::areaTag);
    Node* linkNode = elementUnderFatFinger->enclosingLinkEventParentOrSelf();
    if (!linkNode || !linkNode->isHTMLElement() || (!linkNode->renderer() && !isArea))
        return 0;

    ASSERT(linkNode->isLink());

    // FatFingers class selector ensure only anchor with valid href attr value get here.
    // It includes empty hrefs.
    Element* highlightCandidateElement = static_cast<Element*>(linkNode);

    if (!isArea)
        return highlightCandidateElement;

    HTMLAreaElement* area = static_cast<HTMLAreaElement*>(highlightCandidateElement);
    HTMLImageElement* image = area->imageElement();
    if (image && image->renderer())
        return image;

    return 0;
}

void TouchEventHandler::drawTapHighlight()
{
    Element* elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
    if (!elementUnderFatFinger)
        return;

    Element* element = elementForTapHighlight(elementUnderFatFinger);
    if (!element)
        return;

    // Get the element bounding rect in transformed coordinates so we can extract
    // the focus ring relative position each rect.
    RenderObject* renderer = element->renderer();
    ASSERT(renderer);

    Frame* elementFrame = element->document()->frame();
    ASSERT(elementFrame);

    FrameView* elementFrameView = elementFrame->view();
    if (!elementFrameView)
        return;

    // Tell the client if the element is either in a scrollable container or in a fixed positioned container.
    // On the client side, this info is being used to hide the tap highlight window on scroll.
    RenderLayer* layer = m_webPage->enclosingFixedPositionedAncestorOrSelfIfFixedPositioned(renderer->enclosingLayer());
    bool shouldHideTapHighlightRightAfterScrolling = !layer->renderer()->isRenderView();
    shouldHideTapHighlightRightAfterScrolling |= !!m_webPage->m_inRegionScroller->d->node();

    IntPoint framePos(m_webPage->frameOffset(elementFrame));

    // FIXME: We can get more precise on the <map> case by calculating the rect with HTMLAreaElement::computeRect().
    IntRect absoluteRect(renderer->absoluteClippedOverflowRect());
    absoluteRect.move(framePos.x(), framePos.y());

    IntRect clippingRect;
    if (elementFrame == m_webPage->mainFrame())
        clippingRect = IntRect(IntPoint(0, 0), elementFrameView->contentsSize());
    else
        clippingRect = m_webPage->mainFrame()->view()->windowToContents(m_webPage->getRecursiveVisibleWindowRect(elementFrameView, true /*noClipToMainFrame*/));
    clippingRect = intersection(absoluteRect, clippingRect);

    Vector<FloatQuad> focusRingQuads;
    renderer->absoluteFocusRingQuads(focusRingQuads);

    Platform::IntRectRegion region;
    for (size_t i = 0; i < focusRingQuads.size(); ++i) {
        IntRect rect = focusRingQuads[i].enclosingBoundingBox();
        rect.move(framePos.x(), framePos.y());
        IntRect clippedRect = intersection(clippingRect, rect);
        clippedRect.inflate(2);
        region = unionRegions(region, Platform::IntRect(clippedRect));
    }

    Color highlightColor = element->renderStyle()->tapHighlightColor();

    m_webPage->m_tapHighlight->draw(region,
                                    highlightColor.red(), highlightColor.green(), highlightColor.blue(), highlightColor.alpha(),
                                    shouldHideTapHighlightRightAfterScrolling);
}

}
}
