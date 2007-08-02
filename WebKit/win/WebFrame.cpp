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

#include "config.h"
#include "WebKitDLL.h"
#include "WebFrame.h"

#include "CFDictionaryPropertyBag.h"
#include "COMPtr.h"
#include "DOMCoreClasses.h"
#include "IWebError.h"
#include "IWebErrorPrivate.h"
#include "IWebHistory.h"
#include "IWebHistoryItemPrivate.h"
#include "IWebFrameLoadDelegatePrivate.h"
#include "IWebFormDelegate.h"
#include "IWebUIDelegatePrivate.h"
#include "MarshallingHelpers.h"
#include "WebActionPropertyBag.h"
#include "WebDocumentLoader.h"
#include "WebDownload.h"
#include "WebError.h"
#include "WebMutableURLRequest.h"
#include "WebEditorClient.h"
#include "WebFramePolicyListener.h"
#include "WebHistory.h"
#include "WebKit.h"
#include "WebKitStatisticsPrivate.h"
#include "WebNotificationCenter.h"
#include "WebView.h"
#include "WebDataSource.h"
#include "WebHistoryItem.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLResponse.h"
#pragma warning( push, 0 )
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BString.h>
#include <WebCore/Cache.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DOMImplementation.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Event.h>
#include <WebCore/FormState.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GDIObjectCounter.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLGenericFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseRelatedEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlugInInfoStore.h>
#include <WebCore/PluginDatabaseWin.h>
#include <WebCore/PluginViewWin.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleWin.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RenderFrame.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIterator.h>
#include <WebCore/kjs_binding.h>
#include <WebCore/kjs_proxy.h>
#include <WebCore/kjs_window.h>
#include <JavaScriptCore/APICast.h>
#include <wtf/MathExtras.h>
#pragma warning(pop)

#include <CoreGraphics/CoreGraphics.h>

// CG SPI used for printing
extern "C" {
    CGAffineTransform CGContextGetBaseCTM(CGContextRef c); 
    void CGContextSetBaseCTM(CGContextRef c, CGAffineTransform m); 
}

using namespace WebCore;
using namespace HTMLNames;

#define FLASH_REDRAW 0


// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
const float PrintingMinimumShrinkFactor = 1.25f;

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
const float PrintingMaximumShrinkFactor = 2.0f;


// {A3676398-4485-4a9d-87DC-CB5A40E6351D}
const GUID IID_WebFrame = 
{ 0xa3676398, 0x4485, 0x4a9d, { 0x87, 0xdc, 0xcb, 0x5a, 0x40, 0xe6, 0x35, 0x1d } };


//-----------------------------------------------------------------------------
// Helpers to convert from WebCore to WebKit type
WebFrame* kit(Frame* frame)
{
    if (!frame)
        return 0;

    FrameLoaderClient* frameLoaderClient = frame->loader()->client();
    if (frameLoaderClient)
        return static_cast<WebFrame*>(frameLoaderClient);  // eek, is there a better way than static cast?
    return 0;
}

Frame* core(WebFrame* webFrame)
{
    if (!webFrame)
        return 0;
    return webFrame->impl();
}

// This function is not in WebFrame.h because we don't want to advertise the ability to get a non-const Frame from a const WebFrame
Frame* core(const WebFrame* webFrame)
{
    if (!webFrame)
        return 0;
    return const_cast<WebFrame*>(webFrame)->impl();
}

//-----------------------------------------------------------------------------

class FormValuesPropertyBag : public IPropertyBag, public IPropertyBag2
{
public:
    FormValuesPropertyBag(HashMap<String, String>* formValues) : m_formValues(formValues) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IPropertyBag
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [in] */ LPCOLESTR pszPropName,
        /* [out][in] */ VARIANT* pVar,
        /* [in] */ IErrorLog* pErrorLog);

    virtual HRESULT STDMETHODCALLTYPE Write( 
        /* [in] */ LPCOLESTR pszPropName,
        /* [in] */ VARIANT* pVar);

    // IPropertyBag2
    virtual HRESULT STDMETHODCALLTYPE Read( 
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2 *pPropBag,
        /* [in] */ IErrorLog *pErrLog,
        /* [out] */ VARIANT *pvarValue,
        /* [out] */ HRESULT *phrError);
    
    virtual HRESULT STDMETHODCALLTYPE Write( 
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2 *pPropBag,
        /* [in] */ VARIANT *pvarValue);
    
    virtual HRESULT STDMETHODCALLTYPE CountProperties( 
        /* [out] */ ULONG *pcProperties);
    
    virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo( 
        /* [in] */ ULONG iProperty,
        /* [in] */ ULONG cProperties,
        /* [out] */ PROPBAG2 *pPropBag,
        /* [out] */ ULONG *pcProperties);
    
    virtual HRESULT STDMETHODCALLTYPE LoadObject( 
        /* [in] */ LPCOLESTR pstrName,
        /* [in] */ DWORD dwHint,
        /* [in] */ IUnknown *pUnkObject,
        /* [in] */ IErrorLog *pErrLog);
    
protected:
    HashMap<String, String>* m_formValues;
};

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag2))
        *ppvObject = static_cast<IPropertyBag2*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::AddRef(void)
{
    return 1;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::Release(void)
{
    return 0;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* /*pErrorLog*/)
{
    HRESULT hr = S_OK;

    if (!pszPropName || !pVar)
        return E_POINTER;

    String key(pszPropName);
    if (!m_formValues->contains(key))
        return E_INVALIDARG;
    
    String value = m_formValues->get(key);

    VARTYPE requestedType = V_VT(pVar);
    VariantClear(pVar);
    V_VT(pVar) = VT_BSTR;
    V_BSTR(pVar) = SysAllocStringLen(value.characters(), value.length());
    if (value.length() && !V_BSTR(pVar))
        return E_OUTOFMEMORY;

    if (requestedType != VT_BSTR && requestedType != VT_EMPTY)
        hr = VariantChangeType(pVar, pVar, VARIANT_NOUSEROVERRIDE | VARIANT_ALPHABOOL, requestedType);
    
    return hr;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;
    VariantClear(pVar);
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read( 
    /* [in] */ ULONG cProperties,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ IErrorLog* pErrLog,
    /* [out] */ VARIANT* pvarValue,
    /* [out] */ HRESULT* phrError)
{
    if (cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pvarValue || !phrError)
        return E_POINTER;

    for (ULONG i=0; i<cProperties; i++) {
        VariantInit(&pvarValue[i]);
        phrError[i] = Read(pPropBag->pstrName, &pvarValue[i], pErrLog);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write( 
    /* [in] */ ULONG /*cProperties*/,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ VARIANT* pvarValue)
{
    if (!pPropBag || !pvarValue)
        return E_POINTER;
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::CountProperties( 
    /* [out] */ ULONG* pcProperties)
{
    *pcProperties = m_formValues->size();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::GetPropertyInfo( 
    /* [in] */ ULONG iProperty,
    /* [in] */ ULONG cProperties,
    /* [out] */ PROPBAG2* pPropBag,
    /* [out] */ ULONG* pcProperties)
{
    if (iProperty > (size_t)m_formValues->size() || iProperty+cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pcProperties)
        return E_POINTER;

    *pcProperties = 0;
    ULONG i = 0;
    ULONG endProperty = iProperty + cProperties;
    for (HashMap<String, String>::iterator it = m_formValues->begin(); i<endProperty; i++) {
        if (i >= iProperty) {
            int storeIndex = (*pcProperties)++;
            pPropBag[storeIndex].dwType = PROPBAG2_TYPE_DATA;
            pPropBag[storeIndex].vt = VT_BSTR;
            pPropBag[storeIndex].cfType = CF_TEXT;
            pPropBag[storeIndex].dwHint = 0;
            pPropBag[storeIndex].pstrName = const_cast<LPOLESTR>(it->first.charactersWithNullTermination());
        }
        ++it;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::LoadObject( 
    /* [in] */ LPCOLESTR pstrName,
    /* [in] */ DWORD /*dwHint*/,
    /* [in] */ IUnknown* pUnkObject,
    /* [in] */ IErrorLog* /*pErrLog*/)
{
    if (!pstrName || !pUnkObject)
        return E_POINTER;
    return E_FAIL;
}

//-----------------------------------------------------------------------------

static Element *elementFromDOMElement(IDOMElement *element)
{
    if (!element)
        return 0;

    COMPtr<IDOMElementPrivate> elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        if (SUCCEEDED(hr))
            return ele;
    }
    return 0;
}

static HTMLFormElement *formElementFromDOMElement(IDOMElement *element)
{
    if (!element)
        return 0;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && ele && ele->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(ele);
    }
    return 0;
}

static HTMLInputElement* inputElementFromDOMElement(IDOMElement* element)
{
    if (!element)
        return 0;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && ele && ele->hasTagName(inputTag))
            return static_cast<HTMLInputElement*>(ele);
    }
    return 0;
}

// WebFramePrivate ------------------------------------------------------------

class WebFrame::WebFramePrivate {
public:
    WebFramePrivate() :frame(0), webView(0), m_policyFunction(0) { }
    ~WebFramePrivate() { }
    FrameView* frameView() { return frame ? frame->view() : 0; }

    Frame* frame;
    WebView* webView;
    FramePolicyFunction m_policyFunction;
    COMPtr<WebFramePolicyListener> m_policyListener;
};

// WebFrame ----------------------------------------------------------------

WebFrame::WebFrame()
: m_refCount(0)
, d(new WebFrame::WebFramePrivate)
, m_quickRedirectComing(false)
, m_inPrintingMode(false)
{
    WebFrameCount++;
    gClassCount++;
}

WebFrame::~WebFrame()
{
    delete d;
    WebFrameCount--;
    gClassCount--;
}

WebFrame* WebFrame::createInstance()
{
    WebFrame* instance = new WebFrame();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_WebFrame))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFramePrivate))
        *ppvObject = static_cast<IWebFramePrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebFrame::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebFrame::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebFrame -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::name( 
    /* [retval][out] */ BSTR* frameName)
{
    if (!frameName) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frameName = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    *frameName = BString(coreFrame->tree()->name()).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::webView( 
    /* [retval][out] */ IWebView** view)
{
    *view = 0;
    if (!d->webView)
        return E_FAIL;
    *view = d->webView;
    (*view)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameView( 
    /* [retval][out] */ IWebFrameView** /*view*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::DOMDocument( 
    /* [retval][out] */ IDOMDocument** result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    if (Frame* coreFrame = core(this))
        if (Document* document = coreFrame->document())
            *result = DOMDocument::createInstance(document);

    return *result ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameElement( 
    /* [retval][out] */ IDOMHTMLElement** /*frameElement*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::currentForm( 
        /* [retval][out] */ IDOMElement **currentForm)
{
    if (!currentForm) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *currentForm = 0;

    if (Frame* coreFrame = core(this))
        if (HTMLFormElement* formElement = coreFrame->currentForm())
            *currentForm = DOMElement::createInstance(formElement);

    return *currentForm ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadRequest( 
    /* [in] */ IWebURLRequest* request)
{
    COMPtr<WebMutableURLRequest> requestImpl;

    HRESULT hr = request->QueryInterface(CLSID_WebMutableURLRequest, (void**)&requestImpl);
    if (FAILED(hr))
        return hr;
 
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    coreFrame->loader()->load(requestImpl->resourceRequest());
    return S_OK;
}

void WebFrame::loadData(PassRefPtr<WebCore::SharedBuffer> data, BSTR mimeType, BSTR textEncodingName, BSTR baseURL, BSTR failingURL)
{
    String mimeTypeString(mimeType, SysStringLen(mimeType));
    if (!mimeType)
        mimeTypeString = "text/html";

    String encodingString(textEncodingName, SysStringLen(textEncodingName));
    KURL baseKURL = DeprecatedString((DeprecatedChar*)baseURL, SysStringLen(baseURL));
    KURL failingKURL = DeprecatedString((DeprecatedChar*)failingURL, SysStringLen(failingURL));

    ResourceRequest request(baseKURL);
    SubstituteData substituteData(data, mimeTypeString, encodingString, failingKURL);

    // This method is only called from IWebFrame methods, so don't ASSERT that the Frame pointer isn't null.
    if (Frame* coreFrame = core(this))
        coreFrame->loader()->load(request, substituteData);
}


HRESULT STDMETHODCALLTYPE WebFrame::loadData( 
    /* [in] */ IStream* data,
    /* [in] */ BSTR mimeType,
    /* [in] */ BSTR textEncodingName,
    /* [in] */ BSTR url)
{
    RefPtr<SharedBuffer> sharedBuffer = new SharedBuffer();

    STATSTG stat;
    if (SUCCEEDED(data->Stat(&stat, STATFLAG_NONAME))) {
        if (!stat.cbSize.HighPart && stat.cbSize.LowPart) {
            Vector<char> dataBuffer(stat.cbSize.LowPart);
            ULONG read;
            // FIXME: this does a needless copy, would be better to read right into the SharedBuffer
            // or adopt the Vector or something.
            if (SUCCEEDED(data->Read(dataBuffer.data(), (ULONG)dataBuffer.size(), &read)))
                sharedBuffer->append(dataBuffer.data(), (int)dataBuffer.size());
        }
    }

    loadData(sharedBuffer, mimeType, textEncodingName, url, 0);
    return S_OK;
}

void WebFrame::loadHTMLString(BSTR string, BSTR baseURL, BSTR unreachableURL)
{
    RefPtr<SharedBuffer> sharedBuffer = new SharedBuffer(reinterpret_cast<char*>(string), sizeof(UChar) * SysStringLen(string));
    BString utf16Encoding(TEXT("utf-16"), 6);
    loadData(sharedBuffer.release(), 0, utf16Encoding, baseURL, unreachableURL);
}

HRESULT STDMETHODCALLTYPE WebFrame::loadHTMLString( 
    /* [in] */ BSTR string,
    /* [in] */ BSTR baseURL)
{
    loadHTMLString(string, baseURL, 0);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadAlternateHTMLString( 
    /* [in] */ BSTR str,
    /* [in] */ BSTR baseURL,
    /* [in] */ BSTR unreachableURL)
{
    loadHTMLString(str, baseURL, unreachableURL);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadArchive( 
    /* [in] */ IWebArchive* /*archive*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

static inline WebDataSource *getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : 0;
}

HRESULT STDMETHODCALLTYPE WebFrame::dataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (!source) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *source = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    WebDataSource* webDataSource = getWebDataSource(coreFrame->loader()->documentLoader());

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return *source ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebFrame::provisionalDataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (!source) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *source = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    WebDataSource* webDataSource = getWebDataSource(coreFrame->loader()->provisionalDocumentLoader());

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return *source ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebFrame::stopLoading( void)
{
    if (Frame* coreFrame = core(this))
        coreFrame->loader()->stopAllLoaders();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::reload( void)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    coreFrame->loader()->reload();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::findFrameNamed( 
    /* [in] */ BSTR name,
    /* [retval][out] */ IWebFrame** frame)
{
    if (!frame) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frame = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    Frame* foundFrame = coreFrame->tree()->find(AtomicString(name, SysStringLen(name)));
    if (!foundFrame)
        return S_OK;

    WebFrame* foundWebFrame = kit(foundFrame);
    if (!foundWebFrame)
        return E_FAIL;

    return foundWebFrame->QueryInterface(IID_IWebFrame, (void**)frame);
}

HRESULT STDMETHODCALLTYPE WebFrame::parentFrame( 
    /* [retval][out] */ IWebFrame** frame)
{
    HRESULT hr = S_OK;
    *frame = 0;
    if (Frame* coreFrame = core(this))
        if (WebFrame* webFrame = kit(coreFrame->tree()->parent()))
            hr = webFrame->QueryInterface(IID_IWebFrame, (void**) frame);

    return hr;
}

class EnumChildFrames : public IEnumVARIANT
{
public:
    EnumChildFrames(Frame* f) : m_refCount(1), m_frame(f), m_curChild(f ? f->tree()->firstChild() : 0) { }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        *ppvObject = 0;
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumVARIANT))
            *ppvObject = this;
        else
            return E_NOINTERFACE;

        AddRef();
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return ++m_refCount;
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        ULONG newRef = --m_refCount;
        if (!newRef)
            delete(this);
        return newRef;
    }

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, VARIANT *rgVar, ULONG *pCeltFetched)
    {
        if (pCeltFetched)
            *pCeltFetched = 0;
        if (!rgVar)
            return E_POINTER;
        VariantInit(rgVar);
        if (!celt || celt > 1)
            return S_FALSE;
        if (!m_frame || !m_curChild)
            return S_FALSE;

        WebFrame* webFrame = kit(m_curChild);
        IUnknown* unknown;
        HRESULT hr = webFrame->QueryInterface(IID_IUnknown, (void**)&unknown);
        if (FAILED(hr))
            return hr;

        V_VT(rgVar) = VT_UNKNOWN;
        V_UNKNOWN(rgVar) = unknown;

        m_curChild = m_curChild->tree()->nextSibling();
        if (pCeltFetched)
            *pCeltFetched = 1;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        if (!m_frame)
            return S_FALSE;
        for (unsigned i = 0; i < celt && m_curChild; i++)
            m_curChild = m_curChild->tree()->nextSibling();
        return m_curChild ? S_OK : S_FALSE;
    }

    virtual HRESULT STDMETHODCALLTYPE Reset(void)
    {
        if (!m_frame)
            return S_FALSE;
        m_curChild = m_frame->tree()->firstChild();
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**)
    {
        return E_NOTIMPL;
    }

private:
    ULONG m_refCount;
    Frame* m_frame;
    Frame* m_curChild;
};

HRESULT STDMETHODCALLTYPE WebFrame::childFrames( 
    /* [retval][out] */ IEnumVARIANT **enumFrames)
{
    if (!enumFrames)
        return E_POINTER;

    *enumFrames = new EnumChildFrames(core(this));
    return S_OK;
}

// IWebFramePrivaete ------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::renderTreeAsExternalRepresentation(
    /* [retval][out] */ BSTR *result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    DeprecatedString representation = externalRepresentation(coreFrame->renderer());

    *result = SysAllocStringLen((LPCOLESTR)representation.unicode(), representation.length());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::scrollOffset(
        /* [retval][out] */ SIZE* offset)
{
    if (!offset) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    *offset = view->scrollOffset();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::layout()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    view->layout();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::firstLayoutDone(
    /* [retval][out] */ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    *result = coreFrame->loader()->firstLayoutDone();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadType( 
    /* [retval][out] */ WebFrameLoadType* type)
{
    if (!type) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *type = (WebFrameLoadType)0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    *type = (WebFrameLoadType)coreFrame->loader()->loadType();
    return S_OK;
}


// WebFrame ---------------------------------------------------------------

void WebFrame::initWithWebFrameView(IWebFrameView* /*view*/, IWebView* webView, Page* page, HTMLFrameOwnerElement* ownerElement)
{
    if (FAILED(webView->QueryInterface(CLSID_WebView, (void**)&d->webView)))
        return;
    d->webView->Release(); // don't hold the extra ref

    HWND viewWindow;
    d->webView->viewWindow((OLE_HANDLE*)&viewWindow);

    this->AddRef(); // We release this ref in frameLoaderDestroyed()
    Frame* frame = new Frame(page, ownerElement, this);
    d->frame = frame;

    FrameView* frameView = new FrameView(frame);

    frame->setView(frameView);
    frameView->deref(); // FrameViews are created with a ref count of 1. Release this ref since we've assigned it to frame.

    frameView->setContainingWindow(viewWindow);
}

void WebFrame::layoutIfNeededRecursive(FrameView* frameView)
{
    LOCAL_GDI_COUNTER(0, __FUNCTION__);

    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.
    if (!frameView) {
        frameView = d->frameView();
        if (!frameView)
            return;
    }

    // Layout if we need to.
    if (frameView->needsLayout())
        frameView->layout();

    // Now lay out any children that need to.
    HashSet<Widget*>* children = frameView->children();
    HashSet<Widget*>::iterator end = children->end();
    for (HashSet<Widget*>::iterator current = children->begin(); current != end; ++current)
        if ((*current)->isFrameView())
            layoutIfNeededRecursive(static_cast<FrameView*>(*current));
}

Frame* WebFrame::impl()
{
    return d->frame;
}

void WebFrame::invalidate()
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    if (Document* document = coreFrame->document())
        document->recalcStyle(Node::Force);
}

void WebFrame::setTextSizeMultiplier(float multiplier)
{
    int newZoomFactor = (int)round(multiplier * 100);
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    if (coreFrame->zoomFactor() == newZoomFactor)
        return;

    coreFrame->setZoomFactor(newZoomFactor);
}

HRESULT WebFrame::inViewSourceMode(BOOL* flag)
{
    if (!flag) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *flag = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    *flag = coreFrame->inViewSourceMode() ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebFrame::setInViewSourceMode(BOOL flag)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    coreFrame->setInViewSourceMode(!!flag);
    return S_OK;
}

HRESULT WebFrame::elementWithName(BSTR name, IDOMElement* form, IDOMElement** element)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
        AtomicString targetName((UChar*)name, SysStringLen(name));
        for (unsigned int i = 0; i < elements.size(); i++) {
            HTMLGenericFormElement *elt = elements[i];
            // Skip option elements, other duds
            if (elt->name() == targetName) {
                *element = DOMElement::createInstance(elt);
                return S_OK;
            }
        }
    }
    return E_FAIL;
}

HRESULT WebFrame::formForElement(IDOMElement* element, IDOMElement** form)
{
    if (!element)
        return E_INVALIDARG;

    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (!inputElement)
        return E_FAIL;

    HTMLFormElement *formElement = inputElement->form();
    if (!formElement)
        return E_FAIL;

    *form = DOMElement::createInstance(formElement);
    return S_OK;
}

HRESULT WebFrame::elementDoesAutoComplete(IDOMElement *element, bool *result)
{
    *result = false;
    if (!element)
        return E_INVALIDARG;

    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (!inputElement)
        *result = false;
    else
        *result = (inputElement->inputType() == HTMLInputElement::TEXT && inputElement->autoComplete());

    return S_OK;
}

HRESULT WebFrame::controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (!formElement)
        return E_FAIL;

    int inCount = *cControls;
    int count = (int) formElement->formElements.size();
    *cControls = count;
    if (!controls)
        return S_OK;
    if (inCount < count)
        return E_FAIL;

    *cControls = 0;
    Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
    for (int i = 0; i < count; i++) {
        if (elements.at(i)->isEnumeratable()) { // Skip option elements, other duds
            controls[*cControls] = DOMElement::createInstance(elements.at(i));
            (*cControls)++;
        }
    }
    return S_OK;
}

HRESULT WebFrame::elementIsPassword(IDOMElement *element, bool *result)
{
    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    *result = inputElement != 0
        && inputElement->inputType() == HTMLInputElement::PASSWORD;
    return S_OK;
}

HRESULT WebFrame::searchForLabelsBeforeElement(const BSTR* labels, int cLabels, IDOMElement* beforeElement, BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    if (!cLabels)
        return S_OK;
    if (cLabels < 1)
        return E_INVALIDARG;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    Vector<String> labelStrings(cLabels);
    for (int i=0; i<cLabels; i++)
        labelStrings[i] = String(labels[i], SysStringLen(labels[i]));
    Element *coreElement = elementFromDOMElement(beforeElement);
    if (!coreElement)
        return E_FAIL;

    String label = coreFrame->searchForLabelsBeforeElement(labelStrings, coreElement);
    
    *result = SysAllocStringLen(label.characters(), label.length());
    if (label.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT WebFrame::matchLabelsAgainstElement(const BSTR* labels, int cLabels, IDOMElement* againstElement, BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    if (!cLabels)
        return S_OK;
    if (cLabels < 1)
        return E_INVALIDARG;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    Vector<String> labelStrings(cLabels);
    for (int i=0; i<cLabels; i++)
        labelStrings[i] = String(labels[i], SysStringLen(labels[i]));
    Element *coreElement = elementFromDOMElement(againstElement);
    if (!coreElement)
        return E_FAIL;

    String label = coreFrame->matchLabelsAgainstElement(labelStrings, coreElement);
    
    *result = SysAllocStringLen(label.characters(), label.length());
    if (label.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT WebFrame::canProvideDocumentSource(bool* result)
{
    HRESULT hr = S_OK;
    *result = false;

    COMPtr<IWebDataSource> dataSource;
    hr = WebFrame::dataSource(&dataSource);
    if (FAILED(hr))
        return hr;

    COMPtr<IWebURLResponse> urlResponse;
    hr = dataSource->response(&urlResponse);
    if (SUCCEEDED(hr) && urlResponse) {
        BSTR mimeTypeBStr;
        if (SUCCEEDED(urlResponse->MIMEType(&mimeTypeBStr))) {
            String mimeType(mimeTypeBStr, SysStringLen(mimeTypeBStr));
            *result = mimeType == "text/html" || WebCore::DOMImplementation::isXMLMIMEType(mimeType);
            SysFreeString(mimeTypeBStr);
        }
    }
    return hr;
}

// FrameWinClient

void WebFrame::ref()
{
    this->AddRef();
}

void WebFrame::deref()
{
    this->Release();
}

void WebFrame::frameLoaderDestroyed()
{
    // The FrameLoader going away is equivalent to the Frame going away,
    // so we now need to clear our frame pointer.
    d->frame = 0;

    this->Release();
}

Frame* WebFrame::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    COMPtr<WebFrame> webFrame;
    webFrame.adoptRef(WebFrame::createInstance());

    webFrame->initWithWebFrameView(0, d->webView, coreFrame->page(), ownerElement);

    Frame* childFrame = core(webFrame.get());
    ASSERT(childFrame);

    coreFrame->tree()->appendChild(childFrame);
    childFrame->deref(); // Frames are created with a refcount of 1. Release this ref, since we've assigned it to d->frame.
    childFrame->tree()->setName(name);
    childFrame->init();

    loadURLIntoChild(URL, referrer, webFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame;
}

void WebFrame::loadURLIntoChild(const KURL& originalURL, const String& referrer, WebFrame* childFrame)
{
    ASSERT(childFrame);
    ASSERT(core(childFrame));

    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    HistoryItem* parentItem = coreFrame->loader()->currentHistoryItem();
    FrameLoadType loadType = coreFrame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedHistory;

    KURL url = originalURL;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (parentItem && parentItem->children().size() &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        if (HistoryItem* childItem = parentItem->childItemWithName(core(childFrame)->tree()->name())) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            url = childItem->originalURLString().deprecatedString();
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                core(childFrame)->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                core(childFrame)->loader()->setCurrentHistoryItem(childItem);
        }
    }

    // FIXME: Handle loading WebArchives here

    core(childFrame)->loader()->load(url, referrer, childLoadType, String(), 0, 0);
}

void WebFrame::openURL(const String& URL, const Event* triggeringEvent, bool newWindow, bool lockHistory)
{
    bool ctrlPressed = false;
    bool shiftPressed = false;
    if (triggeringEvent) {
        if (triggeringEvent->isMouseEvent()) {
            const MouseRelatedEvent* mouseEvent = static_cast<const MouseRelatedEvent*>(triggeringEvent);
            ctrlPressed = mouseEvent->ctrlKey();
            shiftPressed = mouseEvent->shiftKey();
        } else if (triggeringEvent->isKeyboardEvent()) {
            const KeyboardEvent* keyEvent = static_cast<const KeyboardEvent*>(triggeringEvent);
            ctrlPressed = keyEvent->ctrlKey();
            shiftPressed = keyEvent->shiftKey();
        }
    }

    if (ctrlPressed)
        newWindow = true;

    BString urlBStr = URL;

    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    if (FAILED(request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0)))
        goto exit;

    if (newWindow) {
        // new tab/window
        IWebUIDelegate* ui;
        IWebView* newWebView;
        if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
            if (SUCCEEDED(ui->createWebViewWithRequest(d->webView, request, &newWebView))) {
                if (shiftPressed) {
                    // Ctrl-Option-Shift-click:  Opens a link in a new window and selects it.
                    // Ctrl-Shift-click:  Opens a link in a new tab and selects it.
                    ui->webViewShow(d->webView);
                }
                newWebView->Release();
                newWebView = 0;
            }
            ui->Release();
        }
    } else {
        m_quickRedirectComing = lockHistory;
        loadRequest(request);
    }

exit:
    request->Release();
}

void WebFrame::dispatchDidHandleOnloadEvents()
{
    IWebFrameLoadDelegatePrivate* frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv))  && frameLoadDelegatePriv) {
        frameLoadDelegatePriv->didHandleOnloadEventsForFrame(d->webView, this);
        frameLoadDelegatePriv->Release();
    }
}

void WebFrame::windowScriptObjectAvailable(JSContextRef context, JSObjectRef windowObject)
{
    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)) && frameLoadDelegate) {
        frameLoadDelegate->windowScriptObjectAvailable(d->webView, context, windowObject);
        frameLoadDelegate->Release();
    }
}

WebHistory* WebFrame::webHistory()
{
    if (this != d->webView->topLevelFrame())
        return 0;

    IWebHistoryPrivate* historyInternal = WebHistory::optionalSharedHistoryInternal(); // does not add a ref
    if (!historyInternal)
        return 0;

    WebHistory* webHistory;
    if (FAILED(historyInternal->QueryInterface(CLSID_WebHistory, (void**)&webHistory)))
        return 0;

    return webHistory;
}

bool WebFrame::hasWebView() const
{
    return !!d->webView;
}

bool WebFrame::hasFrameView() const
{
    return !!d->frameView();
}

bool WebFrame::privateBrowsingEnabled() const
{
    BOOL privateBrowsingEnabled = FALSE;
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    return !!privateBrowsingEnabled;
}

void WebFrame::makeDocumentView()
{
    ASSERT(core(this));
    
    // On the mac, this is done in Frame::setView, but since we don't have separate 
    // frame views, we'll just do it here instead.
    core(this)->loader()->resetMultipleFormSubmissionProtection();
}

void WebFrame::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::forceLayout()
{
    notImplemented();
}

void WebFrame::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrame::setCopiesOnScroll()
{
    notImplemented();
}

void WebFrame::detachedFromParent1()
{
    notImplemented();
}

void WebFrame::detachedFromParent2()
{
    notImplemented();
}

void WebFrame::detachedFromParent3()
{
    notImplemented();
}

void WebFrame::detachedFromParent4()
{
    notImplemented();
}

void WebFrame::loadedFromCachedPage()
{
    notImplemented();
}

void WebFrame::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidCancelClientRedirect()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCancelClientRedirectForFrame(d->webView, this);
}

void WebFrame::dispatchWillPerformClientRedirect(const KURL& url, double delay, double fireDate)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willPerformClientRedirectToURL(d->webView, BString(url.url()), delay, MarshallingHelpers::CFAbsoluteTimeToDATE(fireDate), this);
}

void WebFrame::dispatchDidChangeLocationWithinPage()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didChangeLocationWithinPageForFrame(d->webView, this);
}

void WebFrame::dispatchWillClose()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willCloseFrame(d->webView, this);
}

void WebFrame::dispatchDidReceiveIcon()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        // FIXME: Pass in the right HBITMAP. 
        frameLoadDelegate->didReceiveIcon(d->webView, 0, this);
}

void WebFrame::dispatchDidStartProvisionalLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didStartProvisionalLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidReceiveTitle(const String& title)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveTitle(d->webView, BString(title), this);
}

void WebFrame::dispatchDidCommitLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) 
        frameLoadDelegate->didCommitLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidFinishDocumentLoad()
{
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFinishDocumentLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidFinishLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) 
        frameLoadDelegate->didFinishLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidFirstLayout()
{
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFirstLayoutInFrame(d->webView, this);
}

void WebFrame::dispatchShow()
{
    COMPtr<IWebUIDelegate> ui;

    if (SUCCEEDED(d->webView->uiDelegate(&ui)))
        ui->webViewShow(d->webView);
}

void WebFrame::cancelPolicyCheck()
{
    if (d->m_policyListener) {
        d->m_policyListener->invalidate();
        d->m_policyListener = 0;
    }

    d->m_policyFunction = 0;
}

void WebFrame::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState> formState)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    COMPtr<IWebFormDelegate> formDelegate;

    if (FAILED(d->webView->formDelegate(&formDelegate))) {
        (coreFrame->loader()->*function)(PolicyUse);
        return;
    }

    COMPtr<IDOMElement> formElement;
    formElement.adoptRef(DOMElement::createInstance(formState->form()));

    // FIXME: The FormValuesPropertyBag constructor should take a const pointer
    FormValuesPropertyBag formValuesPropBag(const_cast<HashMap<String, String>*>(&formState->values()));

    COMPtr<WebFrame> sourceFrame(kit(formState->sourceFrame()));
    if (SUCCEEDED(formDelegate->willSubmitForm(this, sourceFrame.get(), formElement.get(), &formValuesPropBag, setUpPolicyListener(function).get())))
        return;

    // FIXME: Add a sane default implementation
    (coreFrame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::clearUnarchivingState(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrame::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::finishedLoading(DocumentLoader* loader)
{
    // Telling the frame we received some data and passing 0 as the data is our
    // way to get work done that is noramlly done when the first bit of data is
    // received, even for the case of a documenbt with no data (like about:blank)
    committedLoad(loader, 0, 0);
}

void WebFrame::finalSetupForReplace(DocumentLoader*)
{
    notImplemented();
}

void WebFrame::setDefersLoading(bool)
{
    notImplemented();
}

bool WebFrame::isArchiveLoadPending(ResourceLoader*) const
{
    notImplemented();
    return false;
}

void WebFrame::cancelPendingArchiveLoad(ResourceLoader*)
{
    notImplemented();
}

void WebFrame::clearArchivedResources()
{
    notImplemented();
}

bool WebFrame::canHandleRequest(const ResourceRequest& request) const
{
    return WebView::canHandleRequest(request);
}

bool WebFrame::canShowMIMEType(const String& /*MIMEType*/) const
{
    notImplemented();
    return true;
}

bool WebFrame::representationExistsForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    return false;
}

String WebFrame::generatedMIMETypeForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    ASSERT_NOT_REACHED();
    return String();
}

void WebFrame::frameLoadCompleted()
{
    if (Frame* coreFrame = core(this))
        coreFrame->loader()->setPreviousHistoryItem(0);
}

void WebFrame::restoreViewState()
{
    // FIXME: Need to restore view state for page caching
    notImplemented();
}

void WebFrame::provisionalLoadStarted()
{
    notImplemented();
}

bool WebFrame::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    notImplemented();
    return false;
}

void WebFrame::addHistoryItemForFragmentScroll()
{
    notImplemented();
}

void WebFrame::didFinishLoad()
{
    notImplemented();
}

void WebFrame::prepareForDataSourceReplacement()
{
    notImplemented();
}

void WebFrame::setTitle(const String& title, const KURL& url)
{
    BOOL privateBrowsingEnabled = FALSE;
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    if (!privateBrowsingEnabled) {
        // update title in global history
        COMPtr<WebHistory> history;
        history.adoptRef(webHistory());
        if (history) {
            COMPtr<IWebHistoryItem> item;
            if (SUCCEEDED(history->itemForURL(BString(url.url()), &item))) {
                COMPtr<IWebHistoryItemPrivate> itemPrivate;
                if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate)))
                    itemPrivate->setTitle(BString(title));
            }
        }
    }
}

String WebFrame::userAgent(const KURL& url)
{
    return d->webView->userAgentForKURL(url);
}

void WebFrame::setDocumentViewFromCachedPage(CachedPage*)
{
    notImplemented();
}

void WebFrame::updateGlobalHistoryForStandardLoad(const KURL& url)
{
    COMPtr<WebHistory> history;
    history.adoptRef(webHistory());

    if (!history)
        return;

    history->addItemForURL(BString(url.url()), 0);                 
}

void WebFrame::updateGlobalHistoryForReload(const KURL& url)
{
    BString urlBStr(url.url());

    COMPtr<WebHistory> history;
    history.adoptRef(webHistory());

    if (!history)
        return;

    COMPtr<IWebHistoryItem> item;
    if (SUCCEEDED(history->itemForURL(urlBStr, &item))) {
        COMPtr<IWebHistoryItemPrivate> itemPrivate;
        if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate))) {
            SYSTEMTIME currentTime;
            GetSystemTime(&currentTime);
            DATE visitedTime = 0;
            SystemTimeToVariantTime(&currentTime, &visitedTime);

            // FIXME - bumping the last visited time doesn't mark the history as changed
            itemPrivate->setLastVisitedTimeInterval(visitedTime);
        }
    }
}

bool WebFrame::shouldGoToHistoryItem(HistoryItem*) const
{
    notImplemented();
    return true;
}

void WebFrame::saveViewStateToItem(HistoryItem*)
{
    // FIXME: Need to save view state for page caching
    notImplemented();
}

void WebFrame::saveDocumentViewToCachedPage(CachedPage*)
{
    notImplemented();
}

bool WebFrame::canCachePage() const
{
    notImplemented();
    return false;
}

PassRefPtr<DocumentLoader> WebFrame::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoader> loader = new WebDocumentLoader(request, substituteData);
 
    COMPtr<WebDataSource> dataSource;
    dataSource.adoptRef(WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

void WebFrame::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
    notImplemented();
}

ResourceError WebFrame::cancelledError(const ResourceRequest& request)
{
    // FIXME: Need ChickenCat to include CFNetwork/CFURLError.h to get these values
    // Alternatively, we could create our own error domain/codes.
    return ResourceError(String(WebURLErrorDomain), -999, request.url().url(), String());
}

ResourceError WebFrame::blockedError(const ResourceRequest& request)
{
    // FIXME: Need to implement the String descriptions for errors in the WebKitErrorDomain and have them localized
    return ResourceError(String(WebKitErrorDomain), WebKitErrorCannotUseRestrictedPort, request.url().url(), String());
}

ResourceError WebFrame::cannotShowURLError(const ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

ResourceError WebFrame::interruptForPolicyChangeError(const ResourceRequest& request)
{
    // FIXME: Need to implement the String descriptions for errors in the WebKitErrorDomain and have them localized
    return ResourceError(String(WebKitErrorDomain), WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().url(), String());
}

ResourceError WebFrame::cannotShowMIMETypeError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

ResourceError WebFrame::fileDoesNotExistError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

bool WebFrame::shouldFallBack(const ResourceError& error)
{
    return error.errorCode() != WebURLErrorCancelled;
}

void WebFrame::receivedData(const char* data, int length, const String& textEncoding)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return;

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = coreFrame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    coreFrame->loader()->setEncoding(encoding, userChosen);

    coreFrame->loader()->addData(data, length);
}

COMPtr<WebFramePolicyListener> WebFrame::setUpPolicyListener(WebCore::FramePolicyFunction function)
{
    ASSERT(!d->m_policyListener);
    ASSERT(!d->m_policyFunction);

    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    d->m_policyListener.adoptRef(WebFramePolicyListener::createInstance(coreFrame));
    d->m_policyFunction = function;

    return d->m_policyListener;
}

void WebFrame::receivedPolicyDecision(PolicyAction action)
{
    ASSERT(d->m_policyListener);
    ASSERT(d->m_policyFunction);

    FramePolicyFunction function = d->m_policyFunction;

    d->m_policyListener = 0;
    d->m_policyFunction = 0;

    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    (coreFrame->loader()->*function)(action);
}

void WebFrame::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // FIXME: This should probably go through the data source.
    const String& textEncoding = loader->response().textEncodingName();

    receivedData(data, length, textEncoding);
}

void WebFrame::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const String& mimeType, const ResourceRequest& request)
{
    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (SUCCEEDED(d->webView->policyDelegate(&policyDelegate))) {
        COMPtr<IWebURLRequest> urlRequest;
        urlRequest.adoptRef(WebMutableURLRequest::createInstance(request));
        if (SUCCEEDED(policyDelegate->decidePolicyForMIMEType(d->webView, BString(mimeType), urlRequest.get(), this, setUpPolicyListener(function).get())))
            return;
    }

    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    // FIXME: This is a stopgap default implementation to tide us over until
    // <rdar://4911042/> is taken care of
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType) || MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        (coreFrame->loader()->*function)(PolicyUse);
    else
        (coreFrame->loader()->*function)(PolicyDownload);
}

void WebFrame::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request, const String& frameName)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (SUCCEEDED(d->webView->policyDelegate(&policyDelegate))) {
        COMPtr<IWebURLRequest> urlRequest;
        urlRequest.adoptRef(WebMutableURLRequest::createInstance(request));
        COMPtr<WebActionPropertyBag> actionInformation;
        actionInformation.adoptRef(WebActionPropertyBag::createInstance(action, coreFrame));

        if (SUCCEEDED(policyDelegate->decidePolicyForNewWindowAction(d->webView, actionInformation.get(), urlRequest.get(), BString(frameName), setUpPolicyListener(function).get())))
            return;
    }

    // FIXME: Add a sane default implementation
    (coreFrame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (SUCCEEDED(d->webView->policyDelegate(&policyDelegate))) {
        COMPtr<IWebURLRequest> urlRequest;
        urlRequest.adoptRef(WebMutableURLRequest::createInstance(request));
        COMPtr<WebActionPropertyBag> actionInformation;
        actionInformation.adoptRef(WebActionPropertyBag::createInstance(action, coreFrame));

        if (SUCCEEDED(policyDelegate->decidePolicyForNavigationAction(d->webView, actionInformation.get(), urlRequest.get(), this, setUpPolicyListener(function).get())))
            return;
    }

    // FIXME: Add a sane default implementation
    (coreFrame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchUnableToImplementPolicy(const ResourceError& error)
{
    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (SUCCEEDED(d->webView->policyDelegate(&policyDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        policyDelegate->unableToImplementPolicyWithError(d->webView, webError.get(), this);
    }
}

void WebFrame::download(ResourceHandle* handle, const ResourceRequest& request, const ResourceRequest&, const ResourceResponse& response)
{
    COMPtr<IWebDownloadDelegate> downloadDelegate;
    COMPtr<IWebView> webView;
    if (SUCCEEDED(this->webView(&webView))) {
        if (FAILED(webView->downloadDelegate(&downloadDelegate))) {
            // If the WebView doesn't successfully provide a download delegate we'll pass a null one
            // into the WebDownload - which may or may not decide to use a DefaultDownloadDelegate
            LOG_ERROR("Failed to get downloadDelegate from WebView");
            downloadDelegate = 0;
        }
    }

    // Its the delegate's job to ref the WebDownload to keep it alive - otherwise it will be destroyed
    // when this method returns
    COMPtr<WebDownload> download;
    download.adoptRef(WebDownload::createInstance(handle, request, response, downloadDelegate.get()));
}

bool WebFrame::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL&) const
{
    notImplemented();
    return false;
}

void WebFrame::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLRequest> webURLRequest;
        webURLRequest.adoptRef(WebMutableURLRequest::createInstance(request));

        resourceLoadDelegate->identifierForInitialRequest(d->webView, webURLRequest.get(), getWebDataSource(loader), identifier);
    }
}

void WebFrame::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLRequest> webURLRequest;
        webURLRequest.adoptRef(WebMutableURLRequest::createInstance(request));
        COMPtr<IWebURLResponse> webURLRedirectResponse;
        webURLRedirectResponse.adoptRef(WebURLResponse::createInstance(redirectResponse));
        COMPtr<IWebURLRequest> newWebURLRequest;

        if (FAILED(resourceLoadDelegate->willSendRequest(d->webView, identifier, webURLRequest.get(), webURLRedirectResponse.get(), getWebDataSource(loader), &newWebURLRequest)))
            return;

        if (webURLRequest == newWebURLRequest)
            return;

        COMPtr<WebMutableURLRequest> newWebURLRequestImpl;
        if (FAILED(newWebURLRequest->QueryInterface(CLSID_WebMutableURLRequest, (void**)&newWebURLRequestImpl)))
            return;

        request = newWebURLRequestImpl->resourceRequest();
    }
}

void WebFrame::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLResponse> webURLResponse;
        webURLResponse.adoptRef(WebURLResponse::createInstance(response));

        resourceLoadDelegate->didReceiveResponse(d->webView, identifier, webURLResponse.get(), getWebDataSource(loader));
    }
}

void WebFrame::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        resourceLoadDelegate->didReceiveContentLength(d->webView, identifier, length, getWebDataSource(loader));
}

void WebFrame::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        resourceLoadDelegate->didFinishLoadingFromDataSource(d->webView, identifier, getWebDataSource(loader));
}

void WebFrame::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        resourceLoadDelegate->didFailLoadingWithError(d->webView, identifier, webError.get(), getWebDataSource(loader));
    }
}

bool WebFrame::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int /*length*/)
{
    notImplemented();
    return false;
}

void WebFrame::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailProvisionalLoadWithError(d->webView, webError.get(), this);
    }
}

void WebFrame::dispatchDidFailLoad(const ResourceError& error)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailLoadWithError(d->webView, webError.get(), this);
    }
}

Frame* WebFrame::dispatchCreatePage()
{
    COMPtr<IWebUIDelegate> ui;

    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
        COMPtr<IWebView> newWebView;

        if (SUCCEEDED(ui->createWebViewWithRequest(d->webView, 0, &newWebView))) {
            COMPtr<IWebFrame> mainFrame;

            if (SUCCEEDED(newWebView->mainFrame(&mainFrame))) {
                COMPtr<WebFrame> mainFrameImpl;

                if (SUCCEEDED(mainFrame->QueryInterface(IID_WebFrame, (void**)&mainFrameImpl)))
                    return core(mainFrameImpl.get());
            }
        }
    }
    return 0;
}

void WebFrame::postProgressStartedNotification()
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::postProgressEstimateChangedNotification()
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::postProgressFinishedNotification()
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::startDownload(const ResourceRequest&)
{
    notImplemented();
}

void WebFrame::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    ASSERT(challenge.sourceHandle());

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLAuthenticationChallenge> webChallenge;
        webChallenge.adoptRef(WebURLAuthenticationChallenge::createInstance(challenge));

        if (SUCCEEDED(resourceLoadDelegate->didReceiveAuthenticationChallenge(d->webView, identifier, webChallenge.get(), getWebDataSource(loader))))
            return;
    }

    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.sourceHandle()->receivedRequestToContinueWithoutCredential(challenge);
}

void WebFrame::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLAuthenticationChallenge> webChallenge;
        webChallenge.adoptRef(WebURLAuthenticationChallenge::createInstance(challenge));

        if (SUCCEEDED(resourceLoadDelegate->didCancelAuthenticationChallenge(d->webView, identifier, webChallenge.get(), getWebDataSource(loader))))
            return;
    }
}

Frame* WebFrame::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                            const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    Frame* result = createFrame(url, name, ownerElement, referrer);
    if (!result)
        return 0;

    // Propagate the marginwidth/height and scrolling modes to the view.
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            result->view()->setScrollbarsMode(ScrollbarAlwaysOff);
        int marginWidth = frameElt->getMarginWidth();
        int marginHeight = frameElt->getMarginHeight();
        if (marginWidth != -1)
            result->view()->setMarginWidth(marginWidth);
        if (marginHeight != -1)
            result->view()->setMarginHeight(marginHeight);
    }

    return result;
}

Widget* WebFrame::createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool /*loadManually*/)
{
    PluginViewWin* pluginView = PluginDatabaseWin::installedPlugins()->createPluginView(core(this), element, url, paramNames, paramValues, mimeType);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;

    if (FAILED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return pluginView;

    RetainPtr<CFMutableDictionaryRef> userInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    unsigned size = (unsigned)paramNames.size();
    for (unsigned i = 0; i < size; i++) {
        if (paramNames[i] == "pluginspage") {
            static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInPageURLStringKey);
            RetainPtr<CFStringRef> str(AdoptCF, paramValues[i].createCFString());
            CFDictionarySetValue(userInfo.get(), key, str.get());
            break;
        }
    }

    if (!mimeType.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorMIMETypeKey);

        RetainPtr<CFStringRef> str(AdoptCF, mimeType.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    String pluginName;
    if (pluginView->plugin())
        pluginName = pluginView->plugin()->name();
    if (!pluginName.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInNameKey);
        RetainPtr<CFStringRef> str(AdoptCF, mimeType.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    COMPtr<CFDictionaryPropertyBag> userInfoBag(AdoptCOM, CFDictionaryPropertyBag::createInstance());
    userInfoBag->setDictionary(userInfo.get());
 
    int errorCode = 0;
    switch (pluginView->status()) {
        case PluginStatusCanNotFindPlugin:
            errorCode = WebKitErrorCannotFindPlugIn;
            break;
        case PluginStatusCanNotLoadPlugin:
            errorCode = WebKitErrorCannotLoadPlugIn;
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    ResourceError resourceError(String(WebKitErrorDomain), errorCode, url.url(), String());
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));
     
    resourceLoadDelegate->plugInFailedWithError(d->webView, error.get(), getWebDataSource(d->frame->loader()->documentLoader()));

    return pluginView;
}

void WebFrame::redirectDataToPlugin(Widget* /*pluginWidget*/)
{
    // FIXME: Don't hardcode the error domain
    ResourceError error(String(WebKitErrorDomain), WebKitErrorPlugInWillHandleLoad, String(), String());

    // FIXME: We should really redirect the data coming in to the plugin instead of
    // cancelling the load and letting the plugin start another one.
    // Ideally, this function shouldn't be necessary, see <rdar://problem/4852889>

    ASSERT(core(this));
    core(this)->loader()->documentLoader()->cancelMainResourceLoad(error);
}
    
Widget* WebFrame::createJavaAppletWidget(const IntSize&, Element* element, const KURL& /*baseURL*/, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    PluginViewWin* pluginView = PluginDatabaseWin::installedPlugins()->
        createPluginView(core(this), element, KURL(), paramNames, paramValues, "application/x-java-applet");

    // Check if the plugin can be loaded successfully
    if (pluginView->plugin()->load())
        return pluginView;

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return pluginView;

    COMPtr<CFDictionaryPropertyBag> userInfoBag(AdoptCOM, CFDictionaryPropertyBag::createInstance());

    ResourceError resourceError(String(WebKitErrorDomain), WebKitErrorJavaUnavailable, String(), String());
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));
     
    resourceLoadDelegate->plugInFailedWithError(d->webView, error.get(), getWebDataSource(d->frame->loader()->documentLoader()));

    return pluginView;
}

ObjectContentType WebFrame::objectContentType(const KURL& url, const String& mimeTypeIn)
{
    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty()) {
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(url.path().mid(url.path().findRev('.')+1));
        if(mimeType.isEmpty())
            return WebCore::ObjectContentNone;
    }

    if (mimeType.isEmpty())
        return ObjectContentFrame; // Go ahead and hope that we can display the content.

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return WebCore::ObjectContentImage;

    if (PluginDatabaseWin::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentPlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return WebCore::ObjectContentFrame;

    return (ObjectContentType)0;
}

String WebFrame::overrideMediaType() const
{
    notImplemented();
    return String();
}

void WebFrame::windowObjectCleared() const
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    Settings* settings = coreFrame->settings();
    if (!settings || !settings->isJavaScriptEnabled())
        return;

    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        JSContextRef context = toRef(coreFrame->scriptProxy()->interpreter()->globalExec());
        JSObjectRef windowObject = toRef(KJS::Window::retrieve(coreFrame)->getObject());
        ASSERT(windowObject);

        frameLoadDelegate->windowScriptObjectAvailable(d->webView, context, windowObject);
    }
}

static IntRect printerRect(HDC printDC)
{
    return IntRect(0, 0, 
                   GetDeviceCaps(printDC, PHYSICALWIDTH)  - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETX),
                   GetDeviceCaps(printDC, PHYSICALHEIGHT) - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETY));
}

void WebFrame::setPrinting(bool printing, float minPageWidth, float maxPageWidth, bool adjustViewSize)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);
    coreFrame->setPrinting(printing, minPageWidth, maxPageWidth, adjustViewSize);
}

HRESULT STDMETHODCALLTYPE WebFrame::setInPrintingMode( 
    /* [in] */ BOOL value,
    /* [in] */ HDC printDC)
{
    if (m_inPrintingMode == !!value)
        return S_OK;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    m_inPrintingMode = !!value;

    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0f;
    float maxLayoutWidth = 0.0f;
    if (m_inPrintingMode && !coreFrame->isFrameSet()) {
        if (!printDC) {
            ASSERT_NOT_REACHED();
            return E_POINTER;
        }

        const int desiredHorizontalPixelsPerInch = 72;
        int paperHorizontalPixelsPerInch = ::GetDeviceCaps(printDC, LOGPIXELSX);
        int paperWidth = printerRect(printDC).width() * desiredHorizontalPixelsPerInch / paperHorizontalPixelsPerInch;
        minLayoutWidth = paperWidth * PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth * PrintingMaximumShrinkFactor;
    }

    setPrinting(m_inPrintingMode, minLayoutWidth, maxLayoutWidth, true);

    if (!m_inPrintingMode)
        m_pageRects.clear();

    return S_OK;
}

void WebFrame::headerAndFooterHeights(float* headerHeight, float* footerHeight)
{
    if (headerHeight)
        *headerHeight = 0;
    if (footerHeight)
        *footerHeight = 0;
    float height = 0;
    COMPtr<IWebUIDelegate> ui;
    if (FAILED(d->webView->uiDelegate(&ui)))
        return;
    COMPtr<IWebUIDelegate2> ui2;
    if (FAILED(ui->QueryInterface(IID_IWebUIDelegate2, (void**) &ui2)))
        return;
    if (headerHeight && SUCCEEDED(ui2->webViewHeaderHeight(d->webView, &height)))
        *headerHeight = height;
    if (footerHeight && SUCCEEDED(ui2->webViewFooterHeight(d->webView, &height)))
        *footerHeight = height;
}

const Vector<WebCore::IntRect>& WebFrame::computePageRects(HDC printDC)
{
    ASSERT(m_inPrintingMode);
    
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);
    ASSERT(coreFrame->document());

    if (m_pageRects.size())
        return m_pageRects;

    if (!printDC)
        return m_pageRects;

    // adjust the page rect by the header and footer
    float headerHeight = 0, footerHeight = 0;
    headerAndFooterHeights(&headerHeight, &footerHeight);
    m_pageRects = computePageRectsForFrame(coreFrame, printerRect(printDC), headerHeight, footerHeight, 1.0);
    
    return m_pageRects;
}

HRESULT STDMETHODCALLTYPE WebFrame::getPrintedPageCount( 
    /* [in] */ HDC printDC,
    /* [retval][out] */ UINT *pageCount)
{
    if (!pageCount || !printDC) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *pageCount = 0;

    if (!m_inPrintingMode) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    Frame* coreFrame = core(this);
    if (!coreFrame || !coreFrame->document())
        return E_FAIL;

    const Vector<IntRect>& pages = computePageRects(printDC);
    *pageCount = (UINT) pages.size();
    
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::spoolPages( 
    /* [in] */ HDC printDC,
    /* [in] */ UINT startPage,
    /* [in] */ UINT endPage,
    /* [retval][out] */ void* ctx)
{
    if (!printDC || !ctx) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (!m_inPrintingMode) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    Frame* coreFrame = core(this);
    if (!coreFrame || !coreFrame->document())
        return E_FAIL;

    UINT pageCount = (UINT) m_pageRects.size();
    PlatformGraphicsContext* pctx = (PlatformGraphicsContext*)ctx;

    if (!pageCount || startPage > pageCount) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    if (startPage > 0)
        startPage--;

    if (endPage == 0)
        endPage = pageCount;

    COMPtr<IWebUIDelegate> ui;
    if (FAILED(d->webView->uiDelegate(&ui)))
        return E_FAIL;
    // FIXME: we can return early after the updated app is released
    COMPtr<IWebUIDelegate2> ui2;
    if (FAILED(ui->QueryInterface(IID_IWebUIDelegate2, (void**) &ui2)))
        ui2 = 0;

    float headerHeight = 0, footerHeight = 0;
    headerAndFooterHeights(&headerHeight, &footerHeight);
    GraphicsContext spoolCtx(pctx);

    for (UINT ii = startPage; ii < endPage; ii++) {
        IntRect pageRect = m_pageRects[ii];

        CGContextSaveGState(pctx);

        IntRect printRect = printerRect(printDC);
        CGRect mediaBox = CGRectMake(CGFloat(0),
                                     CGFloat(0),
                                     CGFloat(printRect.width()),
                                     CGFloat(printRect.height()));

        CGContextBeginPage(pctx, &mediaBox);

        CGFloat scale = (float)mediaBox.size.width/ (float)pageRect.width();
        CGAffineTransform ctm = CGContextGetBaseCTM(pctx);
        ctm = CGAffineTransformScale(ctm, -scale, -scale);
        ctm = CGAffineTransformTranslate(ctm, CGFloat(-pageRect.x()), CGFloat(-pageRect.y()+headerHeight)); // reserves space for header
        CGContextScaleCTM(pctx, scale, scale);
        CGContextTranslateCTM(pctx, CGFloat(-pageRect.x()), CGFloat(-pageRect.y()+headerHeight));   // reserves space for header
        CGContextSetBaseCTM(pctx, ctm);

        coreFrame->paint(&spoolCtx, pageRect);

        if (ui2) {
            CGContextTranslateCTM(pctx, CGFloat(pageRect.x()), CGFloat(pageRect.y())-headerHeight);

            int x = pageRect.x();
            int y = 0;
            if (headerHeight) {
                RECT headerRect = {x, y, x+pageRect.width(), y+(int)headerHeight};
                ui2->drawHeaderInRect(d->webView, &headerRect, (OLE_HANDLE)(LONG64)pctx);
            }

            if (footerHeight) {
                y = (int)(mediaBox.size.height/scale - footerHeight);
                RECT footerRect = {x, y, x+pageRect.width(), y+(int)footerHeight};
                ui2->drawFooterInRect(d->webView, &footerRect, (OLE_HANDLE)(LONG64)pctx, ii+1, pageCount);
            }
        }

        CGContextEndPage(pctx);
        CGContextRestoreGState(pctx);
    }
 
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::isFrameSet( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    *result = coreFrame->isFrameSet() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::string( 
    /* [retval][out] */ BSTR *result)
{
    *result = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    RefPtr<Range> allRange(rangeOfContents(coreFrame->document()));
    DeprecatedString allString = plainText(allRange.get());
    *result = BString(allString).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::size( 
    /* [retval][out] */ SIZE *size)
{
    if (!size)
        return E_POINTER;
    size->cx = size->cy = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;
    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;
    size->cx = view->width();
    size->cy = view->height();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::hasScrollBars( 
    /* [retval][out] */ BOOL *result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    if (view->vScrollbarMode() == ScrollbarAlwaysOn || view->visibleHeight() < view->contentsHeight() ||
            view->hScrollbarMode() == ScrollbarAlwaysOn || view->visibleWidth() < view->contentsWidth())
        *result = TRUE;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::contentBounds( 
    /* [retval][out] */ RECT *result)
{
    if (!result)
        return E_POINTER;
    ::SetRectEmpty(result);

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    result->bottom = view->contentsHeight();
    result->right = view->contentsWidth();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameBounds( 
    /* [retval][out] */ RECT *result)
{
    if (!result)
        return E_POINTER;
    ::SetRectEmpty(result);

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_FAIL;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    FloatRect bounds = view->visibleContentRectConsideringExternalScrollers();
    result->bottom = (LONG) bounds.height();
    result->right = (LONG) bounds.width();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::isDescendantOfFrame( 
    /* [in] */ IWebFrame *ancestor,
    /* [retval][out] */ BOOL *result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;

    Frame* coreFrame = core(this);
    COMPtr<WebFrame> ancestorWebFrame;
    if (!ancestor || FAILED(ancestor->QueryInterface(IID_WebFrame, (void**)&ancestorWebFrame)))
        return S_OK;

    *result = (coreFrame && coreFrame->tree()->isDescendantOf(core(ancestorWebFrame.get()))) ? TRUE : FALSE;
    return S_OK;
}

void WebFrame::unmarkAllMisspellings()
{
    Frame* coreFrame = core(this);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Spelling);
    }
}

void WebFrame::unmarkAllBadGrammar()
{
    Frame* coreFrame = core(this);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Grammar);
    }
}
