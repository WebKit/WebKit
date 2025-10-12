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
#include "WebInspectorUIProxy.h"

#include "APINavigation.h"
#include "APINavigationAction.h"
#include "APIPageConfiguration.h"
#include "InspectorResourceURLSchemeHandler.h"
#include "PageClientImpl.h"
#include "WKAPICast.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebKitDLL.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebView.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendClientLocal.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/WebCoreBundleWin.h>
#include <WebCore/WindowMessageBroadcaster.h>
#include <WebKit/WKPage.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

static const LPCWSTR WebInspectorUIProxyPointerProp = L"WebInspectorUIProxyPointer";
static const LPCWSTR WebInspectorUIProxyClassName = L"WebInspectorUIProxyClass";

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

static String systemErrorMessage(DWORD errorCode)
{
    if (!errorCode)
        return emptyString();

    wchar_t* messageBuffer = nullptr;
    size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), reinterpret_cast<LPWSTR>(&messageBuffer), 0, nullptr);
    String message(messageBuffer, size);
    LocalFree(messageBuffer);

    return message;
}

void WebInspectorUIProxy::showSavePanelForSingleFile(HWND parentWindow, Vector<WebCore::InspectorFrontendClient::SaveData>&& saveDatas)
{
    // Remove custom URI schemes such as "web-inspector://" and also remove the leading "/".
    URL url { saveDatas[0].url };
    auto filePath = url.path().substring(1).toString().wideCharacters();
    filePath.grow(MAX_PATH);

    OPENFILENAME ofn { };
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parentWindow;
    ofn.lpstrFile = filePath.mutableSpan().data();
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        auto fileHandle = FileSystem::openFile(filePath.span().data(), FileSystem::FileOpenMode::ReadWrite);
        if (!fileHandle)
            return;

        auto content = saveDatas[0].content.utf8();
        auto contentSize = content.length();
        auto bytesWritten = fileHandle.write(byteCast<uint8_t>(content.span()));
        if (bytesWritten != contentSize) {
            auto message = systemErrorMessage(GetLastError());
            if (message.isEmpty())
                message = makeString("Error: writeToFile returns "_s, bytesWritten ? static_cast<int64_t>(*bytesWritten) : -1, ", contentLength = "_s, content.length());
            MessageBox(parentWindow, message.wideCharacters().span().data(), L"Export HAR", MB_OK | MB_ICONEXCLAMATION);
        }
    } else {
        auto errorCode = CommDlgExtendedError();
        if (errorCode) {
            String message;
            // FNERR_INVALIDFILENAME is treated specially because this is a common error.
            if (errorCode == FNERR_INVALIDFILENAME)
                message = "Error: A file name is invalid."_s;
            else
                message = makeString("Error: "_s, errorCode);
            MessageBox(parentWindow, message.wideCharacters().span().data(), L"Export HAR", MB_OK | MB_ICONEXCLAMATION);
        }
    }
}

void WebInspectorUIProxy::windowReceivedMessage(HWND hwnd, UINT msg, WPARAM, LPARAM lParam)
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

LRESULT CALLBACK WebInspectorUIProxy::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebInspectorUIProxy* inspector = reinterpret_cast<WebInspectorUIProxy*>(::GetProp(hwnd, WebInspectorUIProxyPointerProp));
    switch (msg) {
    case WM_SIZE:
        ::SetWindowPos(inspector->m_inspectorViewWindow, 0, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
        return 0;
    case WM_CLOSE:
        inspector->close();
        return 0;
    case WM_DPICHANGED: {
        RECT& rect = *reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    default:
        break;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WebInspectorUIProxy::registerWindowClass()
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
    wcex.hInstance = instanceHandle();
    wcex.hIcon = 0;
    wcex.hCursor = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = WebInspectorUIProxyClassName;
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

    const WebInspectorUIProxy* inspector = static_cast<const WebInspectorUIProxy*>(clientInfo);
    ASSERT(inspector);

    WebCore::ResourceRequest request = toImpl(navigationActionRef)->request();

    // Allow loading of the main inspector file.
    if (WebInspectorUIProxy::isMainOrTestInspectorPage(request.url())) {
        toImpl(listenerRef)->use({ });
        return;
    }

    // Prevent everything else from loading in the inspector's page.
    toImpl(listenerRef)->ignore();

    // And instead load it in the inspected page.
    inspector->protectedInspectedPage()->loadRequest(WTFMove(request));
}

static void webProcessDidCrash(WKPageRef, const void* clientInfo)
{
    WebInspectorUIProxy* inspector = static_cast<WebInspectorUIProxy*>(const_cast<void*>(clientInfo));
    ASSERT(inspector);
    inspector->closeForCrash();
}

RefPtr<WebPageProxy> WebInspectorUIProxy::platformCreateFrontendPage()
{
    ASSERT(m_inspectedPage);

    auto preferences = WebPreferences::create(String(), "WebKit2."_s, "WebKit2."_s);
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    preferences->setJavaScriptRuntimeFlags({ });
    auto pageGroup = WebPageGroup::create(WebKit::defaultInspectorPageGroupIdentifierForPage(protectedInspectedPage().get()));
    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&WebKit::defaultInspectorProcessPool(inspectionLevel()));
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
    RefPtr page = inspectedPage();
    m_inspectedViewWindow = reinterpret_cast<HWND>(page->viewWidget());
    m_inspectedViewParentWindow = ::GetParent(m_inspectedViewWindow);
    auto view = WebView::create(r, pageConfiguration, m_inspectedViewParentWindow);
    m_inspectorView = &view.leakRef();
    RefPtr inspectorPage = m_inspectorView->page();
    m_inspectorViewWindow = reinterpret_cast<HWND>(inspectorPage->viewWidget());
    WKPageSetPageNavigationClient(toAPI(inspectorPage.get()), &navigationClient.base);

    inspectorPage->setURLSchemeHandlerForScheme(InspectorResourceURLSchemeHandler::create(), "inspector-resource"_s);

    return inspectorPage;
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    WebCore::WindowMessageBroadcaster::removeListener(m_inspectedViewWindow, this);
    m_inspectorView = nullptr;
    m_inspectorPage = nullptr;
    if (m_inspectorViewWindow) {
        ::DestroyWindow(m_inspectorViewWindow);
        m_inspectorViewWindow = nullptr;
    }
    if (m_inspectorDetachWindow) {
        ::RemoveProp(m_inspectorDetachWindow, WebInspectorUIProxyPointerProp);
        ::DestroyWindow(m_inspectorDetachWindow);
        m_inspectorDetachWindow = nullptr;
    }
    m_inspectedViewWindow = nullptr;
    m_inspectedViewParentWindow = nullptr;
}

String WebInspectorUIProxy::inspectorPageURL()
{
    return "inspector-resource:///Main.html"_s;
}

String WebInspectorUIProxy::inspectorTestPageURL()
{
    return "inspector-resource:///Test.html"_s;
}

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    // FIXME <https://webkit.org/b/205537>: this should infer more useful data.
    return DebuggableInfoData::empty();
}

// Set a default sizes based on InspectorFrontendClientLocal.
static constexpr unsigned s_defaultAttachedSize = 300;
static constexpr unsigned s_minimumAttachedHeight = 250;
static const float s_maximumAttachedHeightRatio = 0.75;
static constexpr unsigned s_minimumAttachedWidth = 500;

bool WebInspectorUIProxy::platformCanAttach(bool)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return false;
    auto deviceScaleFactor = inspectedPage->intrinsicDeviceScaleFactor();
    RECT inspectedWindowRect;
    GetClientRect(m_inspectedViewWindow, &inspectedWindowRect);

    unsigned inspectedWindowHeight = (inspectedWindowRect.bottom - inspectedWindowRect.top) / deviceScaleFactor;
    unsigned inspectedPageWidth = (inspectedWindowRect.right - inspectedWindowRect.left) / deviceScaleFactor;
    unsigned maximumAttachedHeight = inspectedWindowHeight * s_maximumAttachedHeightRatio;

    return s_minimumAttachedHeight <= maximumAttachedHeight && s_minimumAttachedWidth <= inspectedPageWidth;
}

void WebInspectorUIProxy::platformAttach()
{
    auto deviceScaleFactor = protectedInspectorPage()->deviceScaleFactor();

    if (m_inspectorDetachWindow && ::GetParent(m_inspectorViewWindow) == m_inspectorDetachWindow) {
        ::SetParent(m_inspectorViewWindow, m_inspectedViewParentWindow);
        ::ShowWindow(m_inspectorDetachWindow, SW_HIDE);
    }

    WebCore::WindowMessageBroadcaster::addListener(m_inspectedViewWindow, this);

    RECT inspectedWindowRect;
    ::GetClientRect(m_inspectedViewWindow, &inspectedWindowRect);

    if (m_attachmentSide == AttachmentSide::Bottom) {
        unsigned inspectedWindowHeight = (inspectedWindowRect.bottom - inspectedWindowRect.top) / deviceScaleFactor;
        unsigned maximumAttachedHeight = inspectedWindowHeight * s_maximumAttachedHeightRatio;
        platformSetAttachedWindowHeight(std::max(s_minimumAttachedHeight, std::min(s_defaultAttachedSize, maximumAttachedHeight)));
    } else {
        unsigned inspectedWindowWidth = (inspectedWindowRect.right - inspectedWindowRect.left) / deviceScaleFactor;
        unsigned maximumAttachedWidth = inspectedWindowWidth * s_maximumAttachedHeightRatio;
        platformSetAttachedWindowWidth(std::max(s_minimumAttachedWidth, std::min(s_defaultAttachedSize, maximumAttachedWidth)));
    }
    ::ShowWindow(m_inspectorViewWindow, SW_SHOW);
}

void WebInspectorUIProxy::platformDetach()
{
    if (!protectedInspectedPage()->hasRunningProcess())
        return;

    if (!m_inspectorDetachWindow) {
        registerWindowClass();
        m_inspectorDetachWindow = ::CreateWindowEx(0, WebInspectorUIProxyClassName, 0, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, initialWindowWidth, initialWindowHeight,
            0, 0, instanceHandle(), 0);
        ::SetProp(m_inspectorDetachWindow, WebInspectorUIProxyPointerProp, reinterpret_cast<HANDLE>(this));
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

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned height)
{
    auto deviceScaleFactor = protectedInspectorPage()->deviceScaleFactor();
    height *= deviceScaleFactor;
    auto windowInfo = getInspectedWindowInfo(m_inspectedViewWindow, m_inspectedViewParentWindow);
    ::SetWindowPos(m_inspectorViewWindow, 0, windowInfo.left, windowInfo.parentHeight - height, windowInfo.parentWidth - windowInfo.left, height, SWP_NOZORDER);
    ::SetWindowPos(m_inspectedViewWindow, 0, windowInfo.left, windowInfo.top, windowInfo.parentWidth - windowInfo.left, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned width)
{
    auto deviceScaleFactor = protectedInspectorPage()->deviceScaleFactor();
    width *= deviceScaleFactor;
    auto windowInfo = getInspectedWindowInfo(m_inspectedViewWindow, m_inspectedViewParentWindow);
    ::SetWindowPos(m_inspectorViewWindow, 0, windowInfo.parentWidth - width, windowInfo.top, width, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
    ::SetWindowPos(m_inspectedViewWindow, 0, windowInfo.left, windowInfo.top, windowInfo.parentWidth - windowInfo.left, windowInfo.parentHeight - windowInfo.top, SWP_NOZORDER);
}

void WebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
    notImplemented();
}

bool WebInspectorUIProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorUIProxy::platformHide()
{
    notImplemented();
}

void WebInspectorUIProxy::platformResetState()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
    notImplemented();
}

void WebInspectorUIProxy::platformRevealFileExternally(const String&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformInspectedURLChanged(const String& /* url */)
{
    notImplemented();
}

void WebInspectorUIProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveDatas, bool /* forceSaveAs */)
{
    // Currently, file saving is only possible with SaveMode::SingleFile.
    // This is determined in WebInspectorUI::canSave().
    ASSERT(saveDatas.size() == 1);
    WebInspectorUIProxy::showSavePanelForSingleFile(m_inspectedViewWindow, WTFMove(saveDatas));
}

void WebInspectorUIProxy::platformLoad(const String&, CompletionHandler<void(const String&)>&& completionHandler)
{
    notImplemented();
    completionHandler(nullString());
}

void WebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool /* available */)
{
    notImplemented();
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    platformDetach();
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
    notImplemented();
}

void WebInspectorUIProxy::platformInvalidate()
{
    notImplemented();
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    notImplemented();
}

}
