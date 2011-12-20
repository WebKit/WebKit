/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PopupContainer.h"

#include "Chrome.h"
#include "ChromeClientChromium.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "FramelessScrollView.h"
#include "FramelessScrollViewClient.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformScreen.h"
#include "PlatformWheelEvent.h"
#include "PopupListBox.h"
#include "PopupMenuClient.h"
#include "UserGestureIndicator.h"
#include <limits>

#if ENABLE(GESTURE_EVENTS)
#include "PlatformGestureEvent.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "PlatformTouchEvent.h"
#endif

using namespace std;

namespace WebCore {

static const int kBorderSize = 1;

static PlatformMouseEvent constructRelativeMouseEvent(const PlatformMouseEvent& e,
                                                      FramelessScrollView* parent,
                                                      FramelessScrollView* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.position());

    // FIXME: This is a horrible hack since PlatformMouseEvent has no setters for x/y.
    PlatformMouseEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.position());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

static PlatformWheelEvent constructRelativeWheelEvent(const PlatformWheelEvent& e,
                                                      FramelessScrollView* parent,
                                                      FramelessScrollView* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.position());

    // FIXME: This is a horrible hack since PlatformWheelEvent has no setters for x/y.
    PlatformWheelEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.position());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

// static
PassRefPtr<PopupContainer> PopupContainer::create(PopupMenuClient* client,
                                                  PopupType popupType,
                                                  const PopupContainerSettings& settings)
{
    return adoptRef(new PopupContainer(client, popupType, settings));
}

PopupContainer::PopupContainer(PopupMenuClient* client,
                               PopupType popupType,
                               const PopupContainerSettings& settings)
    : m_listBox(PopupListBox::create(client, settings))
    , m_settings(settings)
    , m_popupType(popupType)
    , m_popupOpen(false)
{
    setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
}

PopupContainer::~PopupContainer()
{
    if (m_listBox && m_listBox->parent())
        removeChild(m_listBox.get());
}

IntRect PopupContainer::layoutAndCalculateWidgetRect(int targetControlHeight, const IntPoint& popupInitialCoordinate)
{
    // Reset the max width and height to their default values, they will be recomputed below
    // if necessary.
    m_listBox->setMaxHeight(kMaxHeight);
    m_listBox->setMaxWidth(std::numeric_limits<int>::max());

    // Lay everything out to figure out our preferred size, then tell the view's
    // WidgetClient about it. It should assign us a client.
    int rtlOffset = layoutAndGetRTLOffset();
    bool isRTL = this->isRTL();
    int rightOffset = isRTL ? rtlOffset : 0;

    // Assume m_listBox size is already calculated.
    IntSize targetSize(m_listBox->width() + kBorderSize * 2,
                       m_listBox->height() + kBorderSize * 2);

    IntRect widgetRect;
    ChromeClientChromium* chromeClient = chromeClientChromium();
    if (chromeClient) {
        // If the popup would extend past the bottom of the screen, open upwards
        // instead.
        FloatRect screen = screenAvailableRect(m_frameView.get());
        // Use popupInitialCoordinate.x() + rightOffset because RTL position
        // needs to be considered.
        widgetRect = chromeClient->rootViewToScreen(IntRect(popupInitialCoordinate.x() + rightOffset, popupInitialCoordinate.y(), targetSize.width(), targetSize.height()));

        // If we have multiple screens and the browser rect is in one screen, we have
        // to clip the window width to the screen width.
        // When clipping, we also need to set a maximum width for the list box.
        FloatRect windowRect = chromeClient->windowRect();
        if (windowRect.x() >= screen.x() && windowRect.maxX() <= screen.maxX() && (widgetRect.x() < screen.x() || widgetRect.maxX() > screen.maxX())) {
            // First, inverse the popup alignment if it does not fit the screen - this might fix things (or make them better).
            IntRect inverseWidgetRect = chromeClient->rootViewToScreen(IntRect(popupInitialCoordinate.x() + (isRTL ? 0 : rtlOffset), popupInitialCoordinate.y(), targetSize.width(), targetSize.height()));
            IntRect enclosingScreen = enclosingIntRect(screen);
            unsigned originalCutoff = max(enclosingScreen.x() - widgetRect.x(), 0) + max(widgetRect.maxX() - enclosingScreen.maxX(), 0);
            unsigned inverseCutoff = max(enclosingScreen.x() - inverseWidgetRect.x(), 0) + max(inverseWidgetRect.maxX() - enclosingScreen.maxX(), 0);

            // Accept the inverse popup alignment if the trimmed content gets shorter than that in the original alignment case.
            if (inverseCutoff < originalCutoff)
                widgetRect = inverseWidgetRect;

            if (widgetRect.x() < screen.x()) {
                unsigned widgetRight = widgetRect.maxX();
                widgetRect.setWidth(widgetRect.maxX() - screen.x());
                widgetRect.setX(widgetRight - widgetRect.width());
                listBox()->setMaxWidthAndLayout(max(widgetRect.width() - kBorderSize * 2, 0));
            } else if (widgetRect.maxX() > screen.maxX()) {
                widgetRect.setWidth(screen.maxX() - widgetRect.x());
                listBox()->setMaxWidthAndLayout(max(widgetRect.width() - kBorderSize * 2, 0));
            }
        }

        // Calculate Y axis size.
        if (widgetRect.maxY() > static_cast<int>(screen.maxY())) {
            if (widgetRect.y() - widgetRect.height() - targetControlHeight > 0) {
                // There is enough room to open upwards.
                widgetRect.move(0, -(widgetRect.height() + targetControlHeight));
            } else {
                // Figure whether upwards or downwards has more room and set the
                // maximum number of items.
                int spaceAbove = widgetRect.y() - targetControlHeight;
                int spaceBelow = screen.maxY() - widgetRect.y();
                if (spaceAbove > spaceBelow)
                    m_listBox->setMaxHeight(spaceAbove);
                else
                    m_listBox->setMaxHeight(spaceBelow);
                layoutAndGetRTLOffset();
                // Our height has changed, so recompute only Y axis of widgetRect.
                // We don't have to recompute X axis, so we only replace Y axis
                // in widgetRect.
                IntRect frameInScreen = chromeClient->rootViewToScreen(frameRect());
                widgetRect.setY(frameInScreen.y());
                widgetRect.setHeight(frameInScreen.height());
                // And move upwards if necessary.
                if (spaceAbove > spaceBelow)
                    widgetRect.move(0, -(widgetRect.height() + targetControlHeight));
            }
        }
    }
    return widgetRect;
}

void PopupContainer::showPopup(FrameView* view)
{
    m_frameView = view;
    listBox()->m_focusedNode = m_frameView->frame()->document()->focusedNode();

    ChromeClientChromium* chromeClient = chromeClientChromium();
    if (chromeClient) {
        IntRect popupRect = frameRect();
        chromeClient->popupOpened(this, layoutAndCalculateWidgetRect(popupRect.height(), popupRect.location()), false);
        m_popupOpen = true;
    }

    if (!m_listBox->parent())
        addChild(m_listBox.get());

    // Enable scrollbars after the listbox is inserted into the hierarchy,
    // so it has a proper WidgetClient.
    m_listBox->setVerticalScrollbarMode(ScrollbarAuto);

    m_listBox->scrollToRevealSelection();

    invalidate();
}

void PopupContainer::hidePopup()
{
    listBox()->hidePopup();
}

void PopupContainer::notifyPopupHidden()
{
    if (!m_popupOpen)
        return;
    m_popupOpen = false;
    chromeClientChromium()->popupClosed(this);
}

int PopupContainer::layoutAndGetRTLOffset()
{
    m_listBox->layout();

    // Place the listbox within our border.
    m_listBox->move(kBorderSize, kBorderSize);

    // Size ourselves to contain listbox + border.
    int listBoxWidth = m_listBox->width() + kBorderSize * 2;
    resize(listBoxWidth, m_listBox->height() + kBorderSize * 2);
    invalidate();

    // Compute the starting x-axis for a normal RTL or right-aligned LTR dropdown. For those,
    // the right edge of dropdown box should be aligned with the right edge of <select>/<input> element box,
    // and the dropdown box should be expanded to the left if more space is needed.
    // m_originalFrameRect.width() is the width of the target <select>/<input> element.
    return m_originalFrameRect.width() - listBoxWidth;
}

bool PopupContainer::handleMouseDownEvent(const PlatformMouseEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    return m_listBox->handleMouseDownEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    return m_listBox->handleMouseMoveEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    RefPtr<PopupContainer> protect(this);
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    return m_listBox->handleMouseReleaseEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleWheelEvent(const PlatformWheelEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    return m_listBox->handleWheelEvent(
        constructRelativeWheelEvent(event, this, m_listBox.get()));
}

#if ENABLE(TOUCH_EVENTS)
bool PopupContainer::handleTouchEvent(const PlatformTouchEvent&)
{
    return false;
}
#endif

#if ENABLE(GESTURE_RECOGNIZER)
// FIXME: Refactor this code to share functionality with EventHandler::handleGestureEvent.
bool PopupContainer::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    switch (gestureEvent.type()) {
    case PlatformEvent::GestureTap: {
        PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(), NoButton, PlatformEvent::MouseMoved, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), gestureEvent.timestamp());
        PlatformMouseEvent fakeMouseDown(gestureEvent.position(), gestureEvent.globalPosition(), LeftButton, PlatformEvent::MousePressed, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), gestureEvent.timestamp());
        PlatformMouseEvent fakeMouseUp(gestureEvent.position(), gestureEvent.globalPosition(), LeftButton, PlatformEvent::MouseReleased, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), gestureEvent.timestamp());
        // handleMouseMoveEvent(fakeMouseMove);
        handleMouseDownEvent(fakeMouseDown);
        handleMouseReleaseEvent(fakeMouseUp);
        return true;
    }
    case PlatformEvent::GestureDoubleTap:
        break;
    case PlatformEvent::GestureScrollUpdate: {
        PlatformWheelEvent syntheticWheelEvent(gestureEvent.position(), gestureEvent.globalPosition(), gestureEvent.deltaX(), gestureEvent.deltaY(), gestureEvent.deltaX() / 120.0f, gestureEvent.deltaY() / 120.0f, ScrollByPixelWheelEvent, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey());
        handleWheelEvent(syntheticWheelEvent);
        return true;
    }
    case PlatformEvent::GestureScrollBegin:
    case PlatformEvent::GestureScrollEnd:
    case PlatformEvent::GestureTapDown:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}
#endif

bool PopupContainer::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);
    return m_listBox->handleKeyEvent(event);
}

void PopupContainer::hide()
{
    m_listBox->abandon();
}

void PopupContainer::paint(GraphicsContext* gc, const IntRect& rect)
{
    // adjust coords for scrolled frame
    IntRect r = intersection(rect, frameRect());
    int tx = x();
    int ty = y();

    r.move(-tx, -ty);

    gc->translate(static_cast<float>(tx), static_cast<float>(ty));
    m_listBox->paint(gc, r);
    gc->translate(-static_cast<float>(tx), -static_cast<float>(ty));

    paintBorder(gc, rect);
}

void PopupContainer::paintBorder(GraphicsContext* gc, const IntRect& rect)
{
    // FIXME: Where do we get the border color from?
    Color borderColor(127, 157, 185);

    gc->setStrokeStyle(NoStroke);
    gc->setFillColor(borderColor, ColorSpaceDeviceRGB);

    int tx = x();
    int ty = y();

    // top, left, bottom, right
    gc->drawRect(IntRect(tx, ty, width(), kBorderSize));
    gc->drawRect(IntRect(tx, ty, kBorderSize, height()));
    gc->drawRect(IntRect(tx, ty + height() - kBorderSize, width(), kBorderSize));
    gc->drawRect(IntRect(tx + width() - kBorderSize, ty, kBorderSize, height()));
}

bool PopupContainer::isInterestedInEventForKey(int keyCode)
{
    return m_listBox->isInterestedInEventForKey(keyCode);
}

ChromeClientChromium* PopupContainer::chromeClientChromium()
{
    return static_cast<ChromeClientChromium*>(m_frameView->frame()->page()->chrome()->client());
}

void PopupContainer::showInRect(const IntRect& r, FrameView* v, int index)
{
    // The rect is the size of the select box. It's usually larger than we need.
    // subtract border size so that usually the container will be displayed
    // exactly the same width as the select box.
    listBox()->setBaseWidth(max(r.width() - kBorderSize * 2, 0));

    listBox()->updateFromElement();

    // We set the selected item in updateFromElement(), and disregard the
    // index passed into this function (same as Webkit's PopupMenuWin.cpp)
    // FIXME: make sure this is correct, and add an assertion.
    // ASSERT(popupWindow(popup)->listBox()->selectedIndex() == index);

    // Convert point to main window coords.
    IntPoint location = v->contentsToWindow(r.location());

    // Move it below the select widget.
    location.move(0, r.height());

    m_originalFrameRect = IntRect(location, r.size());
    setFrameRect(m_originalFrameRect);
    showPopup(v);
}

void PopupContainer::refresh(const IntRect& targetControlRect)
{
    IntPoint location = m_frameView->contentsToWindow(targetControlRect.location());
    // Move it below the select widget.
    location.move(0, targetControlRect.height());

    listBox()->setBaseWidth(max(m_originalFrameRect.width() - kBorderSize * 2, 0));

    listBox()->updateFromElement();
    // Store the original size to check if we need to request the location.
    IntSize originalSize = size();
    IntRect widgetRect = layoutAndCalculateWidgetRect(targetControlRect.height(), location);
    if (originalSize != widgetRect.size()) {
        ChromeClientChromium* chromeClient = chromeClientChromium();
        if (chromeClient) {
            IntPoint widgetLocation = chromeClient->screenToRootView(widgetRect.location());
            widgetRect.setLocation(widgetLocation);
            setFrameRect(widgetRect);
        }
    }

    invalidate();
}

inline bool PopupContainer::isRTL() const
{
    return m_listBox->m_popupClient->menuStyle().textDirection() == RTL;
}

int PopupContainer::selectedIndex() const
{
    return m_listBox->selectedIndex();
}

int PopupContainer::menuItemHeight() const
{
    return m_listBox->getRowHeight(0);
}

int PopupContainer::menuItemFontSize() const
{
    return m_listBox->getRowFont(0).size();
}

PopupMenuStyle PopupContainer::menuStyle() const
{
    return m_listBox->m_popupClient->menuStyle();
}

const WTF::Vector<PopupItem*>& PopupContainer:: popupData() const
{
    return m_listBox->items();
}

String PopupContainer::getSelectedItemToolTip()
{
    // We cannot use m_popupClient->selectedIndex() to choose tooltip message,
    // because the selectedIndex() might return final selected index, not hovering selection.
    return listBox()->m_popupClient->itemToolTip(listBox()->m_selectedIndex);
}

} // namespace WebCore
