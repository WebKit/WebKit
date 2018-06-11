/*
 * Copyright (C) 2009, 2014-2015 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Brent Fulgham. All Rights Reserved.
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

#ifndef PrintWebUIDelegate_h
#define PrintWebUIDelegate_h

#include <WebKitLegacy/WebKit.h>

class WebKitLegacyBrowserWindow;

class PrintWebUIDelegate : public IWebUIDelegate {
public:
    PrintWebUIDelegate(WebKitLegacyBrowserWindow& client)
        : m_client(client) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE webViewShow(_In_opt_ IWebView*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewClose(_In_opt_ IWebView*);
    virtual HRESULT STDMETHODCALLTYPE webViewFocus(_In_opt_ IWebView*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewUnfocus(_In_opt_ IWebView*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewFirstResponder(_In_opt_ IWebView*, _Out_ HWND*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE makeFirstResponder(_In_opt_ IWebView*, _In_ HWND) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setStatusText(_In_opt_ IWebView*, _In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewStatusText(_In_opt_ IWebView*, __deref_opt_out BSTR*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewAreToolbarsVisible(_In_opt_ IWebView*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setToolbarsVisible(_In_opt_ IWebView*, BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewIsStatusBarVisible(_In_opt_ IWebView*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setStatusBarVisible(_In_opt_ IWebView*, BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewIsResizable(_In_opt_ IWebView*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setResizable(_In_opt_ IWebView*, BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setFrame(_In_opt_ IWebView*, _In_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE webViewFrame(_In_opt_ IWebView*, _Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE setContentRect(_In_opt_ IWebView*, _In_ RECT*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewContentRect(_In_opt_ IWebView*, _Out_ RECT*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage(_In_opt_ IWebView*, _In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptConfirmPanelWithMessage(_In_opt_ IWebView*, _In_ BSTR, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptTextInputPanelWithPrompt(_In_opt_ IWebView*, _In_ BSTR, _In_ BSTR, __deref_opt_out BSTR*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE runBeforeUnloadConfirmPanelWithMessage(_In_opt_ IWebView*, _In_ BSTR, IWebFrame*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE runOpenPanelForFileButtonWithResultListener(_In_opt_ IWebView*, _In_opt_ IWebOpenPanelResultListener*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE mouseDidMoveOverElement(_In_opt_ IWebView*, _In_opt_ IPropertyBag*, UINT) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemsForElement(_In_opt_ IWebView*, _In_opt_ IPropertyBag*, _In_ HMENU, _Out_ HMENU*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE validateUserInterfaceItem(_In_opt_ IWebView*, UINT, BOOL, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE shouldPerformAction(_In_opt_ IWebView*, UINT, UINT) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE dragDestinationActionMaskForDraggingInfo(_In_opt_ IWebView*, _In_opt_ IDataObject*, _In_opt_ WebDragDestinationAction*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE willPerformDragDestinationAction(_In_opt_ IWebView*, WebDragDestinationAction, _In_opt_ IDataObject*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE dragSourceActionMaskForPoint(_In_opt_ IWebView*, _In_ LPPOINT, _Out_ WebDragSourceAction*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE willPerformDragSourceAction(_In_opt_ IWebView*, WebDragSourceAction, _In_ LPPOINT, _In_opt_ IDataObject*, _COM_Outptr_opt_ IDataObject**);
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemSelected(_In_opt_ IWebView*, void*, IPropertyBag*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE hasCustomMenuImplementation(_Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE trackCustomPopupMenu(_In_opt_ IWebView*, _In_ HMENU, _In_ LPPOINT) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE measureCustomMenuItem(IWebView*, void*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE drawCustomMenuItem(IWebView*, void*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE addCustomMenuDrawingData(_In_opt_ IWebView*, _In_ HMENU) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE cleanUpCustomMenuDrawingData(_In_opt_ IWebView*, _In_ HMENU) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE canTakeFocus(_In_opt_ IWebView*, BOOL, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE takeFocus(_In_opt_ IWebView*, BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE registerUndoWithTarget(_In_opt_ IWebUndoTarget*, _In_ BSTR, _In_opt_ IUnknown*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE removeAllActionsWithTarget(_In_opt_ IWebUndoTarget*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setActionTitle(_In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE undo() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE redo() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE canUndo(_Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE canRedo(_Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE printFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE ftpDirectoryTemplatePath(_In_opt_ IWebView*, __deref_opt_out BSTR*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webViewHeaderHeight(_In_opt_ IWebView*, _Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE webViewFooterHeight(_In_opt_ IWebView*, _Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE drawHeaderInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR);
    virtual HRESULT STDMETHODCALLTYPE drawFooterInRect(_In_opt_ IWebView*, _In_ RECT*, ULONG_PTR, UINT, UINT);
    virtual HRESULT STDMETHODCALLTYPE webViewPrintingMarginRect(_In_opt_ IWebView*, _Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE canRunModal(_In_opt_ IWebView*, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE createModalDialog(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE runModal(_In_opt_ IWebView*);
    virtual HRESULT STDMETHODCALLTYPE isMenuBarVisible(_In_opt_ IWebView*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setMenuBarVisible(_In_opt_ IWebView*, BOOL) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE runDatabaseSizeLimitPrompt(_In_opt_ IWebView*, _In_ BSTR, _In_opt_ IWebFrame*, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE paintCustomScrollbar(_In_opt_ IWebView*, _In_ HDC, RECT, WebScrollBarControlSize, WebScrollbarControlState, WebScrollbarControlPart, BOOL, float, float, WebScrollbarControlPartMask) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE paintCustomScrollCorner(_In_opt_ IWebView*, _In_ HDC, RECT) { return E_NOTIMPL; }

private:
    WebKitLegacyBrowserWindow& m_client;
    HWND m_modalDialogParent { nullptr };
};

#endif
