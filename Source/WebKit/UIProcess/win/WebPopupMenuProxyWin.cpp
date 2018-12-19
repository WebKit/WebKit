/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// NOTE: This implementation is very similar to the implementation of popups in WebCore::PopupMenuWin.
// We should try and factor out the common bits and share them.

#include "config.h"
#include "WebPopupMenuProxyWin.h"

#include "NativeWebMouseEvent.h"
#include "PlatformPopupMenuData.h"
#include "WebView.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/GDIUtilities.h>
#include <WebCore/HWndDC.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/ScrollbarTheme.h>
#include <WebCore/ScrollbarThemeWin.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <windowsx.h>

namespace WebKit {
using namespace WebCore;

static const LPCWSTR kWebKit2WebPopupMenuProxyWindowClassName = L"WebKit2WebPopupMenuProxyWindowClass";

static constexpr int defaultAnimationDuration = 200;
static constexpr int maxPopupHeight = 320;
static constexpr int popupWindowBorderWidth = 1;

// This is used from within our custom message pump when we want to send a
// message to the web view and not have our message stolen and sent to
// the popup window.
static constexpr UINT WM_HOST_WINDOW_FIRST = WM_USER;
static constexpr UINT WM_HOST_WINDOW_CHAR = WM_USER + WM_CHAR;
static constexpr UINT WM_HOST_WINDOW_MOUSEMOVE = WM_USER + WM_MOUSEMOVE;

static inline bool isASCIIPrintable(unsigned c)
{
    return c >= 0x20 && c <= 0x7E;
}

static void translatePoint(LPARAM& lParam, HWND from, HWND to)
{
    POINT pt;
    pt.x = static_cast<short>(GET_X_LPARAM(lParam));
    pt.y = static_cast<short>(GET_Y_LPARAM(lParam));
    ::MapWindowPoints(from, to, &pt, 1);
    lParam = MAKELPARAM(pt.x, pt.y);
}

LRESULT CALLBACK WebPopupMenuProxyWin::WebPopupMenuProxyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = ::GetWindowLongPtr(hWnd, 0);

    if (WebPopupMenuProxyWin* popupMenuProxy = reinterpret_cast<WebPopupMenuProxyWin*>(longPtr))
        return popupMenuProxy->wndProc(hWnd, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);

        // Associate the WebView with the window.
        ::SetWindowLongPtr(hWnd, 0, (LONG_PTR)createStruct->lpCreateParams);
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT WebPopupMenuProxyWin::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    bool handled = true;

    switch (message) {
    case WM_MOUSEACTIVATE:
        lResult = onMouseActivate(hWnd, message, wParam, lParam, handled);
        break;
    case WM_SIZE:
        lResult = onSize(hWnd, message, wParam, lParam, handled);
        break;
    case WM_KEYDOWN:
        lResult = onKeyDown(hWnd, message, wParam, lParam, handled);
        break;
    case WM_CHAR:
        lResult = onChar(hWnd, message, wParam, lParam, handled);
        break;
    case WM_MOUSEMOVE:
        lResult = onMouseMove(hWnd, message, wParam, lParam, handled);
        break;
    case WM_LBUTTONDOWN:
        lResult = onLButtonDown(hWnd, message, wParam, lParam, handled);
        break;
    case WM_LBUTTONUP:
        lResult = onLButtonUp(hWnd, message, wParam, lParam, handled);
        break;
    case WM_MOUSEWHEEL:
        lResult = onMouseWheel(hWnd, message, wParam, lParam, handled);
        break;
    case WM_PAINT:
        lResult = onPaint(hWnd, message, wParam, lParam, handled);
        break;
    case WM_PRINTCLIENT:
        lResult = onPrintClient(hWnd, message, wParam, lParam, handled);
        break;
    default:
        handled = false;
        break;
    }

    if (!handled)
        lResult = ::DefWindowProc(hWnd, message, wParam, lParam);

    return lResult;
}

bool WebPopupMenuProxyWin::registerWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;
    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_DROPSHADOW;
    wcex.lpfnWndProc    = WebPopupMenuProxyWin::WebPopupMenuProxyWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(WebPopupMenuProxyWin*);
    wcex.hInstance      = instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = ::LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebKit2WebPopupMenuProxyWindowClassName;
    wcex.hIconSm        = 0;

    return !!::RegisterClassEx(&wcex);
}

WebPopupMenuProxyWin::WebPopupMenuProxyWin(WebView* webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
{
}

WebPopupMenuProxyWin::~WebPopupMenuProxyWin()
{
    if (m_popup)
        ::DestroyWindow(m_popup);
    if (m_scrollbar)
        m_scrollbar->setParent(0);
}

void WebPopupMenuProxyWin::showPopupMenu(const IntRect& rect, TextDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData& data, int32_t selectedIndex)
{
    m_items = items;
    m_data = data;
    m_newSelectedIndex = selectedIndex;
    m_scaleFactor = pageScaleFactor;

    calculatePositionAndSize(rect);
    if (clientRect().isEmpty())
        return;

    HWND hostWindow = m_webView->window();

    if (!m_scrollbar && visibleItems() < m_items.size()) {
        m_scrollbar = Scrollbar::createNativeScrollbar(*this, VerticalScrollbar, SmallScrollbar);
        m_scrollbar->styleChanged();
    }

    if (!m_popup) {
        registerWindowClass();

        DWORD exStyle = WS_EX_LTRREADING;

        m_popup = ::CreateWindowEx(exStyle, kWebKit2WebPopupMenuProxyWindowClassName, TEXT("PopupMenu"),
            WS_POPUP | WS_BORDER,
            m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height(),
            hostWindow, 0, instanceHandle(), this);

        if (!m_popup)
            return;
    }

    BOOL shouldAnimate = FALSE;

    if (selectedIndex > 0)
        setFocusedIndex(selectedIndex);

    if (!::SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldAnimate, 0))
        shouldAnimate = FALSE;

    if (shouldAnimate) {
        RECT viewRect { };
        ::GetWindowRect(hostWindow, &viewRect);

        if (!::IsRectEmpty(&viewRect)) {
            // Popups should slide into view away from the <select> box
            // NOTE: This may have to change for Vista
            DWORD slideDirection = (m_windowRect.y() < viewRect.top + rect.location().y()) ? AW_VER_NEGATIVE : AW_VER_POSITIVE;

            ::AnimateWindow(m_popup, defaultAnimationDuration, AW_SLIDE | slideDirection);
        }
    } else
        ::ShowWindow(m_popup, SW_SHOWNOACTIVATE);

    m_showPopup = true;

    // Protect the popup menu in case its owner is destroyed while we're running the message pump.
    RefPtr<WebPopupMenuProxyWin> protectedThis(this);

    ::SetCapture(hostWindow);

    MSG msg;
    HWND activeWindow;

    while (::GetMessage(&msg, 0, 0, 0)) {
        switch (msg.message) {
        case WM_HOST_WINDOW_MOUSEMOVE:
        case WM_HOST_WINDOW_CHAR: 
            if (msg.hwnd == m_popup) {
                // This message should be sent to the host window.
                msg.hwnd = hostWindow;
                msg.message -= WM_HOST_WINDOW_FIRST;
            }
            break;

        // Steal mouse messages.
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
        case WM_NCRBUTTONDBLCLK:
        case WM_NCMBUTTONDOWN:
        case WM_NCMBUTTONUP:
        case WM_NCMBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
            msg.hwnd = m_popup;
            break;

        // These mouse messages use client coordinates so we need to convert them.
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK: {
            // Translate the coordinate.
            translatePoint(msg.lParam, msg.hwnd, m_popup);
            msg.hwnd = m_popup;
            break;
        }

        // Steal all keyboard messages.
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_SYSKEYUP:
        case WM_SYSCHAR:
        case WM_SYSDEADCHAR:
            msg.hwnd = m_popup;
            break;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);

        if (!m_showPopup)
            break;
        activeWindow = ::GetActiveWindow();
        if (activeWindow != hostWindow && !::IsChild(activeWindow, hostWindow))
            break;
        if (::GetCapture() != hostWindow)
            break;
    }

    if (::GetCapture() == hostWindow)
        ::ReleaseCapture();

    m_showPopup = false;
    ::ShowWindow(m_popup, SW_HIDE);

    if (!WebPopupMenuProxy::m_client)
        return;

    WebPopupMenuProxy::m_client->valueChangedForPopupMenu(this, m_newSelectedIndex);

    // <https://bugs.webkit.org/show_bug.cgi?id=57904> In order to properly call the onClick()
    // handler on a <select> element, we need to fake a mouse up event in the main window.
    // The main window already received the mouse down, which showed this popup, but upon
    // selection of an item the mouse up gets eaten by the popup menu. So we take the mouse down
    // event, change the message type to a mouse up event, and post that in the message queue.
    // Thus, we are virtually clicking at the
    // same location where the mouse down event occurred. This allows the hit test to select
    // the correct element, and thereby call the onClick() JS handler.
    if (!WebPopupMenuProxy::m_client->currentlyProcessedMouseDownEvent())
        return;

    const MSG* initiatingWinEvent = WebPopupMenuProxy::m_client->currentlyProcessedMouseDownEvent()->nativeEvent();
    MSG fakeEvent = *initiatingWinEvent;
    fakeEvent.message = WM_LBUTTONUP;
    ::PostMessage(fakeEvent.hwnd, fakeEvent.message, fakeEvent.wParam, fakeEvent.lParam);
}

void WebPopupMenuProxyWin::hidePopupMenu()
{
    if (!m_showPopup)
        return;
    m_showPopup = false;

    ::ShowWindow(m_popup, SW_HIDE);

    // Post a WM_NULL message to wake up the message pump if necessary.
    ::PostMessage(m_popup, WM_NULL, 0, 0);
}

void WebPopupMenuProxyWin::calculatePositionAndSize(const IntRect& rect)
{
    IntRect rectInScreenCoords(rect);
    rectInScreenCoords.scale(m_scaleFactor);

    POINT location(rectInScreenCoords .location());
    if (!::ClientToScreen(m_webView->window(), &location))
        return;
    rectInScreenCoords.setLocation(location);

    int itemCount = m_items.size();
    m_itemHeight = m_data.m_itemHeight;

    int naturalHeight = m_itemHeight * itemCount;
    int popupHeight = std::min(maxPopupHeight, naturalHeight);

    // The popup should show an integral number of items (i.e. no partial items should be visible)
    popupHeight -= popupHeight % m_itemHeight;

    // Next determine its width
    int popupWidth = m_data.m_popupWidth;

    if (naturalHeight > maxPopupHeight) {
        // We need room for a scrollbar
        popupWidth += ScrollbarTheme::theme().scrollbarThickness(SmallScrollbar);
    }

    popupHeight += 2 * popupWindowBorderWidth;

    // The popup should be at least as wide as the control on the page
    popupWidth = std::max(rectInScreenCoords.width() - m_data.m_clientInsetLeft - m_data.m_clientInsetRight, popupWidth);

    // Always left-align items in the popup. This matches popup menus on the mac.
    int popupX = rectInScreenCoords.x() + m_data.m_clientInsetLeft;

    IntRect popupRect(popupX, rectInScreenCoords.maxY(), popupWidth, popupHeight);

    // The popup needs to stay within the bounds of the screen and not overlap any toolbars
    HMONITOR monitor = ::MonitorFromWindow(m_webView->window(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    ::GetMonitorInfo(monitor, &monitorInfo);
    FloatRect screen = static_cast<IntRect>(monitorInfo.rcWork);

    // Check that we don't go off the screen vertically
    if (popupRect.maxY() > screen.height()) {
        // The popup will go off the screen, so try placing it above the client
        if (rectInScreenCoords.y() - popupRect.height() < 0) {
            // The popup won't fit above, either, so place it whereever's bigger and resize it to fit
            if ((rectInScreenCoords.y() + rectInScreenCoords.height() / 2) < (screen.height() / 2)) {
                // Below is bigger
                popupRect.setHeight(screen.height() - popupRect.y());
            } else {
                // Above is bigger
                popupRect.setY(0);
                popupRect.setHeight(rectInScreenCoords.y());
            }
        } else {
            // The popup fits above, so reposition it
            popupRect.setY(rectInScreenCoords.y() - popupRect.height());
        }
    }

    // Check that we don't go off the screen horizontally
    if (popupRect.x() < screen.x()) {
        popupRect.setWidth(popupRect.width() - (screen.x() - popupRect.x()));
        popupRect.setX(screen.x());
    }

    m_windowRect = popupRect;
}

IntRect WebPopupMenuProxyWin::clientRect() const
{
    IntRect clientRect = m_windowRect;
    clientRect.inflate(-popupWindowBorderWidth);
    clientRect.setLocation(IntPoint(0, 0));
    return clientRect;
}

void WebPopupMenuProxyWin::invalidateItem(int index)
{
    if (!m_popup)
        return;

    IntRect damageRect(clientRect());
    damageRect.setY(m_itemHeight * (index - m_scrollOffset));
    damageRect.setHeight(m_itemHeight);
    if (m_scrollbar)
        damageRect.setWidth(damageRect.width() - m_scrollbar->frameRect().width());

    RECT r = damageRect;
    ::InvalidateRect(m_popup, &r, TRUE);
}

int WebPopupMenuProxyWin::scrollSize(ScrollbarOrientation orientation) const
{
    return ((orientation == VerticalScrollbar) && m_scrollbar) ? (m_scrollbar->totalSize() - m_scrollbar->visibleSize()) : 0;
}

void WebPopupMenuProxyWin::setScrollOffset(const IntPoint& offset)
{
    scrollTo(offset.y());
}

IntSize WebPopupMenuProxyWin::visibleSize() const
{
    int scrollbarWidth = m_scrollbar ? m_scrollbar->frameRect().width() : 0;
    return IntSize(contentsSize().width() - scrollbarWidth, m_scrollbar ? m_scrollbar->visibleSize() : contentsSize().height());
}

WebCore::IntSize WebPopupMenuProxyWin::contentsSize() const
{
    return IntSize(m_windowRect.width(), m_scrollbar ? m_scrollbar->totalSize() : m_windowRect.height());
}

WebCore::IntRect WebPopupMenuProxyWin::scrollableAreaBoundingBox(bool*) const
{
    return m_windowRect;
}

void WebPopupMenuProxyWin::scrollTo(int offset)
{
    ASSERT(m_scrollbar);

    if (!m_popup)
        return;

    if (m_scrollOffset == offset)
        return;

    int scrolledLines = m_scrollOffset - offset;
    m_scrollOffset = offset;

    UINT flags = SW_INVALIDATE;

#ifdef CAN_SET_SMOOTH_SCROLLING_DURATION
    BOOL shouldSmoothScroll = FALSE;
    ::SystemParametersInfo(SPI_GETLISTBOXSMOOTHSCROLLING, 0, &shouldSmoothScroll, 0);
    if (shouldSmoothScroll)
        flags |= MAKEWORD(SW_SMOOTHSCROLL, smoothScrollAnimationDuration);
#endif

    IntRect listRect = clientRect();
    if (m_scrollbar)
        listRect.setWidth(listRect.width() - m_scrollbar->frameRect().width());
    RECT r = listRect;
    ::ScrollWindowEx(m_popup, 0, scrolledLines * m_itemHeight, &r, 0, 0, 0, flags);
    if (m_scrollbar) {
        r = m_scrollbar->frameRect();
        ::InvalidateRect(m_popup, &r, TRUE);
    }
    ::UpdateWindow(m_popup);
}

void WebPopupMenuProxyWin::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    scrollRect.move(scrollbar.x(), scrollbar.y());
    RECT r = scrollRect;
    ::InvalidateRect(m_popup, &r, false);
}

// Message pump messages.

LRESULT WebPopupMenuProxyWin::onMouseActivate(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled)
{
    handled = true;
    return MA_NOACTIVATE;
}

LRESULT WebPopupMenuProxyWin::onSize(HWND hWnd, UINT message, WPARAM, LPARAM lParam, bool& handled)
{
    handled = true;
    if (!scrollbar())
        return 0;

    IntSize size(LOWORD(lParam), HIWORD(lParam));
    scrollbar()->setFrameRect(IntRect(size.width() - scrollbar()->width(), 0, scrollbar()->width(), size.height()));

    int visibleItems = this->visibleItems();
    scrollbar()->setEnabled(visibleItems < m_items.size());
    scrollbar()->setSteps(1, std::max(1, visibleItems - 1));
    scrollbar()->setProportion(visibleItems, m_items.size());
    return 0;
}

LRESULT WebPopupMenuProxyWin::onKeyDown(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    handled = true;

    LRESULT lResult = 0;
    switch (LOWORD(wParam)) {
    case VK_DOWN:
    case VK_RIGHT:
        down();
        break;
    case VK_UP:
    case VK_LEFT:
        up();
        break;
    case VK_HOME:
        focusFirst();
        break;
    case VK_END:
        focusLast();
        break;
    case VK_PRIOR:
        if (focusedIndex() != scrollOffset(VerticalScrollbar)) {
            // Set the selection to the first visible item
            int firstVisibleItem = scrollOffset(VerticalScrollbar);
            up(focusedIndex() - firstVisibleItem);
        } else {
            // The first visible item is selected, so move the selection back one page
            up(visibleItems());
        }
        break;
    case VK_NEXT: {
        int lastVisibleItem = scrollOffset(VerticalScrollbar) + visibleItems() - 1;
        if (focusedIndex() != lastVisibleItem) {
            // Set the selection to the last visible item
            down(lastVisibleItem - focusedIndex());
        } else {
            // The last visible item is selected, so move the selection forward one page
            down(visibleItems());
        }
        break;
    }
    case VK_TAB:
        ::SendMessage(m_webView->window(), message, wParam, lParam);
        hide();
        break;
    case VK_ESCAPE:
        hide();
        break;
    default:
        if (isASCIIPrintable(wParam)) {
            // Send the keydown to the WebView so it can be used for type-to-select.
            // Since we know that the virtual key is ASCII printable, it's OK to convert this to
            // a WM_CHAR message. (We don't want to call TranslateMessage because that will post a
            // WM_CHAR message that will be stolen and redirected to the popup HWND.
            ::PostMessage(m_popup, WM_HOST_WINDOW_CHAR, wParam, lParam);
        } else
            lResult = 1;
        break;
    }

    return lResult;
}

LRESULT WebPopupMenuProxyWin::onChar(HWND hWnd, UINT message, WPARAM wParam, LPARAM, bool& handled)
{
    handled = true;

    LRESULT lResult = 0;
    int index;
    switch (wParam) {
    case 0x0D: // Enter/Return
        hide();
        index = focusedIndex();
        ASSERT(index >= 0);
        // FIXME: Do we need to send back the index right away?
        m_newSelectedIndex = index;
        break;
    case 0x1B: // Escape
        hide();
        break;
    case 0x09: // TAB
    case 0x08: // Backspace
    case 0x0A: // Linefeed
    default: // Character
        lResult = 1;
        break;
    }

    return lResult;
}

LRESULT WebPopupMenuProxyWin::onMouseMove(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    handled = true;

    IntPoint mousePoint(MAKEPOINTS(lParam));
    if (scrollbar()) {
        IntRect scrollBarRect = scrollbar()->frameRect();
        if (scrollbarCapturingMouse() || scrollBarRect.contains(mousePoint)) {
            // Put the point into coordinates relative to the scroll bar
            mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
            PlatformMouseEvent event(hWnd, message, wParam, makeScaledPoint(mousePoint, m_scaleFactor));
            scrollbar()->mouseMoved(event);
            return 0;
        }
    }

    BOOL shouldHotTrack = FALSE;
    if (!::SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldHotTrack, 0))
        shouldHotTrack = FALSE;

    RECT bounds;
    ::GetClientRect(m_popup, &bounds);
    if (!::PtInRect(&bounds, mousePoint) && !(wParam & MK_LBUTTON)) {
        // When the mouse is not inside the popup menu and the left button isn't down, just
        // repost the message to the web view.

        // Translate the coordinate.
        translatePoint(lParam, m_popup, m_webView->window());

        ::PostMessage(m_popup, WM_HOST_WINDOW_MOUSEMOVE, wParam, lParam);
        return 0;
    }

    if ((shouldHotTrack || wParam & MK_LBUTTON) && ::PtInRect(&bounds, mousePoint)) {
        setFocusedIndex(listIndexAtPoint(mousePoint), true);
        m_hoveredIndex = listIndexAtPoint(mousePoint);
    }

    return 0;
}

LRESULT WebPopupMenuProxyWin::onLButtonDown(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    handled = true;

    IntPoint mousePoint(MAKEPOINTS(lParam));
    if (scrollbar()) {
        IntRect scrollBarRect = scrollbar()->frameRect();
        if (scrollBarRect.contains(mousePoint)) {
            // Put the point into coordinates relative to the scroll bar
            mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
            PlatformMouseEvent event(hWnd, message, wParam, makeScaledPoint(mousePoint, m_scaleFactor));
            scrollbar()->mouseDown(event);
            setScrollbarCapturingMouse(true);
            return 0;
        }
    }

    // If the mouse is inside the window, update the focused index. Otherwise, 
    // hide the popup.
    RECT bounds;
    ::GetClientRect(m_popup, &bounds);
    if (::PtInRect(&bounds, mousePoint)) {
        setFocusedIndex(listIndexAtPoint(mousePoint), true);
        m_hoveredIndex = listIndexAtPoint(mousePoint);
    } else
        hide();

    return 0;
}


LRESULT WebPopupMenuProxyWin::onLButtonUp(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    handled = true;

    IntPoint mousePoint(MAKEPOINTS(lParam));
    if (scrollbar()) {
        IntRect scrollBarRect = scrollbar()->frameRect();
        if (scrollbarCapturingMouse() || scrollBarRect.contains(mousePoint)) {
            setScrollbarCapturingMouse(false);
            // Put the point into coordinates relative to the scroll bar
            mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
            PlatformMouseEvent event(hWnd, message, wParam, makeScaledPoint(mousePoint, m_scaleFactor));
            scrollbar()->mouseUp(event);
            // FIXME: This is a hack to work around Scrollbar not invalidating correctly when it doesn't have a parent widget
            RECT r = scrollBarRect;
            ::InvalidateRect(m_popup, &r, TRUE);
            return 0;
        }
    }
    // Only hide the popup if the mouse is inside the popup window.
    RECT bounds;
    ::GetClientRect(m_popup, &bounds);
    if (::PtInRect(&bounds, mousePoint)) {
        hide();
        int index = m_hoveredIndex;
        if (!m_items[index].m_isEnabled)
            index = m_focusedIndex;

        if (index >= 0) {
            // FIXME: Do we need to send back the index right away?
            m_newSelectedIndex = index;
        }
    }

    return 0;
}

LRESULT WebPopupMenuProxyWin::onMouseWheel(HWND hWnd, UINT message, WPARAM wParam, LPARAM, bool& handled)
{
    handled = true;

    if (!scrollbar())
        return 0;

    int i = 0;
    for (incrementWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam)); abs(wheelDelta()) >= WHEEL_DELTA; reduceWheelDelta(WHEEL_DELTA)) {
        if (wheelDelta() > 0)
            ++i;
        else
            --i;
    }

    ScrollableArea::scroll(i > 0 ? ScrollUp : ScrollDown, ScrollByLine, abs(i));
    return 0;
}

LRESULT WebPopupMenuProxyWin::onPaint(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled)
{
    handled = true;

    PAINTSTRUCT paintStruct;
    ::BeginPaint(m_popup, &paintStruct);
    paint(paintStruct.rcPaint, paintStruct.hdc);
    ::EndPaint(m_popup, &paintStruct);

    return 0;
}

LRESULT WebPopupMenuProxyWin::onPrintClient(HWND hWnd, UINT, WPARAM wParam, LPARAM, bool& handled)
{
    handled = true;

    HDC hdc = reinterpret_cast<HDC>(wParam);
    paint(clientRect(), hdc);

    return 0;
}

bool WebPopupMenuProxyWin::down(unsigned lines)
{
    int size = m_items.size();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = focusedIndex();
    for (int i = selectedListIndex + 1; i >= 0 && i < size; ++i) {
        if (m_items[i].m_isEnabled) {
            lastSelectableIndex = i;
            if (i >= selectedListIndex + (int)lines)
                break;
        }
    }

    return setFocusedIndex(lastSelectableIndex);
}

bool WebPopupMenuProxyWin::up(unsigned lines)
{
    int size = m_items.size();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = focusedIndex();
    for (int i = selectedListIndex - 1; i >= 0 && i < size; --i) {
        if (m_items[i].m_isEnabled) {
            lastSelectableIndex = i;
            if (i <= selectedListIndex - (int)lines)
                break;
        }
    }

    return setFocusedIndex(lastSelectableIndex);
}

void WebPopupMenuProxyWin::paint(const IntRect& damageRect, HDC hdc)
{
    if (!m_popup)
        return;

    if (!m_DC) {
        m_DC = adoptGDIObject(::CreateCompatibleDC(HWndDC(m_popup)));
        if (!m_DC)
            return;
    }

    if (m_bmp) {
        bool keepBitmap = false;
        BITMAP bitmap;
        if (::GetObject(m_bmp.get(), sizeof(bitmap), &bitmap))
            keepBitmap = bitmap.bmWidth == clientRect().width() && bitmap.bmHeight == clientRect().height();
        if (!keepBitmap)
            m_bmp.clear();
    }
    if (!m_bmp) {
        BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(clientRect().size());
        void* pixels = 0;
        m_bmp = adoptGDIObject(::CreateDIBSection(m_DC.get(), &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));
        if (!m_bmp)
            return;

        ::SelectObject(m_DC.get(), m_bmp.get());
    }

    GraphicsContext context(m_DC.get());

    IntRect translatedDamageRect = damageRect;
    translatedDamageRect.move(IntSize(0, m_scrollOffset * m_itemHeight));
    m_data.m_notSelectedBackingStore->paint(context, damageRect.location(), translatedDamageRect);

    IntRect selectedIndexRectInBackingStore(0, focusedIndex() * m_itemHeight, m_data.m_selectedBackingStore->size().width(), m_itemHeight);
    IntPoint selectedIndexDstPoint = selectedIndexRectInBackingStore.location();
    selectedIndexDstPoint.move(0, -m_scrollOffset * m_itemHeight);

    m_data.m_selectedBackingStore->paint(context, selectedIndexDstPoint, selectedIndexRectInBackingStore);

    if (m_scrollbar)
        m_scrollbar->paint(context, damageRect);


    HWndDC hWndDC;
    HDC localDC = hdc ? hdc : hWndDC.setHWnd(m_popup);

    ::BitBlt(localDC, damageRect.x(), damageRect.y(), damageRect.width(), damageRect.height(), m_DC.get(), damageRect.x(), damageRect.y(), SRCCOPY);
}

bool WebPopupMenuProxyWin::setFocusedIndex(int i, bool hotTracking)
{
    if (i < 0 || i >= m_items.size() || i == focusedIndex())
        return false;

    if (!m_items[i].m_isEnabled)
        return false;

    invalidateItem(focusedIndex());
    invalidateItem(i);

    m_focusedIndex = i;

    if (!hotTracking) {
        if (WebPopupMenuProxy::m_client)
            WebPopupMenuProxy::m_client->setTextFromItemForPopupMenu(this, i);
    }

    scrollToRevealSelection();

    return true;
}

int WebPopupMenuProxyWin::visibleItems() const
{
    return clientRect().height() / m_itemHeight;
}

int WebPopupMenuProxyWin::listIndexAtPoint(const IntPoint& point) const
{
    return m_scrollOffset + point.y() / m_itemHeight;
}

int WebPopupMenuProxyWin::focusedIndex() const
{
    return m_focusedIndex;
}

void WebPopupMenuProxyWin::focusFirst()
{
    int size = m_items.size();

    for (int i = 0; i < size; ++i) {
        if (m_items[i].m_isEnabled) {
            setFocusedIndex(i);
            break;
        }
    }
}

void WebPopupMenuProxyWin::focusLast()
{
    int size = m_items.size();

    for (int i = size - 1; i > 0; --i) {
        if (m_items[i].m_isEnabled) {
            setFocusedIndex(i);
            break;
        }
    }
}


void WebPopupMenuProxyWin::incrementWheelDelta(int delta)
{
    m_wheelDelta += delta;
}

void WebPopupMenuProxyWin::reduceWheelDelta(int delta)
{
    ASSERT(delta >= 0);
    ASSERT(delta <= abs(m_wheelDelta));

    if (m_wheelDelta > 0)
        m_wheelDelta -= delta;
    else if (m_wheelDelta < 0)
        m_wheelDelta += delta;
    else
        return;
}

bool WebPopupMenuProxyWin::scrollToRevealSelection()
{
    if (!m_scrollbar)
        return false;

    int index = focusedIndex();

    if (index < m_scrollOffset) {
        ScrollableArea::scrollToOffsetWithoutAnimation(VerticalScrollbar, index);
        return true;
    }

    if (index >= m_scrollOffset + visibleItems()) {
        ScrollableArea::scrollToOffsetWithoutAnimation(VerticalScrollbar, index - visibleItems() + 1);
        return true;
    }

    return false;
}

} // namespace WebKit
