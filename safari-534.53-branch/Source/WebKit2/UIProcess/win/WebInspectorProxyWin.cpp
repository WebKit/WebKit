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
#include "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#include "WebKitBundle.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebView.h"
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static const LPCWSTR kWebKit2InspectorWindowClassName = L"WebKit2InspectorWindowClass";

bool WebInspectorProxy::registerInspectorViewWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;
    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebInspectorProxy::InspectorViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(WebInspectorProxy*);
    wcex.hInstance      = instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = ::LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebKit2InspectorWindowClassName;
    wcex.hIconSm        = 0;

    return !!::RegisterClassEx(&wcex);
}

LRESULT CALLBACK WebInspectorProxy::InspectorViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = ::GetWindowLongPtr(hWnd, 0);
    
    if (WebInspectorProxy* inspectorView = reinterpret_cast<WebInspectorProxy*>(longPtr))
        return inspectorView->wndProc(hWnd, message, wParam, lParam);

    if (message == WM_CREATE) {
        LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);

        // Associate the WebInspectorProxy with the window.
        ::SetWindowLongPtr(hWnd, 0, (LONG_PTR)createStruct->lpCreateParams);
        return 0;
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

void WebInspectorProxy::windowReceivedMessage(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ASSERT(m_isAttached);

    switch (msg) {
    case WM_WINDOWPOSCHANGING:
        onWebViewWindowPosChangingEvent(wParam, lParam);
        break;
    default:
        break;
    }
}

LRESULT WebInspectorProxy::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    bool handled = true;

    switch (message) {
    case WM_SIZE:
        lResult = onSizeEvent(hWnd, message, wParam, lParam, handled);
        break;
    case WM_GETMINMAXINFO:
        lResult = onMinMaxInfoEvent(hWnd, message, wParam, lParam, handled);
        break;
    case WM_SETFOCUS:
        lResult = onSetFocusEvent(hWnd, message, wParam, lParam, handled);
        break;
    case WM_CLOSE:
        lResult = onCloseEvent(hWnd, message, wParam, lParam, handled);
        break;
    default:
        handled = false;
        break;
    }

    if (!handled)
        lResult = ::DefWindowProc(hWnd, message, wParam, lParam);

    return lResult;
}

LRESULT WebInspectorProxy::onSizeEvent(HWND, UINT, WPARAM, LPARAM, bool&)
{
    RECT rect;
    ::GetClientRect(m_inspectorWindow, &rect);

    ::SetWindowPos(m_inspectorView->window(), 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);

    return 0;
}

LRESULT WebInspectorProxy::onSetFocusEvent(HWND, UINT, WPARAM, LPARAM lParam, bool&)
{    
    ::SetFocus(m_inspectorView->window());

    return 0;
}

LRESULT WebInspectorProxy::onMinMaxInfoEvent(HWND, UINT, WPARAM, LPARAM lParam, bool&)
{
    MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
    POINT size = {minimumWindowWidth, minimumWindowHeight};
    info->ptMinTrackSize = size;

    return 0;
}

LRESULT WebInspectorProxy::onCloseEvent(HWND, UINT, WPARAM, LPARAM, bool&)
{
    ::ShowWindow(m_inspectorWindow, SW_HIDE);
    close();

    return 0;
}

void WebInspectorProxy::onWebViewWindowPosChangingEvent(WPARAM wParam, LPARAM lParam)
{
    WINDOWPOS* windowPos = reinterpret_cast<WINDOWPOS*>(lParam);

    if (windowPos->flags & SWP_NOSIZE)
        return;

    HWND inspectorWindow = m_inspectorView->window();

    RECT inspectorRect;
    ::GetClientRect(inspectorWindow, &inspectorRect);
    unsigned inspectorHeight = inspectorRect.bottom - inspectorRect.top;

    RECT parentRect;
    ::GetClientRect(::GetParent(inspectorWindow), &parentRect);
    inspectorHeight = InspectorFrontendClientLocal::constrainedAttachedWindowHeight(inspectorHeight, parentRect.bottom - parentRect.top);

    windowPos->cy -= inspectorHeight;

    ::SetWindowPos(inspectorWindow, 0, windowPos->x, windowPos->y + windowPos->cy, windowPos->cx, inspectorHeight, SWP_NOZORDER);
}

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(!m_inspectorView);
    ASSERT(!m_inspectorWindow);

    RECT initialRect = { 0, 0, initialWindowWidth, initialWindowHeight };
    m_inspectorView = WebView::create(initialRect, m_page->process()->context(), inspectorPageGroup(), 0);

    return m_inspectorView->page();
}

void WebInspectorProxy::platformOpen(bool willOpenAttached)
{
    registerInspectorViewWindowClass();

    m_inspectorWindow = ::CreateWindowEx(0, kWebKit2InspectorWindowClassName, 0, WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, initialWindowWidth, initialWindowHeight, 0, 0, instanceHandle(), this);
    ASSERT(::IsWindow(m_inspectorWindow));

    m_inspectorView->setParentWindow(m_inspectorWindow);

    if (!willOpenAttached)
        ::ShowWindow(m_inspectorWindow, SW_SHOW);
}

void WebInspectorProxy::platformDidClose()
{
    ASSERT(!m_isAttached);
    ASSERT(!m_isVisible || m_inspectorWindow);
    ASSERT(!m_isVisible || m_inspectorView);

    if (m_inspectorWindow) {
        ASSERT(::IsWindow(m_inspectorWindow));
        ::DestroyWindow(m_inspectorWindow);
    }

    m_inspectorWindow = 0;
    m_inspectorView = 0;
}

void WebInspectorProxy::platformBringToFront()
{
    // FIXME: this will not bring a background tab in Safari to the front, only its window.
    HWND parentWindow = m_isAttached ? ::GetAncestor(m_page->nativeWindow(), GA_ROOT) : m_inspectorWindow;
    if (!parentWindow)
        return;

    ASSERT(::IsWindow(parentWindow));
    ::SetWindowPos(parentWindow, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
}

void WebInspectorProxy::platformInspectedURLChanged(const String& urlString)
{
    // FIXME: this should be made localizable once WebKit2 supports it. <rdar://problem/8728860>
    String title = makeString("Web Inspector ", static_cast<UChar>(0x2014), ' ', urlString);
    ::SetWindowTextW(m_inspectorWindow, title.charactersWithNullTermination());
}

void WebInspectorProxy::platformAttach()
{
    HWND webViewWindow = m_page->nativeWindow();
    HWND parentWindow = ::GetParent(webViewWindow);

    WindowMessageBroadcaster::addListener(webViewWindow, this);
    m_inspectorView->setParentWindow(parentWindow);
    ::ShowWindow(m_inspectorWindow, SW_HIDE);

    ::SendMessage(parentWindow, WM_SIZE, 0, 0);
}

void WebInspectorProxy::platformDetach()
{
    HWND webViewWindow = m_page->nativeWindow();
    WindowMessageBroadcaster::removeListener(webViewWindow, this);

    m_inspectorView->setParentWindow(m_inspectorWindow);

    if (m_isVisible)
        ::ShowWindow(m_inspectorWindow, SW_SHOW);

    // Send the detached inspector window and the WebView's parent window WM_SIZE messages
    // to have them re-layout correctly.
    ::SendMessage(m_inspectorWindow, WM_SIZE, 0, 0);
    ::SendMessage(::GetParent(webViewWindow), WM_SIZE, 0, 0);
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    HWND inspectedWindow = m_page->nativeWindow();
    HWND parentWindow = ::GetParent(inspectedWindow);

    RECT parentWindowRect;
    ::GetWindowRect(parentWindow, &parentWindowRect);

    RECT inspectedWindowRect;
    ::GetWindowRect(inspectedWindow, &inspectedWindowRect);

    int totalHeight = parentWindowRect.bottom - parentWindowRect.top;
    int webViewWidth = inspectedWindowRect.right - inspectedWindowRect.left;

    POINT inspectedWindowOrigin = { inspectedWindowRect.left, inspectedWindowRect.top };
    ::ScreenToClient(parentWindow, &inspectedWindowOrigin);

    HWND inspectorWindow = m_inspectorView->window();
    ::SetWindowPos(inspectorWindow, 0, inspectedWindowOrigin.x, totalHeight - height, webViewWidth, height, SWP_NOZORDER);

    // We want to set the inspected web view height to the totalHeight, because the height adjustment
    // of the inspected WebView happens in onWindowPosChanging, not here.
    ::SetWindowPos(inspectedWindow, 0, inspectedWindowOrigin.x, inspectedWindowOrigin.y, webViewWidth, totalHeight, SWP_NOZORDER);

    ::RedrawWindow(inspectorWindow, 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW); 
    ::RedrawWindow(inspectedWindow, 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

String WebInspectorProxy::inspectorPageURL() const
{
    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, CFBundleCopyResourceURL(webKitBundle(), CFSTR("inspector"), CFSTR("html"), CFSTR("inspector")));
    if (!htmlURLRef)
        return String();

    return String(CFURLGetString(htmlURLRef.get()));
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
