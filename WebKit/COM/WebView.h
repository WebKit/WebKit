/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef WebView_H
#define WebView_H

#include "IWebView.h"
#include "WebFrame.h"

class WebFrame;
class WebBackForwardList;

class WebView : public IWebView, public IWebViewExt, public IWebIBActions, public IWebViewCSS, public IWebViewEditing, public IWebViewUndoableEditing, public IWebViewEditingActions
{
public:
    static WebView* createInstance();
protected:
    WebView();
    ~WebView();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebView

    virtual HRESULT STDMETHODCALLTYPE canShowMIMEType( 
        /* [in] */ BSTR mimeType,
        /* [retval][out] */ BOOL *canShow);
    
    virtual HRESULT STDMETHODCALLTYPE canShowMIMETypeAsHTML( 
        /* [in] */ BSTR mimeType,
        /* [retval][out] */ BOOL *canShow);
    
    virtual HRESULT STDMETHODCALLTYPE MIMETypesShownAsHTML( 
        /* [out] */ int *count,
        /* [retval][out] */ BSTR **mimeTypes);
    
    virtual HRESULT STDMETHODCALLTYPE setMIMETypesShownAsHTML( 
        /* [size_is][in] */ BSTR *mimeTypes,
        /* [in] */ int cMimeTypes);
    
    virtual HRESULT STDMETHODCALLTYPE URLFromPasteboard( 
        /* [in] */ IDataObject *pasteboard,
        /* [retval][out] */ BSTR *url);
    
    virtual HRESULT STDMETHODCALLTYPE URLTitleFromPasteboard( 
        /* [in] */ IDataObject *pasteboard,
        /* [retval][out] */ BSTR *urlTitle);
    
    virtual HRESULT STDMETHODCALLTYPE initWithFrame( 
        /* [in] */ RECT *frame,
        /* [in] */ BSTR frameName,
        /* [in] */ BSTR groupName);
    
    virtual HRESULT STDMETHODCALLTYPE setUIDelegate( 
        /* [in] */ IWebUIDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE uiDelegate( 
        /* [out][retval] */ IWebUIDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setResourceLoadDelegate( 
        /* [in] */ IWebResourceLoadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE resourceLoadDelegate( 
        /* [out][retval] */ IWebResourceLoadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setDownloadDelegate( 
        /* [in] */ IWebDownloadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE downloadDelegate( 
        /* [out][retval] */ IWebDownloadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setFrameLoadDelegate( 
        /* [in] */ IWebFrameLoadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE frameLoadDelegate( 
        /* [out][retval] */ IWebFrameLoadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setPolicyDelegate( 
        /* [in] */ IWebPolicyDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE policyDelegate( 
        /* [out][retval] */ IWebPolicyDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrame( 
        /* [out][retval] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE backForwardList( 
        /* [out][retval] */ IWebBackForwardList **list);
    
    virtual HRESULT STDMETHODCALLTYPE setMaintainsBackForwardList( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE goBack( 
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE goForward( 
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE goToBackForwardItem( 
        /* [in] */ IWebHistoryItem *item,
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE setTextSizeMultiplier( 
        /* [in] */ float multiplier);
    
    virtual HRESULT STDMETHODCALLTYPE textSizeMultiplier( 
        /* [retval][out] */ float *multiplier);
    
    virtual HRESULT STDMETHODCALLTYPE setApplicationNameForUserAgent( 
        /* [in] */ BSTR applicationName);
    
    virtual HRESULT STDMETHODCALLTYPE applicationNameForUserAgent( 
        /* [retval][out] */ BSTR *applicationName);
    
    virtual HRESULT STDMETHODCALLTYPE setCustomUserAgent( 
        /* [in] */ BSTR userAgentString);
    
    virtual HRESULT STDMETHODCALLTYPE customUserAgent( 
        /* [retval][out] */ BSTR *userAgentString);
    
    virtual HRESULT STDMETHODCALLTYPE userAgentForURL( 
        /* [in] */ BSTR url,
        /* [retval][out] */ BSTR *userAgent);
    
    virtual HRESULT STDMETHODCALLTYPE supportsTextEncoding( 
        /* [retval][out] */ BOOL *supports);
    
    virtual HRESULT STDMETHODCALLTYPE setCustomTextEncodingName( 
        /* [in] */ BSTR encodingName);
    
    virtual HRESULT STDMETHODCALLTYPE customTextEncodingName( 
        /* [retval][out] */ BSTR *encodingName);
    
    virtual HRESULT STDMETHODCALLTYPE setMediaStyle( 
        /* [in] */ BSTR media);
    
    virtual HRESULT STDMETHODCALLTYPE mediaStyle( 
        /* [retval][out] */ BSTR *media);
    
    virtual HRESULT STDMETHODCALLTYPE stringByEvaluatingJavaScriptFromString( 
        /* [in] */ BSTR script,
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE windowScriptObject( 
        /* [retval][out] */ IWebScriptObject *webScriptObject);
    
    virtual HRESULT STDMETHODCALLTYPE setPreferences( 
        /* [in] */ IWebPreferences *prefs);
    
    virtual HRESULT STDMETHODCALLTYPE preferences( 
        /* [retval][out] */ IWebPreferences **prefs);
    
    virtual HRESULT STDMETHODCALLTYPE setPreferencesIdentifier( 
        /* [in] */ BSTR anIdentifier);
    
    virtual HRESULT STDMETHODCALLTYPE preferencesIdentifier( 
        /* [retval][out] */ BSTR *anIdentifier);
    
    virtual HRESULT STDMETHODCALLTYPE setHostWindow( 
        /* [in] */ HWND window);
    
    virtual HRESULT STDMETHODCALLTYPE hostWindow( 
        /* [retval][out] */ HWND *window);
    
    virtual HRESULT STDMETHODCALLTYPE searchFor( 
        /* [in] */ BSTR str,
        /* [in] */ BOOL forward,
        /* [in] */ BOOL caseFlag,
        /* [in] */ BOOL wrapFlag,
        /* [retval][out] */ BOOL *found);
    
    virtual HRESULT STDMETHODCALLTYPE registerViewClass( 
        /* [in] */ IWebDocumentView *view,
        /* [in] */ IWebDocumentRepresentation *representation,
        /* [in] */ BSTR forMIMEType);

    // IWebIBActions

    virtual HRESULT STDMETHODCALLTYPE takeStringURLFrom( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE stopLoading( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE reload( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canGoBack( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE goBack( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canGoForward( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE goForward( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canMakeTextLarger( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE makeTextLarger( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canMakeTextSmaller( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE makeTextSmaller( 
        /* [in] */ IUnknown *sender);

    // IWebViewCSS

    virtual HRESULT STDMETHODCALLTYPE computedStyleForElement( 
        /* [in] */ IDOMElement *element,
        /* [in] */ BSTR pseudoElement,
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);

    // IWebViewEditing

    virtual HRESULT STDMETHODCALLTYPE editableDOMRangeForPoint( 
        /* [in] */ LPPOINT point,
        /* [retval][out] */ IDOMRange **range);
    
    virtual HRESULT STDMETHODCALLTYPE setSelectedDOMRange( 
        /* [in] */ IDOMRange *range,
        /* [in] */ WebSelectionAffinity affinity);
    
    virtual HRESULT STDMETHODCALLTYPE selectedDOMRange( 
        /* [retval][out] */ IDOMRange **range);
    
    virtual HRESULT STDMETHODCALLTYPE selectionAffinity( 
        /* [retval][out][retval][out] */ WebSelectionAffinity *affinity);
    
    virtual HRESULT STDMETHODCALLTYPE setEditable( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE isEditable( 
        /* [retval][out] */ BOOL *isEditable);
    
    virtual HRESULT STDMETHODCALLTYPE setTypingStyle( 
        /* [in] */ IDOMCSSStyleDeclaration *style);
    
    virtual HRESULT STDMETHODCALLTYPE typingStyle( 
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);
    
    virtual HRESULT STDMETHODCALLTYPE setSmartInsertDeleteEnabled( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE smartInsertDeleteEnabled( 
        /* [in] */ BOOL enabled);
    
    virtual HRESULT STDMETHODCALLTYPE setContinuousSpellCheckingEnabled( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE isContinuousSpellCheckingEnabled( 
        /* [retval][out] */ BOOL *enabled);
    
    virtual HRESULT STDMETHODCALLTYPE spellCheckerDocumentTag( 
        /* [retval][out] */ int *tag);
    
    virtual HRESULT STDMETHODCALLTYPE undoManager( 
        /* [retval][out] */ IWebUndoManager *manager);
    
    virtual HRESULT STDMETHODCALLTYPE setEditingDelegate( 
        /* [in] */ IWebViewEditingDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE editingDelegate( 
        /* [retval][out] */ IWebViewEditingDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE styleDeclarationWithText( 
        /* [in] */ BSTR text,
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);

    // IWebViewUndoableEditing

    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithNode( 
        /* [in] */ IDOMNode *node);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithText( 
        /* [in] */ BSTR text);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithMarkupString( 
        /* [in] */ BSTR markupString);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithArchive( 
        /* [in] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE deleteSelection( void);
    
    virtual HRESULT STDMETHODCALLTYPE applyStyle( 
        /* [in] */ IDOMCSSStyleDeclaration *style);

    // IWebViewEditingActions

    virtual HRESULT STDMETHODCALLTYPE copy( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE cut( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE paste( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE copyFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE delete_( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteAsPlainText( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteAsRichText( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeAttributes( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeDocumentBackgroundColor( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeColor( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignCenter( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignJustified( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignLeft( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignRight( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE checkSpelling( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE showGuessPanel( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE performFindPanelAction( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE startSpeaking( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE stopSpeaking( 
        /* [in] */ IUnknown *sender);

    // IWebViewExt
    
    virtual HRESULT STDMETHODCALLTYPE viewWindow( 
        /* [retval][out] */ HWND *window);

    // WebView

    void mouseMoved(WPARAM, LPARAM);
    void mouseDown(WPARAM, LPARAM);
    void mouseUp(WPARAM, LPARAM);
    void mouseDoubleClick(WPARAM, LPARAM);
    bool keyPress(WPARAM, LPARAM);
    HRESULT goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);

protected:
    ULONG m_refCount;
    RECT m_frame;
    BSTR m_frameName;
    BSTR m_groupName;
    HWND m_hostWindow;
    HWND m_viewWindow;
    WebFrame* m_mainFrame;
    IWebFrameLoadDelegate* m_frameLoadDelegate;
    IWebUIDelegate* m_uiDelegate;
    IWebBackForwardList* m_backForwardList;
};

#endif
