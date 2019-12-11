/*
 * Copyright (C) 2006-2008, 2011, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile Inc.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "config.h"
#include "PopupMenuWin.h"

#include "BString.h"
#include "BitmapInfo.h"
#include "Document.h"
#include "FloatRect.h"
#include "Font.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameView.h"
#include "GDIUtilities.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HWndDC.h"
#include "HostWindow.h"
#include "LengthFunctions.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformScreen.h"
#include "RenderMenuList.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollbarThemeWin.h"
#include "TextRun.h"
#include "WebCoreInstanceHandle.h"
#include <wtf/WindowsExtras.h>

#include <windows.h>
#include <windowsx.h>

#define HIGH_BIT_MASK_SHORT 0x8000

using std::min;

namespace WebCore {

using namespace HTMLNames;

// Default Window animation duration in milliseconds
static const int defaultAnimationDuration = 200;
// Maximum height of a popup window
static const int maxPopupHeight = 320;

const int optionSpacingMiddle = 1;
const int popupWindowBorderWidth = 1;

static LPCWSTR kPopupWindowClassName = L"PopupWindowClass";

// This is used from within our custom message pump when we want to send a
// message to the web view and not have our message stolen and sent to
// the popup window.
static const UINT WM_HOST_WINDOW_FIRST = WM_USER;
static const UINT WM_HOST_WINDOW_CHAR = WM_USER + WM_CHAR; 
static const UINT WM_HOST_WINDOW_MOUSEMOVE = WM_USER + WM_MOUSEMOVE;

// FIXME: Remove this as soon as practical.
static inline bool isASCIIPrintable(unsigned c)
{
    return c >= 0x20 && c <= 0x7E;
}

static void translatePoint(LPARAM& lParam, HWND from, HWND to)
{
    POINT pt;
    pt.x = (short)GET_X_LPARAM(lParam);
    pt.y = (short)GET_Y_LPARAM(lParam);    
    ::MapWindowPoints(from, to, &pt, 1);
    lParam = MAKELPARAM(pt.x, pt.y);
}

static FloatRect monitorFromHwnd(HWND hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(monitor, &monitorInfo);
    return monitorInfo.rcWork;
}

PopupMenuWin::PopupMenuWin(PopupMenuClient* client)
    : m_popupClient(client)
{
}

PopupMenuWin::~PopupMenuWin()
{
    if (m_popup)
        ::DestroyWindow(m_popup);
    if (m_scrollbar)
        m_scrollbar->setParent(0);
}

void PopupMenuWin::disconnectClient()
{
    m_popupClient = 0;
}

LPCWSTR PopupMenuWin::popupClassName()
{
    return kPopupWindowClassName;
}

void PopupMenuWin::show(const IntRect& r, FrameView* view, int index)
{
    if (view && view->frame().page())
        m_scaleFactor = view->frame().page()->deviceScaleFactor();

    calculatePositionAndSize(r, view);
    if (clientRect().isEmpty())
        return;

    HWND hostWindow = view->hostWindow()->platformPageClient();

    if (!m_scrollbar && visibleItems() < client()->listSize()) {
        // We need a scroll bar
        m_scrollbar = client()->createScrollbar(*this, VerticalScrollbar, SmallScrollbar);
        m_scrollbar->styleChanged();
    }

    // We need to reposition the popup window to its final coordinates.
    // Before calling this, the popup hwnd is currently the size of and at the location of the menu list client so it needs to be updated.
    ::MoveWindow(m_popup, m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height(), false);

    // Determine whether we should animate our popups
    // Note: Must use 'BOOL' and 'FALSE' instead of 'bool' and 'false' to avoid stack corruption with SystemParametersInfo
    BOOL shouldAnimate = FALSE;

    if (client()) {
        int index = client()->selectedIndex();
        if (index >= 0)
            setFocusedIndex(index);
    }

    if (!::SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldAnimate, 0))
        shouldAnimate = FALSE;

    if (shouldAnimate) {
        RECT viewRect { };
        ::GetWindowRect(hostWindow, &viewRect);
        if (!::IsRectEmpty(&viewRect))
            ::AnimateWindow(m_popup, defaultAnimationDuration, AW_BLEND);
    } else
        ::ShowWindow(m_popup, SW_SHOWNOACTIVATE);

    m_showPopup = true;

    // Protect the popup menu in case its owner is destroyed while we're running the message pump.
    RefPtr<PopupMenu> protectedThis(this);

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
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
            case WM_SYSDEADCHAR:
                msg.hwnd = m_popup;
                break;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);

        if (!m_popupClient)
            break;

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

    // We're done, hide the popup if necessary.
    hide();
}

void PopupMenuWin::hide()
{
    if (!m_showPopup)
        return;

    m_showPopup = false;

    ::ShowWindow(m_popup, SW_HIDE);

    if (client())
        client()->popupDidHide();

    // Post a WM_NULL message to wake up the message pump if necessary.
    ::PostMessage(m_popup, WM_NULL, 0, 0);
}

// The screen that the popup is placed on should be whichever one the popup menu button lies on.
// We fake an hwnd (here we use the popup's hwnd) on top of the button which we can then use to determine the screen.
// We can then proceed with our final position/size calculations.
void PopupMenuWin::calculatePositionAndSize(const IntRect& r, FrameView* v)
{
    // First get the screen coordinates of the popup menu client.
    HWND hostWindow = v->hostWindow()->platformPageClient();
    IntRect absoluteBounds = ((RenderMenuList*)m_popupClient)->absoluteBoundingBoxRect();
    IntRect absoluteScreenCoords(v->contentsToWindow(absoluteBounds.location()), absoluteBounds.size());
    POINT absoluteLocation(absoluteScreenCoords.location());
    if (!::ClientToScreen(hostWindow, &absoluteLocation))
        return;
    absoluteScreenCoords.setLocation(absoluteLocation);

    // Now set the popup menu's location temporarily to these coordinates so we can determine which screen the popup should lie on.
    // We create or move m_popup as necessary.
    if (!m_popup) {
        registerClass();
        DWORD exStyle = WS_EX_LTRREADING;
        m_popup = ::CreateWindowExW(exStyle, kPopupWindowClassName, L"PopupMenu",
            WS_POPUP | WS_BORDER,
            absoluteScreenCoords.x(), absoluteScreenCoords.y(), absoluteScreenCoords.width(), absoluteScreenCoords.height(),
            hostWindow, 0, WebCore::instanceHandle(), this);

        if (!m_popup)
            return;
    } else
        ::MoveWindow(m_popup, absoluteScreenCoords.x(), absoluteScreenCoords.y(), absoluteScreenCoords.width(), absoluteScreenCoords.height(), false);

    FloatRect screen = monitorFromHwnd(m_popup);
    
    // Now we determine the actual location and measurements of the popup itself.
    // r is in absolute document coordinates, but we want to be in screen coordinates.

    // First, move to WebView coordinates
    IntRect rScreenCoords(v->contentsToWindow(r.location()), r.size());
    if (Page* page = v->frame().page())
        rScreenCoords.scale(page->deviceScaleFactor());

    // Then, translate to screen coordinates
    POINT location(rScreenCoords.location());
    if (!::ClientToScreen(hostWindow, &location))
        return;

    rScreenCoords.setLocation(location);

    m_font = client()->menuStyle().font();
    auto d = m_font.fontDescription();
    d.setComputedSize(d.computedSize() * m_scaleFactor);
    m_font = FontCascade(WTFMove(d), m_font.letterSpacing(), m_font.wordSpacing());
    m_font.update(m_popupClient->fontSelector());

    // First, determine the popup's height
    int itemCount = client()->listSize();
    m_itemHeight = m_font.fontMetrics().height() + optionSpacingMiddle;

    int naturalHeight = m_itemHeight * itemCount;
    int popupHeight = std::min(maxPopupHeight, naturalHeight);
    // The popup should show an integral number of items (i.e. no partial items should be visible)
    popupHeight -= popupHeight % m_itemHeight;
    
    // Next determine its width
    int popupWidth = 0;
    for (int i = 0; i < itemCount; ++i) {
        String text = client()->itemText(i);
        if (text.isEmpty())
            continue;

        FontCascade itemFont = m_font;
        if (client()->itemIsLabel(i)) {
            auto d = itemFont.fontDescription();
            d.setWeight(d.bolderWeight());
            itemFont = FontCascade(WTFMove(d), itemFont.letterSpacing(), itemFont.wordSpacing());
            itemFont.update(m_popupClient->fontSelector());
        }

        popupWidth = std::max(popupWidth, static_cast<int>(ceilf(itemFont.width(TextRun(text)))));
    }

    if (naturalHeight > maxPopupHeight)
        // We need room for a scrollbar
        popupWidth += ScrollbarTheme::theme().scrollbarThickness(SmallScrollbar);

    // Add padding to align the popup text with the <select> text
    popupWidth += std::max<int>(0, client()->clientPaddingRight() - client()->clientInsetRight()) + std::max<int>(0, client()->clientPaddingLeft() - client()->clientInsetLeft());

    // Leave room for the border
    popupWidth += 2 * popupWindowBorderWidth;
    popupHeight += 2 * popupWindowBorderWidth;

    // The popup should be at least as wide as the control on the page
    popupWidth = std::max(rScreenCoords.width() - client()->clientInsetLeft() - client()->clientInsetRight(), popupWidth);

    // Always left-align items in the popup.  This matches popup menus on the mac.
    int popupX = rScreenCoords.x() + client()->clientInsetLeft();

    IntRect popupRect(popupX, rScreenCoords.maxY(), popupWidth, popupHeight);

    // Check that we don't go off the screen vertically
    if (popupRect.maxY() > screen.height()) {
        // The popup will go off the screen, so try placing it above the client
        if (rScreenCoords.y() - popupRect.height() < 0) {
            // The popup won't fit above, either, so place it whereever's bigger and resize it to fit
            if ((rScreenCoords.y() + rScreenCoords.height() / 2) < (screen.height() / 2)) {
                // Below is bigger
                popupRect.setHeight(screen.height() - popupRect.y());
            } else {
                // Above is bigger
                popupRect.setY(0);
                popupRect.setHeight(rScreenCoords.y());
            }
        } else {
            // The popup fits above, so reposition it
            popupRect.setY(rScreenCoords.y() - popupRect.height());
        }
    }

    // Check that we don't go off the screen horizontally
    if (popupRect.x() + popupRect.width() > screen.width() + screen.x())
        popupRect.setX(screen.x() + screen.width() - popupRect.width());
    if (popupRect.x() < screen.x())
        popupRect.setX(screen.x());

    m_windowRect = popupRect;
    return;
}

bool PopupMenuWin::setFocusedIndex(int i, bool hotTracking)
{
    if (i < 0 || i >= client()->listSize() || i == focusedIndex())
        return false;

    if (!client()->itemIsEnabled(i))
        return false;

    invalidateItem(focusedIndex());
    invalidateItem(i);

    m_focusedIndex = i;

    if (!hotTracking)
        client()->setTextFromItem(i);

    if (!scrollToRevealSelection())
        ::UpdateWindow(m_popup);

    return true;
}

int PopupMenuWin::visibleItems() const
{
    return clientRect().height() / m_itemHeight;
}

int PopupMenuWin::listIndexAtPoint(const IntPoint& point) const
{
    return m_scrollOffset + point.y() / m_itemHeight;
}

int PopupMenuWin::focusedIndex() const
{
    return m_focusedIndex;
}

void PopupMenuWin::focusFirst()
{
    if (!client())
        return;

    int size = client()->listSize();

    for (int i = 0; i < size; ++i)
        if (client()->itemIsEnabled(i)) {
            setFocusedIndex(i);
            break;
        }
}

void PopupMenuWin::focusLast()
{
    if (!client())
        return;

    int size = client()->listSize();

    for (int i = size - 1; i > 0; --i)
        if (client()->itemIsEnabled(i)) {
            setFocusedIndex(i);
            break;
        }
}

bool PopupMenuWin::down(unsigned lines)
{
    if (!client())
        return false;

    int size = client()->listSize();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = focusedIndex();
    for (int i = selectedListIndex + 1; i >= 0 && i < size; ++i)
        if (client()->itemIsEnabled(i)) {
            lastSelectableIndex = i;
            if (i >= selectedListIndex + (int)lines)
                break;
        }

    return setFocusedIndex(lastSelectableIndex);
}

bool PopupMenuWin::up(unsigned lines)
{
    if (!client())
        return false;

    int size = client()->listSize();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = focusedIndex();
    for (int i = selectedListIndex - 1; i >= 0 && i < size; --i)
        if (client()->itemIsEnabled(i)) {
            lastSelectableIndex = i;
            if (i <= selectedListIndex - (int)lines)
                break;
        }

    return setFocusedIndex(lastSelectableIndex);
}

void PopupMenuWin::invalidateItem(int index)
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

IntRect PopupMenuWin::clientRect() const
{
    IntRect clientRect = m_windowRect;
    clientRect.inflate(-popupWindowBorderWidth);
    clientRect.setLocation(IntPoint(0, 0));
    return clientRect;
}

void PopupMenuWin::incrementWheelDelta(int delta)
{
    m_wheelDelta += delta;
}

void PopupMenuWin::reduceWheelDelta(int delta)
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

bool PopupMenuWin::scrollToRevealSelection()
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

void PopupMenuWin::updateFromElement()
{
    if (!m_popup)
        return;

    m_focusedIndex = client()->selectedIndex();

    ::InvalidateRect(m_popup, 0, TRUE);
    scrollToRevealSelection();
}

const int separatorPadding = 4;
const int separatorHeight = 1;
void PopupMenuWin::paint(const IntRect& damageRect, HDC hdc)
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
            keepBitmap = bitmap.bmWidth == clientRect().width()
                && bitmap.bmHeight == clientRect().height();
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

    // listRect is the damageRect translated into the coordinates of the entire menu list (which is listSize * m_itemHeight pixels tall)
    IntRect listRect = damageRect;
    listRect.move(IntSize(0, m_scrollOffset * m_itemHeight));

    for (int y = listRect.y(); y < listRect.maxY(); y += m_itemHeight) {
        int index = y / m_itemHeight;

        Color optionBackgroundColor, optionTextColor;
        PopupMenuStyle itemStyle = client()->itemStyle(index);
        if (index == focusedIndex()) {
            optionBackgroundColor = RenderTheme::singleton().activeListBoxSelectionBackgroundColor({ });
            optionTextColor = RenderTheme::singleton().activeListBoxSelectionForegroundColor({ });
        } else {
            optionBackgroundColor = itemStyle.backgroundColor();
            optionTextColor = itemStyle.foregroundColor();
        }

        // itemRect is in client coordinates
        IntRect itemRect(0, (index - m_scrollOffset) * m_itemHeight, damageRect.width(), m_itemHeight);

        // Draw the background for this menu item
        if (itemStyle.isVisible())
            context.fillRect(itemRect, optionBackgroundColor);

        if (client()->itemIsSeparator(index)) {
            IntRect separatorRect(itemRect.x() + separatorPadding, itemRect.y() + (itemRect.height() - separatorHeight) / 2, itemRect.width() - 2 * separatorPadding, separatorHeight);
            context.fillRect(separatorRect, optionTextColor);
            continue;
        }

        String itemText = client()->itemText(index);

        TextRun textRun(itemText, 0, 0, AllowTrailingExpansion, itemStyle.textDirection(), itemStyle.hasTextDirectionOverride());
        context.setFillColor(optionTextColor);

        FontCascade itemFont = m_font;
        if (client()->itemIsLabel(index)) {
            auto d = itemFont.fontDescription();
            d.setWeight(d.bolderWeight());
            itemFont = FontCascade(WTFMove(d), itemFont.letterSpacing(), itemFont.wordSpacing());
            itemFont.update(m_popupClient->fontSelector());
        }
        
        // Draw the item text
        if (itemStyle.isVisible()) {
            int textX = 0;
            if (client()->menuStyle().textDirection() == TextDirection::LTR) {
                textX = std::max<int>(0, client()->clientPaddingLeft() - client()->clientInsetLeft());
                if (RenderTheme::singleton().popupOptionSupportsTextIndent())
                    textX += minimumIntValueForLength(itemStyle.textIndent(), itemRect.width());
            } else {
                textX = itemRect.width() - client()->menuStyle().font().width(textRun);
                textX = std::min<int>(textX, textX - client()->clientPaddingRight() + client()->clientInsetRight());
                if (RenderTheme::singleton().popupOptionSupportsTextIndent())
                    textX -= minimumIntValueForLength(itemStyle.textIndent(), itemRect.width());
            }
            int textY = itemRect.y() + itemFont.fontMetrics().ascent() + (itemRect.height() - itemFont.fontMetrics().height()) / 2;
            context.drawBidiText(itemFont, textRun, IntPoint(textX, textY));
        }
    }

    if (m_scrollbar)
        m_scrollbar->paint(context, damageRect);

    HWndDC hWndDC;
    HDC localDC = hdc ? hdc : hWndDC.setHWnd(m_popup);

    ::BitBlt(localDC, damageRect.x(), damageRect.y(), damageRect.width(), damageRect.height(), m_DC.get(), damageRect.x(), damageRect.y(), SRCCOPY);
}

ScrollPosition PopupMenuWin::scrollPosition() const
{
    return { 0, m_scrollOffset };
}

void PopupMenuWin::setScrollOffset(const IntPoint& offset)
{
    scrollTo(offset.y());
}

void PopupMenuWin::scrollTo(int offset)
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

void PopupMenuWin::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    scrollRect.move(scrollbar.x(), scrollbar.y());
    RECT r = scrollRect;
    ::InvalidateRect(m_popup, &r, false);
}

IntSize PopupMenuWin::visibleSize() const
{
    return IntSize(m_windowRect.width(), m_scrollbar ? m_scrollbar->visibleSize() : m_windowRect.height());
}

IntSize PopupMenuWin::contentsSize() const
{
    return IntSize(m_windowRect.width(), m_scrollbar ? m_scrollbar->totalSize() : m_windowRect.height());
}

IntRect PopupMenuWin::scrollableAreaBoundingBox(bool*) const
{
    return m_windowRect;
}

bool PopupMenuWin::onGetObject(WPARAM wParam, LPARAM lParam, LRESULT& lResult)
{
    lResult = 0;

    if (static_cast<LONG>(lParam) != OBJID_CLIENT)
        return false;

    if (!m_accessiblePopupMenu)
        m_accessiblePopupMenu = new AccessiblePopupMenu(*this);

    static HMODULE accessibilityLib = nullptr;
    if (!accessibilityLib) {
        if (!(accessibilityLib = ::LoadLibraryW(L"oleacc.dll")))
            return false;
    }

    static LPFNLRESULTFROMOBJECT procPtr = reinterpret_cast<LPFNLRESULTFROMOBJECT>(::GetProcAddress(accessibilityLib, "LresultFromObject"));
    if (!procPtr)
        return false;

    // LresultFromObject returns a reference to the accessible object, stored
    // in an LRESULT. If this call is not successful, Windows will handle the
    // request through DefWindowProc.
    return SUCCEEDED(lResult = procPtr(__uuidof(IAccessible), wParam, m_accessiblePopupMenu.get()));
}

void PopupMenuWin::registerClass()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return;

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.hIconSm        = 0;
    wcex.style          = CS_DROPSHADOW;

    wcex.lpfnWndProc    = PopupMenuWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(PopupMenu*); // For the PopupMenu pointer
    wcex.hInstance      = WebCore::instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kPopupWindowClassName;

    haveRegisteredWindowClass = true;

    RegisterClassEx(&wcex);
}


LRESULT CALLBACK PopupMenuWin::PopupMenuWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (PopupMenuWin* popup = static_cast<PopupMenuWin*>(getWindowPointer(hWnd, 0)))
        return popup->wndProc(hWnd, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);

        // Associate the PopupMenu with the window.
        setWindowPointer(hWnd, 0, createStruct->lpCreateParams);
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT PopupMenuWin::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;

    switch (message) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_SIZE: {
            if (!scrollbar())
                break;

            IntSize size(LOWORD(lParam), HIWORD(lParam));
            scrollbar()->setFrameRect(IntRect(size.width() - scrollbar()->width(), 0, scrollbar()->width(), size.height()));

            int visibleItems = this->visibleItems();
            scrollbar()->setEnabled(visibleItems < client()->listSize());
            scrollbar()->setSteps(1, std::max(1, visibleItems - 1));
            scrollbar()->setProportion(visibleItems, client()->listSize());

            break;
        }
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: {
            if (!client())
                break;

            bool altKeyPressed = GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT;
            bool ctrlKeyPressed = GetKeyState(VK_CONTROL) & HIGH_BIT_MASK_SHORT;

            lResult = 0;
            switch (LOWORD(wParam)) {
                case VK_F4: {
                    if (!altKeyPressed && !ctrlKeyPressed) {
                        int index = focusedIndex();
                        ASSERT(index >= 0);
                        client()->valueChanged(index);
                        hide();
                    }
                    break;
                }
                case VK_DOWN:
                    if (altKeyPressed) {
                        int index = focusedIndex();
                        ASSERT(index >= 0);
                        client()->valueChanged(index);
                        hide();
                    } else
                        down();
                    break;
                case VK_RIGHT:
                    down();
                    break;
                case VK_UP:
                    if (altKeyPressed) {
                        int index = focusedIndex();
                        ASSERT(index >= 0);
                        client()->valueChanged(index);
                        hide();
                    } else
                        up();
                    break;
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
                    if (focusedIndex() != m_scrollOffset) {
                        // Set the selection to the first visible item
                        int firstVisibleItem = m_scrollOffset;
                        up(focusedIndex() - firstVisibleItem);
                    } else {
                        // The first visible item is selected, so move the selection back one page
                        up(visibleItems());
                    }
                    break;
                case VK_NEXT: {
                    int lastVisibleItem = m_scrollOffset + visibleItems() - 1;
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
                    ::SendMessage(client()->hostWindow()->platformPageClient(), message, wParam, lParam);
                    hide();
                    break;
                case VK_ESCAPE:
                    hide();
                    break;
                default:
                    if (isASCIIPrintable(wParam))
                        // Send the keydown to the WebView so it can be used for type-to-select.
                        // Since we know that the virtual key is ASCII printable, it's OK to convert this to
                        // a WM_CHAR message. (We don't want to call TranslateMessage because that will post a
                        // WM_CHAR message that will be stolen and redirected to the popup HWND.
                        ::PostMessage(m_popup, WM_HOST_WINDOW_CHAR, wParam, lParam);
                    else
                        lResult = 1;
                    break;
            }
            break;
        }
        case WM_CHAR: {
            if (!client())
                break;

            lResult = 0;
            int index;
            switch (wParam) {
                case 0x0D:   // Enter/Return
                    hide();
                    index = focusedIndex();
                    ASSERT(index >= 0);
                    client()->valueChanged(index);
                    break;
                case 0x1B:   // Escape
                    hide();
                    break;
                case 0x09:   // TAB
                case 0x08:   // Backspace
                case 0x0A:   // Linefeed
                default:     // Character
                    lResult = 1;
                    break;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            IntPoint mousePoint(MAKEPOINTS(lParam));
            if (scrollbar()) {
                IntRect scrollBarRect = scrollbar()->frameRect();
                if (scrollbarCapturingMouse() || scrollBarRect.contains(mousePoint)) {
                    // Put the point into coordinates relative to the scroll bar
                    mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
                    PlatformMouseEvent event(hWnd, message, wParam, makeScaledPoint(mousePoint, m_scaleFactor));
                    scrollbar()->mouseMoved(event);
                    break;
                }
            }

            BOOL shouldHotTrack = FALSE;
            if (!::SystemParametersInfo(SPI_GETHOTTRACKING, 0, &shouldHotTrack, 0))
                shouldHotTrack = FALSE;

            RECT bounds;
            GetClientRect(popupHandle(), &bounds);
            if (!::PtInRect(&bounds, mousePoint) && !(wParam & MK_LBUTTON) && client()) {
                // When the mouse is not inside the popup menu and the left button isn't down, just
                // repost the message to the web view.

                // Translate the coordinate.
                translatePoint(lParam, m_popup, client()->hostWindow()->platformPageClient());

                ::PostMessage(m_popup, WM_HOST_WINDOW_MOUSEMOVE, wParam, lParam);
                break;
            }

            if ((shouldHotTrack || wParam & MK_LBUTTON) && ::PtInRect(&bounds, mousePoint)) {
                setFocusedIndex(listIndexAtPoint(mousePoint), true);
                m_hoveredIndex = listIndexAtPoint(mousePoint);
            }

            break;
        }
        case WM_LBUTTONDOWN: {
            IntPoint mousePoint(MAKEPOINTS(lParam));
            if (scrollbar()) {
                IntRect scrollBarRect = scrollbar()->frameRect();
                if (scrollBarRect.contains(mousePoint)) {
                    // Put the point into coordinates relative to the scroll bar
                    mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
                    PlatformMouseEvent event(hWnd, message, wParam, makeScaledPoint(mousePoint, m_scaleFactor));
                    scrollbar()->mouseDown(event);
                    setScrollbarCapturingMouse(true);
                    break;
                }
            }

            // If the mouse is inside the window, update the focused index. Otherwise, 
            // hide the popup.
            RECT bounds;
            GetClientRect(m_popup, &bounds);
            if (::PtInRect(&bounds, mousePoint)) {
                setFocusedIndex(listIndexAtPoint(mousePoint), true);
                m_hoveredIndex = listIndexAtPoint(mousePoint);
            }
            else
                hide();
            break;
        }
        case WM_LBUTTONUP: {
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
                    ::InvalidateRect(popupHandle(), &r, TRUE);
                    break;
                }
            }
            // Only hide the popup if the mouse is inside the popup window.
            RECT bounds;
            GetClientRect(popupHandle(), &bounds);
            if (client() && ::PtInRect(&bounds, mousePoint)) {
                hide();
                int index = m_hoveredIndex;
                if (!client()->itemIsEnabled(index))
                    index = client()->selectedIndex();
                if (index >= 0)
                    client()->valueChanged(index);
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            if (!scrollbar())
                break;

            int i = 0;
            for (incrementWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam)); abs(wheelDelta()) >= WHEEL_DELTA; reduceWheelDelta(WHEEL_DELTA)) {
                if (wheelDelta() > 0)
                    ++i;
                else
                    --i;
            }

            ScrollableArea::scroll(i > 0 ? ScrollUp : ScrollDown, ScrollByLine, abs(i));
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT paintInfo;
            ::BeginPaint(popupHandle(), &paintInfo);
            paint(paintInfo.rcPaint, paintInfo.hdc);
            ::EndPaint(popupHandle(), &paintInfo);
            lResult = 0;
            break;
        }
        case WM_PRINTCLIENT:
            paint(clientRect(), (HDC)wParam);
            break;
        case WM_GETOBJECT:
            onGetObject(wParam, lParam, lResult);
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}

AccessiblePopupMenu::AccessiblePopupMenu(const PopupMenuWin& popupMenu)
    : m_popupMenu(popupMenu)
{
}

AccessiblePopupMenu::~AccessiblePopupMenu() = default;

HRESULT AccessiblePopupMenu::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    if (IsEqualGUID(riid, __uuidof(IAccessible)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IDispatch)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IUnknown)))
        *ppvObject = static_cast<IAccessible*>(this);
    else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG AccessiblePopupMenu::AddRef()
{
    return ++m_refCount;
}

ULONG AccessiblePopupMenu::Release()
{
    int refCount = --m_refCount;
    if (!refCount)
        delete this;
    return refCount;
}

HRESULT AccessiblePopupMenu::GetTypeInfoCount(_Out_ UINT* count)
{
    if (!count)
        return E_POINTER;
    *count = 0;
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::GetTypeInfo(UINT, LCID, _COM_Outptr_opt_ ITypeInfo** ppTInfo)
{
    if (!ppTInfo)
        return E_POINTER;
    *ppTInfo = nullptr;
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::GetIDsOfNames(_In_ REFIID, __in_ecount(cNames) LPOLESTR*, UINT cNames, LCID, __out_ecount_full(cNames) DISPID*)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accParent(_COM_Outptr_opt_ IDispatch** parent)
{
    if (!parent)
        return E_POINTER;
    *parent = nullptr;
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accChildCount(_Out_ long* count)
{
    if (!count)
        return E_POINTER;

    *count = m_popupMenu.visibleItems();
    return S_OK;
}

HRESULT AccessiblePopupMenu::get_accChild(VARIANT vChild, _COM_Outptr_opt_ IDispatch** ppChild)
{
    if (!ppChild)
        return E_POINTER;

    *ppChild = nullptr;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    notImplemented();
    return S_FALSE;
}

HRESULT AccessiblePopupMenu::get_accName(VARIANT vChild, __deref_out_opt BSTR* name)
{
    return get_accValue(vChild, name);
}

HRESULT AccessiblePopupMenu::get_accValue(VARIANT vChild, __deref_out_opt BSTR* value)
{
    if (!value)
        return E_POINTER;

    *value = nullptr;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    int index = vChild.lVal - 1;

    if (index < 0)
        return E_INVALIDARG;

    BString itemText(m_popupMenu.client()->itemText(index));
    *value = itemText.release();

    return S_OK;
}

HRESULT AccessiblePopupMenu::get_accDescription(VARIANT, __deref_out_opt BSTR*)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accRole(VARIANT vChild, _Out_ VARIANT* pvRole)
{
    if (!pvRole)
        return E_POINTER;
    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    // Scrollbar parts are encoded as negative values.
    if (vChild.lVal < 0) {
        V_VT(pvRole) = VT_I4;
        V_I4(pvRole) = ROLE_SYSTEM_SCROLLBAR;
    } else {
        V_VT(pvRole) = VT_I4;
        V_I4(pvRole) = ROLE_SYSTEM_LISTITEM;
    }

    return S_OK;
}

HRESULT AccessiblePopupMenu::get_accState(VARIANT vChild, _Out_ VARIANT* pvState)
{
    if (!pvState)
        return E_POINTER;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    V_VT(pvState) = VT_I4;
    V_I4(pvState) = 0; // STATE_SYSTEM_NORMAL
    
    return S_OK;
}

HRESULT AccessiblePopupMenu::get_accHelp(VARIANT vChild, __deref_out_opt BSTR* helpText)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accKeyboardShortcut(VARIANT vChild, __deref_out_opt BSTR*)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accFocus(_Out_ VARIANT* pvFocusedChild)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accSelection(_Out_ VARIANT* pvSelectedChild)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accDefaultAction(VARIANT vChild, __deref_out_opt BSTR* actionDescription)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::accSelect(long selectionFlags, VARIANT vChild)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::accLocation(_Out_ long* left, _Out_ long* top, _Out_ long* width, _Out_ long* height, VARIANT vChild)
{
    if (!left || !top || !width || !height)
        return E_POINTER;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    const IntRect& windowRect = m_popupMenu.windowRect();

    // Scrollbar parts are encoded as negative values.
    if (vChild.lVal < 0) {
        if (!m_popupMenu.scrollbar())
            return E_FAIL;

        Scrollbar& scrollbar = *m_popupMenu.scrollbar();
        WebCore::ScrollbarPart part = static_cast<WebCore::ScrollbarPart>(-vChild.lVal);

        ScrollbarThemeWin& theme = static_cast<ScrollbarThemeWin&>(scrollbar.theme());

        IntRect partRect;

        switch (part) {
        case BackTrackPart:
        case BackButtonStartPart:
            partRect = theme.backButtonRect(scrollbar, WebCore::BackTrackPart);
            break;
        case ThumbPart:
            partRect = theme.thumbRect(scrollbar);
            break;
        case ForwardTrackPart:
        case ForwardButtonEndPart:
            partRect = theme.forwardButtonRect(scrollbar, WebCore::ForwardTrackPart);
            break;
        case ScrollbarBGPart:
            partRect = theme.trackRect(scrollbar);
            break;
        default:
            return E_FAIL;
        }

        partRect.move(windowRect.x(), windowRect.y());

        *left = partRect.x();
        *top = partRect.y();
        *width = partRect.width();
        *height = partRect.height();

        return S_OK;
    }

    int index = vChild.lVal - 1;

    if (index < 0)
        return E_INVALIDARG;

    *left = windowRect.x();
    *top = windowRect.y() + (index - m_popupMenu.m_scrollOffset) * m_popupMenu.itemHeight();
    *width = windowRect.width();
    *height = m_popupMenu.itemHeight();

    return S_OK;
}

HRESULT AccessiblePopupMenu::accNavigate(long direction, VARIANT vFromChild, _Out_ VARIANT* pvNavigatedTo)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::accHitTest(long x, long y, _Out_ VARIANT* pvChildAtPoint)
{
    if (!pvChildAtPoint)
        return E_POINTER;

    ::VariantInit(pvChildAtPoint);

    IntRect windowRect = m_popupMenu.windowRect();

    IntPoint pt(x - windowRect.x(), y - windowRect.y());

    IntRect scrollRect;

    if (m_popupMenu.scrollbar())
        scrollRect = m_popupMenu.scrollbar()->frameRect();

    if (m_popupMenu.scrollbar() && scrollRect.contains(pt)) {
        Scrollbar& scrollbar = *m_popupMenu.scrollbar();

        pt.move(-scrollRect.x(), -scrollRect.y());

        WebCore::ScrollbarPart part = scrollbar.theme().hitTest(scrollbar, pt);

        V_VT(pvChildAtPoint) = VT_I4;
        V_I4(pvChildAtPoint) = -part; // Scrollbar parts are encoded as negative, to avoid mixup with item indexes.
        return S_OK;
    }

    int index = m_popupMenu.listIndexAtPoint(pt);

    if (index < 0) {
        V_VT(pvChildAtPoint) = VT_EMPTY;
        return S_OK;
    }

    V_VT(pvChildAtPoint) = VT_I4;
    V_I4(pvChildAtPoint) = index + 1; // CHILDID_SELF is 0, need to add 1.

    return S_OK;
}

HRESULT AccessiblePopupMenu::accDoDefaultAction(VARIANT vChild)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::put_accName(VARIANT, BSTR)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::put_accValue(VARIANT, BSTR)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessiblePopupMenu::get_accHelpTopic(BSTR* helpFile, VARIANT, _Out_ long* topicID)
{
    notImplemented();
    return E_NOTIMPL;
}

}
