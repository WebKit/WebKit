/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "PopupMenu.h"

#include "BitmapInfo.h"
#include "Document.h"
#include "FloatRect.h"
#include "FontSelector.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformScreen.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "SimpleFontData.h"
#include <tchar.h>
#include <windows.h>

using std::min;

namespace WebCore {

using namespace HTMLNames;

// Default Window animation duration in milliseconds
static const int defaultAnimationDuration = 200;
// Maximum height of a popup window
static const int maxPopupHeight = 320;

const int optionSpacingMiddle = 1;
const int popupWindowBorderWidth = 1;

static LPCTSTR kPopupWindowClassName = _T("PopupWindowClass");
static ATOM registerPopup();
static LRESULT CALLBACK PopupWndProc(HWND, UINT, WPARAM, LPARAM);

// FIXME: Remove this as soon as practical.
static inline bool isASCIIPrintable(unsigned c)
{
    return c >= 0x20 && c <= 0x7E;
}

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
    , m_scrollbar(0)
    , m_popup(0)
    , m_DC(0)
    , m_bmp(0)
    , m_wasClicked(false)
    , m_itemHeight(0)
    , m_scrollOffset(0)
    , m_wheelDelta(0)
    , m_focusedIndex(0)
    , m_scrollbarCapturingMouse(false)
{
}

PopupMenu::~PopupMenu()
{
    if (m_bmp)
        ::DeleteObject(m_bmp);
    if (m_DC)
        ::DeleteObject(m_DC);
    if (m_popup)
        ::DestroyWindow(m_popup);
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    calculatePositionAndSize(r, v);
    if (clientRect().isEmpty())
        return;

    if (!m_popup) {
        registerPopup();

        DWORD exStyle = WS_EX_LTRREADING;

        // Even though we already know our size and location at this point, we pass (0,0,0,0) as our size/location here.
        // We need to wait until after the call to ::SetWindowLongPtr to set our size so that in our WM_SIZE handler we can get access to the PopupMenu object
        m_popup = ::CreateWindowEx(exStyle, kPopupWindowClassName, _T("PopupMenu"),
            WS_POPUP | WS_BORDER,
            0, 0, 0, 0,
            v->hostWindow()->platformWindow(), 0, 0, 0);

        if (!m_popup)
            return;

        ::SetWindowLongPtr(m_popup, 0, (LONG_PTR)this);
    }

    if (!m_scrollbar)
        if (visibleItems() < client()->listSize()) {
            // We need a scroll bar
            m_scrollbar = client()->createScrollbar(this, VerticalScrollbar, SmallScrollbar);
        }

    ::SetWindowPos(m_popup, HWND_TOP, m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height(), 0);

    // Determine whether we should animate our popups
    // Note: Must use 'BOOL' and 'FALSE' instead of 'bool' and 'false' to avoid stack corruption with SystemParametersInfo
    BOOL shouldAnimate = FALSE;
    ::SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldAnimate, 0);

    if (shouldAnimate) {
        RECT viewRect = {0};
        ::GetWindowRect(v->hostWindow()->platformWindow(), &viewRect);

        if (!::IsRectEmpty(&viewRect)) {
            // Popups should slide into view away from the <select> box
            // NOTE: This may have to change for Vista
            DWORD slideDirection = (m_windowRect.y() < viewRect.top + v->contentsToWindow(r.location()).y()) ? AW_VER_NEGATIVE : AW_VER_POSITIVE;

            ::AnimateWindow(m_popup, defaultAnimationDuration, AW_SLIDE | slideDirection | AW_ACTIVATE);
        }
    } else
        ::ShowWindow(m_popup, SW_SHOWNORMAL);
    ::SetCapture(m_popup);

    if (client()) {
        int index = client()->selectedIndex();
        if (index >= 0)
            setFocusedIndex(index);
    }
}

void PopupMenu::hide()
{
    ::ShowWindow(m_popup, SW_HIDE);
}

const int endOfLinePadding = 2;
void PopupMenu::calculatePositionAndSize(const IntRect& r, FrameView* v)
{
    // r is in absolute document coordinates, but we want to be in screen coordinates

    // First, move to WebView coordinates
    IntRect rScreenCoords(v->contentsToWindow(r.location()), r.size());

    // Then, translate to screen coordinates
    POINT location(rScreenCoords.location());
    if (!::ClientToScreen(v->hostWindow()->platformWindow(), &location))
        return;

    rScreenCoords.setLocation(location);

    // First, determine the popup's height
    int itemCount = client()->listSize();
    m_itemHeight = client()->menuStyle().font().height() + optionSpacingMiddle;
    int naturalHeight = m_itemHeight * itemCount;
    int popupHeight = min(maxPopupHeight, naturalHeight);
    // The popup should show an integral number of items (i.e. no partial items should be visible)
    popupHeight -= popupHeight % m_itemHeight;
    
    // Next determine its width
    int popupWidth = 0;
    for (int i = 0; i < itemCount; ++i) {
        String text = client()->itemText(i);
        if (text.isEmpty())
            continue;

        Font itemFont = client()->menuStyle().font();
        if (client()->itemIsLabel(i)) {
            FontDescription d = itemFont.fontDescription();
            d.setWeight(d.bolderWeight());
            itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
            itemFont.update(m_popupClient->fontSelector());
        }

        popupWidth = max(popupWidth, itemFont.width(TextRun(text.characters(), text.length())));
    }

    if (naturalHeight > maxPopupHeight)
        // We need room for a scrollbar
        popupWidth += ScrollbarTheme::nativeTheme()->scrollbarThickness(SmallScrollbar);

    // Add padding to align the popup text with the <select> text
    // Note: We can't add paddingRight() because that value includes the width
    // of the dropdown button, so we must use our own endOfLinePadding constant.
    popupWidth += max(0, endOfLinePadding - client()->clientInsetRight()) + max(0, client()->clientPaddingLeft() - client()->clientInsetLeft());

    // Leave room for the border
    popupWidth += 2 * popupWindowBorderWidth;
    popupHeight += 2 * popupWindowBorderWidth;

    // The popup should be at least as wide as the control on the page
    popupWidth = max(rScreenCoords.width() - client()->clientInsetLeft() - client()->clientInsetRight(), popupWidth);

    // Always left-align items in the popup.  This matches popup menus on the mac.
    int popupX = rScreenCoords.x() + client()->clientInsetLeft();

    IntRect popupRect(popupX, rScreenCoords.bottom(), popupWidth, popupHeight);

    // The popup needs to stay within the bounds of the screen and not overlap any toolbars
    FloatRect screen = screenAvailableRect(v);

    // Check that we don't go off the screen vertically
    if (popupRect.bottom() > screen.height()) {
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
    if (popupRect.x() < screen.x()) {
        popupRect.setWidth(popupRect.width() - (screen.x() - popupRect.x()));
        popupRect.setX(screen.x());
    }
    m_windowRect = popupRect;
    return;
}

bool PopupMenu::setFocusedIndex(int i, bool hotTracking)
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

int PopupMenu::visibleItems() const
{
    return clientRect().height() / m_itemHeight;
}

int PopupMenu::listIndexAtPoint(const IntPoint& point) const
{
    return m_scrollOffset + point.y() / m_itemHeight;
}

int PopupMenu::focusedIndex() const
{
    return m_focusedIndex;
}

void PopupMenu::focusFirst()
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

void PopupMenu::focusLast()
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

bool PopupMenu::down(unsigned lines)
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

bool PopupMenu::up(unsigned lines)
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

void PopupMenu::invalidateItem(int index)
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

IntRect PopupMenu::clientRect() const
{
    IntRect clientRect = m_windowRect;
    clientRect.inflate(-popupWindowBorderWidth);
    clientRect.setLocation(IntPoint(0, 0));
    return clientRect;
}

void PopupMenu::incrementWheelDelta(int delta)
{
    m_wheelDelta += delta;
}

void PopupMenu::reduceWheelDelta(int delta)
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

bool PopupMenu::scrollToRevealSelection()
{
    if (!m_scrollbar)
        return false;

    int index = focusedIndex();

    if (index < m_scrollOffset) {
        m_scrollbar->setValue(index);
        return true;
    }

    if (index >= m_scrollOffset + visibleItems()) {
        m_scrollbar->setValue(index - visibleItems() + 1);
        return true;
    }

    return false;
}

void PopupMenu::updateFromElement()
{
    if (!m_popup)
        return;

    m_focusedIndex = client()->selectedIndex();

    ::InvalidateRect(m_popup, 0, TRUE);
    if (!scrollToRevealSelection())
        ::UpdateWindow(m_popup);
}

bool PopupMenu::itemWritingDirectionIsNatural() 
{ 
    return true; 
}

const int separatorPadding = 4;
const int separatorHeight = 1;
void PopupMenu::paint(const IntRect& damageRect, HDC hdc)
{
    if (!m_popup)
        return;

    if (!m_DC) {
        m_DC = ::CreateCompatibleDC(::GetDC(m_popup));
        if (!m_DC)
            return;
    }

    if (m_bmp) {
        bool keepBitmap = false;
        BITMAP bitmap;
        if (GetObject(m_bmp, sizeof(bitmap), &bitmap))
            keepBitmap = bitmap.bmWidth == clientRect().width()
                && bitmap.bmHeight == clientRect().height();
        if (!keepBitmap) {
            DeleteObject(m_bmp);
            m_bmp = 0;
        }
    }
    if (!m_bmp) {
        BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(clientRect().size());

        void* pixels = 0;
        m_bmp = ::CreateDIBSection(m_DC, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!m_bmp)
            return;

        ::SelectObject(m_DC, m_bmp);
    }

    GraphicsContext context(m_DC);

    int itemCount = client()->listSize();

    // listRect is the damageRect translated into the coordinates of the entire menu list (which is itemCount * m_itemHeight pixels tall)
    IntRect listRect = damageRect;
    listRect.move(IntSize(0, m_scrollOffset * m_itemHeight));

    for (int y = listRect.y(); y < listRect.bottom(); y += m_itemHeight) {
        int index = y / m_itemHeight;

        Color optionBackgroundColor, optionTextColor;
        PopupMenuStyle itemStyle = client()->itemStyle(index);
        if (index == focusedIndex()) {
            optionBackgroundColor = RenderTheme::defaultTheme()->activeListBoxSelectionBackgroundColor();
            optionTextColor = RenderTheme::defaultTheme()->activeListBoxSelectionForegroundColor();
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
            
        unsigned length = itemText.length();
        const UChar* string = itemText.characters();
        TextRun textRun(string, length, false, 0, 0, itemText.defaultWritingDirection() == WTF::Unicode::RightToLeft);

        context.setFillColor(optionTextColor);
        
        Font itemFont = client()->menuStyle().font();
        if (client()->itemIsLabel(index)) {
            FontDescription d = itemFont.fontDescription();
            d.setWeight(d.bolderWeight());
            itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
            itemFont.update(m_popupClient->fontSelector());
        }
        
        // Draw the item text
        if (itemStyle.isVisible()) {
            int textX = max(0, client()->clientPaddingLeft() - client()->clientInsetLeft());
            if (RenderTheme::defaultTheme()->popupOptionSupportsTextIndent() && itemStyle.textDirection() == LTR)
                textX += itemStyle.textIndent().calcMinValue(itemRect.width());
            int textY = itemRect.y() + itemFont.ascent() + (itemRect.height() - itemFont.height()) / 2;
            context.drawBidiText(itemFont, textRun, IntPoint(textX, textY));
        }
    }

    if (m_scrollbar)
        m_scrollbar->paint(&context, damageRect);

    if (!hdc)
        hdc = ::GetDC(m_popup);

    ::BitBlt(hdc, damageRect.x(), damageRect.y(), damageRect.width(), damageRect.height(), m_DC, damageRect.x(), damageRect.y(), SRCCOPY);
}

void PopupMenu::valueChanged(Scrollbar* scrollBar)
{
    ASSERT(m_scrollbar);

    if (!m_popup)
        return;

    int offset = scrollBar->value();

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

void PopupMenu::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    scrollRect.move(scrollbar->x(), scrollbar->y());
    RECT r = scrollRect;
    ::InvalidateRect(m_popup, &r, false);
}

static ATOM registerPopup()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DROPSHADOW;
    wcex.lpfnWndProc    = PopupWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(PopupMenu*); // For the PopupMenu pointer
    wcex.hInstance      = Page::instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kPopupWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return ::RegisterClassEx(&wcex);
}

const int smoothScrollAnimationDuration = 5000;
static LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    PopupMenu* popup = reinterpret_cast<PopupMenu*>(longPtr);

    switch (message) {
        case WM_SIZE:
            if (popup && popup->scrollbar()) {
                IntSize size(LOWORD(lParam), HIWORD(lParam));
                popup->scrollbar()->setFrameRect(IntRect(size.width() - popup->scrollbar()->width(), 0, popup->scrollbar()->width(), size.height()));

                int visibleItems = popup->visibleItems();
                popup->scrollbar()->setEnabled(visibleItems < popup->client()->listSize());
                popup->scrollbar()->setSteps(1, max(1, visibleItems - 1));
                popup->scrollbar()->setProportion(visibleItems, popup->client()->listSize());
            }
            break;
        case WM_ACTIVATE:
            if (popup && popup->client() && wParam == WA_INACTIVE)
                popup->client()->hidePopup();
            break;
        case WM_KILLFOCUS:
            if (popup && popup->client() && (HWND)wParam != popup->popupHandle())
                // Focus is going elsewhere, so hide
                popup->client()->hidePopup();
            break;
        case WM_KEYDOWN:
            if (popup && popup->client()) {
                lResult = 0;
                switch (LOWORD(wParam)) {
                    case VK_DOWN:
                    case VK_RIGHT:
                        popup->down();
                        break;
                    case VK_UP:
                    case VK_LEFT:
                        popup->up();
                        break;
                    case VK_HOME:
                        popup->focusFirst();
                        break;
                    case VK_END:
                        popup->focusLast();
                        break;
                    case VK_PRIOR:
                        if (popup->focusedIndex() != popup->scrollOffset()) {
                            // Set the selection to the first visible item
                            int firstVisibleItem = popup->scrollOffset();
                            popup->up(popup->focusedIndex() - firstVisibleItem);
                        } else
                            // The first visible item is selected, so move the selection back one page
                            popup->up(popup->visibleItems());
                        break;
                    case VK_NEXT:
                        if (popup) {
                            int lastVisibleItem = popup->scrollOffset() + popup->visibleItems() - 1;
                            if (popup->focusedIndex() != lastVisibleItem) {
                                // Set the selection to the last visible item
                                popup->down(lastVisibleItem - popup->focusedIndex());
                            } else
                                // The last visible item is selected, so move the selection forward one page
                                popup->down(popup->visibleItems());
                        }
                        break;
                    case VK_TAB:
                        ::SendMessage(popup->client()->hostWindow()->platformWindow(), message, wParam, lParam);
                        popup->client()->hidePopup();
                        break;
                    case VK_ESCAPE:
                        popup->client()->hidePopup();
                        break;
                    default:
                        if (isASCIIPrintable(wParam))
                            // Send the keydown to the WebView so it can be used for type-to-select.
                            ::PostMessage(popup->client()->hostWindow()->platformWindow(), message, wParam, lParam);
                        else
                            lResult = 1;
                        break;
                }
            }
            break;
        case WM_CHAR:
            if (popup && popup->client()) {
                lResult = 0;
                int index;
                switch (wParam) {
                    case 0x0D:   // Enter/Return
                        popup->client()->hidePopup();
                        index = popup->focusedIndex();
                        ASSERT(index >= 0);
                        popup->client()->valueChanged(index);
                        break;
                    case 0x1B:   // Escape
                        popup->client()->hidePopup();
                        break;
                    case 0x09:   // TAB
                    case 0x08:   // Backspace
                    case 0x0A:   // Linefeed
                    default:     // Character
                        lResult = 1;
                        break;
                }
            }
            break;
        case WM_MOUSEMOVE:
            if (popup) {
                IntPoint mousePoint(MAKEPOINTS(lParam));
                if (popup->scrollbar()) {
                    IntRect scrollBarRect = popup->scrollbar()->frameRect();
                    if (popup->scrollbarCapturingMouse() || scrollBarRect.contains(mousePoint)) {
                        // Put the point into coordinates relative to the scroll bar
                        mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
                        PlatformMouseEvent event(hWnd, message, wParam, MAKELPARAM(mousePoint.x(), mousePoint.y()));
                        popup->scrollbar()->mouseMoved(event);
                        break;
                    }
                }

                BOOL shouldHotTrack = FALSE;
                ::SystemParametersInfo(SPI_GETHOTTRACKING, 0, &shouldHotTrack, 0);

                RECT bounds;
                GetClientRect(popup->popupHandle(), &bounds);
                if ((shouldHotTrack || wParam & MK_LBUTTON) && ::PtInRect(&bounds, mousePoint))
                    popup->setFocusedIndex(popup->listIndexAtPoint(mousePoint), true);

                // Release capture if the left button isn't down, and the mousePoint is outside the popup window.
                // This way, the WebView will get future mouse events in the rest of the window.
                if (!(wParam & MK_LBUTTON) && !::PtInRect(&bounds, mousePoint)) {
                    ::ReleaseCapture();
                    break;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            if (popup) {
                ::SetCapture(popup->popupHandle());
                IntPoint mousePoint(MAKEPOINTS(lParam));
                if (popup->scrollbar()) {
                    IntRect scrollBarRect = popup->scrollbar()->frameRect();
                    if (scrollBarRect.contains(mousePoint)) {
                        // Put the point into coordinates relative to the scroll bar
                        mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
                        PlatformMouseEvent event(hWnd, message, wParam, MAKELPARAM(mousePoint.x(), mousePoint.y()));
                        popup->scrollbar()->mouseDown(event);
                        popup->setScrollbarCapturingMouse(true);
                        break;
                    }
                }

                popup->setFocusedIndex(popup->listIndexAtPoint(mousePoint), true);
            }
            break;
        case WM_LBUTTONUP:
            if (popup) {
                IntPoint mousePoint(MAKEPOINTS(lParam));
                if (popup->scrollbar()) {
                    ::ReleaseCapture();
                    IntRect scrollBarRect = popup->scrollbar()->frameRect();
                    if (popup->scrollbarCapturingMouse() || scrollBarRect.contains(mousePoint)) {
                        popup->setScrollbarCapturingMouse(false);
                        // Put the point into coordinates relative to the scroll bar
                        mousePoint.move(-scrollBarRect.x(), -scrollBarRect.y());
                        PlatformMouseEvent event(hWnd, message, wParam, MAKELPARAM(mousePoint.x(), mousePoint.y()));
                        popup->scrollbar()->mouseUp();
                        // FIXME: This is a hack to work around Scrollbar not invalidating correctly when it doesn't have a parent widget
                        RECT r = scrollBarRect;
                        ::InvalidateRect(popup->popupHandle(), &r, TRUE);
                        break;
                    }
                }
                // Only release capture and hide the popup if the mouse is inside the popup window.
                RECT bounds;
                GetClientRect(popup->popupHandle(), &bounds);
                if (popup->client() && ::PtInRect(&bounds, mousePoint)) {
                    ::ReleaseCapture();
                    popup->client()->hidePopup();
                    int index = popup->focusedIndex();
                    if (index >= 0)
                        popup->client()->valueChanged(index);
                }
            }
            break;
        case WM_MOUSEWHEEL:
            if (popup && popup->scrollbar()) {
                int i = 0;
                for (popup->incrementWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam)); abs(popup->wheelDelta()) >= WHEEL_DELTA; popup->reduceWheelDelta(WHEEL_DELTA))
                    if (popup->wheelDelta() > 0)
                        ++i;
                    else
                        --i;

                popup->scrollbar()->scroll(i > 0 ? ScrollUp : ScrollDown, ScrollByLine, abs(i));
            }
            break;
        case WM_PAINT:
            if (popup) {
                PAINTSTRUCT paintInfo;
                ::BeginPaint(popup->popupHandle(), &paintInfo);
                popup->paint(paintInfo.rcPaint, paintInfo.hdc);
                ::EndPaint(popup->popupHandle(), &paintInfo);
                lResult = 0;
            }
            break;
        case WM_PRINTCLIENT:
            if (popup)
                popup->paint(popup->clientRect(), (HDC)wParam);
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}

}
