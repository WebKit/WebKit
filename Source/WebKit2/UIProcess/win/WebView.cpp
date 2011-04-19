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

#include "config.h"
#include "WebView.h"

#include "ChunkedUpdateDrawingAreaProxy.h"
#include "DrawingAreaProxyImpl.h"
#include "FindIndicator.h"
#include "Logging.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "Region.h"
#include "RunLoop.h"
#include "WKAPICast.h"
#include "WebContext.h"
#include "WebContextMenuProxyWin.h"
#include "WebEditCommandProxy.h"
#include "WebEventFactory.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyWin.h"
#include <Commctrl.h>
#include <WebCore/BitmapInfo.h>
#include <WebCore/Cursor.h>
#include <WebCore/FloatRect.h>
#if USE(CG)
#include <WebCore/GraphicsContextCG.h>
#endif
#include <WebCore/IntRect.h>
#include <WebCore/SoftLinking.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <WebCore/WindowsTouch.h>
#include <wtf/text/WTFString.h>

namespace Ime {
// We need these functions in a separate namespace, because in the global namespace they conflict
// with the definitions in imm.h only by the type modifier (the macro defines them as static) and
// imm.h is included by windows.h
SOFT_LINK_LIBRARY(IMM32)
SOFT_LINK(IMM32, ImmGetContext, HIMC, WINAPI, (HWND hwnd), (hwnd))
SOFT_LINK(IMM32, ImmReleaseContext, BOOL, WINAPI, (HWND hWnd, HIMC hIMC), (hWnd, hIMC))
SOFT_LINK(IMM32, ImmGetCompositionStringW, LONG, WINAPI, (HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen), (hIMC, dwIndex, lpBuf, dwBufLen))
SOFT_LINK(IMM32, ImmSetCandidateWindow, BOOL, WINAPI, (HIMC hIMC, LPCANDIDATEFORM lpCandidate), (hIMC, lpCandidate))
SOFT_LINK(IMM32, ImmSetOpenStatus, BOOL, WINAPI, (HIMC hIMC, BOOL fOpen), (hIMC, fOpen))
SOFT_LINK(IMM32, ImmNotifyIME, BOOL, WINAPI, (HIMC hIMC, DWORD dwAction, DWORD dwIndex, DWORD dwValue), (hIMC, dwAction, dwIndex, dwValue))
SOFT_LINK(IMM32, ImmAssociateContextEx, BOOL, WINAPI, (HWND hWnd, HIMC hIMC, DWORD dwFlags), (hWnd, hIMC, dwFlags))
};

// Soft link functions for gestures and panning.
SOFT_LINK_LIBRARY(USER32);
SOFT_LINK_OPTIONAL(USER32, GetGestureInfo, BOOL, WINAPI, (HGESTUREINFO, PGESTUREINFO));
SOFT_LINK_OPTIONAL(USER32, SetGestureConfig, BOOL, WINAPI, (HWND, DWORD, UINT, PGESTURECONFIG, UINT));
SOFT_LINK_OPTIONAL(USER32, CloseGestureInfoHandle, BOOL, WINAPI, (HGESTUREINFO));

SOFT_LINK_LIBRARY(Uxtheme);
SOFT_LINK_OPTIONAL(Uxtheme, BeginPanningFeedback, BOOL, WINAPI, (HWND));
SOFT_LINK_OPTIONAL(Uxtheme, EndPanningFeedback, BOOL, WINAPI, (HWND, BOOL));
SOFT_LINK_OPTIONAL(Uxtheme, UpdatePanningFeedback, BOOL, WINAPI, (HWND, LONG, LONG, BOOL));

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

static bool useNewDrawingArea()
{
    // FIXME: Remove this function and the old drawing area code once we aren't interested in
    // testing the old drawing area anymore.
    return true;
}

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
    case WM_CLOSE:
        m_page->tryClose();
        break;
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
    case WM_MOUSEACTIVATE:
        setWasActivatedByMouseEvent(true);
        handled = false;
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
    case WM_HSCROLL:
        lResult = onHorizontalScroll(hWnd, message, wParam, lParam, handled);
        break;
    case WM_VSCROLL:
        lResult = onVerticalScroll(hWnd, message, wParam, lParam, handled);
        break;
    case WM_GESTURENOTIFY:
        lResult = onGestureNotify(hWnd, message, wParam, lParam, handled);
        break;
    case WM_GESTURE:
        lResult = onGesture(hWnd, message, wParam, lParam, handled);
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
    case WM_IME_STARTCOMPOSITION:
        handled = onIMEStartComposition();
        break;
    case WM_IME_REQUEST:
        lResult = onIMERequest(wParam, lParam);
        break;
    case WM_IME_COMPOSITION:
        handled = onIMEComposition(lParam);
        break;
    case WM_IME_ENDCOMPOSITION:
        handled = onIMEEndComposition();
        break;
    case WM_IME_SELECT:
        handled = onIMESelect(wParam, lParam);
        break;
    case WM_IME_SETCONTEXT:
        handled = onIMESetContext(wParam, lParam);
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

WebView::WebView(RECT rect, WebContext* context, WebPageGroup* pageGroup, HWND parentWindow)
    : m_topLevelParentWindow(0)
    , m_toolTipWindow(0)
    , m_lastCursorSet(0)
    , m_webCoreCursor(0)
    , m_overrideCursor(0)
    , m_trackingMouseLeave(false)
    , m_isInWindow(false)
    , m_isVisible(false)
    , m_wasActivatedByMouseEvent(false)
    , m_isBeingDestroyed(false)
    , m_inIMEComposition(0)
    , m_findIndicatorCallback(0)
    , m_findIndicatorCallbackContext(0)
    , m_lastPanX(0)
    , m_lastPanY(0)
    , m_overPanY(0)
    , m_gestureReachedScrollingLimit(false)
{
    registerWebViewWindowClass();

    m_window = ::CreateWindowExW(0, kWebKit2WebViewWindowClassName, 0, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
        rect.top, rect.left, rect.right - rect.left, rect.bottom - rect.top, parentWindow ? parentWindow : HWND_MESSAGE, 0, instanceHandle(), this);
    ASSERT(::IsWindow(m_window));
    // We only check our window style, and not ::IsWindowVisible, because m_isVisible only tracks
    // this window's visibility status, while ::IsWindowVisible takes our ancestors' visibility
    // status into account. <http://webkit.org/b/54104>
    ASSERT(m_isVisible == static_cast<bool>(::GetWindowLong(m_window, GWL_STYLE) & WS_VISIBLE));

    m_page = context->createWebPage(this, pageGroup);
    m_page->initializeWebPage();

    CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper, (void**)&m_dropTargetHelper);

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

void WebView::initialize()
{
    ::RegisterDragDrop(m_window, this);

    if (shouldInitializeTrackPointHack()) {
        // If we detected a registry key belonging to a TrackPoint driver, then create fake
        // scrollbars, so the WebView will receive WM_VSCROLL and WM_HSCROLL messages.
        // We create an invisible vertical scrollbar and an invisible horizontal scrollbar to allow
        // for receiving both types of messages.
        ::CreateWindow(TEXT("SCROLLBAR"), TEXT("FAKETRACKPOINTHSCROLLBAR"), WS_CHILD | WS_VISIBLE | SBS_HORZ, 0, 0, 0, 0, m_window, 0, instanceHandle(), 0);
        ::CreateWindow(TEXT("SCROLLBAR"), TEXT("FAKETRACKPOINTVSCROLLBAR"), WS_CHILD | WS_VISIBLE | SBS_VERT, 0, 0, 0, 0, m_window, 0, instanceHandle(), 0);
    }
}

void WebView::initializeUndoClient(const WKViewUndoClient* client)
{
    m_undoClient.initialize(client);
}

void WebView::setParentWindow(HWND parentWindow)
{
    if (m_window) {
        // If the host window hasn't changed, bail.
        if (::GetParent(m_window) == parentWindow)
            return;
        if (parentWindow)
            ::SetParent(m_window, parentWindow);
        else if (!m_isBeingDestroyed) {
            // Turn the WebView into a message-only window so it will no longer be a child of the
            // old parent window and will be hidden from screen. We only do this when
            // isBeingDestroyed() is false because doing this while handling WM_DESTROY can leave
            // m_window in a weird state (see <http://webkit.org/b/29337>).
            ::SetParent(m_window, HWND_MESSAGE);
        }
    }

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
        newTopLevelParentWindow = findTopLevelParentWindow(m_window);
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
    NativeWebMouseEvent mouseEvent = NativeWebMouseEvent(hWnd, message, wParam, lParam, m_wasActivatedByMouseEvent);
    setWasActivatedByMouseEvent(false);
    
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

    m_page->handleMouseEvent(mouseEvent);

    handled = true;
    return 0;
}

LRESULT WebView::onWheelEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    WebWheelEvent wheelEvent = WebEventFactory::createWebWheelEvent(hWnd, message, wParam, lParam);
    if (wheelEvent.controlKey()) {
        // We do not want WebKit to handle Control + Wheel, this should be handled by the client application
        // to zoom the page.
        handled = false;
        return 0;
    }

    m_page->handleWheelEvent(wheelEvent);

    handled = true;
    return 0;
}

LRESULT WebView::onHorizontalScroll(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (LOWORD(wParam)) {
    case SB_LINELEFT:
        granularity = ScrollByLine;
        direction = ScrollLeft;
        break;
    case SB_LINERIGHT:
        granularity = ScrollByLine;
        direction = ScrollRight;
        break;
    case SB_PAGELEFT:
        granularity = ScrollByDocument;
        direction = ScrollLeft;
        break;
    case SB_PAGERIGHT:
        granularity = ScrollByDocument;
        direction = ScrollRight;
        break;
    default:
        handled = false;
        return 0;
    }

    m_page->scrollBy(direction, granularity);

    handled = true;
    return 0;
}

LRESULT WebView::onVerticalScroll(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (LOWORD(wParam)) {
    case SB_LINEDOWN:
        granularity = ScrollByLine;
        direction = ScrollDown;
        break;
    case SB_LINEUP:
        granularity = ScrollByLine;
        direction = ScrollUp;
        break;
    case SB_PAGEDOWN:
        granularity = ScrollByDocument;
        direction = ScrollDown;
        break;
    case SB_PAGEUP:
        granularity = ScrollByDocument;
        direction = ScrollUp;
        break;
    default:
        handled = false;
        return 0;
    }

    m_page->scrollBy(direction, granularity);

    handled = true;
    return 0;
}

LRESULT WebView::onGestureNotify(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    // We shouldn't be getting any gesture messages without SetGestureConfig soft-linking correctly.
    ASSERT(SetGestureConfigPtr());

    GESTURENOTIFYSTRUCT* gn = reinterpret_cast<GESTURENOTIFYSTRUCT*>(lParam);

    POINT localPoint = { gn->ptsLocation.x, gn->ptsLocation.y };
    ::ScreenToClient(m_window, &localPoint);

    bool canPan = m_page->gestureWillBegin(localPoint);

    DWORD dwPanWant = GC_PAN | GC_PAN_WITH_INERTIA | GC_PAN_WITH_GUTTER;
    DWORD dwPanBlock = GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
    if (canPan)
        dwPanWant |= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
    else
        dwPanBlock |= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;

    GESTURECONFIG gc = { GID_PAN, dwPanWant, dwPanBlock };
    return SetGestureConfigPtr()(m_window, 0, 1, &gc, sizeof(gc));
}

LRESULT WebView::onGesture(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    ASSERT(GetGestureInfoPtr());
    ASSERT(CloseGestureInfoHandlePtr());
    ASSERT(UpdatePanningFeedbackPtr());
    ASSERT(BeginPanningFeedbackPtr());
    ASSERT(EndPanningFeedbackPtr());

    if (!GetGestureInfoPtr() || !CloseGestureInfoHandlePtr() || !UpdatePanningFeedbackPtr() || !BeginPanningFeedbackPtr() || !EndPanningFeedbackPtr()) {
        handled = false;
        return 0;
    }

    HGESTUREINFO gestureHandle = reinterpret_cast<HGESTUREINFO>(lParam);
    GESTUREINFO gi = {0};
    gi.cbSize = sizeof(GESTUREINFO);

    if (!GetGestureInfoPtr()(gestureHandle, &gi)) {
        handled = false;
        return 0;
    }

    switch (gi.dwID) {
    case GID_BEGIN:
        m_lastPanX = gi.ptsLocation.x;
        m_lastPanY = gi.ptsLocation.y;
        break;
    case GID_END:
        m_page->gestureDidEnd();
        break;
    case GID_PAN: {
        int currentX = gi.ptsLocation.x;
        int currentY = gi.ptsLocation.y;

        // Reverse the calculations because moving your fingers up should move the screen down, and
        // vice-versa.
        int deltaX = m_lastPanX - currentX;
        int deltaY = m_lastPanY - currentY;

        m_lastPanX = currentX;
        m_lastPanY = currentY;

        // Calculate the overpan for window bounce.
        m_overPanY -= deltaY;

        if (deltaX || deltaY)
            m_page->gestureDidScroll(IntSize(deltaX, deltaY));

        if (gi.dwFlags & GF_BEGIN) {
            BeginPanningFeedbackPtr()(m_window);
            m_gestureReachedScrollingLimit = false;
            m_overPanY = 0;
        } else if (gi.dwFlags & GF_END) {
            EndPanningFeedbackPtr()(m_window, true);
            m_overPanY = 0;
        }

        // FIXME: Support horizontal window bounce - <http://webkit.org/b/58068>.
        // FIXME: Window Bounce doesn't undo until user releases their finger - <http://webkit.org/b/58069>.

        if (m_gestureReachedScrollingLimit)
            UpdatePanningFeedbackPtr()(m_window, 0, m_overPanY, gi.dwFlags & GF_INERTIA);

        CloseGestureInfoHandlePtr()(gestureHandle);

        handled = true;
        return 0;
    }
    default:
        break;
    }

    // If we get to this point, the gesture has not been handled. We forward
    // the call to DefWindowProc by returning false, and we don't need to 
    // to call CloseGestureInfoHandle. 
    // http://msdn.microsoft.com/en-us/library/dd353228(VS.85).aspx
    handled = false;
    return 0;
}

LRESULT WebView::onKeyEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    m_page->handleKeyboardEvent(NativeWebKeyboardEvent(hWnd, message, wParam, lParam));

    // We claim here to always have handled the event. If the event is not in fact handled, we will
    // find out later in didNotHandleKeyEvent.
    handled = true;
    return 0;
}

static void drawPageBackground(HDC dc, const RECT& rect)
{
    // Mac checks WebPageProxy::drawsBackground and
    // WebPageProxy::drawsTransparentBackground here, but those are always false on
    // Windows currently (see <http://webkit.org/b/52009>).
    ::FillRect(dc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
}

void WebView::paint(HDC hdc, const IntRect& dirtyRect)
{
    m_page->endPrinting();
    if (useNewDrawingArea()) {
        if (DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(m_page->drawingArea())) {
            // FIXME: We should port WebKit1's rect coalescing logic here.
            Region unpaintedRegion;
            drawingArea->paint(hdc, dirtyRect, unpaintedRegion);

            Vector<IntRect> unpaintedRects = unpaintedRegion.rects();
            for (size_t i = 0; i < unpaintedRects.size(); ++i) {
                RECT winRect = unpaintedRects[i];
                drawPageBackground(hdc, unpaintedRects[i]);
            }
        } else
            drawPageBackground(hdc, dirtyRect);

        m_page->didDraw();
    } else {
        if (m_page->isValid() && m_page->drawingArea() && m_page->drawingArea()->paint(dirtyRect, hdc))
            m_page->didDraw();
        else
            drawPageBackground(hdc, dirtyRect);
    }
}

static void flashRects(HDC dc, const IntRect rects[], size_t rectCount, HBRUSH brush)
{
    for (size_t i = 0; i < rectCount; ++i) {
        RECT winRect = rects[i];
        ::FillRect(dc, &winRect, brush);
    }

    ::GdiFlush();
    ::Sleep(50);
}

static OwnPtr<HBRUSH> createBrush(const Color& color)
{
    return adoptPtr(::CreateSolidBrush(RGB(color.red(), color.green(), color.blue())));
}

LRESULT WebView::onPaintEvent(HWND hWnd, UINT message, WPARAM, LPARAM, bool& handled)
{
    PAINTSTRUCT paintStruct;
    HDC hdc = ::BeginPaint(m_window, &paintStruct);

    if (WebPageProxy::debugPaintFlags() & kWKDebugFlashViewUpdates) {
        static HBRUSH brush = createBrush(WebPageProxy::viewUpdatesFlashColor().rgb()).leakPtr();
        IntRect rect = paintStruct.rcPaint;
        flashRects(hdc, &rect, 1, brush);
    }

    paint(hdc, paintStruct.rcPaint);

    ::EndPaint(m_window, &paintStruct);

    handled = true;
    return 0;
}

LRESULT WebView::onPrintClientEvent(HWND hWnd, UINT, WPARAM wParam, LPARAM, bool& handled)
{
    HDC hdc = reinterpret_cast<HDC>(wParam);
    RECT winRect;
    ::GetClientRect(hWnd, &winRect);

    // Twidding the visibility flags tells the DrawingArea to resume painting. Right now, the
    // the visible state of the view only affects whether or not painting happens, but in the
    // future it could affect more, which we wouldn't want to touch here.

    // FIXME: We should have a better way of telling the WebProcess to draw even if we're
    // invisible than twiddling the visibility flag.

    bool wasVisible = isViewVisible();
    if (!wasVisible)
        setIsVisible(true);

    paint(hdc, winRect);

    if (!wasVisible)
        setIsVisible(false);

    handled = true;
    return 0;
}

LRESULT WebView::onSizeEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    int width = LOWORD(lParam);
    int height = HIWORD(lParam);

    if (m_page && m_page->drawingArea()) {
        m_page->drawingArea()->setSize(IntSize(width, height), m_nextResizeScrollOffset);
        m_nextResizeScrollOffset = IntSize();
    }

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
    m_page->viewStateDidChange(WebPageProxy::ViewIsFocused);
    handled = true;
    return 0;
}

LRESULT WebView::onKillFocusEvent(HWND, UINT, WPARAM, LPARAM lParam, bool& handled)
{
    m_page->viewStateDidChange(WebPageProxy::ViewIsFocused);
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
    // FIXME: Since we don't get notified when an ancestor window is hidden or shown, we will keep
    // painting even when we have a hidden ancestor. <http://webkit.org/b/54104>
    if (!lParam)
        setIsVisible(wParam);

    handled = false;
    return 0;
}

LRESULT WebView::onSetCursor(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    if (!m_lastCursorSet) {
        handled = false;
        return 0;
    }

    ::SetCursor(m_lastCursorSet);
    return 0;
}

void WebView::updateActiveState()
{
    m_page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
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

bool WebView::shouldInitializeTrackPointHack()
{
    static bool shouldCreateScrollbars;
    static bool hasRunTrackPointCheck;

    if (hasRunTrackPointCheck)
        return shouldCreateScrollbars;

    hasRunTrackPointCheck = true;
    const wchar_t* trackPointKeys[] = { 
        L"Software\\Lenovo\\TrackPoint",
        L"Software\\Lenovo\\UltraNav",
        L"Software\\Alps\\Apoint\\TrackPoint",
        L"Software\\Synaptics\\SynTPEnh\\UltraNavUSB",
        L"Software\\Synaptics\\SynTPEnh\\UltraNavPS2"
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(trackPointKeys); ++i) {
        HKEY trackPointKey;
        int readKeyResult = ::RegOpenKeyExW(HKEY_CURRENT_USER, trackPointKeys[i], 0, KEY_READ, &trackPointKey);
        ::RegCloseKey(trackPointKey);
        if (readKeyResult == ERROR_SUCCESS) {
            shouldCreateScrollbars = true;
            return shouldCreateScrollbars;
        }
    }

    return shouldCreateScrollbars;
}

void WebView::close()
{
    m_undoClient.initialize(0);
    ::RevokeDragDrop(m_window);
    if (m_window) {
        // We can't check IsWindow(m_window) here, because that will return true even while
        // we're already handling WM_DESTROY. So we check !m_isBeingDestroyed instead.
        if (!m_isBeingDestroyed)
            DestroyWindow(m_window);
        // Either we just destroyed m_window, or it's in the process of being destroyed. Either
        // way, we clear it out to make sure we don't try to use it later.
        m_window = 0;
    }
    setParentWindow(0);
    m_page->close();
}

// PageClient

PassOwnPtr<DrawingAreaProxy> WebView::createDrawingAreaProxy()
{
    if (useNewDrawingArea())
        return DrawingAreaProxyImpl::create(m_page.get());

    return ChunkedUpdateDrawingAreaProxy::create(this, m_page.get());
}

void WebView::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    RECT r = rect;
    ::InvalidateRect(m_window, &r, false);
}

void WebView::displayView()
{
    ::UpdateWindow(m_window);
}

void WebView::scrollView(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    // FIXME: Actually scroll the view contents.
    setViewNeedsDisplay(scrollRect);
}

void WebView::flashBackingStoreUpdates(const Vector<IntRect>& updateRects)
{
    static HBRUSH brush = createBrush(WebPageProxy::backingStoreUpdatesFlashColor().rgb()).leakPtr();
    HDC dc = ::GetDC(m_window);
    flashRects(dc, updateRects.data(), updateRects.size(), brush);
    ::ReleaseDC(m_window, dc);
}

WebCore::IntSize WebView::viewSize()
{
    RECT clientRect;
    GetClientRect(m_window, &clientRect);

    return IntRect(clientRect).size();
}

bool WebView::isViewWindowActive()
{    
    HWND activeWindow = ::GetActiveWindow();
    return (activeWindow && m_topLevelParentWindow == findTopLevelParentWindow(activeWindow));
}

bool WebView::isViewFocused()
{
    return ::GetFocus() == m_window;
}

bool WebView::isViewVisible()
{
    return m_isVisible;
}

bool WebView::isViewInWindow()
{
    return m_isInWindow;
}

void WebView::pageClosed()
{
}

void WebView::processDidCrash()
{
    updateNativeCursor();
    ::InvalidateRect(m_window, 0, TRUE);
}

void WebView::didRelaunchProcess()
{
    updateNativeCursor();
    ::InvalidateRect(m_window, 0, TRUE);
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

HCURSOR WebView::cursorToShow() const
{
    if (!m_page->isValid())
        return 0;

    // We only show the override cursor if the default (arrow) cursor is showing.
    static HCURSOR arrowCursor = ::LoadCursor(0, IDC_ARROW);
    if (m_overrideCursor && m_webCoreCursor == arrowCursor)
        return m_overrideCursor;

    return m_webCoreCursor;
}

void WebView::updateNativeCursor()
{
    m_lastCursorSet = cursorToShow();
    if (!m_lastCursorSet)
        return;
    ::SetCursor(m_lastCursorSet);
}

void WebView::setCursor(const WebCore::Cursor& cursor)
{
    if (!cursor.platformCursor()->nativeCursor())
        return;
    m_webCoreCursor = cursor.platformCursor()->nativeCursor();
    updateNativeCursor();
}

void WebView::setOverrideCursor(HCURSOR overrideCursor)
{
    m_overrideCursor = overrideCursor;
    updateNativeCursor();
}

void WebView::setInitialFocus(bool forward)
{
    m_page->setInitialFocus(forward);
}

void WebView::setScrollOffsetOnNextResize(const IntSize& scrollOffset)
{
    // The next time we get a WM_SIZE message, scroll by the specified amount in onSizeEvent().
    m_nextResizeScrollOffset = scrollOffset;
}

void WebView::setViewportArguments(const WebCore::ViewportArguments&)
{
}

void WebView::registerEditCommand(PassRefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    RefPtr<WebEditCommandProxy> command = prpCommand;
    m_undoClient.registerEditCommand(this, command, undoOrRedo);
}

void WebView::clearAllEditCommands()
{
    m_undoClient.clearAllEditCommands(this);
}

bool WebView::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return m_undoClient.canUndoRedo(this, undoOrRedo);
}

void WebView::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_undoClient.executeUndoRedo(this, undoOrRedo);
}
    
void WebView::reapplyEditCommand(WebEditCommandProxy* command)
{
    if (!m_page->isValid() || !m_page->isValidEditCommand(command))
        return;
    
    command->reapply();
}

void WebView::unapplyEditCommand(WebEditCommandProxy* command)
{
    if (!m_page->isValid() || !m_page->isValidEditCommand(command))
        return;
    
    command->unapply();
}

FloatRect WebView::convertToDeviceSpace(const FloatRect& rect)
{
    return rect;
}

IntRect WebView::windowToScreen(const IntRect& rect)
{
    return rect;
}

FloatRect WebView::convertToUserSpace(const FloatRect& rect)
{
    return rect;
}

HIMC WebView::getIMMContext() 
{
    return Ime::ImmGetContext(m_window);
}

void WebView::prepareCandidateWindow(HIMC hInputContext) 
{
    IntRect caret = m_page->firstRectForCharacterInSelectedRange(0);
    CANDIDATEFORM form;
    form.dwIndex = 0;
    form.dwStyle = CFS_EXCLUDE;
    form.ptCurrentPos.x = caret.x();
    form.ptCurrentPos.y = caret.maxY();
    form.rcArea.top = caret.y();
    form.rcArea.bottom = caret.maxY();
    form.rcArea.left = caret.x();
    form.rcArea.right = caret.maxX();
    Ime::ImmSetCandidateWindow(hInputContext, &form);
}

void WebView::resetIME()
{
    HIMC hInputContext = getIMMContext();
    if (!hInputContext)
        return;
    Ime::ImmNotifyIME(hInputContext, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    Ime::ImmReleaseContext(m_window, hInputContext);
}

void WebView::setInputMethodState(bool enabled)
{
    Ime::ImmAssociateContextEx(m_window, 0, enabled ? IACE_DEFAULT : 0);
}

void WebView::compositionSelectionChanged(bool hasChanged)
{
    if (m_page->editorState().hasComposition && !hasChanged)
        resetIME();
}

bool WebView::onIMEStartComposition()
{
    LOG(TextInput, "onIMEStartComposition");
    m_inIMEComposition++;

    HIMC hInputContext = getIMMContext();
    if (!hInputContext)
        return false;
    prepareCandidateWindow(hInputContext);
    Ime::ImmReleaseContext(m_window, hInputContext);
    return true;
}

static bool getCompositionString(HIMC hInputContext, DWORD type, String& result)
{
    LONG compositionLength = Ime::ImmGetCompositionStringW(hInputContext, type, 0, 0);
    if (compositionLength <= 0)
        return false;
    Vector<UChar> compositionBuffer(compositionLength / 2);
    compositionLength = Ime::ImmGetCompositionStringW(hInputContext, type, compositionBuffer.data(), compositionLength);
    result = String::adopt(compositionBuffer);
    return true;
}

static void compositionToUnderlines(const Vector<DWORD>& clauses, const Vector<BYTE>& attributes, Vector<CompositionUnderline>& underlines)
{
    if (clauses.isEmpty()) {
        underlines.clear();
        return;
    }
  
    size_t numBoundaries = clauses.size() - 1;
    underlines.resize(numBoundaries);
    for (unsigned i = 0; i < numBoundaries; ++i) {
        underlines[i].startOffset = clauses[i];
        underlines[i].endOffset = clauses[i + 1];
        BYTE attribute = attributes[clauses[i]];
        underlines[i].thick = attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED;
        underlines[i].color = Color::black;
    }
}

#if !LOG_DISABLED
#define APPEND_ARGUMENT_NAME(name) \
    if (lparam & name) { \
        if (needsComma) \
            result += ", "; \
            result += #name; \
        needsComma = true; \
    }

static String imeCompositionArgumentNames(LPARAM lparam)
{
    String result;
    bool needsComma = false;

    APPEND_ARGUMENT_NAME(GCS_COMPATTR);
    APPEND_ARGUMENT_NAME(GCS_COMPCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_COMPREADSTR);
    APPEND_ARGUMENT_NAME(GCS_COMPREADATTR);
    APPEND_ARGUMENT_NAME(GCS_COMPREADCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_COMPSTR);
    APPEND_ARGUMENT_NAME(GCS_CURSORPOS);
    APPEND_ARGUMENT_NAME(GCS_DELTASTART);
    APPEND_ARGUMENT_NAME(GCS_RESULTCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_RESULTREADCLAUSE);
    APPEND_ARGUMENT_NAME(GCS_RESULTREADSTR);
    APPEND_ARGUMENT_NAME(GCS_RESULTSTR);
    APPEND_ARGUMENT_NAME(CS_INSERTCHAR);
    APPEND_ARGUMENT_NAME(CS_NOMOVECARET);

    return result;
}

static String imeRequestName(WPARAM wparam)
{
    switch (wparam) {
    case IMR_CANDIDATEWINDOW:
        return "IMR_CANDIDATEWINDOW";
    case IMR_COMPOSITIONFONT:
        return "IMR_COMPOSITIONFONT";
    case IMR_COMPOSITIONWINDOW:
        return "IMR_COMPOSITIONWINDOW";
    case IMR_CONFIRMRECONVERTSTRING:
        return "IMR_CONFIRMRECONVERTSTRING";
    case IMR_DOCUMENTFEED:
        return "IMR_DOCUMENTFEED";
    case IMR_QUERYCHARPOSITION:
        return "IMR_QUERYCHARPOSITION";
    case IMR_RECONVERTSTRING:
        return "IMR_RECONVERTSTRING";
    default:
        return "Unknown (" + String::number(wparam) + ")";
    }
}
#endif

bool WebView::onIMEComposition(LPARAM lparam)
{
    LOG(TextInput, "onIMEComposition %s", imeCompositionArgumentNames(lparam).latin1().data());
    HIMC hInputContext = getIMMContext();
    if (!hInputContext)
        return true;

    if (!m_page->editorState().isContentEditable)
        return true;

    prepareCandidateWindow(hInputContext);

    if (lparam & GCS_RESULTSTR || !lparam) {
        String compositionString;
        if (!getCompositionString(hInputContext, GCS_RESULTSTR, compositionString) && lparam)
            return true;
        
        m_page->confirmComposition(compositionString);
        return true;
    }

    String compositionString;
    if (!getCompositionString(hInputContext, GCS_COMPSTR, compositionString))
        return true;
    
    // Composition string attributes
    int numAttributes = Ime::ImmGetCompositionStringW(hInputContext, GCS_COMPATTR, 0, 0);
    Vector<BYTE> attributes(numAttributes);
    Ime::ImmGetCompositionStringW(hInputContext, GCS_COMPATTR, attributes.data(), numAttributes);

    // Get clauses
    int numBytes = Ime::ImmGetCompositionStringW(hInputContext, GCS_COMPCLAUSE, 0, 0);
    Vector<DWORD> clauses(numBytes / sizeof(DWORD));
    Ime::ImmGetCompositionStringW(hInputContext, GCS_COMPCLAUSE, clauses.data(), numBytes);

    Vector<CompositionUnderline> underlines;
    compositionToUnderlines(clauses, attributes, underlines);

    int cursorPosition = LOWORD(Ime::ImmGetCompositionStringW(hInputContext, GCS_CURSORPOS, 0, 0));

    m_page->setComposition(compositionString, underlines, cursorPosition);

    return true;
}

bool WebView::onIMEEndComposition()
{
    LOG(TextInput, "onIMEEndComposition");
    // If the composition hasn't been confirmed yet, it needs to be cancelled.
    // This happens after deleting the last character from inline input hole.
    if (m_page->editorState().hasComposition)
        m_page->confirmComposition(String());

    if (m_inIMEComposition)
        m_inIMEComposition--;

    return true;
}

LRESULT WebView::onIMERequestCharPosition(IMECHARPOSITION* charPos)
{
    if (charPos->dwCharPos && !m_page->editorState().hasComposition)
        return 0;
    IntRect caret = m_page->firstRectForCharacterInSelectedRange(charPos->dwCharPos);
    charPos->pt.x = caret.x();
    charPos->pt.y = caret.y();
    ::ClientToScreen(m_window, &charPos->pt);
    charPos->cLineHeight = caret.height();
    ::GetWindowRect(m_window, &charPos->rcDocument);
    return true;
}

LRESULT WebView::onIMERequestReconvertString(RECONVERTSTRING* reconvertString)
{
    String text = m_page->getSelectedText();
    unsigned totalSize = sizeof(RECONVERTSTRING) + text.length() * sizeof(UChar);
    
    if (!reconvertString)
        return totalSize;

    if (totalSize > reconvertString->dwSize)
        return 0;
    reconvertString->dwCompStrLen = text.length();
    reconvertString->dwStrLen = text.length();
    reconvertString->dwTargetStrLen = text.length();
    reconvertString->dwStrOffset = sizeof(RECONVERTSTRING);
    memcpy(reconvertString + 1, text.characters(), text.length() * sizeof(UChar));
    return totalSize;
}

LRESULT WebView::onIMERequest(WPARAM request, LPARAM data)
{
    LOG(TextInput, "onIMERequest %s", imeRequestName(request).latin1().data());
    if (!m_page->editorState().isContentEditable)
        return 0;

    switch (request) {
    case IMR_RECONVERTSTRING:
        return onIMERequestReconvertString(reinterpret_cast<RECONVERTSTRING*>(data));

    case IMR_QUERYCHARPOSITION:
        return onIMERequestCharPosition(reinterpret_cast<IMECHARPOSITION*>(data));
    }
    return 0;
}

bool WebView::onIMESelect(WPARAM wparam, LPARAM lparam)
{
    UNUSED_PARAM(wparam);
    UNUSED_PARAM(lparam);
    LOG(TextInput, "onIMESelect locale %ld %s", lparam, wparam ? "select" : "deselect");
    return false;
}

bool WebView::onIMESetContext(WPARAM wparam, LPARAM)
{
    LOG(TextInput, "onIMESetContext %s", wparam ? "active" : "inactive");
    return false;
}

void WebView::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
    // Calling ::DefWindowProcW will ensure that pressing the Alt key will generate a WM_SYSCOMMAND
    // event, e.g. See <http://webkit.org/b/47671>.
    if (!wasEventHandled)
        ::DefWindowProcW(event.nativeEvent()->hwnd, event.nativeEvent()->message, event.nativeEvent()->wParam, event.nativeEvent()->lParam);
}

PassRefPtr<WebPopupMenuProxy> WebView::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuProxyWin::create(this, page);
}

PassRefPtr<WebContextMenuProxy> WebView::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyWin::create(m_window, page);
}

void WebView::setFindIndicator(PassRefPtr<FindIndicator> prpFindIndicator, bool fadeOut)
{
    if (!m_findIndicatorCallback)
        return;

    HBITMAP hbmp = 0;
    IntRect selectionRect;

    if (RefPtr<FindIndicator> findIndicator = prpFindIndicator) {
        if (ShareableBitmap* contentImage = findIndicator->contentImage()) {
            // Render the contentImage to an HBITMAP.
            void* bits;
            HDC hdc = ::CreateCompatibleDC(0);
            int width = contentImage->bounds().width();
            int height = contentImage->bounds().height();
            BitmapInfo bitmapInfo = BitmapInfo::create(contentImage->size());

            hbmp = CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, static_cast<void**>(&bits), 0, 0);
            HBITMAP hbmpOld = static_cast<HBITMAP>(SelectObject(hdc, hbmp));
#if USE(CG)
            RetainPtr<CGContextRef> context(AdoptCF, CGBitmapContextCreate(bits, width, height,
                8, width * sizeof(RGBQUAD), deviceRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst));

            GraphicsContext graphicsContext(context.get());
            contentImage->paint(graphicsContext, IntPoint(), contentImage->bounds());
#else
            // FIXME: Implement!
#endif

            ::SelectObject(hdc, hbmpOld);
            ::DeleteDC(hdc);
        }

        selectionRect = IntRect(findIndicator->selectionRectInWindowCoordinates());
    }
    
    // The callback is responsible for calling ::DeleteObject(hbmp).
    (*m_findIndicatorCallback)(toAPI(this), hbmp, selectionRect, fadeOut, m_findIndicatorCallbackContext);
}

void WebView::setFindIndicatorCallback(WKViewFindIndicatorCallback callback, void* context)
{
    m_findIndicatorCallback = callback;
    m_findIndicatorCallbackContext = context;
}

WKViewFindIndicatorCallback WebView::getFindIndicatorCallback(void** context)
{
    if (context)
        *context = m_findIndicatorCallbackContext;
    
    return m_findIndicatorCallback;
}

void WebView::didCommitLoadForMainFrame(bool useCustomRepresentation)
{
}

void WebView::didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&)
{
}

double WebView::customRepresentationZoomFactor()
{
    return 1;
}

void WebView::setCustomRepresentationZoomFactor(double)
{
}

void WebView::didChangeScrollbarsForMainFrame() const
{
}

void WebView::findStringInCustomRepresentation(const String&, FindOptions, unsigned)
{
}

void WebView::countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned)
{
}

void WebView::setIsInWindow(bool isInWindow)
{
    m_isInWindow = isInWindow;
    m_page->viewStateDidChange(WebPageProxy::ViewIsInWindow);
}

void WebView::setIsVisible(bool isVisible)
{
    m_isVisible = isVisible;

    if (m_page)
        m_page->viewStateDidChange(WebPageProxy::ViewIsVisible);
}

#if USE(ACCELERATED_COMPOSITING)

void WebView::enterAcceleratedCompositingMode(const LayerTreeContext&)
{
    ASSERT(useNewDrawingArea());
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

void WebView::exitAcceleratedCompositingMode()
{
    ASSERT(useNewDrawingArea());
    // FIXME: Implement.
    ASSERT_NOT_REACHED();
}

#endif // USE(ACCELERATED_COMPOSITING)

HWND WebView::nativeWindow()
{
    return m_window;
}

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

HRESULT STDMETHODCALLTYPE WebView::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IDropTarget))
        *ppvObject = static_cast<IDropTarget*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebView::AddRef(void)
{
    ref();
    return refCount();
}

ULONG STDMETHODCALLTYPE WebView::Release(void)
{
    deref();
    return refCount();
}

static DWORD dragOperationToDragCursor(DragOperation op)
{
    DWORD res = DROPEFFECT_NONE;
    if (op & DragOperationCopy) 
        res = DROPEFFECT_COPY;
    else if (op & DragOperationLink) 
        res = DROPEFFECT_LINK;
    else if (op & DragOperationMove) 
        res = DROPEFFECT_MOVE;
    else if (op & DragOperationGeneric) 
        res = DROPEFFECT_MOVE; // This appears to be the Firefox behaviour
    return res;
}

WebCore::DragOperation WebView::keyStateToDragOperation(DWORD grfKeyState) const
{
    if (!m_page)
        return DragOperationNone;

    // Conforms to Microsoft's key combinations as documented for 
    // IDropTarget::DragOver. Note, grfKeyState is the current 
    // state of the keyboard modifier keys on the keyboard. See:
    // <http://msdn.microsoft.com/en-us/library/ms680129(VS.85).aspx>.
    DragOperation operation = m_page->dragOperation();

    if ((grfKeyState & (MK_CONTROL | MK_SHIFT)) == (MK_CONTROL | MK_SHIFT))
        operation = DragOperationLink;
    else if ((grfKeyState & MK_CONTROL) == MK_CONTROL)
        operation = DragOperationCopy;
    else if ((grfKeyState & MK_SHIFT) == MK_SHIFT)
        operation = DragOperationGeneric;

    return operation;
}

HRESULT STDMETHODCALLTYPE WebView::DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    m_dragData = 0;
    m_page->resetDragOperation();

    if (m_dropTargetHelper)
        m_dropTargetHelper->DragEnter(m_window, pDataObject, (POINT*)&pt, *pdwEffect);

    POINTL localpt = pt;
    ::ScreenToClient(m_window, (LPPOINT)&localpt);
    DragData data(pDataObject, IntPoint(localpt.x, localpt.y), IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));
    m_page->dragEntered(&data);
    *pdwEffect = dragOperationToDragCursor(m_page->dragOperation());

    m_lastDropEffect = *pdwEffect;
    m_dragData = pDataObject;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->DragOver((POINT*)&pt, *pdwEffect);

    if (m_dragData) {
        POINTL localpt = pt;
        ::ScreenToClient(m_window, (LPPOINT)&localpt);
        DragData data(m_dragData.get(), IntPoint(localpt.x, localpt.y), IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));
        m_page->dragUpdated(&data);
        *pdwEffect = dragOperationToDragCursor(m_page->dragOperation());
    } else
        *pdwEffect = DROPEFFECT_NONE;

    m_lastDropEffect = *pdwEffect;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::DragLeave()
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->DragLeave();

    if (m_dragData) {
        DragData data(m_dragData.get(), IntPoint(), IntPoint(), DragOperationNone);
        m_page->dragExited(&data);
        m_dragData = 0;
        m_page->resetDragOperation();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
    if (m_dropTargetHelper)
        m_dropTargetHelper->Drop(pDataObject, (POINT*)&pt, *pdwEffect);

    m_dragData = 0;
    *pdwEffect = m_lastDropEffect;
    POINTL localpt = pt;
    ::ScreenToClient(m_window, (LPPOINT)&localpt);
    DragData data(pDataObject, IntPoint(localpt.x, localpt.y), IntPoint(pt.x, pt.y), keyStateToDragOperation(grfKeyState));

    SandboxExtension::Handle sandboxExtensionHandle;
    m_page->performDrag(&data, String(), sandboxExtensionHandle);
    return S_OK;
}

} // namespace WebKit
