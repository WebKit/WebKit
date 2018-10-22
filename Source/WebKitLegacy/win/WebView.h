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
    HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IWebView
    HRESULT STDMETHODCALLTYPE canShowMIMEType(_In_ BSTR mimeType, _Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE canShowMIMETypeAsHTML(_In_ BSTR mimeType, _Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE MIMETypesShownAsHTML(_COM_Outptr_opt_ IEnumVARIANT**) override;
    HRESULT STDMETHODCALLTYPE setMIMETypesShownAsHTML(__inout_ecount_full(cMimeTypes) BSTR* mimeTypes, int cMimeTypes) override;
    HRESULT STDMETHODCALLTYPE URLFromPasteboard(_In_opt_ IDataObject* pasteboard, _Deref_opt_out_ BSTR* url) override;
    HRESULT STDMETHODCALLTYPE URLTitleFromPasteboard(_In_opt_ IDataObject* pasteboard, _Deref_opt_out_ BSTR* urlTitle) override;
    HRESULT STDMETHODCALLTYPE initWithFrame(RECT frame, _In_ BSTR frameName, _In_ BSTR groupName) override;
    HRESULT STDMETHODCALLTYPE setAccessibilityDelegate(_In_opt_ IAccessibilityDelegate*) override;
    HRESULT STDMETHODCALLTYPE accessibilityDelegate(_COM_Outptr_opt_ IAccessibilityDelegate**) override;
    HRESULT STDMETHODCALLTYPE setUIDelegate(_In_opt_ IWebUIDelegate*) override;
    HRESULT STDMETHODCALLTYPE uiDelegate(_COM_Outptr_opt_ IWebUIDelegate**) override;
    HRESULT STDMETHODCALLTYPE setResourceLoadDelegate(_In_opt_ IWebResourceLoadDelegate*) override;
    HRESULT STDMETHODCALLTYPE resourceLoadDelegate(_COM_Outptr_opt_ IWebResourceLoadDelegate**) override;
    HRESULT STDMETHODCALLTYPE setDownloadDelegate(_In_opt_ IWebDownloadDelegate*) override;
    HRESULT STDMETHODCALLTYPE downloadDelegate(_COM_Outptr_opt_ IWebDownloadDelegate**) override;
    HRESULT STDMETHODCALLTYPE setFrameLoadDelegate(_In_opt_ IWebFrameLoadDelegate*) override;
    HRESULT STDMETHODCALLTYPE frameLoadDelegate(_COM_Outptr_opt_ IWebFrameLoadDelegate**) override;
    HRESULT STDMETHODCALLTYPE setPolicyDelegate(_In_opt_ IWebPolicyDelegate*) override;
    HRESULT STDMETHODCALLTYPE policyDelegate(_COM_Outptr_opt_ IWebPolicyDelegate**) override;
    HRESULT STDMETHODCALLTYPE mainFrame(_COM_Outptr_opt_ IWebFrame**) override;
    HRESULT STDMETHODCALLTYPE focusedFrame(_COM_Outptr_opt_ IWebFrame**) override;
    HRESULT STDMETHODCALLTYPE backForwardList(_COM_Outptr_opt_ IWebBackForwardList**) override;
    HRESULT STDMETHODCALLTYPE setMaintainsBackForwardList(BOOL) override;
    HRESULT STDMETHODCALLTYPE goBack(_Out_ BOOL* succeeded) override;
    HRESULT STDMETHODCALLTYPE goForward(_Out_ BOOL* succeeded) override;
    HRESULT STDMETHODCALLTYPE goToBackForwardItem(_In_opt_ IWebHistoryItem*, _Out_ BOOL* succeeded) override;
    HRESULT STDMETHODCALLTYPE setTextSizeMultiplier(float) override;
    HRESULT STDMETHODCALLTYPE textSizeMultiplier(_Out_ float*) override;
    HRESULT STDMETHODCALLTYPE setApplicationNameForUserAgent(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE applicationNameForUserAgent(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE setCustomUserAgent(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE customUserAgent(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE userAgentForURL(_In_ BSTR url, _Deref_opt_out_ BSTR* userAgent) override;
    HRESULT STDMETHODCALLTYPE supportsTextEncoding(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setCustomTextEncodingName(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE customTextEncodingName(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE setMediaStyle(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE mediaStyle(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE stringByEvaluatingJavaScriptFromString(_In_ BSTR script, _Deref_opt_out_ BSTR* result) override;
    HRESULT STDMETHODCALLTYPE windowScriptObject(_COM_Outptr_opt_ IWebScriptObject**) override;
    HRESULT STDMETHODCALLTYPE setPreferences(_In_opt_ IWebPreferences*) override;
    HRESULT STDMETHODCALLTYPE preferences(_COM_Outptr_opt_ IWebPreferences**) override;
    HRESULT STDMETHODCALLTYPE setPreferencesIdentifier(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE preferencesIdentifier(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE setHostWindow(_In_ HWND) override;
    HRESULT STDMETHODCALLTYPE hostWindow(_Deref_opt_out_ HWND*) override;
    HRESULT STDMETHODCALLTYPE searchFor(_In_ BSTR, BOOL forward, BOOL caseFlag, BOOL wrapFlag, _Out_ BOOL *found) override;
    HRESULT STDMETHODCALLTYPE registerViewClass(_In_opt_ IWebDocumentView*, _In_opt_ IWebDocumentRepresentation*, _In_ BSTR forMIMEType) override;
    HRESULT STDMETHODCALLTYPE setGroupName(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE groupName(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE estimatedProgress(_Out_ double*) override;
    HRESULT STDMETHODCALLTYPE isLoading(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE elementAtPoint(_In_ LPPOINT, _COM_Outptr_opt_ IPropertyBag** elementDictionary) override;
    HRESULT STDMETHODCALLTYPE pasteboardTypesForSelection(_COM_Outptr_opt_ IEnumVARIANT**) override;
    HRESULT STDMETHODCALLTYPE writeSelectionWithPasteboardTypes(__inout_ecount_full(cTypes) BSTR* types, int cTypes, _In_opt_ IDataObject* pasteboard) override;
    HRESULT STDMETHODCALLTYPE pasteboardTypesForElement(_In_opt_ IPropertyBag* elementDictionary, _COM_Outptr_opt_ IEnumVARIANT**) override;
    HRESULT STDMETHODCALLTYPE writeElement(_In_opt_ IPropertyBag* elementDictionary, __inout_ecount_full(cWithPasteboardTypes) BSTR* withPasteboardTypes, int cWithPasteboardTypes, _In_opt_ IDataObject* pasteboard) override;
    HRESULT STDMETHODCALLTYPE selectedText(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE centerSelectionInVisibleArea(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE moveDragCaretToPoint(_In_ LPPOINT) override;
    HRESULT STDMETHODCALLTYPE removeDragCaret() override;
    HRESULT STDMETHODCALLTYPE setDrawsBackground(BOOL) override;
    HRESULT STDMETHODCALLTYPE drawsBackground(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setMainFrameURL(_In_ BSTR urlString) override;
    HRESULT STDMETHODCALLTYPE mainFrameURL(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE mainFrameDocument(_COM_Outptr_opt_ IDOMDocument**) override;
    HRESULT STDMETHODCALLTYPE mainFrameTitle(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE mainFrameIcon(_Deref_opt_out_ HBITMAP*) override;
    HRESULT STDMETHODCALLTYPE registerURLSchemeAsLocal(_In_ BSTR scheme) override;
    HRESULT STDMETHODCALLTYPE close() override;

    // IWebIBActions
    HRESULT STDMETHODCALLTYPE takeStringURLFrom(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE stopLoading(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE reload(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canGoBack(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE goBack(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canGoForward(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE goForward(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canMakeTextLarger(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE makeTextLarger(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canMakeTextSmaller(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE makeTextSmaller(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canMakeTextStandardSize(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE makeTextStandardSize(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE toggleContinuousSpellChecking(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE toggleSmartInsertDelete(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE toggleGrammarChecking(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE reloadFromOrigin(_In_opt_ IUnknown* sender) override;

    // IWebViewCSS
    HRESULT STDMETHODCALLTYPE computedStyleForElement(_In_opt_ IDOMElement*, _In_ BSTR pseudoElement, _COM_Outptr_opt_ IDOMCSSStyleDeclaration**) override;

    // IWebViewEditing
    HRESULT STDMETHODCALLTYPE editableDOMRangeForPoint(_In_ LPPOINT, _COM_Outptr_opt_ IDOMRange**) override;
    HRESULT STDMETHODCALLTYPE setSelectedDOMRange(_In_opt_ IDOMRange*, WebSelectionAffinity) override;
    HRESULT STDMETHODCALLTYPE selectedDOMRange(_COM_Outptr_opt_ IDOMRange**) override;
    HRESULT STDMETHODCALLTYPE selectionAffinity(_Out_ WebSelectionAffinity*) override;
    HRESULT STDMETHODCALLTYPE setEditable(BOOL) override;
    HRESULT STDMETHODCALLTYPE isEditable(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setTypingStyle(_In_opt_ IDOMCSSStyleDeclaration*) override;
    HRESULT STDMETHODCALLTYPE typingStyle(_COM_Outptr_opt_ IDOMCSSStyleDeclaration**) override;
    HRESULT STDMETHODCALLTYPE setSmartInsertDeleteEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE smartInsertDeleteEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setSelectTrailingWhitespaceEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE isSelectTrailingWhitespaceEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setContinuousSpellCheckingEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE isContinuousSpellCheckingEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE spellCheckerDocumentTag(_Out_ int* tag) override;
    HRESULT STDMETHODCALLTYPE undoManager(_COM_Outptr_opt_ IWebUndoManager**) override;
    HRESULT STDMETHODCALLTYPE setEditingDelegate(_In_opt_ IWebEditingDelegate*) override;
    HRESULT STDMETHODCALLTYPE editingDelegate(_COM_Outptr_opt_ IWebEditingDelegate**) override;
    HRESULT STDMETHODCALLTYPE styleDeclarationWithText(_In_ BSTR text, _COM_Outptr_opt_ IDOMCSSStyleDeclaration**) override;
    HRESULT STDMETHODCALLTYPE hasSelectedRange(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE cutEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE copyEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE pasteEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE deleteEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE editingEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE isGrammarCheckingEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setGrammarCheckingEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE setPageSizeMultiplier(float) override;
    HRESULT STDMETHODCALLTYPE pageSizeMultiplier(_Out_ float*) override;
    HRESULT STDMETHODCALLTYPE canZoomPageIn(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE zoomPageIn(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canZoomPageOut(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE zoomPageOut(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE canResetPageZoom(_In_opt_ IUnknown* sender, _Out_ BOOL* result) override;
    HRESULT STDMETHODCALLTYPE resetPageZoom(_In_opt_ IUnknown* sender) override;

    // IWebViewUndoableEditing
    HRESULT STDMETHODCALLTYPE replaceSelectionWithNode(_In_opt_ IDOMNode*) override;
    HRESULT STDMETHODCALLTYPE replaceSelectionWithText(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE replaceSelectionWithMarkupString(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE replaceSelectionWithArchive(_In_opt_ IWebArchive*) override;
    HRESULT STDMETHODCALLTYPE deleteSelection() override;
    HRESULT STDMETHODCALLTYPE clearSelection() override;
    HRESULT STDMETHODCALLTYPE applyStyle(_In_opt_ IDOMCSSStyleDeclaration*) override;

    // IWebViewEditingActions
    HRESULT STDMETHODCALLTYPE copy(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE cut(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE paste(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE copyURL(_In_ BSTR url) override;
    HRESULT STDMETHODCALLTYPE copyFont(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE pasteFont(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE delete_(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE pasteAsPlainText(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE pasteAsRichText(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE changeFont(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE changeAttributes(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE changeDocumentBackgroundColor(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE changeColor(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE alignCenter(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE alignJustified(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE alignLeft(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE alignRight(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE checkSpelling(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE showGuessPanel(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE performFindPanelAction(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE startSpeaking(_In_opt_ IUnknown* sender) override;
    HRESULT STDMETHODCALLTYPE stopSpeaking(_In_opt_ IUnknown* sender) override;

    // IWebNotificationObserver
    HRESULT STDMETHODCALLTYPE onNotify(_In_opt_ IWebNotification*) override;

    // IWebViewPrivate
    HRESULT STDMETHODCALLTYPE MIMETypeForExtension(_In_ BSTR extension, _Deref_opt_out_ BSTR *mimeType) override;
    HRESULT STDMETHODCALLTYPE setCustomDropTarget(_In_opt_ IDropTarget*) override;
    HRESULT STDMETHODCALLTYPE removeCustomDropTarget() override;
    HRESULT STDMETHODCALLTYPE setInViewSourceMode(BOOL) override;
    HRESULT STDMETHODCALLTYPE inViewSourceMode(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE viewWindow(_Deref_opt_out_ HWND*) override;
    HRESULT STDMETHODCALLTYPE setFormDelegate(_In_opt_ IWebFormDelegate*) override;
    HRESULT STDMETHODCALLTYPE formDelegate(_COM_Outptr_opt_ IWebFormDelegate**) override;
    HRESULT STDMETHODCALLTYPE setFrameLoadDelegatePrivate(_In_opt_ IWebFrameLoadDelegatePrivate*) override;
    HRESULT STDMETHODCALLTYPE frameLoadDelegatePrivate(_COM_Outptr_opt_ IWebFrameLoadDelegatePrivate**) override;
    HRESULT STDMETHODCALLTYPE scrollOffset(_Out_ LPPOINT) override;
    HRESULT STDMETHODCALLTYPE scrollBy(_In_ LPPOINT) override;
    HRESULT STDMETHODCALLTYPE visibleContentRect(_Out_ LPRECT) override;
    HRESULT STDMETHODCALLTYPE updateFocusedAndActiveState() override;
    HRESULT STDMETHODCALLTYPE executeCoreCommandByName(_In_ BSTR name, _In_ BSTR value) override;
    HRESULT STDMETHODCALLTYPE clearMainFrameName() override;
    HRESULT STDMETHODCALLTYPE markAllMatchesForText(_In_ BSTR search, BOOL caseSensitive, BOOL highlight, UINT limit, _Out_ UINT* matches) override;
    HRESULT STDMETHODCALLTYPE unmarkAllTextMatches() override;
    HRESULT STDMETHODCALLTYPE rectsForTextMatches(_COM_Outptr_opt_ IEnumTextMatches**) override;
    HRESULT STDMETHODCALLTYPE generateSelectionImage(BOOL forceWhiteText, _Deref_opt_out_ HBITMAP* hBitmap) override;
    HRESULT STDMETHODCALLTYPE selectionRect(_Inout_ RECT*) override;
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*, DWORD grfKeyState, POINTL, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject*, DWORD grfKeyState, POINTL, DWORD* pdwEffect) override;
    HRESULT STDMETHODCALLTYPE canHandleRequest(_In_opt_ IWebURLRequest*, _Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE standardUserAgentWithApplicationName(_In_ BSTR applicationName, _Deref_opt_out_ BSTR *groupName) override;
    HRESULT STDMETHODCALLTYPE clearFocusNode() override;
    HRESULT STDMETHODCALLTYPE setInitialFocus(BOOL forward) override;
    HRESULT STDMETHODCALLTYPE setTabKeyCyclesThroughElements(BOOL) override;
    HRESULT STDMETHODCALLTYPE tabKeyCyclesThroughElements(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setAllowSiteSpecificHacks(BOOL) override;
    HRESULT STDMETHODCALLTYPE addAdditionalPluginDirectory(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE loadBackForwardListFromOtherView(_In_opt_ IWebView*) override;
    HRESULT STDMETHODCALLTYPE inspector(_COM_Outptr_opt_ IWebInspector**) override;
    HRESULT STDMETHODCALLTYPE clearUndoRedoOperations(void) override;
    HRESULT STDMETHODCALLTYPE shouldClose(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setProhibitsMainFrameScrolling(BOOL) override;
    HRESULT STDMETHODCALLTYPE setShouldApplyMacFontAscentHack(BOOL) override;
    HRESULT STDMETHODCALLTYPE windowAncestryDidChange() override;
    HRESULT STDMETHODCALLTYPE paintDocumentRectToContext(RECT, _In_ HDC) override;
    HRESULT STDMETHODCALLTYPE paintScrollViewRectToContextAtPoint(RECT, POINT, _In_ HDC) override;
    HRESULT STDMETHODCALLTYPE reportException(_In_ JSContextRef, _In_ JSValueRef exception) override;
    HRESULT STDMETHODCALLTYPE elementFromJS(_In_ JSContextRef, _In_ JSValueRef nodeObject, _COM_Outptr_opt_ IDOMElement**) override;
    HRESULT STDMETHODCALLTYPE setCustomHTMLTokenizerTimeDelay(double) override;
    HRESULT STDMETHODCALLTYPE setCustomHTMLTokenizerChunkSize(int) override;
    HRESULT STDMETHODCALLTYPE backingStore(_Deref_opt_out_ HBITMAP*) override;
    HRESULT STDMETHODCALLTYPE setTransparent(BOOL) override;
    HRESULT STDMETHODCALLTYPE transparent(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setDefersCallbacks(BOOL) override;
    HRESULT STDMETHODCALLTYPE defersCallbacks(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE globalHistoryItem(_COM_Outptr_opt_ IWebHistoryItem**) override;
    HRESULT STDMETHODCALLTYPE setAlwaysUsesComplexTextCodePath(BOOL) override;
    HRESULT STDMETHODCALLTYPE alwaysUsesComplexTextCodePath(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setCookieEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE cookieEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setMediaVolume(float) override;
    HRESULT STDMETHODCALLTYPE mediaVolume(_Out_ float* volume) override;
    HRESULT STDMETHODCALLTYPE registerEmbeddedViewMIMEType(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE setMemoryCacheDelegateCallsEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE setJavaScriptURLsAreAllowed(BOOL) override;
    HRESULT STDMETHODCALLTYPE setCanStartPlugins(BOOL) override;
    HRESULT STDMETHODCALLTYPE addUserScriptToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount,
        __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserScriptInjectionTime) override;
    HRESULT STDMETHODCALLTYPE addUserStyleSheetToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist) override;
    HRESULT STDMETHODCALLTYPE removeUserScriptFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR url) override;
    HRESULT STDMETHODCALLTYPE removeUserStyleSheetFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR url) override;
    HRESULT STDMETHODCALLTYPE removeUserScriptsFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*) override;
    HRESULT STDMETHODCALLTYPE removeUserStyleSheetsFromGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*) override;
    HRESULT STDMETHODCALLTYPE removeAllUserContentFromGroup(_In_ BSTR groupName) override;
    HRESULT STDMETHODCALLTYPE unused1() override;
    HRESULT STDMETHODCALLTYPE unused2() override;
    HRESULT STDMETHODCALLTYPE invalidateBackingStore(_In_opt_ const RECT*) override;
    HRESULT STDMETHODCALLTYPE addOriginAccessWhitelistEntry(_In_ BSTR sourceOrigin, _In_ BSTR destinationProtocol, _In_ BSTR destinationHost, BOOL allowDestinationSubdomains) override;
    HRESULT STDMETHODCALLTYPE removeOriginAccessWhitelistEntry(_In_ BSTR sourceOrigin, _In_ BSTR destinationProtocol, _In_ BSTR destinationHost, BOOL allowDestinationSubdomains) override;
    HRESULT STDMETHODCALLTYPE resetOriginAccessWhitelists() override;
    HRESULT STDMETHODCALLTYPE setHistoryDelegate(_In_ IWebHistoryDelegate*) override;
    HRESULT STDMETHODCALLTYPE historyDelegate(_COM_Outptr_opt_ IWebHistoryDelegate**) override;
    HRESULT STDMETHODCALLTYPE addVisitedLinks(__inout_ecount_full(visitedURLCount) BSTR* visitedURLs, unsigned visitedURLCount) override;
    HRESULT STDMETHODCALLTYPE unused3() override;
    HRESULT STDMETHODCALLTYPE unused4() override;
    HRESULT STDMETHODCALLTYPE unused5() override;
    HRESULT STDMETHODCALLTYPE setGeolocationProvider(_In_opt_ IWebGeolocationProvider*) override;
    HRESULT STDMETHODCALLTYPE geolocationProvider(_COM_Outptr_opt_ IWebGeolocationProvider**) override;
    HRESULT STDMETHODCALLTYPE geolocationDidChangePosition(_In_opt_ IWebGeolocationPosition*) override;
    HRESULT STDMETHODCALLTYPE geolocationDidFailWithError(_In_opt_ IWebError*) override;
    HRESULT STDMETHODCALLTYPE setDomainRelaxationForbiddenForURLScheme(BOOL forbidden, _In_ BSTR scheme) override;
    HRESULT STDMETHODCALLTYPE registerURLSchemeAsSecure(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing(_In_ BSTR) override;
    HRESULT STDMETHODCALLTYPE nextDisplayIsSynchronous() override;
    HRESULT STDMETHODCALLTYPE defaultMinimumTimerInterval(_Out_ double*) override;
    HRESULT STDMETHODCALLTYPE setMinimumTimerInterval(double) override;
    HRESULT STDMETHODCALLTYPE httpPipeliningEnabled(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE setHTTPPipeliningEnabled(BOOL) override;
    HRESULT STDMETHODCALLTYPE setUsesLayeredWindow(BOOL) override;
    HRESULT STDMETHODCALLTYPE usesLayeredWindow(_Out_ BOOL*) override;

    // IWebViewPrivate2
    HRESULT STDMETHODCALLTYPE setLoadResourcesSerially(BOOL) override;
    HRESULT STDMETHODCALLTYPE scaleWebView(double scale, POINT origin) override;
    HRESULT STDMETHODCALLTYPE dispatchPendingLoadRequests() override;
    HRESULT STDMETHODCALLTYPE setCustomBackingScaleFactor(double) override;
    HRESULT STDMETHODCALLTYPE backingScaleFactor(_Out_ double*) override;
    HRESULT STDMETHODCALLTYPE addUserScriptToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserScriptInjectionTime, WebUserContentInjectedFrames) override;
    HRESULT STDMETHODCALLTYPE addUserStyleSheetToGroup(_In_ BSTR groupName, _In_opt_ IWebScriptWorld*, _In_ BSTR source, _In_ BSTR url,
        unsigned whitelistCount, __inout_ecount_full(whitelistCount) BSTR* whitelist, unsigned blacklistCount, __inout_ecount_full(blacklistCount) BSTR* blacklist, WebUserContentInjectedFrames) override;

    // IWebViewPrivate3
    HRESULT STDMETHODCALLTYPE layerTreeAsString(_Deref_opt_out_ BSTR*) override;
    HRESULT STDMETHODCALLTYPE findString(_In_ BSTR, WebFindOptions, _Deref_opt_out_ BOOL*) override;

    // IWebViewPrivate4
    HRESULT STDMETHODCALLTYPE setVisibilityState(WebPageVisibilityState) override;

    // IWebViewPrivate5
    HRESULT STDMETHODCALLTYPE exitFullscreenIfNeeded() override;

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
    HRESULT STDMETHODCALLTYPE setCompositionForTesting(_In_ BSTR composition, UINT from, UINT length) override;
    HRESULT STDMETHODCALLTYPE hasCompositionForTesting(_Out_ BOOL*) override;
    HRESULT STDMETHODCALLTYPE confirmCompositionForTesting(_In_ BSTR composition) override;
    HRESULT STDMETHODCALLTYPE compositionRangeForTesting(_Out_ UINT* startPosition, _Out_ UINT* length) override;
    HRESULT STDMETHODCALLTYPE firstRectForCharacterRangeForTesting(UINT location, UINT length, _Out_ RECT* resultRect) override;
    HRESULT STDMETHODCALLTYPE selectedRangeForTesting(_Out_ UINT* location, _Out_ UINT* length) override;

    float deviceScaleFactor() const override;

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

#if USE(CA)
    // CACFLayerTreeHostClient
    void flushPendingGraphicsLayerChanges() override;
#endif

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

    void windowReceivedMessage(HWND, UINT message, WPARAM, LPARAM) override;

#if ENABLE(FULLSCREEN_API)
    HWND fullScreenClientWindow() const override;
    HWND fullScreenClientParentWindow() const override;
    void fullScreenClientSetParentWindow(HWND) override;
    void fullScreenClientWillEnterFullScreen() override;
    void fullScreenClientDidEnterFullScreen() override;
    void fullScreenClientWillExitFullScreen() override;
    void fullScreenClientDidExitFullScreen() override;
    void fullScreenClientForceRepaint() override;
    void fullScreenClientSaveScrollPosition() override;
    void fullScreenClientRestoreScrollPosition() override;
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
