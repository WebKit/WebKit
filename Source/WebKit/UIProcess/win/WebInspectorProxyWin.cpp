/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "APINavigation.h"
#include "APINavigationAction.h"
#include "APIPageConfiguration.h"
#include "PageClientImpl.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebView.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/WebCoreBundleWin.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <WebKit/WKPage.h>
#include <wtf/cf/CFURLExtras.h>

namespace WebKit {

static const LPCWSTR WebInspectorProxyPointerProp = L"WebInspectorProxyPointer";
static const LPCWSTR WebInspectorProxyClassName = L"WebInspectorProxyClass";

struct InspectedWindowInfo {
    int left;
    int top;
    int viewWidth;
    int viewHeight;
    int parentWidth;
    int parentHeight;
};

static InspectedWindowInfo getInspectedWindowInfo(HWND inspectedWindow, HWND parentWindow)
{
    RECT rect;
    ::GetClientRect(inspectedWindow, &rect);
    ::MapWindowPoints(inspectedWindow, parentWindow, (LPPOINT)&rect, 2);

    RECT parentRect;
    ::GetClientRect(parentWindow, &parentRect);
    return { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, parentRect.right - parentRect.left, parentRect.bottom - parentRect.top };
}

void WebInspectorProxy::windowReceivedMessage(HWND hwnd, UINT msg, WPARAM, LPARAM lParam)
{
    switch (msg) {
    case WM_WINDOWPOSCHANGING: {
        if (!m_isAttached)
            return;

        auto windowPos = reinterpret_cast<WINDOWPOS*>(lParam);

        if (windowPos->flags & SWP_NOSIZE)
            return;

        HWND parent = GetParent(hwnd);
        RECT parentRect;
        GetClientRect(parent, &parentRect);

        RECT inspectorRect;
        GetClientRect(m_inspectorViewWindow, &inspectorRect);

        switch (m_attachmentSide) {
        case AttachmentSide::Bottom: {
            unsigned inspectorHeight = WebCore::InspectorFrontendClientLocal::constrainedAttachedWindowHeight(inspectorRect.bottom - inspectorRect.top, windowPos->cy);
            windowPos->cy -= inspectorHeight;
            ::SetWindowPos(m_inspectorViewWindow, 0, windowPos->x, windowPos->y + windowPos->cy, windowPos->cx, inspectorHeight, SWP_NOZORDER);
            break;
        }
        case AttachmentSide::Left:
        case AttachmentSide::Right: {
            unsigned inspectorWidth = WebCore::InspectorFrontendClientLocal::constrainedAttachedWindowWidth(inspectorRect.right - inspectorRect.left, windowPos->cx);
            windowPos->cx -= inspectorWidth;
            ::SetWindowPos(m_inspectorViewWindow, 0, windowPos->x + windowPos->cx, windowPos->y, inspectorWidth, windowPos->cy, SWP_NOZORDER);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

LRESULT CALLBACK WebInspectorProxy::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebInspectorProxy* client = reinterpret_cast<WebInspectorProxy*>(::GetProp(hwnd, WebInspectorProxyPointerProp));
    switch (msg) {
    case WM_SIZE:
        ::SetWindowPos(client->m_inspectorViewWindow, 0, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
        return 0;
    case WM_CLOSE:
        client->close();
        return 0;
    default:
        break;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WebInspectorProxy::registerWindowClass()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;
    haveRegisteredWindowClass = true;

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = 0;
    wcex.lpfnWndProc = wndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = WebCore::instanceHandle();
    wcex.hIcon = 0;
    wcex.hCursor = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = WebInspectorProxyClassName;
    wcex.hIconSm = 0;
    return ::RegisterClassEx(&wcex);
}

static void decidePolicyForNavigationAction(WKPageRef pageRef, WKNavigationActionRef navigationActionRef, WKFramePolicyListenerRef listenerRef, WKTypeRef, const void* clientInfo)
{
    // Allow non-main frames to navigate anywhere.
    API::FrameInfo* sourceFrame = toImpl(navigationActionRef)->sourceFrame();
    if (sourceFrame && !sourceFrame->isMainFrame()) {
        toImpl(listenerRef)->use({ });
        return;
    }

    const WebInspectorProxy* webInspectorProxy = static_cast<const WebInspectorProxy*>(clientInfo);
    ASSERT(webInspectorProxy);

    WebCore::ResourceRequest request = toImpl(navigationActionRef)->request();

    // Allow loading of the main inspector file.
    if (WebInspectorProxy::isMainOrTestInspectorPage(request.url())) {
        toImpl(listenerRef)->use({ });
        return;
    }

    // Prevent everything else from loading in the inspector's page.
    toImpl(listenerRef)->ignore();

    // And instead load it in the inspected page.
    webInspectorProxy->inspectedPage()->loadRequest(WTFMove(request));
}

static void webProcessDidCrash(WKPageRef, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);
    webInspectorProxy->closeForCrash();
}

WebPageProxy* WebInspectorProxy::platformCreateFrontendPage()
{
    ASSERT(inspectedPage());

    auto preferences = WebPreferences::create(String(), "WebKit2.", "WebKit2.");
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    preferences->setAllowFileAccessFromFileURLs(true);
    preferences->setJavaScriptRuntimeFlags({ });
    auto pageGroup = WebPageGroup::create(inspectorPageGroupIdentifierForPage(inspectedPage()));
    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&inspectorProcessPool(inspectionLevel()));
    pageConfiguration->setPreferences(preferences.ptr());
    pageConfiguration->setPageGroup(pageGroup.ptr());

    WKPageNavigationClientV0 navigationClient = {
        { 0, this },
        decidePolicyForNavigationAction,
        nullptr, // decidePolicyForNavigationResponse
        nullptr, // decidePolicyForPluginLoad
        nullptr, // didStartProvisionalNavigation
        nullptr, // didReceiveServerRedirectForProvisionalNavigation
        nullptr, // didFailProvisionalNavigation
        nullptr, // didCommitNavigation
        nullptr, // didFinishNavigation
        nullptr, // didFailNavigation
        nullptr, // didFailProvisionalLoadInSubframe
        nullptr, // didFinishDocumentLoad
        nullptr, // didSameDocumentNavigation
        nullptr, // renderingProgressDidChange
        nullptr, // canAuthenticateAgainstProtectionSpace
        nullptr, // didReceiveAuthenticationChallenge
        webProcessDidCrash,
        nullptr, // copyWebCryptoMasterKey

        nullptr, // didBeginNavigationGesture
        nullptr, // willEndNavigationGesture
        nullptr, // didEndNavigationGesture
        nullptr, // didRemoveNavigationGestureSnapshot
    };

    RECT r = { 0, 0, static_cast<LONG>(initialWindowWidth), static_cast<LONG>(initialWindowHeight) };
    auto page = inspectedPage();
    m_inspectedViewWindow = page->viewWidget();
    m_inspectedViewParentWindow = ::GetParent(m_inspectedViewWindow);
    auto view = WebView::create(r, pageConfiguration, m_inspectedViewParentWindow);
    m_inspectorView = &view.leakRef();
    auto inspectorPage = m_inspectorView->page();
    m_inspectorViewWindow = inspectorPage->viewWidget();
    WKPageSetPageNavigationClient(toAPI(inspectorPage), &navigationClient.base);

    return inspectorPage;
}

void WebInspectorProxy::platformCloseFrontendPageAndWindow()
{
    WebCore::WindowMessageBroadcaster::removeListener(m_inspectedViewWindow, this);
    m_inspectorView = nullptr;
    m_inspectorPage = nullptr;
    if (m_inspectorViewWindow) {
        ::DestroyWindow(m_inspectorViewWindow);
        m_inspectorViewWindow = nullptr;
    }
    if (m_inspectorDetachWindow) {
        ::RemoveProp(m_inspectorDetachWindow, WebInspectorProxyPointerProp);
        ::DestroyWindow(m_inspectorDetachWindow);
        m_inspectorDetachWindow = nullptr;
    }
    m_inspectedViewWindow = nullptr;
    m_inspectedViewParentWindow = nullptr;
}

String WebInspectorProxy::inspectorPageURL()
{
    RetainPtr<CFURLRef> htmlURLRef = adoptCF(CFBundleCopyResourceURL(WebCore::webKitBundle(), CFSTR("Main"), CFSTR("html"), CFSTR("WebInspectorUI")));
    return CFURLGetString(htmlURLRef.get());
}

String WebInspectorProxy::inspectorTestPageURL()
{
    RetainPtr<CFURLRef> htmlURLRef = adoptCF(CFBundleCopyResourceURL(WebCore::webKitBundle(), CFSTR("Test"), CFSTR("html"), CFSTR("WebInspectorUI")));
    return CFURLGetString(htmlURLRef.get());
}

String WebInspectorProxy::inspectorBaseURL()
{
    RetainPtr<CFURLRef> baseURLRef = adoptCF(CFBundleCopyResourceURL(WebCore::webKitBundle(), CFSTR("WebInspectorUI"), nullptr, nullptr));
    return CFURLGetString(baseURLRef.get());
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    RECT rect;
    ::GetClientRect(m_inspectedViewWindow, &rect);
    return rect.bottom - rect.top;
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    RECT rect;
    ::GetClientRect(m_inspectedViewWindow, &rect);
    return rect.right - rect.left;
}

void WebInspectorProxy::platformAttach()
{
    static const unsigned defaultAttachedSize = 300;
    static const unsigned minimumAttachedWidth = 750;
    static const unsigned minimumAttachedHeight = 250;

    if (m_inspectorDetachWindow && ::GetParent(m_inspectorViewWindow) == m_inspectorDetachWindow) {
        ::SetParent(m_inspectorViewWindow, m_inspectedViewParentWindow);
        ::ShowWindow(m_inspectorDetachWindow, SW_HIDE);
    }

    WebCore::WindowMessageBroadcaster::addListener(m_inspectedViewWindow, this);

    if (m_attachmentSide == AttachmentSide::Bottom) {
        unsigned maximumAttachedHeight = platformInspectedWindowHeight() * 3 / 4;
        platformSetAttachedWindowHeight(std::max(minimumAttachedHeight, std::min(defaultAttachedSize, maximumAttachedHeight)));
    } else {
        unsigned maximumAttachedWidth = platformInspectedWindowWidth() * 3 / 4;
        platformSetAttachedWindowWidth(std::max(minimumAttachedWidth, std::min(defaultAttachedSize, maximumAttachedWidth)));
    }
    ::ShowWindow(m_inspectorViewWindow, SW_SHOW);
}

void WebInspectorProxy::platformDetach()
{
    if (!inspectedPage()->isValid())
        return;

    if (!m_inspectorDetachWindow) {
        registerWindowClass();
        m_inspectorDetachWindow = ::CreateWindowEx(0, WebInspectorProxyClassName, 0, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, initialWindowWidth, initialWindowHeight,
            0, 0, WebCore::instanceHandle(), 0);
        ::SetProp(m_inspectorDetachWindow, WebInspectorProxyPointerProp, reinterpret_cast<HANDLE>(this));
    }

    WebCore::WindowMessageBroadcaster::removeListener(m_inspectedViewWindow, this);

    RECT rect;
    ::GetClientRect(m_inspectorDetachWindow, &rect);
    auto windowInfo = getInspectedWindowInfo(m_inspectedViewWindow, m_inspectedViewParentWindow);
    ::SetParent(m_inspectorViewWindow, m_inspectorDetachWindow);
    ::SetWindowPos(m_inspectorViewWindow, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
    ::SetWindowPos(m_inspectedViewWindow, 0, windowInfo.left, windowInfo.top, windowInfo.parentWidth - windowInfo.left, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);

    if (m_isVisible)
        ::ShowWindow(m_inspectorDetachWindow, SW_SHOW);
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned height)
{
    auto windowInfo = getInspectedWindowInfo(m_inspectedViewWindow, m_inspectedViewParentWindow);
    ::SetWindowPos(m_inspectorViewWindow, 0, windowInfo.left, windowInfo.parentHeight - height, windowInfo.parentWidth - windowInfo.left, height, SWP_NOZORDER);
    ::SetWindowPos(m_inspectedViewWindow, 0, windowInfo.left, windowInfo.top, windowInfo.parentWidth - windowInfo.left, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned width)
{
    auto windowInfo = getInspectedWindowInfo(m_inspectedViewWindow, m_inspectedViewParentWindow);
    ::SetWindowPos(m_inspectorViewWindow, 0, windowInfo.parentWidth - width, windowInfo.top, width, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
    ::SetWindowPos(m_inspectedViewWindow, 0, windowInfo.left, windowInfo.top, windowInfo.parentWidth - windowInfo.left, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
}

bool WebInspectorProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorProxy::platformHide()
{
    notImplemented();
}

void WebInspectorProxy::platformBringToFront()
{
    notImplemented();
}

void WebInspectorProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

void WebInspectorProxy::platformInspectedURLChanged(const String& /* url */)
{
    notImplemented();
}

void WebInspectorProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    notImplemented();
}

void WebInspectorProxy::platformSave(const String&, const String&, bool, bool)
{
    notImplemented();
}

void WebInspectorProxy::platformAppend(const String&, const String&)
{
    notImplemented();
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool /* available */)
{
    notImplemented();
}

void WebInspectorProxy::platformCreateFrontendWindow()
{
    platformDetach();
}

void WebInspectorProxy::platformDidCloseForCrash()
{
    notImplemented();
}

void WebInspectorProxy::platformInvalidate()
{
    notImplemented();
}

void WebInspectorProxy::platformStartWindowDrag()
{
    notImplemented();
}

}
