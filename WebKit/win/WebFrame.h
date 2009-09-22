/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef WebFrame_H
#define WebFrame_H

#include "WebFrameLoaderClient.h"

#include "WebKit.h"
#include "WebDataSource.h"

#include "AccessibleDocument.h"

#pragma warning(push, 0)
#include <WebCore/FrameWin.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/KURL.h>
#include <WebCore/PlatformString.h>
#include <WebCore/ResourceHandleClient.h>
#pragma warning(pop)

#include <WTF/RefPtr.h>
#include <WTF/HashMap.h>
#include <WTF/OwnPtr.h>

namespace WebCore {
    class AuthenticationChallenge;
    class DocumentLoader;
    class Element;
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

#if PLATFORM(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif PLATFORM(CAIRO)
typedef struct _cairo PlatformGraphicsContext;
#endif

class WebFrame;
class WebFramePolicyListener;
class WebHistory;
class WebView;

interface IWebHistoryItemPrivate;

WebFrame* kit(WebCore::Frame*);
WebCore::Frame* core(WebFrame*);

class DECLSPEC_UUID("{A3676398-4485-4a9d-87DC-CB5A40E6351D}") WebFrame : public IWebFrame, IWebFramePrivate, IWebDocumentText
    , public WebFrameLoaderClient
{
public:
    static WebFrame* createInstance();
protected:
    WebFrame();
    ~WebFrame();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    //IWebFrame
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *frameName);
    
    virtual HRESULT STDMETHODCALLTYPE webView( 
        /* [retval][out] */ IWebView **view);

    virtual HRESULT STDMETHODCALLTYPE frameView(
        /* [retval][out] */ IWebFrameView **view);

    virtual HRESULT STDMETHODCALLTYPE DOMDocument( 
        /* [retval][out] */ IDOMDocument** document);
    
    virtual HRESULT STDMETHODCALLTYPE frameElement( 
        /* [retval][out] */ IDOMHTMLElement **frameElement);
    
    virtual HRESULT STDMETHODCALLTYPE loadRequest( 
        /* [in] */ IWebURLRequest *request);
    
    virtual HRESULT STDMETHODCALLTYPE loadData( 
        /* [in] */ IStream *data,
        /* [in] */ BSTR mimeType,
        /* [in] */ BSTR textEncodingName,
        /* [in] */ BSTR url);
    
    virtual HRESULT STDMETHODCALLTYPE loadHTMLString( 
        /* [in] */ BSTR string,
        /* [in] */ BSTR baseURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadAlternateHTMLString( 
        /* [in] */ BSTR str,
        /* [in] */ BSTR baseURL,
        /* [in] */ BSTR unreachableURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadArchive( 
        /* [in] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE dataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE provisionalDataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE stopLoading( void);
    
    virtual HRESULT STDMETHODCALLTYPE reload( void);
    
    virtual HRESULT STDMETHODCALLTYPE findFrameNamed( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE parentFrame( 
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE childFrames( 
        /* [retval][out] */ IEnumVARIANT **enumFrames);

    virtual HRESULT STDMETHODCALLTYPE currentForm( 
        /* [retval][out] */ IDOMElement **formElement);

    virtual /* [local] */ JSGlobalContextRef STDMETHODCALLTYPE globalContext();

    // IWebFramePrivate
    virtual HRESULT STDMETHODCALLTYPE renderTreeAsExternalRepresentation(
        /* [retval][out] */ BSTR *result);

    virtual HRESULT STDMETHODCALLTYPE scrollOffset(
        /* [retval][out] */ SIZE* offset);

    virtual HRESULT STDMETHODCALLTYPE layout();

    virtual HRESULT STDMETHODCALLTYPE firstLayoutDone(
        /* [retval][out] */ BOOL* result);

    virtual HRESULT STDMETHODCALLTYPE loadType( 
        /* [retval][out] */ WebFrameLoadType* type);

    virtual HRESULT STDMETHODCALLTYPE pendingFrameUnloadEventCount( 
        /* [retval][out] */ UINT* result);

    virtual HRESULT STDMETHODCALLTYPE fetchApplicationIcon( 
        /* [in] */ IWebIconFetcherDelegate *delegate,
        /* [retval][out] */ IWebIconFetcher **result);
    
    virtual HRESULT STDMETHODCALLTYPE setInPrintingMode( 
        /* [in] */ BOOL value,
        /* [in] */ HDC printDC);
        
    virtual HRESULT STDMETHODCALLTYPE getPrintedPageCount( 
        /* [in] */ HDC printDC,
        /* [retval][out] */ UINT *pageCount);
    
    virtual HRESULT STDMETHODCALLTYPE spoolPages( 
        /* [in] */ HDC printDC,
        /* [in] */ UINT startPage,
        /* [in] */ UINT endPage,
        /* [retval][out] */ void* ctx);

    virtual HRESULT STDMETHODCALLTYPE isFrameSet( 
        /* [retval][out] */ BOOL* result);

    virtual HRESULT STDMETHODCALLTYPE string( 
        /* [retval][out] */ BSTR* result);

    virtual HRESULT STDMETHODCALLTYPE size( 
        /* [retval][out] */ SIZE *size);

    virtual HRESULT STDMETHODCALLTYPE hasScrollBars( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE contentBounds( 
        /* [retval][out] */ RECT *result);
    
    virtual HRESULT STDMETHODCALLTYPE frameBounds( 
        /* [retval][out] */ RECT *result);

    virtual HRESULT STDMETHODCALLTYPE isDescendantOfFrame( 
        /* [in] */ IWebFrame *ancestor,
        /* [retval][out] */ BOOL *result);

    virtual HRESULT STDMETHODCALLTYPE setAllowsScrolling(
        /* [in] */ BOOL flag);

    virtual HRESULT STDMETHODCALLTYPE allowsScrolling(
        /* [retval][out] */ BOOL *flag);

    virtual HRESULT STDMETHODCALLTYPE setIsDisconnected(
        /* [in] */ BOOL flag);

    virtual HRESULT STDMETHODCALLTYPE setExcludeFromTextSearch(
        /* [in] */ BOOL flag);

    virtual HRESULT STDMETHODCALLTYPE reloadFromOrigin();

    virtual HRESULT STDMETHODCALLTYPE paintDocumentRectToContext(
        /* [in] */ RECT rect,
        /* [in] */ OLE_HANDLE deviceContext);

    virtual HRESULT STDMETHODCALLTYPE elementDoesAutoComplete(
        /* [in] */ IDOMElement* element, 
        /* [retval][out] */ BOOL* result);

    virtual HRESULT STDMETHODCALLTYPE pauseAnimation(BSTR animationName, IDOMNode*, double secondsFromNow, BOOL* animationWasRunning);
    virtual HRESULT STDMETHODCALLTYPE pauseTransition(BSTR propertyName, IDOMNode*, double secondsFromNow, BOOL* transitionWasRunning);
    virtual HRESULT STDMETHODCALLTYPE numberOfActiveAnimations(UINT*);

    virtual HRESULT STDMETHODCALLTYPE isDisplayingStandaloneImage(BOOL*);

    // IWebDocumentText
    virtual HRESULT STDMETHODCALLTYPE supportsTextEncoding( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE selectedString( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE selectAll();
    
    virtual HRESULT STDMETHODCALLTYPE deselectAll();
    
    // FrameLoaderClient
    virtual void frameLoaderDestroyed();
    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual void forceLayoutForNonHTML();
    virtual void setCopiesOnScroll();
    virtual void detachedFromParent2();
    virtual void detachedFromParent3();
    virtual void cancelPolicyCheck();
    virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, PassRefPtr<WebCore::FormState>);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void setMainFrameDocumentReady(bool);
    virtual void willChangeTitle(WebCore::DocumentLoader*);
    virtual void didChangeTitle(WebCore::DocumentLoader*);
    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
    virtual bool canShowMIMEType(const WebCore::String& MIMEType) const;
    virtual bool representationExistsForURLScheme(const WebCore::String& URLScheme) const;
    virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String& URLScheme) const;
    virtual void frameLoadCompleted();
    virtual void restoreViewState();
    virtual void provisionalLoadStarted();
    virtual bool shouldTreatURLAsSameAsCurrent(const WebCore::KURL&) const;
    virtual void addHistoryItemForFragmentScroll();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual WebCore::String userAgent(const WebCore::KURL&);
    virtual void saveViewStateToItem(WebCore::HistoryItem *);
    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);
    virtual bool shouldFallBack(const WebCore::ResourceError&);
    virtual void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction, const WebCore::String& MIMEType, const WebCore::ResourceRequest&);
    virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const WebCore::String& frameName);
    virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>);
    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);
    virtual void download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
    virtual void startDownload(const WebCore::ResourceRequest&);
        
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::KURL& baseURL, const Vector<WebCore::String>& paramNames, const Vector<WebCore::String>& paramValues);

    virtual WebCore::ObjectContentType objectContentType(const WebCore::KURL& url, const WebCore::String& mimeType);
    virtual WebCore::String overrideMediaType() const;

    virtual void windowObjectCleared();
    virtual void documentElementAvailable();
    virtual void didPerformFirstNavigation() const;

    virtual void registerForIconNotification(bool listen);

    // WebFrame
    PassRefPtr<WebCore::Frame> init(IWebView*, WebCore::Page*, WebCore::HTMLFrameOwnerElement*);
    WebCore::Frame* impl();
    void invalidate();
    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();

    void updateBackground();

    // WebFrame (matching WebCoreFrameBridge)
    void setTextSizeMultiplier(float multiplier);
    HRESULT inViewSourceMode(BOOL *flag);
    HRESULT setInViewSourceMode(BOOL flag);
    HRESULT elementWithName(BSTR name, IDOMElement* form, IDOMElement** element);
    HRESULT formForElement(IDOMElement* element, IDOMElement** form);
    HRESULT controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls);
    HRESULT elementIsPassword(IDOMElement* element, bool* result);
    HRESULT searchForLabelsBeforeElement(const BSTR* labels, int cLabels, IDOMElement* beforeElement, BSTR* result);
    HRESULT matchLabelsAgainstElement(const BSTR* labels, int cLabels, IDOMElement* againstElement, BSTR* result);
    HRESULT canProvideDocumentSource(bool* result);

    COMPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction function);
    void receivedPolicyDecision(WebCore::PolicyAction);

    WebCore::KURL url() const;

    WebView* webView() const;

    COMPtr<IAccessible> accessible() const;

protected:
    void loadHTMLString(BSTR string, BSTR baseURL, BSTR unreachableURL);
    void loadData(PassRefPtr<WebCore::SharedBuffer>, BSTR mimeType, BSTR textEncodingName, BSTR baseURL, BSTR failingURL);
    const Vector<WebCore::IntRect>& computePageRects(HDC printDC);
    void setPrinting(bool printing, float minPageWidth, float maxPageWidth, bool adjustViewSize);
    void headerAndFooterHeights(float*, float*);
    WebCore::IntRect printerMarginRect(HDC);
    void spoolPage (PlatformGraphicsContext* pctx, WebCore::GraphicsContext* spoolCtx, HDC printDC, IWebUIDelegate*, float headerHeight, float footerHeight, UINT page, UINT pageCount);
    void drawHeader(PlatformGraphicsContext* pctx, IWebUIDelegate*, const WebCore::IntRect& pageRect, float headerHeight);
    void drawFooter(PlatformGraphicsContext* pctx, IWebUIDelegate*, const WebCore::IntRect& pageRect, UINT page, UINT pageCount, float headerHeight, float footerHeight);

protected:
    ULONG               m_refCount;
    class WebFramePrivate;
    WebFramePrivate*    d;
    bool                m_quickRedirectComing;
    WebCore::KURL       m_originalRequestURL;
    bool                m_inPrintingMode;
    Vector<WebCore::IntRect> m_pageRects;
    int m_pageHeight;   // height of the page adjusted by margins
    mutable COMPtr<AccessibleDocument> m_accessible;
};

#endif
