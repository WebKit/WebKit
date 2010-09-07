/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebView.h"

#include "ChunkedUpdateDrawingAreaProxy.h"
#include "RunLoop.h"
#include "WebEditCommandProxy.h"
#include "WebEventFactory.h"
#include "WebPageNamespace.h"
#include "WebPageProxy.h"
#include <Commctrl.h>
#include <WebCore/IntRect.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static const LPCWSTR kWebKit2WebViewWindowClassName = L"WebKit2WebViewWindowClass";

// Constants not available on all platforms.
const int WM_XP_THEMECHANGED = 0x031A;
const int WM_VISTA_MOUSEHWHEEL = 0x020E;

static const int kMaxToolTipWidth = 250;

enum {
    UpdateActiveStateTimer = 1,
};

LRESULT CALLBACK WebView::WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = ::GetWindowLongPtr(hWnd, 0);
    
    if (WebView* webView = reinterpret_cast<WebView*>(longPtr))
        return webView->wndProc(hWnd, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);

        // Associate the WebView with the window.
        ::SetWindowLongPtr(hWnd, 0, (LONG_PTR)createStruct->lpCreateParams);
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT WebView::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    bool handled = true;

    switch (message) {
        case WM_DESTROY:
            m_isBeingDestroyed = true;
            close();
            break;
        case WM_ERASEBKGND:
            lResult = 1;
            break;
        case WM_PAINT:
            lResult = onPaintEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_PRINTCLIENT:
            lResult = onPrintClientEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSELEAVE:
            lResult = onMouseEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_MOUSEWHEEL:
        case WM_VISTA_MOUSEHWHEEL:
            lResult = onWheelEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        case WM_SYSCHAR:
        case WM_CHAR:
        case WM_SYSKEYUP:
        case WM_KEYUP:
            lResult = onKeyEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_SIZE:
            lResult = onSizeEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_WINDOWPOSCHANGED:
            lResult = onWindowPositionChangedEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_SETFOCUS:
            lResult = onSetFocusEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_KILLFOCUS:
            lResult = onKillFocusEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_TIMER:
            lResult = onTimerEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_SHOWWINDOW:
            lResult = onShowWindowEvent(hWnd, message, wParam, lParam, handled);
            break;
        case WM_SETCURSOR:
            lResult = onSetCursor(hWnd, message, wParam, lParam, handled);
            break;
        default:
            handled = false;
            break;
    }

    if (!handled)
        lResult = ::DefWindowProc(hWnd, message, wParam, lParam);

    return lResult;
}

bool WebView::registerWebViewWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;
    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebView::WebViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(WebView*);
    wcex.hInstance      = instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = ::LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebKit2WebViewWindowClassName;
    wcex.hIconSm        = 0;

    return !!::RegisterClassEx(&wcex);
}

WebView::WebView(RECT rect, WebPageNamespace* pageNamespace, HWND hostWindow)
    : m_rect(rect)
    , m_hostWindow(hostWindow)
    , m_topLevelParentWindow(0)
    , m_toolTipWindow(0)
    , m_lastCursorSet(0)
    , m_trackingMouseLeave(false)
    , m_isBeingDestroyed(false)
{
    registerWebViewWindowClass();

    m_page = pageNamespace->createWebPage();
    m_page->setPageClient(this);
    m_page->initializeWebPage(IntRect(rect).size(), ChunkedUpdateDrawingAreaProxy::create(this));

    m_window = ::CreateWindowEx(0, kWebKit2WebViewWindowClassName, 0, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        rect.top, rect.left, rect.right - rect.left, rect.bottom - rect.top, m_hostWindow ? m_hostWindow : HWND_MESSAGE, 0, instanceHandle(), this);
    ASSERT(::IsWindow(m_window));

    ::ShowWindow(m_window, SW_SHOW);

    // FIXME: Initializing the tooltip window here matches WebKit win, but seems like something
    // we could do on demand to save resources.
    initializeToolTipWindow();

    // Initialize the top level parent window and register it with the WindowMessageBroadcaster.
    windowAncestryDidChange();
}

WebView::~WebView()
{
    // Tooltip window needs to be explicitly destroyed since it isn't a WS_CHILD.
    if (::IsWindow(m_toolTipWindow))
        ::DestroyWindow(m_toolTipWindow);
}

void WebView::setHostWindow(HWND hostWindow)
{
    if (m_window) {
        // If the host window hasn't changed, bail.
        if (GetParent(m_window) == hostWindow)
            return;
        if (hostWindow)
            SetParent(m_window, hostWindow);
        else if (!m_isBeingDestroyed) {
            // Turn the WebView into a message-only window so it will no longer be a child of the
            // old host window and will be hidden from screen. We only do this when
            // isBeingDestroyed() is false because doing this while handling WM_DESTROY can leave
            // m_window in a weird state (see <http://webkit.org/b/29337>).
            SetParent(m_window, HWND_MESSAGE);
        }
    }

    m_hostWindow = hostWindow;
    
    windowAncestryDidChange();
}

static HWND findTopLevelParentWindow(HWND window)
{
    if (!window)
        return 0;

    HWND current = window;
    for (HWND parent = GetParent(current); current; current = parent, parent = GetParent(parent)) {
        if (!parent || !(GetWindowLongPtr(current, GWL_STYLE) & (WS_POPUP | WS_CHILD)))
            return current;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebView::windowAncestryDidChange()
{
    HWND newTopLevelParentWindow;
    if (m_window)
        newTopLevelParentWindow = findTopLevelParentWindow(m_hostWindow);
    else {
        // There's no point in tracking active state changes of our parent window if we don't have
        // a window ourselves.
        newTopLevelParentWindow = 0;
    }

    if (newTopLevelParentWindow == m_topLevelParentWindow)
        return;

    if (m_topLevelParentWindow)
        WindowMessageBroadcaster::removeListener(m_topLevelParentWindow, this);

    m_topLevelParentWindow = newTopLevelParentWindow;

    if (m_topLevelParentWindow)
        WindowMessageBroadcaster::addListener(m_topLevelParentWindow, this);

    updateActiveState();
}

LRESULT WebView::onMouseEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            ::SetFocus(m_window);
            ::SetCapture(m_window);
            break; 
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ::ReleaseCapture();
            break;
        case WM_MOUSEMOVE:
            startTrackingMouseLeave();
            break;
        case WM_MOUSELEAVE:
            stopTrackingMouseLeave();
            break;
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(hWnd, message, wParam, lParam);
    m_page->mouseEvent(mouseEvent);

    handled = true;
    return 0;
}

LRESULT WebView::onWheelEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    // Ctrl+Mouse wheel doesn't ever go into WebCore.  It is used to
    // zoom instead (Mac zooms the whole Desktop, but Windows browsers trigger their
    // own local zoom modes for Ctrl+wheel).
    /*
    if (wParam & MK_CONTROL) {
        short delta = static_cast<short>(HIWORD(wParam));
        if (delta < 0)
            m_page->makeTextSmaller(0);
        else
            m_page->makeTextLarger(0);

        handled = true;
        return 0;
    }
    */

    WebWheelEvent wheelEvent = WebEventFactory::createWebWheelEvent(hWnd, message, wParam, lParam);
    m_page->wheelEvent(wheelEvent);

    handled = true;
    return 0;
}

LRESULT WebView::onKeyEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(hWnd, message, wParam, lParam);
    m_page->keyEvent(keyboardEvent);

    handled = true;
    return 0;
}

LRESULT WebView::onPaintEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled)
{
    PAINTSTRUCT paintStruct;
    HDC hdc = ::BeginPaint(m_window, &paintStruct);

    m_page->drawingArea()->paint(IntRect(paintStruct.rcPaint), hdc);

    ::EndPaint(m_window, &paintStruct);

    handled = true;
    return 0;
}

LRESULT WebView::onPrintClientEvent(HWND hWnd, UINT, WPARAM wParam, LPARAM, bool& handled)
{
    HDC hdc = reinterpret_cast<HDC>(wParam);
    RECT winRect;
    ::GetClientRect(hWnd, &winRect);
    IntRect rect = winRect;

    m_page->drawingArea()->paint(rect, hdc);

    handled = true;
    return 0;
}

LRESULT WebView::onSizeEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);

    m_page->drawingArea()->setSize(IntSize(width, height));

    handled = true;
    return 0;
}

LRESULT WebView::onWindowPositionChangedEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    if (reinterpret_cast<WINDOWPOS*>(lParam)->flags & SWP_SHOWWINDOW)
        updateActiveStateSoon();

    handled = false;
    return 0;
}

LRESULT WebView::onSetFocusEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    m_page->setFocused(true);

    handled = true;
    return 0;
}

LRESULT WebView::onKillFocusEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    m_page->setFocused(false);

    handled = true;
    return 0;
}

LRESULT WebView::onTimerEvent(HWND hWnd, UINT, WPARAM wParam, LPARAM, bool& handled)
{
    switch (wParam) {
        case UpdateActiveStateTimer:
            ::KillTimer(hWnd, UpdateActiveStateTimer);
            updateActiveState();
            break;
    }

    handled = true;
    return 0;
}

LRESULT WebView::onShowWindowEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    // lParam is 0 when the message is sent because of a ShowWindow call.
    // FIXME: Is WM_SHOWWINDOW sent when ShowWindow is called on an ancestor of our window?
    if (!lParam) {
        bool isVisible = wParam;

        // Notify the drawing area that the visibility changed.
        m_page->drawingArea()->setPageIsVisible(isVisible);

        handled = true;
    }

    return 0;
}

LRESULT WebView::onSetCursor(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    handled = ::SetCursor(m_lastCursorSet);
    return 0;
}

bool WebView::isActive()
{
    HWND activeWindow = ::GetActiveWindow();
    return (activeWindow && m_topLevelParentWindow == findTopLevelParentWindow(activeWindow));
}

void WebView::updateActiveState()
{
    m_page->setActive(isActive());
}

void WebView::updateActiveStateSoon()
{
    // This function is called while processing the WM_NCACTIVATE message.
    // While processing WM_NCACTIVATE when we are being deactivated, GetActiveWindow() will
    // still return our window. If we were to call updateActiveState() in that case, we would
    // wrongly think that we are still the active window. To work around this, we update our
    // active state after a 0-delay timer fires, at which point GetActiveWindow() will return
    // the newly-activated window.

    ::SetTimer(m_window, UpdateActiveStateTimer, 0, 0);
}

static bool initCommonControls()
{
    static bool haveInitialized = false;
    if (haveInitialized)
        return true;

    INITCOMMONCONTROLSEX init;
    init.dwSize = sizeof(init);
    init.dwICC = ICC_TREEVIEW_CLASSES;
    haveInitialized = !!::InitCommonControlsEx(&init);
    return haveInitialized;
}

void WebView::initializeToolTipWindow()
{
    if (!initCommonControls())
        return;

    m_toolTipWindow = ::CreateWindowEx(WS_EX_TRANSPARENT, TOOLTIPS_CLASS, 0, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                       m_window, 0, 0, 0);
    if (!m_toolTipWindow)
        return;

    TOOLINFO info = {0};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.uId = reinterpret_cast<UINT_PTR>(m_window);

    ::SendMessage(m_toolTipWindow, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
    ::SendMessage(m_toolTipWindow, TTM_SETMAXTIPWIDTH, 0, kMaxToolTipWidth);
    ::SetWindowPos(m_toolTipWindow, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WebView::startTrackingMouseLeave()
{
    if (m_trackingMouseLeave)
        return;
    m_trackingMouseLeave = true;

    TRACKMOUSEEVENT trackMouseEvent;
    trackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
    trackMouseEvent.dwFlags = TME_LEAVE;
    trackMouseEvent.hwndTrack = m_window;

    ::TrackMouseEvent(&trackMouseEvent);
}

void WebView::stopTrackingMouseLeave()
{
    if (!m_trackingMouseLeave)
        return;
    m_trackingMouseLeave = false;

    TRACKMOUSEEVENT trackMouseEvent;
    trackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
    trackMouseEvent.dwFlags = TME_LEAVE | TME_CANCEL;
    trackMouseEvent.hwndTrack = m_window;

    ::TrackMouseEvent(&trackMouseEvent);
}

void WebView::close()
{
    setHostWindow(0);
    m_page->close();
}

// PageClient

void WebView::processDidExit()
{
}

void WebView::processDidRevive()
{
}

void WebView::takeFocus(bool)
{
}

void WebView::toolTipChanged(const String&, const String& newToolTip)
{
    if (!m_toolTipWindow)
        return;

    if (!newToolTip.isEmpty()) {
        // This is necessary because String::charactersWithNullTermination() is not const.
        String toolTip = newToolTip;

        TOOLINFO info = {0};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_IDISHWND;
        info.uId = reinterpret_cast<UINT_PTR>(m_window);
        info.lpszText = const_cast<UChar*>(toolTip.charactersWithNullTermination());
        ::SendMessage(m_toolTipWindow, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&info));
    }

    ::SendMessage(m_toolTipWindow, TTM_ACTIVATE, !newToolTip.isEmpty(), 0);
}

void WebView::setCursor(const WebCore::Cursor& cursor)
{
    HCURSOR platformCursor = cursor.platformCursor()->nativeCursor();
    if (!platformCursor)
        return;

    m_lastCursorSet = platformCursor;
    ::SetCursor(platformCursor);
}

void WebView::registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo)
{
}

void WebView::clearAllEditCommands()
{
}

#if USE(ACCELERATED_COMPOSITING)
void WebView::pageDidEnterAcceleratedCompositing()
{
}

void WebView::pageDidLeaveAcceleratedCompositing()
{
}
#endif // USE(ACCELERATED_COMPOSITING)

// WebCore::WindowMessageListener

void WebView::windowReceivedMessage(HWND, UINT message, WPARAM wParam, LPARAM)
{
    switch (message) {
        case WM_NCACTIVATE:
            updateActiveStateSoon();
            break;
        case WM_SETTINGCHANGE:
            // systemParameterChanged(wParam);
            break;
    }
}

} // namespace WebKit
