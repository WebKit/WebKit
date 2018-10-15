/*
 * Copyright (C) 2006-2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2011 Brent Fulgham. All rights reserved.
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
 * EXPRESS OR IMPLIED WARRANTIES, INCfLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABIuLITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "WebKit.h"
#include "WebFrame.h"
#include "WebPreferences.h"
#include <WebCore/COMPtr.h>
#include <WebCore/DragActions.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/IntRect.h>
#include <WebCore/SharedGDIObject.h>
#include <WebCore/SuspendableTimer.h>
#include <WebCore/WindowMessageListener.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

#if USE(CA)
#include <WebCore/CACFLayerTreeHostClient.h>
#endif

#if ENABLE(FULLSCREEN_API)
#include <WebCore/FullScreenControllerClient.h>
#endif

namespace WebCore {
#if USE(CA)
    class CACFLayerTreeHost;
#endif
    class FullScreenController;
#if PLATFORM(WIN) && USE(AVFOUNDATION)
    struct GraphicsDeviceAdapter;
#endif
    class HTMLVideoElement;
    class HistoryItem;
    class KeyboardEvent;
}

class FullscreenVideoController;
class WebBackForwardList;
class WebFrame;
class WebInspector;
class WebInspectorClient;
#if USE(TEXTURE_MAPPER_GL)
class AcceleratedCompositingContext;
#endif
class WebViewGroup;

WebView* kit(WebCore::Page*);
WebCore::Page* core(IWebView*);

interface IDropTargetHelper;
#if USE(DIRECT2D)
interface ID2D1Bitmap;
interface ID2D1BitmapRenderTarget;
interface ID2D1GdiInteropRenderTarget;
interface ID2D1HwndRenderTarget;
interface ID2D1RenderTarget;
#endif

class WebView 
    : public IWebView
    , public IWebViewPrivate5
    , public IWebIBActions
    , public IWebViewCSS
    , public IWebViewEditing
    , public IWebViewUndoableEditing
    , public IWebViewEditingActions
    , public IWebNotificationObserver
    , public IDropTarget
    , WebCore::WindowMessageListener
    , WebCore::GraphicsLayerClient
#if USE(CA)
    , WebCore::CACFLayerTreeHostClient
#endif
#if ENABLE(FULLSCREEN_API)
    , WebCore::FullScreenControllerClient
#endif
{
public:
    static WebView* createInstance();
protected:
    WebView();
    ~WebView();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebView
    virtual HRESULT STDMETHODCALLTYPE canShowMIMEType(_In_ BSTR mimeType, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE canShowMIMETypeAsHTML(_In_ BSTR mimeType, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE MIMETypesShownAsHTML(_COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE setMIMETypesShownAsHTML(__inout_ecount_full(cMimeTypes) BSTR* mimeTypes, int cMimeTypes); 
    virtual HRESULT STDMETHODCALLTYPE URLFromPasteboard(_In_opt_ IDataObject* pasteboard, _Deref_opt_out_ BSTR* url);
    virtual HRESULT STDMETHODCALLTYPE URLTitleFromPasteboard(_In_opt_ IDataObject* pasteboard, _Deref_opt_out_ BSTR* urlTitle);
    virtual HRESULT STDMETHODCALLTYPE initWithFrame(RECT frame, _In_ BSTR frameName, _In_ BSTR groupName);
    virtual HRESULT STDMETHODCALLTYPE setAccessibilityDelegate(_In_opt_ IAccessibilityDelegate*);
    virtual HRESULT STDMETHODCALLTYPE accessibilityDelegate(_COM_Outptr_opt_ IAccessibilityDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setUIDelegate(_In_opt_ IWebUIDelegate*);
    virtual HRESULT STDMETHODCALLTYPE uiDelegate(_COM_Outptr_opt_ IWebUIDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setResourceLoadDelegate(_In_opt_ IWebResourceLoadDelegate*);
    virtual HRESULT STDMETHODCALLTYPE resourceLoadDelegate(_COM_Outptr_opt_ IWebResourceLoadDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setDownloadDelegate(_In_opt_ IWebDownloadDelegate*);
    virtual HRESULT STDMETHODCALLTYPE downloadDelegate(_COM_Outptr_opt_ IWebDownloadDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setFrameLoadDelegate(_In_opt_ IWebFrameLoadDelegate*);
    virtual HRESULT STDMETHODCALLTYPE frameLoadDelegate(_COM_Outptr_opt_ IWebFrameLoadDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setPolicyDelegate(_In_opt_ IWebPolicyDelegate*);
    virtual HRESULT STDMETHODCALLTYPE policyDelegate(_COM_Outptr_opt_ IWebPolicyDelegate**);
    virtual HRESULT STDMETHODCALLTYPE mainFrame(_COM_Outptr_opt_ IWebFrame**);
    virtual HRESULT STDMETHODCALLTYPE focusedFrame(_COM_Outptr_opt_ IWebFrame**);
    virtual HRESULT STDMETHODCALLTYPE backForwardList(_COM_Outptr_opt_ IWebBackForwardList**);
    virtual HRESULT STDMETHODCALLTYPE setMaintainsBackForwardList(BOOL);
    virtual HRESULT STDMETHODCALLTYPE goBack(_Out_ BOOL* succeeded);
    virtual HRESULT STDMETHODCALLTYPE goForward(_Out_ BOOL* succeeded);
    virtual HRESULT STDMETHODCALLTYPE goToBackForwardItem(_In_opt_ IWebHistoryItem*, _Out_ BOOL* succeeded);
    virtual HRESULT STDMETHODCALLTYPE setTextSizeMultiplier(float);
    virtual HRESULT STDMETHODCALLTYPE textSizeMultiplier(_Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE setApplicationNameForUserAgent(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE applicationNameForUserAgent(_Deref_opt_out_ BSTR*);  
    virtual HRESULT STDMETHODCALLTYPE setCustomUserAgent(_In_ BSTR); 
    virtual HRESULT STDMETHODCALLTYPE customUserAgent(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE userAgentForURL(_In_ BSTR url, _Deref_opt_out_ BSTR* userAgent); 
    virtual HRESULT STDMETHODCALLTYPE supportsTextEncoding(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCustomTextEncodingName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE customTextEncodingName(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setMediaStyle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE mediaStyle(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE stringByEvaluatingJavaScriptFromString(_In_ BSTR script, _Deref_opt_out_ BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE windowScriptObject(_COM_Outptr_opt_ IWebScriptObject**);
    virtual HRESULT STDMETHODCALLTYPE setPreferences(_In_opt_ IWebPreferences*);
    virtual HRESULT STDMETHODCALLTYPE preferences(_COM_Outptr_opt_ IWebPreferences**);
    virtual HRESULT STDMETHODCALLTYPE setPreferencesIdentifier(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE preferencesIdentifier(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setHostWindow(_In_ HWND);
    virtual HRESULT STDMETHODCALLTYPE hostWindow(_Deref_opt_out_ HWND*);
    virtual HRESULT STDMETHODCALLTYPE searchFor(_In_ BSTR, BOOL forward, BOOL caseFlag, BOOL wrapFlag, _Out_ BOOL *found);
    virtual HRESULT STDMETHODCALLTYPE registerViewClass(_In_opt_ IWebDocumentView*, _In_opt_ IWebDocumentRepresentation*, _In_ BSTR forMIMEType);
    virtual HRESULT STDMETHODCALLTYPE setGroupName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE groupName(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE estimatedProgress(_Out_ double*);
    virtual HRESULT STDMETHODCALLTYPE isLoading(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE elementAtPoint(_In_ LPPOINT, _COM_Outptr_opt_ IPropertyBag** elementDictionary);
    virtual HRESULT STDMETHODCALLTYPE pasteboardTypesForSelection(_COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE writeSelectionWithPasteboardTypes(__inout_ecount_full(cTypes) BSTR* types, int cTypes, _In_opt_ IDataObject* pasteboard);
    virtual HRESULT STDMETHODCALLTYPE pasteboardTypesForElement(_In_opt_ IPropertyBag* elementDictionary, _COM_Outptr_opt_ IEnumVARIANT**);
    virtual HRESULT STDMETHODCALLTYPE writeElement(_In_opt_ IPropertyBag* elementDictionary, __inout_ecount_full(cWithPasteboardTypes) BSTR* withPasteboardTypes, int cWithPasteboardTypes, _In_opt_ IDataObject* pasteboard);
    virtual HRESULT STDMETHODCALLTYPE selectedText(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE centerSelectionInVisibleArea(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE moveDragCaretToPoint(_In_ LPPOINT);
    virtual HRESULT STDMETHODCALLTYPE removeDragCaret();
    virtual HRESULT STDMETHODCALLTYPE setDrawsBackground(BOOL);
    virtual HRESULT STDMETHODCALLTYPE drawsBackground(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMainFrameURL(_In_ BSTR urlString);
    virtual HRESULT STDMETHODCALLTYPE mainFrameURL(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE mainFrameDocument(_COM_Outptr_opt_ IDOMDocument**);
    virtual HRESULT STDMETHODCALLTYPE mainFrameTitle(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE mainFrameIcon(_Deref_opt_out_ HBITMAP*);
    virtual HRESULT STDMETHODCALLTYPE registerURLSchemeAsLocal(_In_ BSTR scheme);
    virtual HRESULT STDMETHODCALLTYPE close();

    // IWebIBActions
    virtual HRESULT STDMETHODCALLTYPE takeStringURLFrom(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE stopLoading(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE reload(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canGoBack(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE goBack(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canGoForward(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE goForward(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canMakeTextLarger(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE makeTextLarger(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canMakeTextSmaller(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE makeTextSmaller(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canMakeTextStandardSize(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE makeTextStandardSize(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE toggleContinuousSpellChecking(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE toggleSmartInsertDelete(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE toggleGrammarChecking(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE reloadFromOrigin(_In_opt_ IUnknown* sender);

    // IWebViewCSS
    virtual HRESULT STDMETHODCALLTYPE computedStyleForElement(_In_opt_ IDOMElement*, _In_ BSTR pseudoElement, _COM_Outptr_opt_ IDOMCSSStyleDeclaration**);

    // IWebViewEditing
    virtual HRESULT STDMETHODCALLTYPE editableDOMRangeForPoint(_In_ LPPOINT, _COM_Outptr_opt_ IDOMRange**);
    virtual HRESULT STDMETHODCALLTYPE setSelectedDOMRange(_In_opt_ IDOMRange*, WebSelectionAffinity);
    virtual HRESULT STDMETHODCALLTYPE selectedDOMRange(_COM_Outptr_opt_ IDOMRange**);
    virtual HRESULT STDMETHODCALLTYPE selectionAffinity(_Out_ WebSelectionAffinity*);
    virtual HRESULT STDMETHODCALLTYPE setEditable(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isEditable(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setTypingStyle(_In_opt_ IDOMCSSStyleDeclaration*);
    virtual HRESULT STDMETHODCALLTYPE typingStyle(_COM_Outptr_opt_ IDOMCSSStyleDeclaration**);
    virtual HRESULT STDMETHODCALLTYPE setSmartInsertDeleteEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE smartInsertDeleteEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setSelectTrailingWhitespaceEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isSelectTrailingWhitespaceEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setContinuousSpellCheckingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isContinuousSpellCheckingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE spellCheckerDocumentTag(_Out_ int* tag);
    virtual HRESULT STDMETHODCALLTYPE undoManager(_COM_Outptr_opt_ IWebUndoManager**);
    virtual HRESULT STDMETHODCALLTYPE setEditingDelegate(_In_opt_ IWebEditingDelegate*);
    virtual HRESULT STDMETHODCALLTYPE editingDelegate(_COM_Outptr_opt_ IWebEditingDelegate**);
    virtual HRESULT STDMETHODCALLTYPE styleDeclarationWithText(_In_ BSTR text, _COM_Outptr_opt_ IDOMCSSStyleDeclaration**);
    virtual HRESULT STDMETHODCALLTYPE hasSelectedRange(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE cutEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE copyEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE pasteEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE deleteEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE editingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE isGrammarCheckingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setGrammarCheckingEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setPageSizeMultiplier(float);
    virtual HRESULT STDMETHODCALLTYPE pageSizeMultiplier(_Out_ float*);
    virtual HRESULT STDMETHODCALLTYPE canZoomPageIn(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE zoomPageIn(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canZoomPageOut(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE zoomPageOut(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE canResetPageZoom(_In_opt_ IUnknown* sender, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE resetPageZoom(_In_opt_ IUnknown* sender);

    // IWebViewUndoableEditing
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithNode(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithText(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithMarkupString(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithArchive(_In_opt_ IWebArchive*);
    virtual HRESULT STDMETHODCALLTYPE deleteSelection();
    virtual HRESULT STDMETHODCALLTYPE clearSelection();
    virtual HRESULT STDMETHODCALLTYPE applyStyle(_In_opt_ IDOMCSSStyleDeclaration*);

    // IWebViewEditingActions
    virtual HRESULT STDMETHODCALLTYPE copy(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE cut(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE paste(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE copyURL(_In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE copyFont(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE pasteFont(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE delete_(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE pasteAsPlainText(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE pasteAsRichText(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE changeFont(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE changeAttributes(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE changeDocumentBackgroundColor(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE changeColor(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE alignCenter(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE alignJustified(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE alignLeft(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE alignRight(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE checkSpelling(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE showGuessPanel(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE performFindPanelAction(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE startSpeaking(_In_opt_ IUnknown* sender);
    virtual HRESULT STDMETHODCALLTYPE stopSpeaking(_In_opt_ IUnknown* sender);

    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify(_In_opt_ IWebNotification*);

    // IWebViewPrivate
    virtual HRESULT STDMETHODCALLTYPE MIMETypeForExtension(_In_ BSTR extension, _Deref_opt_out_ BSTR *mimeType);
    virtual HRESULT STDMETHODCALLTYPE setCustomDropTarget(_In_opt_ IDropTarget*);
    virtual HRESULT STDMETHODCALLTYPE removeCustomDropTarget();
    virtual HRESULT STDMETHODCALLTYPE setInViewSourceMode(BOOL);
    virtual HRESULT STDMETHODCALLTYPE inViewSourceMode(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE viewWindow(_Deref_opt_out_ HWND*);
    virtual HRESULT STDMETHODCALLTYPE setFormDelegate(_In_opt_ IWebFormDelegate*);
    virtual HRESULT STDMETHODCALLTYPE formDelegate(_COM_Outptr_opt_ IWebFormDelegate**);
    virtual HRESULT STDMETHODCALLTYPE setFrameLoadDelegatePrivate(_In_opt_ IWebFrameLoadDelegatePrivate*);
    virtual HRESULT STDMETHODCALLTYPE frameLoadDelegatePrivate(_COM_Outptr_opt_ IWebFrameLoadDelegatePrivate**);
    virtual HRESULT STDMETHODCALLTYPE scrollOffset(_Out_ LPPOINT);
    virtual HRESULT STDMETHODCALLTYPE scrollBy(_In_ LPPOINT);
    virtual HRESULT STDMETHODCALLTYPE visibleContentRect(_Out_ LPRECT);
    virtual HRESULT STDMETHODCALLTYPE updateFocusedAndActiveState();
    virtual HRESULT STDMETHODCALLTYPE executeCoreCommandByName(_In_ BSTR name, _In_ BSTR value);
    virtual HRESULT STDMETHODCALLTYPE clearMainFrameName();
    virtual HRESULT STDMETHODCALLTYPE markAllMatchesForText(_In_ BSTR search, BOOL caseSensitive, BOOL highlight, UINT limit, _Out_ UINT* matches);
    virtual HRESULT STDMETHODCALLTYPE unmarkAllTextMatches();
    virtual HRESULT STDMETHODCALLTYPE rectsForTextMatches(_COM_Outptr_opt_ IEnumTextMatches**);
    virtual HRESULT STDMETHODCALLTYPE generateSelectionImage(BOOL forceWhiteText, _Deref_opt_out_ HBITMAP* hBitmap);
    virtual HRESULT STDMETHODCALLTYPE selectionRect(_Inout_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*, DWORD grfKeyState, POINTL, DWORD* pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragLeave();
    virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject*, DWORD grfKeyState, POINTL, DWORD* pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE canHandleRequest(_In_opt_ IWebURLRequest*, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE standardUserAgentWithApplicationName(_In_ BSTR applicationName, _Deref_opt_out_ BSTR *groupName);
    virtual HRESULT STDMETHODCALLTYPE clearFocusNode();
    virtual HRESULT STDMETHODCALLTYPE setInitialFocus(BOOL forward);
    virtual HRESULT STDMETHODCALLTYPE setTabKeyCyclesThroughElements(BOOL);
    virtual HRESULT STDMETHODCALLTYPE tabKeyCyclesThroughElements(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setAllowSiteSpecificHacks(BOOL);
    virtual HRESULT STDMETHODCALLTYPE addAdditionalPluginDirectory(_In_ BSTR);    
    virtual HRESULT STDMETHODCALLTYPE loadBackForwardListFromOtherView(_In_opt_ IWebView*);
    virtual HRESULT STDMETHODCALLTYPE inspector(_COM_Outptr_opt_ IWebInspector**);
    virtual HRESULT STDMETHODCALLTYPE clearUndoRedoOperations( void);
    virtual HRESULT STDMETHODCALLTYPE shouldClose(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setProhibitsMainFrameScrolling(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setShouldApplyMacFontAscentHack(BOOL);
    virtual HRESULT STDMETHODCALLTYPE windowAncestryDidChange();
    virtual HRESULT STDMETHODCALLTYPE paintDocumentRectToContext(RECT, _In_ HDC);
    virtual HRESULT STDMETHODCALLTYPE paintScrollViewRectToContextAtPoint(RECT, POINT, _In_ HDC);
    virtual HRESULT STDMETHODCALLTYPE reportException(_In_ JSContextRef, _In_ JSValueRef exception);
    virtual HRESULT STDMETHODCALLTYPE elementFromJS(_In_ JSContextRef, _In_ JSValueRef nodeObject, _COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE setCustomHTMLTokenizerTimeDelay(double);
    virtual HRESULT STDMETHODCALLTYPE setCustomHTMLTokenizerChunkSize(int);
    virtual HRESULT STDMETHODCALLTYPE backingStore(_Deref_opt_out_ HBITMAP*);
    virtual HRESULT STDMETHODCALLTYPE setTransparent(BOOL);
    virtual HRESULT STDMETHODCALLTYPE transparent(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDefersCallbacks(BOOL);
    virtual HRESULT STDMETHODCALLTYPE defersCallbacks(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE globalHistoryItem(_COM_Outptr_opt_ IWebHistoryItem**);
    virtual HRESULT STDMETHODCALLTYPE setAlwaysUsesComplexTextCodePath(BOOL);
    virtual HRESULT STDMETHODCALLTYPE alwaysUsesComplexTextCodePath(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setCookieEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE cookieEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMediaVolume(float);
    virtual HRESULT STDMETHODCALLTYPE mediaVolume(_Out_ float* volume);
    virtual HRESULT STDMETHODCALLTYPE registerEmbeddedViewMIMEType(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE setMemoryCacheDelegateCallsEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setJavaScriptURLsAreAllowed(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setCanStartPlugins(BOOL);
    virtual HRESULT STDMETHODCALLTYPE addUserScriptToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount,
        __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserScriptInjectionTime);
    virtual HRESULT STDMETHODCALLTYPE addUserStyleSheetToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist);
    virtual HRESULT STDMETHODCALLTYPE removeUserScriptFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE removeUserStyleSheetFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE removeUserScriptsFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*);
    virtual HRESULT STDMETHODCALLTYPE removeUserStyleSheetsFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*);
    virtual HRESULT STDMETHODCALLTYPE removeAllUserContentFromGroup(_In_ BSTR groupName);
    virtual HRESULT STDMETHODCALLTYPE unused1();
    virtual HRESULT STDMETHODCALLTYPE unused2();
    virtual HRESULT STDMETHODCALLTYPE invalidateBackingStore(_In_opt_ const RECT*);
    virtual HRESULT STDMETHODCALLTYPE addOriginAccessWhitelistEntry(_In_ BSTR sourceOrigin, _In_ BSTR destinationProtocol, _In_ BSTR destinationHost, BOOL allowDestinationSubdomains);
    virtual HRESULT STDMETHODCALLTYPE removeOriginAccessWhitelistEntry(_In_ BSTR sourceOrigin, _In_ BSTR destinationProtocol, _In_ BSTR destinationHost, BOOL allowDestinationSubdomains);
    virtual HRESULT STDMETHODCALLTYPE resetOriginAccessWhitelists();
    virtual HRESULT STDMETHODCALLTYPE setHistoryDelegate(_In_ IWebHistoryDelegate*);
    virtual HRESULT STDMETHODCALLTYPE historyDelegate(_COM_Outptr_opt_ IWebHistoryDelegate**);
    virtual HRESULT STDMETHODCALLTYPE addVisitedLinks(__inout_ecount_full(visitedURLCount) BSTR* visitedURLs, unsigned visitedURLCount);
    virtual HRESULT STDMETHODCALLTYPE unused3();
    virtual HRESULT STDMETHODCALLTYPE unused4();
    virtual HRESULT STDMETHODCALLTYPE unused5();
    virtual HRESULT STDMETHODCALLTYPE setGeolocationProvider(_In_opt_ IWebGeolocationProvider*);
    virtual HRESULT STDMETHODCALLTYPE geolocationProvider(_COM_Outptr_opt_ IWebGeolocationProvider**);
    virtual HRESULT STDMETHODCALLTYPE geolocationDidChangePosition(_In_opt_ IWebGeolocationPosition* position);
    virtual HRESULT STDMETHODCALLTYPE geolocationDidFailWithError(_In_opt_ IWebError* error);
    virtual HRESULT STDMETHODCALLTYPE setDomainRelaxationForbiddenForURLScheme(BOOL forbidden, _In_ BSTR scheme);
    virtual HRESULT STDMETHODCALLTYPE registerURLSchemeAsSecure(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE nextDisplayIsSynchronous();
    virtual HRESULT STDMETHODCALLTYPE defaultMinimumTimerInterval(_Out_ double*);
    virtual HRESULT STDMETHODCALLTYPE setMinimumTimerInterval(double);
    virtual HRESULT STDMETHODCALLTYPE httpPipeliningEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setHTTPPipeliningEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setUsesLayeredWindow(BOOL);
    virtual HRESULT STDMETHODCALLTYPE usesLayeredWindow(_Out_ BOOL*);

    // IWebViewPrivate2
    HRESULT STDMETHODCALLTYPE setLoadResourcesSerially(BOOL);
    HRESULT STDMETHODCALLTYPE scaleWebView(double scale, POINT origin);
    HRESULT STDMETHODCALLTYPE dispatchPendingLoadRequests();
    virtual HRESULT STDMETHODCALLTYPE setCustomBackingScaleFactor(double);
    virtual HRESULT STDMETHODCALLTYPE backingScaleFactor(_Out_ double*);
    virtual HRESULT STDMETHODCALLTYPE addUserScriptToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserScriptInjectionTime, WebUserContentInjectedFrames);
    virtual HRESULT STDMETHODCALLTYPE addUserStyleSheetToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserContentInjectedFrames);

    // IWebViewPrivate3
    HRESULT STDMETHODCALLTYPE layerTreeAsString(_Deref_opt_out_ BSTR*);
    HRESULT STDMETHODCALLTYPE findString(_In_ BSTR, WebFindOptions, _Deref_opt_out_ BOOL*);

    // IWebViewPrivate4
    HRESULT STDMETHODCALLTYPE setVisibilityState(WebPageVisibilityState);

    // IWebViewPrivate5
    HRESULT STDMETHODCALLTYPE exitFullscreenIfNeeded();

    // WebView
    bool shouldUseEmbeddedView(const WTF::String& mimeType) const;

    WebCore::Page* page();
    bool handleMouseEvent(UINT, WPARAM, LPARAM);
    void setMouseActivated(bool flag) { m_mouseActivated = flag; }
    bool handleContextMenuEvent(WPARAM, LPARAM);
    HMENU createContextMenu();
    bool onMeasureItem(WPARAM, LPARAM);
    bool onDrawItem(WPARAM, LPARAM);
    bool onInitMenuPopup(WPARAM, LPARAM);
    bool onUninitMenuPopup(WPARAM, LPARAM);
    void onMenuCommand(WPARAM, LPARAM);
    bool mouseWheel(WPARAM, LPARAM, bool isMouseHWheel);
    bool verticalScroll(WPARAM, LPARAM);
    bool horizontalScroll(WPARAM, LPARAM);
    bool gesture(WPARAM, LPARAM);
    bool gestureNotify(WPARAM, LPARAM);
    bool execCommand(WPARAM wParam, LPARAM lParam);
    bool keyDown(WPARAM, LPARAM, bool systemKeyDown = false);
    bool keyUp(WPARAM, LPARAM, bool systemKeyDown = false);
    bool keyPress(WPARAM, LPARAM, bool systemKeyDown = false);
    void paintWithDirect2D();
    void paint(HDC, LPARAM);
    void paintIntoWindow(HDC bitmapDC, HDC windowDC, const WebCore::IntRect& dirtyRect);
    bool ensureBackingStore();
    void addToDirtyRegion(const WebCore::IntRect&);
    void addToDirtyRegion(GDIObject<HRGN>);
    void scrollBackingStore(WebCore::FrameView*, int dx, int dy, const WebCore::IntRect& scrollViewRect, const WebCore::IntRect& clipRect);
    void deleteBackingStore();
    void repaint(const WebCore::IntRect&, bool contentChanged, bool immediate = false, bool repaintContentOnly = false);
    void frameRect(RECT* rect);
    void closeWindow();
    void closeWindowSoon();
    void closeWindowTimerFired();
    bool didClose() const { return m_didClose; }

    bool transparent() const { return m_transparent; }
    bool usesLayeredWindow() const { return m_usesLayeredWindow; }
    bool needsDisplay() const { return m_needsDisplay; }

    bool onIMEStartComposition();
    bool onIMEComposition(LPARAM);
    bool onIMEEndComposition();
    bool onIMEChar(WPARAM, LPARAM);
    bool onIMENotify(WPARAM, LPARAM, LRESULT*);
    LRESULT onIMERequest(WPARAM, LPARAM);
    bool onIMESelect(WPARAM, LPARAM);
    bool onIMESetContext(WPARAM, LPARAM);
    void selectionChanged();
    void resetIME(WebCore::Frame*);
    void setInputMethodState(bool);

    HRESULT registerDragDrop();
    HRESULT revokeDragDrop();

    // Convenient to be able to violate the rules of COM here for easy movement to the frame.
    WebFrame* topLevelFrame() const { return m_mainFrame; }
    const WTF::String& userAgentForKURL(const WebCore::URL& url);

    static bool canHandleRequest(const WebCore::ResourceRequest&);

    static WTF::String standardUserAgentWithApplicationName(const WTF::String&);

    void setIsBeingDestroyed();
    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);

    bool isPainting() const { return m_paintCount > 0; }

    void setToolTip(const WTF::String&);

    void registerForIconNotification(bool listen);
    void dispatchDidReceiveIconFromWebFrame(WebFrame*);

    HRESULT notifyDidAddIcon(IWebNotification*);
    HRESULT notifyPreferencesChanged(IWebNotification*);

    static void setCacheModel(WebCacheModel);
    static WebCacheModel cacheModel();
    static bool didSetCacheModel();
    static WebCacheModel maxCacheModelInAnyInstance();

    void updateActiveStateSoon() const;
    void deleteBackingStoreSoon();
    void cancelDeleteBackingStoreSoon();

    HWND topLevelParent() const { return m_topLevelParent; }
    HWND viewWindow() const { return m_viewWindow; }

    void updateActiveState();

    bool onGetObject(WPARAM, LPARAM, LRESULT&) const;
    static STDMETHODIMP AccessibleObjectFromWindow(HWND, DWORD objectID, REFIID, void** ppObject);

    void downloadURL(const WebCore::URL&);

    void flushPendingGraphicsLayerChangesSoon();
    void setRootChildLayer(WebCore::GraphicsLayer*);

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    WebCore::GraphicsDeviceAdapter* graphicsDeviceAdapter() const;
#endif

    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&);

    void setLastCursor(HCURSOR cursor) { m_lastSetCursor = cursor; }

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) const;
    bool isFullScreen() const;
    WebCore::FullScreenController* fullScreenController();
    void setFullScreenElement(RefPtr<WebCore::Element>&&);
    WebCore::Element* fullScreenElement() const { return m_fullScreenElement.get(); }
#endif

    bool canShowMIMEType(const String& mimeType);
    bool canShowMIMETypeAsHTML(const String& mimeType);

    // Used by TextInputController in DumpRenderTree
    HRESULT STDMETHODCALLTYPE setCompositionForTesting(_In_ BSTR composition, UINT from, UINT length);
    HRESULT STDMETHODCALLTYPE hasCompositionForTesting(_Out_ BOOL*);
    HRESULT STDMETHODCALLTYPE confirmCompositionForTesting(_In_ BSTR composition);
    HRESULT STDMETHODCALLTYPE compositionRangeForTesting(_Out_ UINT* startPosition, _Out_ UINT* length);
    HRESULT STDMETHODCALLTYPE firstRectForCharacterRangeForTesting(UINT location, UINT length, _Out_ RECT* resultRect);
    HRESULT STDMETHODCALLTYPE selectedRangeForTesting(_Out_ UINT* location, _Out_ UINT* length);

    float deviceScaleFactor() const;

private:
    void setZoomMultiplier(float multiplier, bool isTextOnly);
    float zoomMultiplier(bool isTextOnly);
    bool canZoomIn(bool isTextOnly);
    HRESULT zoomIn(bool isTextOnly);
    bool canZoomOut(bool isTextOnly);
    HRESULT zoomOut(bool isTextOnly);
    bool canResetZoom(bool isTextOnly);
    HRESULT resetZoom(bool isTextOnly);
    bool active();

    void sizeChanged(const WebCore::IntSize&);
    bool dpiChanged(float, const WebCore::IntSize&);

    enum WindowsToPaint { PaintWebViewOnly, PaintWebViewAndChildren };
    void paintIntoBackingStore(WebCore::FrameView*, HDC bitmapDC, const WebCore::IntRect& dirtyRect, WindowsToPaint);
    void updateBackingStore(WebCore::FrameView*, HDC = 0, bool backingStoreCompletelyDirty = false, WindowsToPaint = PaintWebViewOnly);

    void performLayeredWindowUpdate();

    WebCore::DragOperation keyStateToDragOperation(DWORD grfKeyState) const;

    // FIXME: This variable is part of a workaround. The drop effect (pdwEffect) passed to Drop is incorrect. 
    // We set this variable in DragEnter and DragOver so that it can be used in Drop to set the correct drop effect. 
    // Thus, on return from DoDragDrop we have the correct pdwEffect for the drag-and-drop operation.
    // (see https://bugs.webkit.org/show_bug.cgi?id=29264)
    DWORD m_lastDropEffect { 0 };

    // GraphicsLayerClient
    void notifyAnimationStarted(const WebCore::GraphicsLayer*, const String&, MonotonicTime) override;
    void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& inClip, WebCore::GraphicsLayerPaintBehavior) override;

    // CACFLayerTreeHostClient
    virtual void flushPendingGraphicsLayerChanges();

    bool m_shouldInvertColors { false };
    void setShouldInvertColors(bool);

protected:
    static bool registerWebViewWindowClass();
    static LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void updateWindowIfNeeded(HWND hWnd, UINT message);
    bool paintCompositedContentToHDC(HDC);

    HIMC getIMMContext();
    void releaseIMMContext(HIMC);
    static bool allowSiteSpecificHacks() { return s_allowSiteSpecificHacks; } 
    void preflightSpellChecker();
    bool continuousCheckingAllowed();
    void initializeToolTipWindow();
    void prepareCandidateWindow(WebCore::Frame*, HIMC);
    void updateSelectionForIME();
    LRESULT onIMERequestCharPosition(WebCore::Frame*, IMECHARPOSITION*);
    LRESULT onIMERequestReconvertString(WebCore::Frame*, RECONVERTSTRING*);
    bool developerExtrasEnabled() const;

    bool shouldInitializeTrackPointHack();

    // AllWebViewSet functions
    void addToAllWebViewsSet();
    void removeFromAllWebViewsSet();

    virtual void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM);

#if ENABLE(FULLSCREEN_API)
    virtual HWND fullScreenClientWindow() const;
    virtual HWND fullScreenClientParentWindow() const;
    virtual void fullScreenClientSetParentWindow(HWND);
    virtual void fullScreenClientWillEnterFullScreen();
    virtual void fullScreenClientDidEnterFullScreen();
    virtual void fullScreenClientWillExitFullScreen();
    virtual void fullScreenClientDidExitFullScreen();
    virtual void fullScreenClientForceRepaint();
    virtual void fullScreenClientSaveScrollPosition();
    virtual void fullScreenClientRestoreScrollPosition();
#endif

    ULONG m_refCount { 0 };
#if !ASSERT_DISABLED
    bool m_deletionHasBegun { false };
#endif
    HWND m_hostWindow { nullptr };
    HWND m_viewWindow { nullptr };
    WebFrame* m_mainFrame { nullptr };
    WebCore::Page* m_page { nullptr };
    WebInspectorClient* m_inspectorClient { nullptr };

    HMENU m_currentContextMenu { nullptr };
    RefPtr<WebCore::SharedGDIObject<HBITMAP>> m_backingStoreBitmap;
#if USE(DIRECT2D)
    COMPtr<ID2D1HwndRenderTarget> m_renderTarget;
    COMPtr<ID2D1Bitmap> m_backingStoreD2DBitmap;
    COMPtr<ID2D1BitmapRenderTarget> m_backingStoreRenderTarget;
    COMPtr<ID2D1GdiInteropRenderTarget> m_backingStoreGdiInterop;
#endif
    SIZE m_backingStoreSize;
    RefPtr<WebCore::SharedGDIObject<HRGN>> m_backingStoreDirtyRegion;

    COMPtr<IAccessibilityDelegate> m_accessibilityDelegate;
    COMPtr<IWebEditingDelegate> m_editingDelegate;
    COMPtr<IWebFrameLoadDelegate> m_frameLoadDelegate;
    COMPtr<IWebFrameLoadDelegatePrivate> m_frameLoadDelegatePrivate;
    COMPtr<IWebUIDelegate> m_uiDelegate;
    COMPtr<IWebUIDelegatePrivate> m_uiDelegatePrivate;
    COMPtr<IWebFormDelegate> m_formDelegate;
    COMPtr<IWebPolicyDelegate> m_policyDelegate;
    COMPtr<IWebResourceLoadDelegate> m_resourceLoadDelegate;
    COMPtr<IWebDownloadDelegate> m_downloadDelegate;
    COMPtr<IWebHistoryDelegate> m_historyDelegate;
    COMPtr<WebPreferences> m_preferences;
    COMPtr<WebInspector> m_webInspector;
    COMPtr<IWebGeolocationProvider> m_geolocationProvider;

    bool m_userAgentOverridden { false };
    bool m_useBackForwardList { true };
    WTF::String m_userAgentCustom;
    WTF::String m_userAgentStandard;
    float m_zoomMultiplier { 1.0f };
    float m_customDeviceScaleFactor { 0 };
    bool m_zoomsTextOnly { false };
    WTF::String m_overrideEncoding;
    WTF::String m_applicationName;
    bool m_mouseActivated { false };
    // WebCore dragging logic needs to be able to inspect the drag data
    // this is updated in DragEnter/Leave/Drop
    COMPtr<IDataObject> m_dragData;
    COMPtr<IDropTargetHelper> m_dropTargetHelper;
    UChar m_currentCharacterCode { 0 };
    bool m_isBeingDestroyed { false };
    unsigned m_paintCount { 0 };
    bool m_hasSpellCheckerDocumentTag { false };
    bool m_didClose { false };
    bool m_hasCustomDropTarget { false };
    unsigned m_inIMEComposition { 0 };
    HWND m_toolTipHwnd { nullptr };
    WTF::String m_toolTip;
    bool m_deleteBackingStoreTimerActive { false };

    bool m_transparent { false };

    static bool s_allowSiteSpecificHacks;

    WebCore::SuspendableTimer* m_closeWindowTimer { nullptr };
    std::unique_ptr<TRACKMOUSEEVENT> m_mouseOutTracker;

    HWND m_topLevelParent { nullptr };

    std::unique_ptr<HashSet<WTF::String>> m_embeddedViewMIMETypes;

    //Variables needed to store gesture information
    RefPtr<WebCore::Node> m_gestureTargetNode;
    long m_lastPanX { 0 };
    long m_lastPanY { 0 };
    long m_xOverpan { 0 };
    long m_yOverpan { 0 };

#if ENABLE(VIDEO)
    std::unique_ptr<FullscreenVideoController> m_fullScreenVideoController;
#endif

    bool isAcceleratedCompositing() const { return m_isAcceleratedCompositing; }
    void setAcceleratedCompositing(bool);
#if USE(CA)
    RefPtr<WebCore::CACFLayerTreeHost> m_layerTreeHost;
    RefPtr<WebCore::GraphicsLayer> m_backingLayer;
#elif USE(TEXTURE_MAPPER_GL)
    std::unique_ptr<AcceleratedCompositingContext> m_acceleratedCompositingContext;
#endif
    bool m_isAcceleratedCompositing { false };

    bool m_nextDisplayIsSynchronous { false };
    bool m_usesLayeredWindow { false };
    bool m_needsDisplay { false };

    HCURSOR m_lastSetCursor { nullptr };

#if ENABLE(FULLSCREEN_API)
    RefPtr<WebCore::Element> m_fullScreenElement;
    std::unique_ptr<WebCore::FullScreenController> m_fullscreenController;
    WebCore::IntPoint m_scrollPosition;
#endif

    RefPtr<WebViewGroup> m_webViewGroup;
};

#endif
