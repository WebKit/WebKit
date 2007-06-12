/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef WebInspector_h
#define WebInspector_h

#include "IWebFrameLoadDelegate.h"
#include "IWebNotificationObserver.h"
#include "IWebUIDelegate.h"

#include <wtf/OwnPtr.h>

class WebFrame;
class WebInspectorPrivate;

interface IDOMNode;

namespace WebCore {
    class String;
}

class WebInspector : IWebFrameLoadDelegate, IWebUIDelegate, IWebNotificationObserver {
public:
    static WebInspector* sharedWebInspector();

    void setWebFrame(WebFrame*);
    WebFrame* webFrame() const;

    void setRootDOMNode(IDOMNode*);
    IDOMNode* rootDOMNode() const;

    void setFocusedDOMNode(IDOMNode*);
    IDOMNode* focusedDOMNode() const;

    void setSearchQuery(const WebCore::String&);
    const WebCore::String& searchQuery() const;

    void showOptionsMenu();

    void show();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebFrameLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError( 
        /* [in] */ IWebView*,
        /* [in] */ IWebError*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveIcon( 
        /* [in] */ IWebView*,
        /* [in] */ OLE_HANDLE,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*);
    
    virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError( 
        /* [in] */ IWebView*,
        /* [in] */ IWebError*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR,
        /* [in] */ double /*delaySeconds*/,
        /* [in] */ DATE,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willCloseFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable( 
        /* [in] */ IWebView*,
        /* [in] */ JSContextRef,
        /* [in] */ JSObjectRef);

    // IWebUIDelegate
    virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest( 
        /* [in] */ IWebView*,
        /* [in] */ IWebURLRequest*,
        /* [retval][out] */ IWebView**) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewShow( 
        /* [in] */ IWebView*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewClose( 
        /* [in] */ IWebView*);
    
    virtual HRESULT STDMETHODCALLTYPE webViewFocus( 
        /* [in] */ IWebView*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewUnfocus( 
        /* [in] */ IWebView*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewFirstResponder( 
        /* [in] */ IWebView*,
        /* [retval][out] */ OLE_HANDLE*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE makeFirstResponder( 
        /* [in] */ IWebView*,
        /* [in] */ OLE_HANDLE) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setStatusText( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewStatusText( 
        /* [in] */ IWebView*,
        /* [retval][out] */ BSTR*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewAreToolbarsVisible( 
        /* [in] */ IWebView*,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setToolbarsVisible( 
        /* [in] */ IWebView*,
        /* [in] */ BOOL) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewIsStatusBarVisible( 
        /* [in] */ IWebView*,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setStatusBarVisible( 
        /* [in] */ IWebView*,
        /* [in] */ BOOL) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewIsResizable( 
        /* [in] */ IWebView*,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setResizable( 
        /* [in] */ IWebView*,
        /* [in] */ BOOL) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setFrame( 
        /* [in] */ IWebView*,
        /* [in] */ RECT*);
    
    virtual HRESULT STDMETHODCALLTYPE webViewFrame( 
        /* [in] */ IWebView*,
        /* [retval][out] */ RECT*);
    
    virtual HRESULT STDMETHODCALLTYPE setContentRect( 
        /* [in] */ IWebView*,
        /* [in] */ RECT*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewContentRect( 
        /* [in] */ IWebView*,
        /* [retval][out] */ RECT*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR);
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptConfirmPanelWithMessage( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptTextInputPanelWithPrompt( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR /*message*/,
        /* [in] */ BSTR /*defaultText*/,
        /* [retval][out] */ BSTR*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runBeforeUnloadConfirmPanelWithMessage( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR /*message*/,
        /* [in] */ IWebFrame*  /*initiatedByFrame*/,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runOpenPanelForFileButtonWithResultListener( 
        /* [in] */ IWebView*,
        /* [in] */ IWebOpenPanelResultListener*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE mouseDidMoveOverElement( 
        /* [in] */ IWebView*,
        /* [in] */ IPropertyBag*,
        /* [in] */ UINT /*modifierFlags*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemsForElement( 
        /* [in] */ IWebView*,
        /* [in] */ IPropertyBag*,
        /* [in] */ OLE_HANDLE,
        /* [retval][out] */ OLE_HANDLE*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE validateUserInterfaceItem( 
        /* [in] */ IWebView*,
        /* [in] */ UINT,
        /* [in] */ BOOL /*defaultValidation*/,
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE shouldPerformAction( 
        /* [in] */ IWebView*,
        /* [in] */ UINT /*itemCommandID*/,
        /* [in] */ UINT /*sender*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE dragDestinationActionMaskForDraggingInfo( 
        /* [in] */ IWebView*,
        /* [in] */ IDataObject*,
        /* [retval][out] */ WebDragDestinationAction*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformDragDestinationAction( 
        /* [in] */ IWebView*,
        /* [in] */ WebDragDestinationAction,
        /* [in] */ IDataObject*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE dragSourceActionMaskForPoint( 
        /* [in] */ IWebView*,
        /* [in] */ LPPOINT,
        /* [retval][out] */ WebDragSourceAction*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformDragSourceAction( 
        /* [in] */ IWebView*,
        /* [in] */ WebDragSourceAction,
        /* [in] */ LPPOINT,
        /* [in] */ IDataObject*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemSelected( 
        /* [in] */ IWebView*,
        /* [in] */ void*  /*item*/,
        /* [in] */ IPropertyBag*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE hasCustomMenuImplementation( 
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE trackCustomPopupMenu( 
        /* [in] */ IWebView*,
        /* [in] */ OLE_HANDLE,
        /* [in] */ LPPOINT) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE measureCustomMenuItem( 
        /* [in] */ IWebView*,
        /* [in] */ void*  /*measureItem*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE drawCustomMenuItem( 
        /* [in] */ IWebView*,
        /* [in] */ void*  /*drawItem*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE addCustomMenuDrawingData( 
        /* [in] */ IWebView*,
        /* [in] */ OLE_HANDLE) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE cleanUpCustomMenuDrawingData( 
        /* [in] */ IWebView*,
        /* [in] */ OLE_HANDLE) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE canTakeFocus( 
        /* [in] */ IWebView*,
        /* [in] */ BOOL /*forward*/,
        /* [out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE takeFocus( 
        /* [in] */ IWebView*,
        /* [in] */ BOOL /*forward*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE registerUndoWithTarget( 
        /* [in] */ IWebUndoTarget*,
        /* [in] */ BSTR /*actionName*/,
        /* [in] */ IUnknown*  /*actionArg*/) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE removeAllActionsWithTarget( 
        /* [in] */ IWebUndoTarget*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setActionTitle( 
        /* [in] */ BSTR) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE undo( void) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE redo( void) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE canUndo( 
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE canRedo( 
        /* [retval][out] */ BOOL*) { return E_NOTIMPL; }

    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify(IWebNotification*);

private:
    LRESULT onDestroy(WPARAM, LPARAM);
    LRESULT onSize(WPARAM, LPARAM);
    LRESULT onActivateApp(WPARAM, LPARAM);
    LRESULT onCommand(WPARAM, LPARAM);
    LRESULT onLButtonDown(WPARAM, LPARAM);
    LRESULT onLButtonUp(WPARAM, LPARAM);
    LRESULT onMouseMove(WPARAM, LPARAM);
    LRESULT handleMessageSentToWebView(UINT message, WPARAM, LPARAM);
    

    void inspectedWebViewProgressFinished(IWebNotification*);
    void webFrameDetached(WebFrame*);
    void highlightNode(IDOMNode*);
    void update();
    void updateRoot();
    void updateSystemColors();
    void toggleIgnoreWhitespace();
    void toggleShowUserAgentStyles();

    // FIXME: Right now we only support the sharedWebInspector, so the constructor/destructor need to be private.
    // Eventually we should support multiple inspectors, and at that point these may need to become public.
    WebInspector();
    ~WebInspector();

    OwnPtr<WebInspectorPrivate> m_private;

    friend static LRESULT CALLBACK WebInspectorWndProc(HWND, UINT, WPARAM, LPARAM);
    friend static LRESULT CALLBACK SubclassedWebViewWndProc(HWND, UINT, WPARAM, LPARAM);
};

#endif // !defined(WebInspector_h)
