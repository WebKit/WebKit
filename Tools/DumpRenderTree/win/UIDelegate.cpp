/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
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
#include <WebKit/WebKit.h>
#include <WebKit/WebKitCOMAPI.h>
#include <comutil.h>
#include <stdio.h>

using std::wstring;

class DRTUndoObject {
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
public:
    bool isEmpty() const { return m_undoVector.isEmpty(); }
    void clear() { m_undoVector.clear(); }

    void push(DRTUndoObject* undoObject) { m_undoVector.append(undoObject); }
    std::unique_ptr<DRTUndoObject> pop() { std::unique_ptr<DRTUndoObject> top = std::move(m_undoVector.last()); m_undoVector.removeLast(); return std::move(top); }

private:
    Vector<std::unique_ptr<DRTUndoObject>> m_undoVector;
};

class DRTUndoManager {
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
    bool m_isRedoing;
    bool m_isUndoing;
};

DRTUndoManager::DRTUndoManager()
    : m_redoStack(std::make_unique<DRTUndoStack>())
    , m_undoStack(std::make_unique<DRTUndoStack>())
    , m_isRedoing(false)
    , m_isUndoing(false)
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
    : m_refCount(1)
    , m_undoManager(std::make_unique<DRTUndoManager>())
    , m_desktopNotifications(new DRTDesktopNotificationPresenter)
{
    m_frame.bottom = 0;
    m_frame.top = 0;
    m_frame.left = 0;
    m_frame.right = 0;
}

void UIDelegate::resetUndoManager()
{
    m_undoManager = std::make_unique<DRTUndoManager>();
}

HRESULT UIDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
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

HRESULT UIDelegate::hasCustomMenuImplementation(BOOL* hasCustomMenus)
{
    *hasCustomMenus = TRUE;

    return S_OK;
}

HRESULT UIDelegate::trackCustomPopupMenu(IWebView* /*sender*/, OLE_HANDLE /*menu*/, LPPOINT /*point*/)
{
    // Do nothing
    return S_OK;
}

HRESULT UIDelegate::registerUndoWithTarget(IWebUndoTarget* target, BSTR actionName, IUnknown* actionArg)
{
    m_undoManager->registerUndoWithTarget(target, actionName, actionArg);
    return S_OK;
}

HRESULT UIDelegate::removeAllActionsWithTarget(IWebUndoTarget*)
{
    m_undoManager->removeAllActions();
    return S_OK;
}

HRESULT UIDelegate::setActionTitle(BSTR /*actionTitle*/)
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

HRESULT UIDelegate::canUndo(BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = m_undoManager->canUndo();
    return S_OK;
}

HRESULT UIDelegate::canRedo(BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = m_undoManager->canRedo();
    return S_OK;
}

HRESULT UIDelegate::printFrame(IWebView* /*webView*/, IWebFrame* /*frame*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::ftpDirectoryTemplatePath(IWebView* /*webView*/, BSTR* path)
{
    if (!path)
        return E_POINTER;
    *path = 0;
    return E_NOTIMPL;
}


HRESULT UIDelegate::webViewHeaderHeight(IWebView* /*webView*/, float* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewFooterHeight(IWebView* /*webView*/, float* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawHeaderInRect(IWebView* /*webView*/, RECT* /*rect*/, OLE_HANDLE /*drawingContext*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawFooterInRect(IWebView* /*webView*/, RECT* /*rect*/, OLE_HANDLE /*drawingContext*/, UINT /*pageIndex*/, UINT /*pageCount*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewPrintingMarginRect(IWebView* /*webView*/, RECT* /*rect*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::canRunModal(IWebView* /*webView*/, BOOL* /*canRunBoolean*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::createModalDialog(IWebView* /*sender*/, IWebURLRequest* /*request*/, IWebView** /*newWebView*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::runModal(IWebView* /*webView*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::isMenuBarVisible(IWebView* /*webView*/, BOOL* visible)
{
    if (!visible)
        return E_POINTER;
    *visible = false;
    return E_NOTIMPL;
}

HRESULT UIDelegate::setMenuBarVisible(IWebView* /*webView*/, BOOL /*visible*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::runDatabaseSizeLimitPrompt(IWebView* /*webView*/, BSTR /*displayName*/, IWebFrame* /*initiatedByFrame*/, BOOL* allowed)
{
    if (!allowed)
        return E_POINTER;
    *allowed = false;
    return E_NOTIMPL;
}

HRESULT UIDelegate::paintCustomScrollbar(IWebView* /*webView*/, HDC /*hDC*/, RECT /*rect*/, WebScrollBarControlSize /*size*/, WebScrollbarControlState /*state*/,
    WebScrollbarControlPart /*pressedPart*/, BOOL /*vertical*/, float /*value*/, float /*proportion*/, WebScrollbarControlPartMask /*parts*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::paintCustomScrollCorner(IWebView* /*webView*/, HDC /*hDC*/, RECT /*rect*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::setFrame(IWebView* /*sender*/, RECT* frame)
{
    m_frame = *frame;
    return S_OK;
}

HRESULT UIDelegate::webViewFrame(IWebView* /*sender*/, RECT* frame)
{
    *frame = m_frame;
    return S_OK;
}

HRESULT UIDelegate::runJavaScriptAlertPanelWithMessage(IWebView* /*sender*/, BSTR message)
{
    printf("ALERT: %S\n", message ? message : L"");
    fflush(stdout);

    return S_OK;
}

HRESULT UIDelegate::runJavaScriptConfirmPanelWithMessage(IWebView* /*sender*/, BSTR message, BOOL* result)
{
    printf("CONFIRM: %S\n", message ? message : L"");
    *result = TRUE;

    return S_OK;
}

HRESULT UIDelegate::runJavaScriptTextInputPanelWithPrompt(IWebView* /*sender*/, BSTR message, BSTR defaultText, BSTR* result)
{
    printf("PROMPT: %S, default text: %S\n", message ? message : L"", defaultText ? defaultText : L"");
    *result = SysAllocString(defaultText);

    return S_OK;
}

HRESULT UIDelegate::runBeforeUnloadConfirmPanelWithMessage(IWebView* /*sender*/, BSTR message, IWebFrame* /*initiatedByFrame*/, BOOL* result)
{
    if (!result)
        return E_POINTER;
    printf("CONFIRM NAVIGATION: %S\n", message ? message : L"");
    *result = !gTestRunner->shouldStayOnPageAfterHandlingBeforeUnload();
    return S_OK;
}

HRESULT UIDelegate::webViewAddMessageToConsole(IWebView* /*sender*/, BSTR message, int lineNumber, BSTR url, BOOL isError)
{
    wstring newMessage;
    if (message) {
        newMessage = message;
        size_t fileProtocol = newMessage.find(L"file://");
        if (fileProtocol != wstring::npos)
            newMessage = newMessage.substr(0, fileProtocol) + lastPathComponent(newMessage.substr(fileProtocol));
    }

    printf("CONSOLE MESSAGE: ");
    if (lineNumber)
        printf("line %d: ", lineNumber);
    printf("%s\n", toUTF8(newMessage).c_str());
    return S_OK;
}

HRESULT UIDelegate::doDragDrop(IWebView* /*sender*/, IDataObject* object, IDropSource* source, DWORD /*okEffect*/, DWORD* performedEffect)
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

HRESULT UIDelegate::webViewGetDlgCode(IWebView* /*sender*/, UINT /*keyCode*/, LONG_PTR* code)
{
    if (!code)
        return E_POINTER;
    *code = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::createWebViewWithRequest(IWebView* /*sender*/, IWebURLRequest* /*request*/, IWebView** newWebView)
{
    if (!::gTestRunner->canOpenWindows())
        return E_FAIL;
    *newWebView = createWebViewAndOffscreenWindow();
    return S_OK;
}

HRESULT UIDelegate::webViewClose(IWebView* sender)
{
    HWND hostWindow;
    sender->hostWindow(reinterpret_cast<OLE_HANDLE*>(&hostWindow));
    DestroyWindow(hostWindow);
    return S_OK;
}

HRESULT UIDelegate::webViewFocus(IWebView* sender)
{
    HWND hostWindow;
    sender->hostWindow(reinterpret_cast<OLE_HANDLE*>(&hostWindow));
    SetForegroundWindow(hostWindow);
    return S_OK; 
}

HRESULT UIDelegate::webViewUnfocus(IWebView* /*sender*/)
{
    SetForegroundWindow(GetDesktopWindow());
    return S_OK; 
}

HRESULT UIDelegate::webViewPainted(IWebView* /*sender*/)
{
    return S_OK;
}

HRESULT UIDelegate::exceededDatabaseQuota(IWebView* sender, IWebFrame* frame, IWebSecurityOrigin* origin, BSTR databaseIdentifier)
{
    _bstr_t protocol;
    _bstr_t host;
    unsigned short port;

    origin->protocol(&protocol.GetBSTR());
    origin->host(&host.GetBSTR());
    origin->port(&port);

    if (!done && gTestRunner->dumpDatabaseCallbacks())
        printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%S, %S, %i} database:%S\n", protocol, host, port, databaseIdentifier);

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
    VARIANT var;
    detailsBag->Read(WebDatabaseUsageKey, &var, 0);
    unsigned long long expectedSize = V_UI8(&var);
    unsigned long long newQuota = defaultQuota;

    double maxQuota = gTestRunner->databaseMaxQuota();
    if (maxQuota >= 0) {
        if (defaultQuota < expectedSize && expectedSize <= maxQuota) {
            newQuota = expectedSize;
            printf("UI DELEGATE DATABASE CALLBACK: increased quota to %llu\n", newQuota);
        }
    }
    origin->setQuota(newQuota);

    return S_OK;
}

HRESULT UIDelegate::embeddedViewWithArguments(IWebView* /*sender*/, IWebFrame* /*frame*/, IPropertyBag* /*arguments*/, IWebEmbeddedView** view)
{
    if (!view)
        return E_POINTER;
    *view = 0;
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewClosing(IWebView* /*sender*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewSetCursor(IWebView* /*sender*/, OLE_HANDLE /*cursor*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::webViewDidInvalidate(IWebView* /*sender*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::setStatusText(IWebView*, BSTR text)
{ 
    if (gTestRunner->dumpStatusCallbacks())
        printf("UI DELEGATE STATUS CALLBACK: setStatusText:%S\n", text ? text : L"");
    return S_OK;
}

HRESULT UIDelegate::desktopNotificationsDelegate(IWebDesktopNotificationsDelegate** result)
{
    m_desktopNotifications.copyRefTo(result);
    return S_OK;
}

HRESULT UIDelegate::createWebViewWithRequest(IWebView* /*sender*/, IWebURLRequest* /*request*/, IPropertyBag* /*windowFeatures*/, IWebView** /*newWebView*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::drawBackground(IWebView* /*sender*/, OLE_HANDLE hdc, const RECT* dirtyRect)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::decidePolicyForGeolocationRequest(IWebView* /*sender*/, IWebFrame* frame, IWebSecurityOrigin* /*origin*/, IWebGeolocationPolicyListener* /*listener*/)
{
    return E_NOTIMPL;
}

HRESULT UIDelegate::didPressMissingPluginButton(IDOMElement* /*element*/)
{
    printf("MISSING PLUGIN BUTTON PRESSED\n");
    return S_OK;
}

