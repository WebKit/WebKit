/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebChromeClient.h"

#include "COMPropertyBag.h"
#include "COMVariantSetter.h"
#include "DOMCoreClasses.h"
#include "WebElementPropertyBag.h"
#include "WebFrame.h"
#include "WebHistory.h"
#include "WebMutableURLRequest.h"
#include "WebDesktopNotificationsDelegate.h"
#include "WebSecurityOrigin.h"
#include "WebView.h"
#include <WebCore/BString.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/CookieConsentDecisionResult.h>
#include <WebCore/Cursor.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/FloatRect.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/FullScreenController.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/Icon.h>
#include <WebCore/LocalWindowsContext.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/ModalContainerTypes.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/PopupMenuWin.h>
#include <WebCore/SearchPopupMenuWin.h>
#include <WebCore/WindowFeatures.h>
#include <wchar.h>

using namespace WebCore;

// When you call GetOpenFileName, if the size of the buffer is too small,
// MSDN says that the first two bytes of the buffer contain the required size for the file selection, in bytes or characters
// So we can assume the required size can't be more than the maximum value for a short.
static const size_t maxFilePathsListSize = USHRT_MAX;

WebChromeClient::WebChromeClient(WebView* webView)
    : m_webView(webView)
#if ENABLE(NOTIFICATIONS)
    , m_notificationsDelegate(makeUnique<WebDesktopNotificationsDelegate>(webView))
#endif
{
}

void WebChromeClient::chromeDestroyed()
{
    delete this;
}

void WebChromeClient::setWindowRect(const FloatRect& r)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        RECT rect = IntRect(r);
        uiDelegate->setFrame(m_webView, &rect);
        uiDelegate->Release();
    }
}

FloatRect WebChromeClient::windowRect()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        RECT rect;
        HRESULT retval = uiDelegate->webViewFrame(m_webView, &rect);

        uiDelegate->Release();

        if (SUCCEEDED(retval))
            return rect;
    }

    return FloatRect();
}

FloatRect WebChromeClient::pageRect()
{
    RECT rect;
    m_webView->frameRect(&rect);
    return rect;
}

void WebChromeClient::focus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewFocus(m_webView);
        uiDelegate->Release();
    }
    // Normally this would happen on a timer, but JS might need to know this earlier, so we'll update here.
    m_webView->updateActiveState();
}

void WebChromeClient::unfocus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewUnfocus(m_webView);
        uiDelegate->Release();
    }
    // Normally this would happen on a timer, but JS might need to know this earlier, so we'll update here.
    m_webView->updateActiveState();
}

bool WebChromeClient::canTakeFocus(FocusDirection direction)
{
    IWebUIDelegate* uiDelegate = 0;
    BOOL bForward = (direction == FocusDirection::Forward) ? TRUE : FALSE;
    BOOL result = FALSE;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->canTakeFocus(m_webView, bForward, &result);
        uiDelegate->Release();
    }

    return !!result;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    IWebUIDelegate* uiDelegate = 0;
    BOOL bForward = (direction == FocusDirection::Forward) ? TRUE : FALSE;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->takeFocus(m_webView, bForward);
        uiDelegate->Release();
    }
}

void WebChromeClient::focusedElementChanged(Element*)
{
}

void WebChromeClient::focusedFrameChanged(Frame*)
{
}

static COMPtr<IPropertyBag> createWindowFeaturesPropertyBag(const WindowFeatures& features)
{
    HashMap<String, COMVariant> map;
    if (features.x)
        map.set(WebWindowFeaturesXKey, *features.x);
    if (features.y)
        map.set(WebWindowFeaturesYKey, *features.y);
    if (features.width)
        map.set(WebWindowFeaturesWidthKey, *features.width);
    if (features.height)
        map.set(WebWindowFeaturesHeightKey, *features.height);
    map.set(WebWindowFeaturesMenuBarVisibleKey, features.menuBarVisible);
    map.set(WebWindowFeaturesStatusBarVisibleKey, features.statusBarVisible);
    map.set(WebWindowFeaturesToolBarVisibleKey, features.toolBarVisible);
    map.set(WebWindowFeaturesScrollbarsVisibleKey, features.scrollbarsVisible);
    map.set(WebWindowFeaturesResizableKey, features.resizable);
    map.set(WebWindowFeaturesFullscreenKey, features.fullscreen);
    map.set(WebWindowFeaturesDialogKey, features.dialog);

    return COMPtr<IPropertyBag>(AdoptCOM, COMPropertyBag<COMVariant>::adopt(map));
}

Page* WebChromeClient::createWindow(Frame& frame, const WindowFeatures& features, const NavigationAction& navigationAction)
{
    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return 0;

#if ENABLE(FULLSCREEN_API)
    if (frame.document() && frame.document()->fullscreenManager().currentFullscreenElement())
        frame.document()->fullscreenManager().cancelFullscreen();
#endif

    COMPtr<WebMutableURLRequest> request = adoptCOM(WebMutableURLRequest::createInstance(ResourceRequest(navigationAction.url())));

    COMPtr<IWebUIDelegatePrivate2> delegatePrivate(Query, delegate);
    if (delegatePrivate) {
        COMPtr<IWebView> newWebView;
        HRESULT hr = delegatePrivate->createWebViewWithRequest(m_webView, request.get(), createWindowFeaturesPropertyBag(features).get(), &newWebView);

        if (SUCCEEDED(hr) && newWebView)
            return core(newWebView.get());

        // If the delegate doesn't implement the IWebUIDelegatePrivate2 version of the call, fall back
        // to the old versions (even if they support the IWebUIDelegatePrivate2 interface).
        if (hr != E_NOTIMPL)
            return 0;
    }

    COMPtr<IWebView> newWebView;

    if (features.dialog) {
        if (FAILED(delegate->createModalDialog(m_webView, request.get(), &newWebView)))
            return 0;
    } else if (FAILED(delegate->createWebViewWithRequest(m_webView, request.get(), &newWebView)))
        return 0;

    return newWebView ? core(newWebView.get()) : 0;
}

void WebChromeClient::show()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewShow(m_webView);
        uiDelegate->Release();
    }
}

bool WebChromeClient::canRunModal()
{
    BOOL result = FALSE;
    if (COMPtr<IWebUIDelegate> delegate = uiDelegate())
        delegate->canRunModal(m_webView, &result);
    return result;
}

void WebChromeClient::runModal()
{
    if (COMPtr<IWebUIDelegate> delegate = uiDelegate())
        delegate->runModal(m_webView);
}

void WebChromeClient::setToolbarsVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setToolbarsVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::toolbarsVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewAreToolbarsVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setStatusbarVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setStatusBarVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::statusbarVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->webViewIsStatusBarVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setScrollbarsVisible(bool b)
{
    WebFrame* webFrame = m_webView->topLevelFrame();
    if (webFrame)
        webFrame->setAllowsScrolling(b);
}

bool WebChromeClient::scrollbarsVisible()
{
    WebFrame* webFrame = m_webView->topLevelFrame();
    BOOL b = false;
    if (webFrame)
        webFrame->allowsScrolling(&b);

    return !!b;
}

void WebChromeClient::setMenubarVisible(bool visible)
{
    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return;
    delegate->setMenuBarVisible(m_webView, visible);
}

bool WebChromeClient::menubarVisible()
{
    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return true;
    BOOL result = true;
    delegate->isMenuBarVisible(m_webView, &result);
    return result;
}

void WebChromeClient::setResizable(bool resizable)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setResizable(m_webView, resizable);
        uiDelegate->Release();
    }
}

static BOOL messageIsError(MessageLevel level)
{
    return level == MessageLevel::Error;
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned columnNumber, const String& url)
{
    UNUSED_PARAM(columnNumber);

    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate;
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate)))
            uiPrivate->webViewAddMessageToConsole(m_webView, BString(message), lineNumber, BString(url), messageIsError(level));
    }
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        ui->Release();
        return true;
    }
    return false;
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame& frame)
{
    BOOL result = TRUE;
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        WebFrame* webFrame = kit(&frame);
        ui->runBeforeUnloadConfirmPanelWithMessage(m_webView, BString(message), webFrame, &result);
        ui->Release();
    }
    return !!result;
}

void WebChromeClient::closeWindow()
{
    // We need to remove the parent WebView from WebViewSets here, before it actually
    // closes, to make sure that JavaScript code that executes before it closes
    // can't find it. Otherwise, window.open will select a closed WebView instead of 
    // opening a new one <rdar://problem/3572585>.

    // We also need to stop the load to prevent further parsing or JavaScript execution
    // after the window has torn down <rdar://problem/4161660>.
  
    // FIXME: This code assumes that the UI delegate will respond to a webViewClose
    // message by actually closing the WebView. Safari guarantees this behavior, but other apps might not.
    // This approach is an inherent limitation of not making a close execute immediately
    // after a call to window.close.

    m_webView->setGroupName(0);
    m_webView->stopLoading(0);
    m_webView->closeWindow();
}

void WebChromeClient::runJavaScriptAlert(Frame&, const String& message)
{
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptAlertPanelWithMessage(m_webView, BString(message));
}

bool WebChromeClient::runJavaScriptConfirm(Frame&, const String& message)
{
    BOOL result = FALSE;
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptConfirmPanelWithMessage(m_webView, BString(message), &result);
    return !!result;
}

bool WebChromeClient::runJavaScriptPrompt(Frame&, const String& message, const String& defaultValue, String& result)
{
    COMPtr<IWebUIDelegate> ui;
    if (FAILED(m_webView->uiDelegate(&ui)))
        return false;

    TimerBase::fireTimersInNestedEventLoop();

    BString resultBSTR;
    if (FAILED(ui->runJavaScriptTextInputPanelWithPrompt(m_webView, BString(message), BString(defaultValue), &resultBSTR)))
        return false;

    if (!resultBSTR)
        return false;

    result = String(resultBSTR, SysStringLen(resultBSTR));
    return true;
}

void WebChromeClient::setStatusbarText(const String& statusText)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setStatusText(m_webView, BString(statusText));
    }
}

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    BOOL enabled = FALSE;
    IWebPreferences* preferences;
    if (SUCCEEDED(m_webView->preferences(&preferences)))
        preferences->tabsToLinks(&enabled);

    return enabled ? KeyboardAccessTabsToLinks : KeyboardAccessDefault;
}

void WebChromeClient::invalidateRootView(const IntRect& windowRect)
{
    ASSERT(core(m_webView->topLevelFrame()));
    m_webView->repaint(windowRect, false /*contentChanged*/, false /*immediate*/, false /*repaintContentOnly*/);
}

void WebChromeClient::invalidateContentsAndRootView(const IntRect& windowRect)
{
    ASSERT(core(m_webView->topLevelFrame()));
    m_webView->repaint(windowRect, true /*contentChanged*/, false /*immediate*/, false /*repaintContentOnly*/);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& windowRect)
{
    ASSERT(core(m_webView->topLevelFrame()));
    m_webView->repaint(windowRect, true /*contentChanged*/, false /*immediate*/, true /*repaintContentOnly*/);
}

void WebChromeClient::scroll(const IntSize& delta, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(core(m_webView->topLevelFrame()));

    m_webView->scrollBackingStore(core(m_webView->topLevelFrame())->view(), delta.width(), delta.height(), scrollViewRect, clipRect);
}

IntPoint WebChromeClient::accessibilityScreenToRootView(const WebCore::IntPoint& point) const
{
    return screenToRootView(point);
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const WebCore::IntRect& rect) const
{
    return rootViewToScreen(rect);
}

IntRect WebChromeClient::rootViewToScreen(const IntRect& rect) const
{
    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(&viewWindow)))
        return rect;

    // Find the top left corner of the Widget's containing window in screen coords,
    // and adjust the result rect's position by this amount.
    POINT topLeft = {0, 0};
    IntRect result = rect;
    ::ClientToScreen(viewWindow, &topLeft);
    result.move(topLeft.x, topLeft.y);

    return result;
}

IntPoint WebChromeClient::screenToRootView(const IntPoint& point) const
{
    POINT result = point;

    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(&viewWindow)))
        return point;

    ::ScreenToClient(viewWindow, &result);

    return result;
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(&viewWindow)))
        return 0;
    return viewWindow;
}

void WebChromeClient::contentsSizeChanged(Frame&, const IntSize&) const
{
    notImplemented();
}

void WebChromeClient::intrinsicContentsSizeChanged(const IntSize&) const
{
    notImplemented();
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags, const String& toolTip, TextDirection)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<WebElementPropertyBag> element;
        element.adoptRef(WebElementPropertyBag::createInstance(result));

        uiDelegate->mouseDidMoveOverElement(m_webView, element.get(), modifierFlags);
    }
    m_webView->setToolTip(toolTip);
}

bool WebChromeClient::shouldUnavailablePluginMessageBeButton(RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    if (pluginUnavailabilityReason != RenderEmbeddedObject::PluginMissing)
        return false;

    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return false;
    
    // If the UI delegate implements IWebUIDelegatePrivate3, 
    // which contains didPressMissingPluginButton, then the message should be a button.
    COMPtr<IWebUIDelegatePrivate3> uiDelegatePrivate3(Query, uiDelegate);
    return uiDelegatePrivate3;
}

void WebChromeClient::unavailablePluginButtonClicked(Element& element, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    ASSERT_UNUSED(pluginUnavailabilityReason, pluginUnavailabilityReason == RenderEmbeddedObject::PluginMissing);

    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    COMPtr<IWebUIDelegatePrivate3> uiDelegatePrivate3(Query, uiDelegate);
    if (!uiDelegatePrivate3)
        return;

    COMPtr<IDOMElement> e(AdoptCOM, DOMElement::createInstance(&element));
    uiDelegatePrivate3->didPressMissingPluginButton(e.get());
}

void WebChromeClient::print(Frame& frame, const StringWithDirection&)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)))
        uiDelegate->printFrame(m_webView, kit(&frame));
}

void WebChromeClient::exceededDatabaseQuota(Frame& frame, const String& databaseIdentifier, DatabaseDetails)
{
    COMPtr<WebSecurityOrigin> origin(AdoptCOM, WebSecurityOrigin::createInstance(&frame.document()->securityOrigin()));
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate(Query, uiDelegate);
        if (uiDelegatePrivate)
            uiDelegatePrivate->exceededDatabaseQuota(m_webView, kit(&frame), origin.get(), BString(databaseIdentifier));
        else {
            // FIXME: remove this workaround once shipping Safari has the necessary delegate implemented.
            WCHAR path[MAX_PATH];
            HMODULE safariHandle = GetModuleHandleW(L"Safari.exe");
            if (!safariHandle)
                return;
            if (!::GetModuleFileName(safariHandle, path, WTF_ARRAY_LENGTH(path)))
                return;
            DWORD handle = 0;
            DWORD versionSize = GetFileVersionInfoSize(path, &handle);
            if (!versionSize)
                return;
            Vector<char> data(versionSize);
            if (!GetFileVersionInfo(path, 0, versionSize, data.data()))
                return;

            LPCTSTR productVersion;
            UINT productVersionLength;
            if (!VerQueryValueW(data.data(), L"\\StringFileInfo\\040904b0\\ProductVersion", (void**)&productVersion, &productVersionLength))
                return;
            if (wcsncmp(L"3.1", productVersion, productVersionLength) > 0) {
                const unsigned long long defaultQuota = 5 * 1024 * 1024; // 5 megabytes should hopefully be enough to test storage support.
                origin->setQuota(defaultQuota);
            }
        }
    }
}

// FIXME: Move this include to the top of the file with the other includes.
#include <WebCore/ApplicationCacheStorage.h>

void WebChromeClient::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    // FIXME: Free some space.
    notImplemented();
}

void WebChromeClient::reachedApplicationCacheOriginQuota(SecurityOrigin&, int64_t)
{
    notImplemented();
}

void WebChromeClient::runOpenPanel(Frame&, FileChooser& fileChooser)
{
    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(&viewWindow)))
        return;

    bool multiFile = fileChooser.settings().allowsMultipleFiles;
    Vector<WCHAR> fileBuf(multiFile ? maxFilePathsListSize : MAX_PATH);

    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));

    // Need to zero out the first char of fileBuf so GetOpenFileName doesn't think it's an initialization string
    fileBuf[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = viewWindow;
    String allFiles = makeString(allFilesText(), String("\0*.*\0\0", 6));

    Vector<wchar_t> filterCharacters = allFiles.wideCharacters(); // Retain buffer long enough to make the GetOpenFileName call
    ofn.lpstrFilter = filterCharacters.data();

    ofn.lpstrFile = fileBuf.data();
    ofn.nMaxFile = fileBuf.size();
    String dialogTitle = uploadFileText();
    Vector<wchar_t> dialogTitleCharacters = dialogTitle.wideCharacters(); // Retain buffer long enough to make the GetOpenFileName call
    ofn.lpstrTitle = dialogTitleCharacters.data();
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (multiFile)
        ofn.Flags = ofn.Flags | OFN_ALLOWMULTISELECT;

    if (GetOpenFileName(&ofn)) {
        WCHAR* files = fileBuf.data();
        Vector<String> fileList;
        String file(files);
        if (multiFile) {
            while (!file.isEmpty()) {
                // When using the OFN_EXPLORER flag, the file list is null delimited.
                // When you create a String from a ptr to this list, it will use strlen to look for the null character.
                // Then we find the next file path string by using the length of the string we just created.
                WCHAR* nextFilePtr = files + file.length() + 1;
                String nextFile(nextFilePtr);
                // If multiple files are selected, there will be a directory name first, which we don't want to add to the vector.
                // We know a single file was selected if there is only one filename in the list.  
                // In that case, we don't want to skip adding the first (and only) name.
                if (files != fileBuf.data() || nextFile.isEmpty())
                    fileList.append(file);
                files = nextFilePtr;
                file = nextFile;
            }
        } else
            fileList.append(file);
        ASSERT(fileList.size());
        fileChooser.chooseFiles(fileList);
    }
    // FIXME: Show some sort of error if too many files are selected and the buffer is too small.  For now, this will fail silently.
}

void WebChromeClient::loadIconForFiles(const Vector<WTF::String>& filenames, WebCore::FileIconLoader& loader)
{
    loader.iconLoaded(Icon::createIconForFiles(filenames));
}

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

void WebChromeClient::didFinishLoadingImageForElement(WebCore::HTMLImageElement&)
{
}

void WebChromeClient::setCursor(const Cursor& cursor)
{
    if (!cursor.platformCursor())
        return;

    HCURSOR platformCursor = cursor.platformCursor()->nativeCursor();
    if (!platformCursor)
        return;

    bool shouldSetCursor = true;
    if (COMPtr<IWebUIDelegate> delegate = uiDelegate()) {
        COMPtr<IWebUIDelegatePrivate> delegatePrivate(Query, delegate);
        if (delegatePrivate) {
            if (SUCCEEDED(delegatePrivate->webViewSetCursor(m_webView, platformCursor)))
                shouldSetCursor = false;
        }
    }

    if (shouldSetCursor)
        ::SetCursor(platformCursor);

    setLastSetCursorToCurrentCursor();
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool)
{
    notImplemented();
}

void WebChromeClient::setLastSetCursorToCurrentCursor()
{
    m_webView->setLastCursor(::GetCursor());
}

void WebChromeClient::attachRootGraphicsLayer(Frame&, GraphicsLayer* graphicsLayer)
{
    m_webView->setRootChildLayer(graphicsLayer);
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer*)
{
    // FIXME: If we want view-relative page overlays in Legacy WebKit on Windows, this would be the place to hook them up.
}

void WebChromeClient::triggerRenderingUpdate()
{
    m_webView->flushPendingGraphicsLayerChangesSoon();
}

#if PLATFORM(WIN) && USE(AVFOUNDATION)
WebCore::GraphicsDeviceAdapter* WebChromeClient::graphicsDeviceAdapter() const
{
    return m_webView->graphicsDeviceAdapter();
}
#endif

COMPtr<IWebUIDelegate> WebChromeClient::uiDelegate()
{
    COMPtr<IWebUIDelegate> delegate;
    m_webView->uiDelegate(&delegate);
    return delegate;
}

#if ENABLE(VIDEO)

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode)
{
    return true;
}

void WebChromeClient::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode, bool)
{
    m_webView->enterVideoFullscreenForVideoElement(videoElement);
}

void WebChromeClient::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    m_webView->exitVideoFullscreenForVideoElement(videoElement);
    completionHandler(true);
}

#endif

bool WebChromeClient::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool WebChromeClient::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

RefPtr<PopupMenu> WebChromeClient::createPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new PopupMenuWin(&client));
}

RefPtr<SearchPopupMenu> WebChromeClient::createSearchPopupMenu(PopupMenuClient& client) const
{
    return adoptRef(new SearchPopupMenuWin(&client));
}

#if ENABLE(FULLSCREEN_API)

bool WebChromeClient::supportsFullScreenForElement(const Element& element, bool requestingKeyboardAccess)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate4> uiDelegatePrivate4(Query, uiDelegate);
        BOOL supports = FALSE;
        COMPtr<IDOMElement> domElement(AdoptCOM, DOMElement::createInstance(const_cast<Element*>(&element)));

        if (uiDelegatePrivate4 && SUCCEEDED(uiDelegatePrivate4->supportsFullScreenForElement(domElement.get(), requestingKeyboardAccess, &supports)))
            return supports;
    }

    return m_webView->supportsFullScreenForElement(&element, requestingKeyboardAccess);
}

void WebChromeClient::enterFullScreenForElement(Element& element)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate4> uiDelegatePrivate4(Query, uiDelegate);
        COMPtr<IDOMElement> domElement(AdoptCOM, DOMElement::createInstance(&element));
        if (uiDelegatePrivate4 && SUCCEEDED(uiDelegatePrivate4->enterFullScreenForElement(domElement.get())))
            return;
    } 

    m_webView->setFullScreenElement(&element);
    m_webView->fullScreenController()->enterFullScreen();
}

void WebChromeClient::exitFullScreenForElement(Element* element)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate4> uiDelegatePrivate4(Query, uiDelegate);
        COMPtr<IDOMElement> domElement(AdoptCOM, DOMElement::createInstance(element));
        if (uiDelegatePrivate4 && SUCCEEDED(uiDelegatePrivate4->exitFullScreenForElement(domElement.get())))
            return;
    }

    ASSERT(element == m_webView->fullScreenElement());
    m_webView->fullScreenController()->exitFullScreen();
}

#endif

void WebChromeClient::AXStartFrameLoad()
{
    COMPtr<IAccessibilityDelegate> delegate;
    m_webView->accessibilityDelegate(&delegate);
    if (delegate)
        delegate->fireFrameLoadStartedEvents();
}

void WebChromeClient::AXFinishFrameLoad()
{
    COMPtr<IAccessibilityDelegate> delegate;
    m_webView->accessibilityDelegate(&delegate);
    if (delegate)
        delegate->fireFrameLoadFinishedEvents();
}

bool WebChromeClient::shouldUseTiledBackingForFrameView(const FrameView& frameView) const
{
#if !USE(CAIRO)
    return frameView.frame().isMainFrame();
#else
    return false;
#endif
}

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    completion(CookieConsentDecisionResult::NotSupported);
}

void WebChromeClient::classifyModalContainerControls(Vector<String>&&, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    completion({ });
}

void WebChromeClient::decidePolicyForModalContainer(OptionSet<ModalContainerControlType>, CompletionHandler<void(ModalContainerDecision)>&& completion)
{
    completion(ModalContainerDecision::Show);
}
