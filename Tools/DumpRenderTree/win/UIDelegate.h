/*
 * Copyright (C) 2005-2007, 2014-2015 Apple Inc.  All rights reserved.
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

#ifndef UIDelegate_h
#define UIDelegate_h

#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>
#include <windef.h>

class DRTUndoManager;
class DRTDesktopNotificationPresenter;

class UIDelegate final : public IWebUIDelegate2, IWebUIDelegatePrivate3 {
public:
    UIDelegate();

    void resetUndoManager();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebUIDelegate
    virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest(_In_opt_ IWebView* sender, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE webViewShow(_In_opt_ IWebView* sender) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewClose(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE webViewFocus(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE webViewUnfocus(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE webViewFirstResponder(_In_opt_ IWebView*, _Out_ HWND*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE makeFirstResponder(_In_opt_ IWebView*, _In_ HWND) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setStatusText(_In_opt_ IWebView* sender, _In_ BSTR text);
    virtual HRESULT STDMETHODCALLTYPE webViewStatusText(_In_opt_ IWebView* sender, __deref_opt_out BSTR* text) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewAreToolbarsVisible(_In_opt_ IWebView* sender, _Out_ BOOL* visible) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setToolbarsVisible(_In_opt_ IWebView* sender, BOOL visible) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewIsStatusBarVisible(_In_opt_ IWebView* sender, _Out_ BOOL* visible) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setStatusBarVisible(_In_opt_ IWebView* sender, BOOL visible) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewIsResizable(_In_opt_ IWebView* sender, _Out_ BOOL *resizable) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setResizable(_In_opt_ IWebView* sender, BOOL resizable) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setFrame(_In_opt_ IWebView* sender, _In_ RECT* frame);
    virtual HRESULT STDMETHODCALLTYPE webViewFrame(_In_opt_ IWebView* sender, _Out_ RECT* frame);
    virtual HRESULT STDMETHODCALLTYPE setContentRect(_In_opt_ IWebView* sender, _In_ RECT* contentRect) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewContentRect(_In_opt_ IWebView* sender, _Out_ RECT *contentRect) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage(_In_opt_ IWebView* sender, _In_ BSTR message);
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptConfirmPanelWithMessage(_In_opt_ IWebView* sender, _In_ BSTR message, _Out_ BOOL *result);
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptTextInputPanelWithPrompt(_In_opt_ IWebView* sender, _In_ BSTR message, _In_ BSTR defaultText, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE runBeforeUnloadConfirmPanelWithMessage(_In_opt_ IWebView* sender, _In_ BSTR message, _In_opt_ IWebFrame* initiatedByFrame, _Out_ BOOL *result);
    virtual HRESULT STDMETHODCALLTYPE runOpenPanelForFileButtonWithResultListener(_In_opt_ IWebView* sender, _In_opt_ IWebOpenPanelResultListener*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE mouseDidMoveOverElement(_In_opt_ IWebView* sender, _In_opt_ IPropertyBag* elementInformation, UINT modifierFlags) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemsForElement(IWebView*, IPropertyBag*, HMENU, HMENU*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE validateUserInterfaceItem(_In_opt_ IWebView*, UINT itemCommandID, BOOL defaultValidation, _Out_ BOOL *isValid) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE shouldPerformAction(_In_opt_ IWebView*, UINT itemCommandID, UINT sender) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE dragDestinationActionMaskForDraggingInfo(_In_opt_ IWebView*, _In_opt_ IDataObject* draggingInfo, _Out_ WebDragDestinationAction* action) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE willPerformDragDestinationAction(_In_opt_ IWebView*, WebDragDestinationAction action, _In_opt_ IDataObject* draggingInfo) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE dragSourceActionMaskForPoint(_In_opt_ IWebView*, _In_ LPPOINT, _Out_ WebDragSourceAction*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE willPerformDragSourceAction(_In_opt_ IWebView*, WebDragSourceAction, _In_ LPPOINT, _In_opt_ IDataObject* pasteboard, _COM_Outptr_opt_ IDataObject** newPasteboard);
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemSelected(_In_opt_ IWebView* sender, _In_opt_ void* item, _In_opt_ IPropertyBag* element) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE hasCustomMenuImplementation(_Out_ BOOL* hasCustomMenus);
    virtual HRESULT STDMETHODCALLTYPE trackCustomPopupMenu(_In_opt_ IWebView* sender, _In_ HMENU, _In_ LPPOINT);
    virtual HRESULT STDMETHODCALLTYPE measureCustomMenuItem(_In_opt_ IWebView* sender, _In_ void* measureItem) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE drawCustomMenuItem(_In_opt_ IWebView* sender, _In_ void* drawItem) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE addCustomMenuDrawingData(_In_opt_ IWebView*, HMENU) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE cleanUpCustomMenuDrawingData(_In_opt_ IWebView*, HMENU) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE canTakeFocus(_In_opt_ IWebView* sender, BOOL forward, _Out_ BOOL* result) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE takeFocus(_In_opt_ IWebView* sender, BOOL forward) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE registerUndoWithTarget(_In_opt_ IWebUndoTarget*, _In_ BSTR actionName, _In_opt_ IUnknown* actionArg);
    virtual HRESULT STDMETHODCALLTYPE removeAllActionsWithTarget(_In_opt_ IWebUndoTarget*);
    virtual HRESULT STDMETHODCALLTYPE setActionTitle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE undo();
    virtual HRESULT STDMETHODCALLTYPE redo();
    virtual HRESULT STDMETHODCALLTYPE canUndo(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE canRedo(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE printFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE ftpDirectoryTemplatePath(_In_opt_ IWebView*, __deref_opt_out BSTR* path);
    virtual HRESULT STDMETHODCALLTYPE webViewHeaderHeight(_In_opt_ IWebView*, _Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE webViewFooterHeight(_In_opt_ IWebView*, _Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE drawHeaderInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR drawingContext);
    virtual HRESULT STDMETHODCALLTYPE drawFooterInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR drawingContext, UINT pageIndex, UINT pageCount);
    virtual HRESULT STDMETHODCALLTYPE webViewPrintingMarginRect(_In_opt_ IWebView*, _Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE canRunModal(_In_opt_ IWebView*, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE createModalDialog(_In_opt_ IWebView* sender, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE runModal(_In_opt_ IWebView*);
    virtual HRESULT STDMETHODCALLTYPE isMenuBarVisible(_In_opt_ IWebView*, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMenuBarVisible(_In_opt_ IWebView*, BOOL);
    virtual HRESULT STDMETHODCALLTYPE runDatabaseSizeLimitPrompt(_In_opt_ IWebView*, _In_ BSTR displayName, _In_opt_ IWebFrame* initiatedByFrame, _Out_ BOOL* allowed);
    virtual HRESULT STDMETHODCALLTYPE paintCustomScrollbar(_In_opt_ IWebView*, _In_ HDC, RECT, WebScrollBarControlSize, WebScrollbarControlState,
        WebScrollbarControlPart pressedPart, BOOL vertical, float value, float proportion, WebScrollbarControlPartMask);
    virtual HRESULT STDMETHODCALLTYPE paintCustomScrollCorner(_In_opt_ IWebView*, _In_ HDC, RECT);
    virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest(_In_opt_ IWebView* sender, _In_opt_ IWebURLRequest*, _In_opt_ IPropertyBag* windowFeatures, _COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE drawBackground(_In_opt_ IWebView* sender, _In_ HDC, _In_ const RECT* dirtyRect);
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForGeolocationRequest(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*, _In_opt_ IWebSecurityOrigin*, _In_opt_ IWebGeolocationPolicyListener*);
    virtual HRESULT STDMETHODCALLTYPE didPressMissingPluginButton(_In_opt_ IDOMElement*);

protected:
    // IWebUIDelegatePrivate
    virtual HRESULT STDMETHODCALLTYPE unused1() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE unused2() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE unused3() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewScrolled(_In_opt_ IWebView* sender) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewAddMessageToConsole(_In_opt_ IWebView* sender, _In_ BSTR message, int lineNumber, _In_ BSTR url, BOOL isError);
    virtual HRESULT STDMETHODCALLTYPE webViewShouldInterruptJavaScript(_In_opt_ IWebView*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewReceivedFocus(_In_opt_ IWebView* sender) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewLostFocus(_In_opt_ IWebView*, _In_ HWND) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE doDragDrop(_In_opt_ IWebView* sender, _In_opt_ IDataObject*, _In_opt_ IDropSource*, DWORD okEffect, _Out_ DWORD* performedEffect);
    virtual HRESULT STDMETHODCALLTYPE webViewGetDlgCode(_In_opt_ IWebView* sender, UINT keyCode, _Out_ LONG_PTR* code);
    virtual HRESULT STDMETHODCALLTYPE webViewPainted(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE exceededDatabaseQuota(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*, _In_opt_ IWebSecurityOrigin*, _In_ BSTR databaseIdentifier);
    virtual HRESULT STDMETHODCALLTYPE embeddedViewWithArguments(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*, _In_opt_ IPropertyBag* arguments, _COM_Outptr_opt_ IWebEmbeddedView**);
    virtual HRESULT STDMETHODCALLTYPE webViewClosing(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE webViewSetCursor(_In_opt_ IWebView* sender, _In_ HCURSOR);
    virtual HRESULT STDMETHODCALLTYPE webViewDidInvalidate(_In_opt_ IWebView* sender);
    virtual HRESULT STDMETHODCALLTYPE desktopNotificationsDelegate(_COM_Outptr_opt_ IWebDesktopNotificationsDelegate**);

    ULONG m_refCount { 1 };

private:
    RECT m_frame;
    std::unique_ptr<DRTUndoManager> m_undoManager;

    COMPtr<IWebDesktopNotificationsDelegate> m_desktopNotifications;
    HWND m_modalDialogParent { nullptr };
};

#endif
