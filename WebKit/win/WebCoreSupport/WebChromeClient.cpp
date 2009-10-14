/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebChromeClient.h"

#include "COMPropertyBag.h"
#include "COMVariantSetter.h"
#include "WebElementPropertyBag.h"
#include "WebFrame.h"
#include "WebHistory.h"
#include "WebMutableURLRequest.h"
#include "WebDesktopNotificationsDelegate.h"
#include "WebSecurityOrigin.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/Console.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/Cursor.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameView.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/WindowFeatures.h>
#pragma warning(pop)

#include <tchar.h>

using namespace WebCore;

// When you call GetOpenFileName, if the size of the buffer is too small,
// MSDN says that the first two bytes of the buffer contain the required size for the file selection, in bytes or characters
// So we can assume the required size can't be more than the maximum value for a short.
static const size_t maxFilePathsListSize = USHRT_MAX;

WebChromeClient::WebChromeClient(WebView* webView)
    : m_webView(webView)
#if ENABLE(NOTIFICATIONS)
    , m_notificationsDelegate(new WebDesktopNotificationsDelegate(webView))
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

float WebChromeClient::scaleFactor()
{
    // Windows doesn't support UI scaling.
    return 1.0;
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
    BOOL bForward = (direction == FocusDirectionForward) ? TRUE : FALSE;
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
    BOOL bForward = (direction == FocusDirectionForward) ? TRUE : FALSE;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->takeFocus(m_webView, bForward);
        uiDelegate->Release();
    }
}

static COMPtr<IPropertyBag> createWindowFeaturesPropertyBag(const WindowFeatures& features)
{
    HashMap<String, COMVariant> map;
    if (features.xSet)
        map.set(WebWindowFeaturesXKey, features.x);
    if (features.ySet)
        map.set(WebWindowFeaturesYKey, features.y);
    if (features.widthSet)
        map.set(WebWindowFeaturesWidthKey, features.width);
    if (features.heightSet)
        map.set(WebWindowFeaturesHeightKey, features.height);
    map.set(WebWindowFeaturesMenuBarVisibleKey, features.menuBarVisible);
    map.set(WebWindowFeaturesStatusBarVisibleKey, features.statusBarVisible);
    map.set(WebWindowFeaturesToolBarVisibleKey, features.toolBarVisible);
    map.set(WebWindowFeaturesScrollbarsVisibleKey, features.scrollbarsVisible);
    map.set(WebWindowFeaturesResizableKey, features.resizable);
    map.set(WebWindowFeaturesFullscreenKey, features.fullscreen);
    map.set(WebWindowFeaturesDialogKey, features.dialog);

    return COMPtr<IPropertyBag>(AdoptCOM, COMPropertyBag<COMVariant>::adopt(map));
}

Page* WebChromeClient::createWindow(Frame*, const FrameLoadRequest& frameLoadRequest, const WindowFeatures& features)
{
    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return 0;

    COMPtr<IWebMutableURLRequest> request(AdoptCOM, WebMutableURLRequest::createInstance(frameLoadRequest.resourceRequest()));

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

void WebChromeClient::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned line, const String& url)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate;
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate)))
            uiPrivate->webViewAddMessageToConsole(m_webView, BString(message), line, BString(url), true);
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

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    BOOL result = TRUE;
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        WebFrame* webFrame = kit(frame);
        ui->runBeforeUnloadConfirmPanelWithMessage(m_webView, BString(message), webFrame, &result);
        ui->Release();
    }
    return !!result;
}

void WebChromeClient::closeWindowSoon()
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
    m_webView->closeWindowSoon();
}

void WebChromeClient::runJavaScriptAlert(Frame*, const String& message)
{
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptAlertPanelWithMessage(m_webView, BString(message));
}

bool WebChromeClient::runJavaScriptConfirm(Frame*, const String& message)
{
    BOOL result = FALSE;
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)))
        ui->runJavaScriptConfirmPanelWithMessage(m_webView, BString(message), &result);
    return !!result;
}

bool WebChromeClient::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    COMPtr<IWebUIDelegate> ui;
    if (FAILED(m_webView->uiDelegate(&ui)))
        return false;

    TimerBase::fireTimersInNestedEventLoop();

    BSTR resultBSTR = 0;
    if (FAILED(ui->runJavaScriptTextInputPanelWithPrompt(m_webView, BString(message), BString(defaultValue), &resultBSTR)))
        return false;

    if (resultBSTR) {
        result = String(resultBSTR, SysStringLen(resultBSTR));
        SysFreeString(resultBSTR);
        return true;
    }

    return false;
}

void WebChromeClient::setStatusbarText(const String& statusText)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        uiDelegate->setStatusText(m_webView, BString(statusText));
    }
}

bool WebChromeClient::shouldInterruptJavaScript()
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate;
        if (SUCCEEDED(uiDelegate->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            BOOL result;
            if (SUCCEEDED(uiPrivate->webViewShouldInterruptJavaScript(m_webView, &result)))
                return !!result;
        }
    }
    return false;
}

bool WebChromeClient::tabsToLinks() const
{
    BOOL enabled = FALSE;
    IWebPreferences* preferences;
    if (SUCCEEDED(m_webView->preferences(&preferences)))
        preferences->tabsToLinks(&enabled);

    return !!enabled;
}

IntRect WebChromeClient::windowResizerRect() const
{
    return IntRect();
}

void WebChromeClient::repaint(const IntRect& windowRect, bool contentChanged, bool immediate, bool repaintContentOnly)
{
    ASSERT(core(m_webView->topLevelFrame()));
    m_webView->repaint(windowRect, contentChanged, immediate, repaintContentOnly);
}

void WebChromeClient::scroll(const IntSize& delta, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(core(m_webView->topLevelFrame()));

    m_webView->scrollBackingStore(core(m_webView->topLevelFrame())->view(), delta.width(), delta.height(), scrollViewRect, clipRect);
}

IntRect WebChromeClient::windowToScreen(const IntRect& rect) const
{
    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        return rect;

    // Find the top left corner of the Widget's containing window in screen coords,
    // and adjust the result rect's position by this amount.
    POINT topLeft = {0, 0};
    IntRect result = rect;
    ::ClientToScreen(viewWindow, &topLeft);
    result.move(topLeft.x, topLeft.y);

    return result;
}

IntPoint WebChromeClient::screenToWindow(const IntPoint& point) const
{
    POINT result = point;

    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        return point;

    ::ScreenToClient(viewWindow, &result);

    return result;
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        return 0;
    return viewWindow;
}

void WebChromeClient::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    COMPtr<WebElementPropertyBag> element;
    element.adoptRef(WebElementPropertyBag::createInstance(result));

    uiDelegate->mouseDidMoveOverElement(m_webView, element.get(), modifierFlags);
}

void WebChromeClient::setToolTip(const String& toolTip, TextDirection)
{
    m_webView->setToolTip(toolTip);
}

void WebChromeClient::print(Frame* frame)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)))
        uiDelegate->printFrame(m_webView, kit(frame));
}

#if ENABLE(DATABASE)
void WebChromeClient::exceededDatabaseQuota(Frame* frame, const String& databaseIdentifier)
{
    COMPtr<WebSecurityOrigin> origin(AdoptCOM, WebSecurityOrigin::createInstance(frame->document()->securityOrigin()));
    COMPtr<IWebUIDelegate> uiDelegate;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate))) {
        COMPtr<IWebUIDelegatePrivate> uiDelegatePrivate(Query, uiDelegate);
        if (uiDelegatePrivate)
            uiDelegatePrivate->exceededDatabaseQuota(m_webView, kit(frame), origin.get(), BString(databaseIdentifier));
        else {
            // FIXME: remove this workaround once shipping Safari has the necessary delegate implemented.
            TCHAR path[MAX_PATH];
            HMODULE safariHandle = GetModuleHandle(TEXT("Safari.exe"));
            if (!safariHandle)
                return;
            GetModuleFileName(safariHandle, path, ARRAYSIZE(path));
            DWORD handle;
            DWORD versionSize = GetFileVersionInfoSize(path, &handle);
            if (!versionSize)
                return;
            Vector<char> data(versionSize);
            if (!GetFileVersionInfo(path, 0, versionSize, data.data()))
                return;

            LPCTSTR productVersion;
            UINT productVersionLength;
            if (!VerQueryValue(data.data(), TEXT("\\StringFileInfo\\040904b0\\ProductVersion"), (void**)&productVersion, &productVersionLength))
                return;
            if (_tcsncmp(TEXT("3.1"), productVersion, productVersionLength) > 0) {
                const unsigned long long defaultQuota = 5 * 1024 * 1024; // 5 megabytes should hopefully be enough to test storage support.
                origin->setQuota(defaultQuota);
            }
        }
    }
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "ApplicationCacheStorage.h"
void WebChromeClient::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    // FIXME: Free some space.
    notImplemented();
}
#endif

void WebChromeClient::populateVisitedLinks()
{
    COMPtr<IWebHistoryDelegate> historyDelegate;
    m_webView->historyDelegate(&historyDelegate);
    if (historyDelegate) {
        historyDelegate->populateVisitedLinksForWebView(m_webView);
        return;
    }

    WebHistory* history = WebHistory::sharedHistory();
    if (!history)
        return;
    history->addVisitedLinksToPageGroup(m_webView->page()->group());
}

bool WebChromeClient::paintCustomScrollbar(GraphicsContext* context, const FloatRect& rect, ScrollbarControlSize size, 
                                           ScrollbarControlState state, ScrollbarPart pressedPart, bool vertical,
                                           float value, float proportion, ScrollbarControlPartMask parts)
{
    if (context->paintingDisabled())
        return false;

    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return false;

    WebScrollbarControlPartMask webParts = WebNoScrollPart;
    if (parts & BackButtonStartPart) // FIXME: Hyatt, what about BackButtonEndPart?
        webParts |= WebBackButtonPart;
    if (parts & BackTrackPart)
        webParts |= WebBackTrackPart;
    if (parts & ThumbPart)
        webParts |= WebThumbPart;
    if (parts & ForwardTrackPart)
        webParts |= WebForwardTrackPart;
    if (parts & ForwardButtonStartPart) // FIXME: Hyatt, what about ForwardButtonEndPart?
        webParts |= WebForwardButtonPart;

    WebScrollbarControlPart webPressedPart = WebNoScrollPart;
    switch (pressedPart) {
        case BackButtonStartPart: // FIXME: Hyatt, what about BackButtonEndPart?
            webPressedPart = WebBackButtonPart;
            break;
        case BackTrackPart:
            webPressedPart = WebBackTrackPart;
            break;
        case ThumbPart:
            webPressedPart = WebThumbPart;
            break;
        case ForwardTrackPart:
            webPressedPart = WebForwardTrackPart;
            break;
        case ForwardButtonStartPart: // FIXME: Hyatt, what about ForwardButtonEndPart?
            webPressedPart = WebForwardButtonPart;
            break;
        default:
            break;
    }

    WebScrollBarControlSize webSize;
    switch (size) {
        case SmallScrollbar:
            webSize = WebSmallScrollbar;
            break;
        case RegularScrollbar:
        default:
            webSize = WebRegularScrollbar;
    }
    WebScrollbarControlState webState = 0;
    if (state & ActiveScrollbarState)
        webState |= WebActiveScrollbarState;
    if (state & EnabledScrollbarState)
        webState |= WebEnabledScrollbarState;
    if (state & PressedScrollbarState)
        webState |= WebPressedScrollbarState;
    
    RECT webRect = enclosingIntRect(rect);
    HDC hDC = context->getWindowsContext(webRect);
    HRESULT hr = delegate->paintCustomScrollbar(m_webView, hDC, webRect, webSize, webState, webPressedPart, 
                                                          vertical, value, proportion, webParts);
    context->releaseWindowsContext(hDC, webRect);
    return SUCCEEDED(hr);
}

bool WebChromeClient::paintCustomScrollCorner(GraphicsContext* context, const FloatRect& rect)
{
    if (context->paintingDisabled())
        return false;

    COMPtr<IWebUIDelegate> delegate = uiDelegate();
    if (!delegate)
        return false;

    RECT webRect = enclosingIntRect(rect);
    HDC hDC = context->getWindowsContext(webRect);
    HRESULT hr = delegate->paintCustomScrollCorner(m_webView, hDC, webRect);
    context->releaseWindowsContext(hDC, webRect);
    return SUCCEEDED(hr);
}

void WebChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser> prpFileChooser)
{
    RefPtr<FileChooser> fileChooser = prpFileChooser;

    HWND viewWindow;
    if (FAILED(m_webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        return;

    bool multiFile = fileChooser->allowsMultipleFiles();
    Vector<TCHAR> fileBuf(multiFile ? maxFilePathsListSize : MAX_PATH);

    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));

    // Need to zero out the first char of fileBuf so GetOpenFileName doesn't think it's an initialization string
    fileBuf[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = viewWindow;
    String allFiles = allFilesText();
    allFiles.append(TEXT("\0*.*\0\0"), 6);
    ofn.lpstrFilter = allFiles.charactersWithNullTermination();
    ofn.lpstrFile = fileBuf.data();
    ofn.nMaxFile = fileBuf.size();
    String dialogTitle = uploadFileText();
    ofn.lpstrTitle = dialogTitle.charactersWithNullTermination();
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (multiFile)
        ofn.Flags = ofn.Flags | OFN_ALLOWMULTISELECT;

    if (GetOpenFileName(&ofn)) {
        TCHAR* files = fileBuf.data();
        Vector<String> fileList;
        String file(files);
        if (multiFile) {
            while (!file.isEmpty()) {
                // When using the OFN_EXPLORER flag, the file list is null delimited.
                // When you create a String from a ptr to this list, it will use strlen to look for the null character.
                // Then we find the next file path string by using the length of the string we just created.
                TCHAR* nextFilePtr = files + file.length() + 1;
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
        fileChooser->chooseFiles(fileList);
    }
    // FIXME: Show some sort of error if too many files are selected and the buffer is too small.  For now, this will fail silently.
}

bool WebChromeClient::setCursor(PlatformCursorHandle cursor)
{
    if (!cursor)
        return false;

    if (COMPtr<IWebUIDelegate> delegate = uiDelegate()) {
        COMPtr<IWebUIDelegatePrivate> delegatePrivate(Query, delegate);
        if (delegatePrivate) {
            if (SUCCEEDED(delegatePrivate->webViewSetCursor(m_webView, reinterpret_cast<OLE_HANDLE>(cursor))))
                return true;
        }
    }

    ::SetCursor(cursor);
    return true;
}

void WebChromeClient::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    // See the comment in WebCore/page/ChromeClient.h
    notImplemented();
}

COMPtr<IWebUIDelegate> WebChromeClient::uiDelegate()
{
    COMPtr<IWebUIDelegate> delegate;
    m_webView->uiDelegate(&delegate);
    return delegate;
}
