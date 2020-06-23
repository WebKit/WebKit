/*
 * Copyright (C) 2005-2008, 2014-2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UIDelegate.h"

#include "DumpRenderTree.h"
#include "DraggingInfo.h"
#include "EventSender.h"
#include "DRTDesktopNotificationPresenter.h"
#include "TestRunner.h"
#include <WebCore/COMPtr.h>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <comutil.h>
#include <stdio.h>

using std::wstring;

class DRTUndoObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DRTUndoObject(IWebUndoTarget* target, BSTR actionName, IUnknown* obj)
        : m_target(target)
        , m_actionName(actionName)
        , m_obj(obj)
    {
    }

    ~DRTUndoObject()
    {
    }

    void invoke()
    {
        m_target->invoke(m_actionName, m_obj.get());
    }

private:
    IWebUndoTarget* m_target;
    _bstr_t m_actionName;
    COMPtr<IUnknown> m_obj;
};

class DRTUndoStack {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isEmpty() const { return m_undoVector.isEmpty(); }
    void clear() { m_undoVector.clear(); }

    void push(DRTUndoObject* undoObject) { m_undoVector.append(undoObject); }
    std::unique_ptr<DRTUndoObject> pop() { return m_undoVector.takeLast(); }

private:
    Vector<std::unique_ptr<DRTUndoObject>> m_undoVector;
};

class DRTUndoManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DRTUndoManager();

    void removeAllActions();
    void registerUndoWithTarget(IWebUndoTarget* target, BSTR actionName, IUnknown* obj);
    void redo();
    void undo();
    bool canRedo() { return !m_redoStack->isEmpty(); }
    bool canUndo() { return !m_undoStack->isEmpty(); }

private:
    std::unique_ptr<DRTUndoStack> m_redoStack;
    std::unique_ptr<DRTUndoStack> m_undoStack;
    bool m_isRedoing { false };
    bool m_isUndoing { false };
};

DRTUndoManager::DRTUndoManager()
    : m_redoStack(makeUnique<DRTUndoStack>())
    , m_undoStack(makeUnique<DRTUndoStack>())
{
}

void DRTUndoManager::removeAllActions()
{
    m_redoStack->clear();
    m_undoStack->clear();
}

void DRTUndoManager::registerUndoWithTarget(IWebUndoTarget* target, BSTR actionName, IUnknown* obj)
{
    if (!m_isUndoing && !m_isRedoing)
        m_redoStack->clear();

    DRTUndoStack* stack = m_isUndoing ? m_redoStack.get() : m_undoStack.get();
    stack->push(new DRTUndoObject(target, actionName, obj));
}

void DRTUndoManager::redo()
{
    if (!canRedo())
        return;

    m_isRedoing = true;

    m_redoStack->pop()->invoke();

    m_isRedoing = false;
}

void DRTUndoManager::undo()
{
    if (!canUndo())
        return;

    m_isUndoing = true;

    m_undoStack->pop()->invoke();

    m_isUndoing = false;
}

UIDelegate::UIDelegate()
    : m_undoManager(makeUnique<DRTUndoManager>())
    , m_desktopNotifications(new DRTDesktopNotificationPresenter)
{
    m_frame.bottom = 0;
    m_frame.top = 0;
    m_frame.left = 0;
    m_frame.right = 0;
}

void UIDelegate::resetUndoManager()
{
    m_undoManager = makeUnique<DRTUndoManager>();
}

HRESULT UIDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate2))
        *ppvObject = static_cast<IWebUIDelegate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegatePrivate))
        *ppvObject = static_cast<IWebUIDelegatePrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegatePrivate2))
        *ppvObject = static_cast<IWebUIDelegatePrivate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegatePrivate3))
        *ppvObject = static_cast<IWebUIDelegatePrivate3*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG UIDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG UIDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT UIDelegate::willPerformDragSourceAction(_In_opt_ IWebView*, WebDragSourceAction, _In_ LPPOINT, _In_opt_ IDataObject* pasteboard, _COM_Outptr_opt_ IDataObject** newPasteboard)
{
    if (!newPasteboard)
        return E_POINTER;
    *newPasteboard = nullptr;
    return E_NOTIMPL;
}

HRESULT UIDelegate::hasCustomMenuImplementation(_Out_ BOOL* hasCustomMenus)
{
    *hasCustomMenus = TRUE;

    return S_OK;
}

HRESULT UIDelegate::trackCustomPopupMenu(_In_opt_ IWebView* /*sender*/, _In_ HMENU /*menu*/, _In_ LPPOINT /*point*/)
{
    // Do nothing
    return S_OK;
}

HRESULT UIDelegate::registerUndoWithTarget(_In_opt_ IWebUndoTarget* target, _In_ BSTR actionName, _In_opt_ IUnknown* actionArg)
{
    m_undoManager->registerUndoWithTarget(target, actionName, actionArg);
    return S_OK;
}

HRESULT UIDelegate::removeAllActionsWithTarget(_In_opt_ IWebUndoTarget*)
{
    m_undoManager->removeAllActions();
    return S_OK;
}

HRESULT UIDelegate::setActionTitle(_In_ BSTR /*actionTitle*/)
{
    // It is not neccessary to implement this for DRT because there is
    // menu to write out the title to.
    return S_OK;
}

HRESULT UIDelegate::undo()
{
    m_undoManager->undo();
    return S_OK;
}

HRESULT UIDelegate::redo()
{
    m_undoManager->redo();
    return S_OK;
}

HRESULT UIDelegate::canUndo(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = m_undoManager->canUndo();
    return S_OK;
}

HRESULT UIDelegate::canRedo(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = m_undoManager->canRedo();
    return S_OK;
}

HRESULT UIDelegate::printFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::ftpDirectoryTemplatePath(_In_opt_ IWebView*, __deref_opt_out BSTR* path)
{
    if (!path)
        return E_POINTER;
    *path = nullptr;
    return E_NOTIMPL;
}


HRESULT UIDelegate::webViewHeaderHeight(_In_opt_ IWebView*, _Out_ float* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewFooterHeight(_In_opt_ IWebView*, _Out_ float* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawHeaderInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR /*drawingContext*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawFooterInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR /*drawingContext*/, UINT /*pageIndex*/, UINT /*pageCount*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewPrintingMarginRect(_In_opt_ IWebView*, _Out_ RECT*)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::canRunModal(_In_opt_ IWebView*, _Out_ BOOL* canRunBoolean)
{
    if (!canRunBoolean)
        return E_POINTER;
    *canRunBoolean = TRUE;
    return S_OK;
}

static HWND getHandleFromWebView(IWebView* webView)
{
    COMPtr<IWebViewPrivate2> webViewPrivate;
    HRESULT hr = webView->QueryInterface(&webViewPrivate);
    if (FAILED(hr))
        return nullptr;

    HWND webViewWindow = nullptr;
    hr = webViewPrivate->viewWindow(&webViewWindow);
    if (FAILED(hr))
        return nullptr;

    return webViewWindow;
}

HRESULT UIDelegate::createModalDialog(_In_opt_ IWebView* sender, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView** newWebView)
{
    if (!newWebView)
        return E_POINTER;

    COMPtr<IWebView> webView;
    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&webView);
    if (FAILED(hr))
        return hr;

    m_modalDialogParent = ::CreateWindow(L"STATIC", L"ModalDialog", WS_OVERLAPPED | WS_VISIBLE, 0, 0, 0, 0, getHandleFromWebView(sender), nullptr, nullptr, nullptr);

    hr = webView->setHostWindow(m_modalDialogParent);
    if (FAILED(hr))
        return hr;

    RECT clientRect = { 0, 0, 0, 0 };
    hr = webView->initWithFrame(clientRect, 0, _bstr_t(L""));
    if (FAILED(hr))
        return hr;

    COMPtr<IWebUIDelegate> uiDelegate;
    hr = sender->uiDelegate(&uiDelegate);
    if (FAILED(hr))
        return hr;

    hr = webView->setUIDelegate(uiDelegate.get());
    if (FAILED(hr))
        return hr;

    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    hr = sender->frameLoadDelegate(&frameLoadDelegate);
    if (FAILED(hr))
        return hr;

    hr = webView.get()->setFrameLoadDelegate(frameLoadDelegate.get());
    if (FAILED(hr))
        return hr;

    *newWebView = webView.leakRef();

    return S_OK;
}

HRESULT UIDelegate::runModal(_In_opt_ IWebView* webView)
{
    COMPtr<IWebView> protector(webView);

    auto modalDialogOwner = ::GetWindow(m_modalDialogParent, GW_OWNER);
    auto topLevelParent = ::GetAncestor(modalDialogOwner, GA_ROOT);

    ::EnableWindow(topLevelParent, FALSE);

    while (::IsWindow(getHandleFromWebView(webView))) {
#if USE(CF)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
        MSG msg;
        if (!::GetMessage(&msg, 0, 0, 0))
            break;

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    ::EnableWindow(topLevelParent, TRUE);

    return S_OK;
}

HRESULT UIDelegate::isMenuBarVisible(_In_opt_ IWebView*, _Out_ BOOL* visible)
{
    if (!visible)
        return E_POINTER;
    *visible = false;
    return E_NOTIMPL;
}

HRESULT UIDelegate::setMenuBarVisible(_In_opt_ IWebView*, BOOL /*visible*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::runDatabaseSizeLimitPrompt(_In_opt_ IWebView*, _In_ BSTR /*displayName*/, _In_opt_ IWebFrame* /*initiatedByFrame*/, _Out_ BOOL* allowed)
{
    if (!allowed)
        return E_POINTER;
    *allowed = false;
    return E_NOTIMPL;
}

HRESULT UIDelegate::paintCustomScrollbar(_In_opt_ IWebView*, _In_ HDC, RECT, WebScrollBarControlSize, WebScrollbarControlState,
    WebScrollbarControlPart /*pressedPart*/, BOOL /*vertical*/, float /*value*/, float /*proportion*/, WebScrollbarControlPartMask)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::paintCustomScrollCorner(_In_opt_ IWebView*, _In_ HDC, RECT)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::setFrame(_In_opt_ IWebView* /*sender*/, _In_ RECT* frame)
{
    m_frame = *frame;
    return S_OK;
}

HRESULT UIDelegate::webViewFrame(_In_opt_ IWebView* /*sender*/, _Out_ RECT* frame)
{
    *frame = m_frame;
    return S_OK;
}

const wchar_t* toMessage(const BSTR message)
{
    auto length = SysStringLen(message);
    if (!length)
        return message;
    // Return "(null)" for an invalid UTF-16 sequence to align with WebKitTestRunner.
    auto utf8 = StringView(ucharFrom(message), length).tryGetUtf8(StrictConversion);
    if (!utf8)
        return L"(null)";
    return message;
}

HRESULT UIDelegate::runJavaScriptAlertPanelWithMessage(_In_opt_ IWebView* /*sender*/, _In_ BSTR message)
{
    if (!done) {
        fprintf(testResult, "ALERT: %S\n", toMessage(message));
        fflush(testResult);
    }

    return S_OK;
}

HRESULT UIDelegate::runJavaScriptConfirmPanelWithMessage(_In_opt_ IWebView* /*sender*/, _In_ BSTR message, _Out_ BOOL* result)
{
    if (!done)
        fprintf(testResult, "CONFIRM: %S\n", toMessage(message));

    *result = TRUE;

    return S_OK;
}

HRESULT UIDelegate::runJavaScriptTextInputPanelWithPrompt(_In_opt_ IWebView* /*sender*/, _In_ BSTR message, _In_ BSTR defaultText, __deref_opt_out BSTR* result)
{
    if (!done)
        fprintf(testResult, "PROMPT: %S, default text: %S\n", toMessage(message), toMessage(defaultText));

    *result = SysAllocString(defaultText);

    return S_OK;
}

HRESULT UIDelegate::runBeforeUnloadConfirmPanelWithMessage(_In_opt_ IWebView* /*sender*/, _In_ BSTR message, _In_opt_ IWebFrame* /*initiatedByFrame*/, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    if (!done)
        fprintf(testResult, "CONFIRM NAVIGATION: %S\n", toMessage(message));

    *result = !gTestRunner->shouldStayOnPageAfterHandlingBeforeUnload();

    return S_OK;
}

HRESULT UIDelegate::webViewAddMessageToConsole(_In_opt_ IWebView* /*sender*/, _In_ BSTR message, int /* lineNumber */ , _In_ BSTR url, BOOL isError)
{
    if (done)
        return S_OK;

    wstring newMessage;
    if (message) {
        newMessage = message;
        const std::wstring fileURL(L"file://");
        size_t fileProtocol = newMessage.find(fileURL);
        if (fileProtocol != wstring::npos)
            newMessage = newMessage.substr(0, fileProtocol) + lastPathComponent(newMessage.substr(fileProtocol + fileURL.size()));
    }

    auto out = gTestRunner->dumpJSConsoleLogInStdErr() ? stderr : testResult;
    fprintf(out, "CONSOLE MESSAGE: ");
    fprintf(out, "%s\n", toUTF8(newMessage).c_str());
    return S_OK;
}

HRESULT UIDelegate::doDragDrop(_In_opt_ IWebView* /*sender*/, _In_opt_ IDataObject* object, _In_opt_ IDropSource* source, DWORD /*okEffect*/, _Out_ DWORD* performedEffect)
{
    if (!performedEffect)
        return E_POINTER;

    *performedEffect = 0;

    draggingInfo = new DraggingInfo(object, source);
    HRESULT oleDragAndDropReturnValue = DRAGDROP_S_CANCEL;
    replaySavedEvents(&oleDragAndDropReturnValue);
    if (draggingInfo) {
        *performedEffect = draggingInfo->performedDropEffect();
        delete draggingInfo;
        draggingInfo = 0;
    }
    return oleDragAndDropReturnValue;
}

HRESULT UIDelegate::webViewGetDlgCode(_In_opt_ IWebView* /*sender*/, UINT /*keyCode*/, _Out_ LONG_PTR* code)
{
    if (!code)
        return E_POINTER;
    *code = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::createWebViewWithRequest(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebURLRequest* /*request*/, _COM_Outptr_opt_ IWebView** newWebView)
{
    if (!newWebView)
        return E_POINTER;
    *newWebView = nullptr;

    if (!::gTestRunner->canOpenWindows())
        return E_FAIL;

    *newWebView = createWebViewAndOffscreenWindow();
    return S_OK;
}

HRESULT UIDelegate::webViewClose(_In_opt_ IWebView* sender)
{
    HWND hostWindow;
    sender->hostWindow(&hostWindow);
    DestroyWindow(hostWindow);
    if (hostWindow == m_modalDialogParent)
        m_modalDialogParent = nullptr;
    return S_OK;
}

HRESULT UIDelegate::webViewFocus(_In_opt_ IWebView* sender)
{
    HWND hostWindow;
    sender->hostWindow(&hostWindow);
    SetForegroundWindow(hostWindow);
    return S_OK; 
}

HRESULT UIDelegate::webViewUnfocus(_In_opt_ IWebView* /*sender*/)
{
    SetForegroundWindow(GetDesktopWindow());
    return S_OK; 
}

HRESULT UIDelegate::webViewPainted(_In_opt_ IWebView* /*sender*/)
{
    return S_OK;
}

HRESULT UIDelegate::exceededDatabaseQuota(_In_opt_ IWebView* sender, _In_opt_ IWebFrame* frame, _In_opt_ IWebSecurityOrigin* origin, _In_ BSTR databaseIdentifier)
{
    _bstr_t protocol;
    _bstr_t host;
    unsigned short port;

    origin->protocol(&protocol.GetBSTR());
    origin->host(&host.GetBSTR());
    origin->port(&port);

    if (!done && gTestRunner->dumpDatabaseCallbacks())
        fprintf(testResult, "UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%S\n", static_cast<const char*>(protocol), static_cast<const char*>(host), port, databaseIdentifier);

    unsigned long long defaultQuota = 5 * 1024 * 1024;
    double testDefaultQuota = gTestRunner->databaseDefaultQuota();
    if (testDefaultQuota >= 0)
        defaultQuota = testDefaultQuota;

    COMPtr<IWebDatabaseManager> databaseManager;
    COMPtr<IWebDatabaseManager> tmpDatabaseManager;

    if (FAILED(WebKitCreateInstance(CLSID_WebDatabaseManager, 0, IID_IWebDatabaseManager, (void**)&tmpDatabaseManager))) {
        origin->setQuota(defaultQuota);
        return S_OK;
    }
    if (FAILED(tmpDatabaseManager->sharedWebDatabaseManager(&databaseManager))) {
        origin->setQuota(defaultQuota);
        return S_OK;
    }
    IPropertyBag* detailsBag;
    if (FAILED(databaseManager->detailsForDatabase(databaseIdentifier, origin, &detailsBag))) {
        origin->setQuota(defaultQuota);
        return S_OK;
    }
    _variant_t var;
    detailsBag->Read(WebDatabaseUsageKey, &var.GetVARIANT(), nullptr);
    unsigned long long expectedSize = V_UI8(&var);
    unsigned long long newQuota = defaultQuota;

    double maxQuota = gTestRunner->databaseMaxQuota();
    if (maxQuota >= 0) {
        if (defaultQuota < expectedSize && expectedSize <= maxQuota) {
            newQuota = expectedSize;
            fprintf(testResult, "UI DELEGATE DATABASE CALLBACK: increased quota to %llu\n", newQuota);
        }
    }
    origin->setQuota(newQuota);

    return S_OK;
}

HRESULT UIDelegate::embeddedViewWithArguments(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebFrame* /*frame*/, _In_opt_ IPropertyBag* /*arguments*/, _COM_Outptr_opt_ IWebEmbeddedView** view)
{
    if (!view)
        return E_POINTER;
    *view = nullptr;
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewClosing(_In_opt_ IWebView* /*sender*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewSetCursor(_In_opt_ IWebView* /*sender*/, _In_ HCURSOR /*cursor*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewDidInvalidate(_In_opt_ IWebView* /*sender*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::setStatusText(_In_opt_ IWebView*, _In_ BSTR text)
{
    if (!done && gTestRunner->dumpStatusCallbacks())
        fprintf(testResult, "UI DELEGATE STATUS CALLBACK: setStatusText:%S\n", toMessage(text));
    return S_OK;
}

HRESULT UIDelegate::desktopNotificationsDelegate(_COM_Outptr_opt_ IWebDesktopNotificationsDelegate** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    m_desktopNotifications.copyRefTo(result);
    return S_OK;
}

HRESULT UIDelegate::createWebViewWithRequest(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebURLRequest* /*request*/, _In_opt_ IPropertyBag* /*windowFeatures*/, _COM_Outptr_opt_ IWebView** newWebView)
{
    if (!newWebView)
        return E_POINTER;
    *newWebView = nullptr;
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawBackground(_In_opt_ IWebView* /*sender*/, _In_ HDC, _In_ const RECT* /*dirtyRect*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::decidePolicyForGeolocationRequest(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebFrame*, _In_opt_ IWebSecurityOrigin*, _In_opt_ IWebGeolocationPolicyListener*)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::didPressMissingPluginButton(_In_opt_ IDOMElement* /*element*/)
{
    fprintf(testResult, "MISSING PLUGIN BUTTON PRESSED\n");
    return S_OK;
}
