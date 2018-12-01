/*
 * Copyright (C) 2006-2007, 2011, 2013-2015 Apple Inc. All rights reserved.
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

#ifndef WebFrame_H
#define WebFrame_H

#include "WebKit.h"
#include "WebDataSource.h"

#include "AccessibleDocument.h"
#include <WebCore/AdjustViewSizeOrNot.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ResourceHandleClient.h>
#include <sal.h>
#include <wtf/RefPtr.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    class AuthenticationChallenge;
    class DocumentLoader;
    class Element;
    class FloatSize;
    class Frame;
    class GraphicsContext;
    class HTMLFrameOwnerElement;
    class IntRect;
    class Page;
    class ResourceError;
    class SharedBuffer;
}

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

#if USE(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif USE(CAIRO)
namespace WebCore {
class PlatformContextCairo;
}
typedef class WebCore::PlatformContextCairo PlatformGraphicsContext;
#endif

class WebFrame;
class WebFramePolicyListener;
class WebHistory;
class WebView;

interface IWebHistoryItemPrivate;

WebFrame* kit(WebCore::Frame*);
WEBKIT_API WebCore::Frame* core(WebFrame*);

class DECLSPEC_UUID("{A3676398-4485-4a9d-87DC-CB5A40E6351D}") WebFrame : public IWebFrame2, IWebFramePrivate, IWebDocumentText
{
public:
    static WebFrame* createInstance();
protected:
    WebFrame();
    ~WebFrame();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    //IWebFrame
    virtual HRESULT STDMETHODCALLTYPE name(_Deref_opt_out_ BSTR* frameName);
    virtual HRESULT STDMETHODCALLTYPE webView(_COM_Outptr_opt_ IWebView**);
    virtual HRESULT STDMETHODCALLTYPE frameView(_COM_Outptr_opt_ IWebFrameView**);
    virtual HRESULT STDMETHODCALLTYPE DOMDocument(_COM_Outptr_opt_ IDOMDocument**);
    virtual HRESULT STDMETHODCALLTYPE DOMWindow(_COM_Outptr_opt_ IDOMWindow**);
    virtual HRESULT STDMETHODCALLTYPE frameElement(_COM_Outptr_opt_ IDOMHTMLElement** frameElement);
    virtual HRESULT STDMETHODCALLTYPE loadRequest(_In_opt_ IWebURLRequest*);
    virtual HRESULT STDMETHODCALLTYPE loadData(_In_opt_ IStream* data, _In_ BSTR mimeType, _In_ BSTR textEncodingName, _In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE loadHTMLString(_In_ BSTR, _In_ BSTR baseURL);
    virtual HRESULT STDMETHODCALLTYPE loadAlternateHTMLString(_In_ BSTR, _In_ BSTR baseURL, _In_ BSTR unreachableURL);
    virtual HRESULT STDMETHODCALLTYPE loadArchive(_In_opt_ IWebArchive*);
    virtual HRESULT STDMETHODCALLTYPE dataSource(_COM_Outptr_opt_ IWebDataSource**);
    virtual HRESULT STDMETHODCALLTYPE provisionalDataSource(_COM_Outptr_opt_ IWebDataSource**);
    virtual HRESULT STDMETHODCALLTYPE stopLoading();
    virtual HRESULT STDMETHODCALLTYPE reload();
    virtual HRESULT STDMETHODCALLTYPE findFrameNamed(_In_ BSTR name, _COM_Outptr_opt_ IWebFrame**);
    virtual HRESULT STDMETHODCALLTYPE parentFrame(_COM_Outptr_opt_ IWebFrame**);
    virtual HRESULT STDMETHODCALLTYPE childFrames(_COM_Outptr_opt_ IEnumVARIANT** enumFrames);
    virtual HRESULT STDMETHODCALLTYPE currentForm(_COM_Outptr_opt_ IDOMElement**);
    virtual /* [local] */ JSGlobalContextRef STDMETHODCALLTYPE globalContext();

    // IWebFramePrivate
    virtual HRESULT STDMETHODCALLTYPE unused1() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE renderTreeAsExternalRepresentation(BOOL forPrinting, _Deref_opt_out_ BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE pageNumberForElementById(_In_ BSTR id, float pageWidthInPixels, float pageHeightInPixels, _Out_ int* pageNumber);
    virtual HRESULT STDMETHODCALLTYPE numberOfPages(float pageWidthInPixels, float pageHeightInPixels, _Out_ int* pageCount);
    virtual HRESULT STDMETHODCALLTYPE scrollOffset(_Out_ SIZE*);
    virtual HRESULT STDMETHODCALLTYPE layout();
    virtual HRESULT STDMETHODCALLTYPE firstLayoutDone(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE unused2() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE pendingFrameUnloadEventCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE unused3() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setInPrintingMode(BOOL value, _In_ HDC printDC);
    virtual HRESULT STDMETHODCALLTYPE getPrintedPageCount(_In_ HDC printDC, _Out_ UINT* pageCount);
    virtual HRESULT STDMETHODCALLTYPE spoolPages(HDC printDC, UINT startPage, UINT endPage, void* ctx);
    virtual HRESULT STDMETHODCALLTYPE isFrameSet(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE string(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE size(_Out_ SIZE*);
    virtual HRESULT STDMETHODCALLTYPE hasScrollBars(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE contentBounds(_Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE frameBounds(_Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE isDescendantOfFrame(_In_opt_ IWebFrame* ancestor, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE setAllowsScrolling(BOOL);
    virtual HRESULT STDMETHODCALLTYPE allowsScrolling(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setIsDisconnected(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setExcludeFromTextSearch(BOOL);
    virtual HRESULT STDMETHODCALLTYPE reloadFromOrigin();
    virtual HRESULT STDMETHODCALLTYPE paintDocumentRectToContext(RECT, _In_ HDC);
    virtual HRESULT STDMETHODCALLTYPE paintScrollViewRectToContextAtPoint(RECT, POINT, _In_ HDC);
    virtual HRESULT STDMETHODCALLTYPE elementDoesAutoComplete(_In_opt_ IDOMElement*, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE pauseAnimation(_In_ BSTR animationName, _In_opt_ IDOMNode*, double secondsFromNow, _Out_ BOOL* animationWasRunning);
    virtual HRESULT STDMETHODCALLTYPE pauseTransition(_In_ BSTR propertyName, _In_opt_ IDOMNode*, double secondsFromNow, _Out_ BOOL* transitionWasRunning);
    virtual HRESULT STDMETHODCALLTYPE numberOfActiveAnimations(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE loadPlainTextString(_In_ BSTR, _In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE isDisplayingStandaloneImage(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE allowsFollowingLink(_In_ BSTR, _Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE stringByEvaluatingJavaScriptInScriptWorld(IWebScriptWorld*, JSObjectRef, BSTR script, BSTR* evaluationResult);
    virtual JSGlobalContextRef STDMETHODCALLTYPE globalContextForScriptWorld(IWebScriptWorld*);
    virtual HRESULT STDMETHODCALLTYPE visibleContentRect(_Out_ RECT*);
    virtual HRESULT STDMETHODCALLTYPE layerTreeAsText(_Deref_out_opt_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE hasSpellingMarker(UINT from, UINT length, BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE clearOpener();
    virtual HRESULT STDMETHODCALLTYPE setTextDirection(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE unused4() { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE resumeAnimations();
    virtual HRESULT STDMETHODCALLTYPE suspendAnimations();

    // IWebDocumentText
    virtual HRESULT STDMETHODCALLTYPE supportsTextEncoding(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE selectedString(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE selectAll();    
    virtual HRESULT STDMETHODCALLTYPE deselectAll();

    // IWebFrame2:
    virtual HRESULT STDMETHODCALLTYPE isMainFrame(_Out_ BOOL*);
    
    // FrameLoaderClient
    virtual void frameLoaderDestroyed();

    // WebFrame
    Ref<WebCore::Frame> createSubframeWithOwnerElement(IWebView*, WebCore::Page*, WebCore::HTMLFrameOwnerElement*);
    void initWithWebView(IWebView*, WebCore::Page*);
    WebCore::Frame* impl();
    void invalidate();
    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();

    void updateBackground();

    // WebFrame (matching WebCoreFrameBridge)
    HRESULT inViewSourceMode(BOOL *flag);
    HRESULT setInViewSourceMode(BOOL flag);
    HRESULT elementWithName(BSTR name, IDOMElement* form, IDOMElement** element);
    HRESULT formForElement(IDOMElement* element, IDOMElement** form);
    HRESULT controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls);
    HRESULT elementIsPassword(IDOMElement* element, bool* result);
    HRESULT searchForLabelsBeforeElement(const BSTR* labels, unsigned cLabels, IDOMElement* beforeElement, unsigned* resultDistance, BOOL* resultIsInCellAbove, BSTR* result);
    HRESULT matchLabelsAgainstElement(const BSTR* labels, int cLabels, IDOMElement* againstElement, BSTR* result);
    HRESULT canProvideDocumentSource(bool* result);

    URL url() const;

    WebView* webView() const;
    void setWebView(WebView*);

    COMPtr<IAccessible> accessible() const;

protected:
    void loadHTMLString(_In_ BSTR string, _In_ BSTR baseURL, _In_ BSTR unreachableURL);
    void loadData(RefPtr<WebCore::SharedBuffer>&&, BSTR mimeType, BSTR textEncodingName, BSTR baseURL, BSTR failingURL);
    const Vector<WebCore::IntRect>& computePageRects(HDC printDC);
    void setPrinting(bool printing, const WebCore::FloatSize& pageSize, const WebCore::FloatSize& originalPageSize, float maximumShrinkRatio, WebCore::AdjustViewSizeOrNot);
    void headerAndFooterHeights(float*, float*);
    WebCore::IntRect printerMarginRect(HDC);
    void spoolPage (PlatformGraphicsContext* pctx, WebCore::GraphicsContext& spoolCtx, HDC printDC, IWebUIDelegate*, float headerHeight, float footerHeight, UINT page, UINT pageCount);
    void drawHeader(PlatformGraphicsContext* pctx, IWebUIDelegate*, const WebCore::IntRect& pageRect, float headerHeight);
    void drawFooter(PlatformGraphicsContext* pctx, IWebUIDelegate*, const WebCore::IntRect& pageRect, UINT page, UINT pageCount, float headerHeight, float footerHeight);

protected:
    ULONG m_refCount { 0 };
    class WebFramePrivate;
    WebFramePrivate* d;
    bool m_quickRedirectComing { false };
    URL m_originalRequestURL;
    bool m_inPrintingMode { false };

    Vector<WebCore::IntRect> m_pageRects;
    int m_pageHeight { 0 }; // height of the page adjusted by margins
    mutable COMPtr<AccessibleDocument> m_accessible;
};

#endif
