/*
 * Copyright (C) 2006-2009, 2011, 2013-2015 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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

#include "WebKitDLL.h"
#include "WebFrame.h"

#include "CFDictionaryPropertyBag.h"
#include "COMPropertyBag.h"
#include "DOMCoreClasses.h"
#include "MarshallingHelpers.h"
#include "PluginDatabase.h"
#include "PluginView.h"
#include "WebActionPropertyBag.h"
#include "WebChromeClient.h"
#include "WebDataSource.h"
#include "WebDocumentLoader.h"
#include "WebDownload.h"
#include "WebEditorClient.h"
#include "WebError.h"
#include "WebFrameLoaderClient.h"
#include "WebFramePolicyListener.h"
#include "WebHistory.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebKitStatisticsPrivate.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebScriptWorld.h"
#include "WebURLResponse.h"
#include "WebView.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSObject.h>
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/CSSAnimationController.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/Event.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GDIObjectCounter.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLAppletElement.h>
#include <WebCore/HTMLFormControlElement.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/JSDOMBinding.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MouseRelatedEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PluginData.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/PrintContext.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/RenderView.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIterator.h>
#include <wtf/MathExtras.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#elif USE(CAIRO)
#include <WebCore/PlatformContextCairo.h>
#include <cairo-win32.h>
#endif

#if USE(CG)
// CG SPI used for printing
extern "C" {
    CGAffineTransform CGContextGetBaseCTM(CGContextRef c); 
    void CGContextSetBaseCTM(CGContextRef c, CGAffineTransform m); 
}
#endif

using namespace WebCore;
using namespace HTMLNames;
using namespace std;

using JSC::JSGlobalObject;
using JSC::JSLock;
using JSC::JSValue;

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

//-----------------------------------------------------------------------------
// Helpers to convert from WebCore to WebKit type
WebFrame* kit(Frame* frame)
{
    if (!frame)
        return nullptr;

    // FIXME: Doesn't this need to be aware of EmptyFrameLoaderClient?
    FrameLoaderClient& frameLoaderClient = frame->loader().client();
    return static_cast<WebFrameLoaderClient&>(frameLoaderClient).webFrame();
}

Frame* core(WebFrame* webFrame)
{
    if (!webFrame)
        return nullptr;
    return webFrame->impl();
}

// This function is not in WebFrame.h because we don't want to advertise the ability to get a non-const Frame from a const WebFrame
Frame* core(const WebFrame* webFrame)
{
    if (!webFrame)
        return nullptr;
    return const_cast<WebFrame*>(webFrame)->impl();
}

//-----------------------------------------------------------------------------

static Element* elementFromDOMElement(IDOMElement* element)
{
    if (!element)
        return nullptr;

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
        return nullptr;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && is<HTMLFormElement>(ele))
            return downcast<HTMLFormElement>(ele);
    }
    return nullptr;
}

static HTMLInputElement* inputElementFromDOMElement(IDOMElement* element)
{
    if (!element)
        return nullptr;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && is<HTMLInputElement>(ele))
            return downcast<HTMLInputElement>(ele);
    }
    return nullptr;
}

// WebFramePrivate ------------------------------------------------------------

class WebFrame::WebFramePrivate {
public:
    WebFramePrivate() 
    {
    }

    ~WebFramePrivate() { }
    FrameView* frameView() { return frame ? frame->view() : nullptr; }

    Frame* frame { nullptr };
    WebView* webView { nullptr };
};

// WebFrame ----------------------------------------------------------------

WebFrame::WebFrame()
    : d(new WebFrame::WebFramePrivate)
{
    WebFrameCount++;
    gClassCount++;
    gClassNameCount().add("WebFrame");
}

WebFrame::~WebFrame()
{
    delete d;
    WebFrameCount--;
    gClassCount--;
    gClassNameCount().remove("WebFrame");
}

WebFrame* WebFrame::createInstance()
{
    WebFrame* instance = new WebFrame();
    instance->AddRef();
    return instance;
}

HRESULT WebFrame::setAllowsScrolling(BOOL flag)
{
    if (Frame* frame = core(this))
        if (FrameView* view = frame->view())
            view->setCanHaveScrollbars(!!flag);

    return S_OK;
}

HRESULT WebFrame::allowsScrolling(_Out_ BOOL* flag)
{
    if (!flag)
        return E_POINTER;

    *flag = FALSE;

    if (Frame* frame = core(this)) {
        if (FrameView* view = frame->view())
            *flag = view->canHaveScrollbars();
    }

    return S_OK;
}

HRESULT WebFrame::setIsDisconnected(BOOL flag)
{
    ASSERT_NOT_REACHED();
    return E_FAIL;
}

HRESULT WebFrame::setExcludeFromTextSearch(BOOL flag)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebFrame::reloadFromOrigin()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    coreFrame->loader().reload(WebCore::ReloadOption::FromOrigin);
    return S_OK;
}

HRESULT WebFrame::paintDocumentRectToContext(RECT rect, _In_ HDC deviceContext)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    // We can't paint with a layout still pending.
    view->updateLayoutAndStyleIfNeededRecursive();

    GraphicsContext gc(deviceContext);
    gc.setShouldIncludeChildWindows(true);
    gc.save();
    LONG width = rect.right - rect.left;
    LONG height = rect.bottom - rect.top;
    FloatRect dirtyRect;
    dirtyRect.setWidth(width);
    dirtyRect.setHeight(height);
    gc.clip(dirtyRect);
    gc.translate(-rect.left, -rect.top);
    float scaleFactor = webView()->deviceScaleFactor();
    gc.scale(WebCore::FloatSize(scaleFactor, scaleFactor));
    view->paintContents(gc, rect);
    gc.restore();

    return S_OK;
}

HRESULT WebFrame::paintScrollViewRectToContextAtPoint(RECT rect, POINT pt, _In_ HDC deviceContext)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    // We can't paint with a layout still pending.
    view->updateLayoutAndStyleIfNeededRecursive();

    GraphicsContext gc(deviceContext);
    gc.setShouldIncludeChildWindows(true);
    gc.save();
    IntRect dirtyRect(rect);
    dirtyRect.move(-pt.x, -pt.y);
    float scaleFactor = webView()->deviceScaleFactor();
    gc.scale(WebCore::FloatSize(scaleFactor, scaleFactor));
    view->paint(gc, dirtyRect);
    gc.restore();

    return S_OK;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebFrame::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, __uuidof(WebFrame)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrame2*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame))
        *ppvObject = static_cast<IWebFrame2*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame2))
        *ppvObject = static_cast<IWebFrame2*>(this);
    else if (IsEqualGUID(riid, IID_IWebFramePrivate))
        *ppvObject = static_cast<IWebFramePrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebDocumentText))
        *ppvObject = static_cast<IWebDocumentText*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebFrame::AddRef()
{
    return ++m_refCount;
}

ULONG WebFrame::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebFrame -------------------------------------------------------------------

HRESULT WebFrame::name(_Deref_opt_out_ BSTR* frameName)
{
    if (!frameName) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frameName = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    *frameName = BString(coreFrame->tree().uniqueName()).release();
    return S_OK;
}

HRESULT WebFrame::webView(_COM_Outptr_opt_ IWebView** view)
{
    if (!view)
        return E_POINTER;
    *view = nullptr;
    if (!d->webView)
        return E_FAIL;
    *view = d->webView;
    (*view)->AddRef();
    return S_OK;
}

HRESULT WebFrame::frameView(_COM_Outptr_opt_ IWebFrameView** view)
{
    ASSERT_NOT_REACHED();
    if (!view)
        return E_POINTER;
    *view = nullptr;
    return E_NOTIMPL;
}

HRESULT WebFrame::DOMDocument(_COM_Outptr_opt_ IDOMDocument** result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (Document* document = coreFrame->document())
        *result = DOMDocument::createInstance(document);

    return *result ? S_OK : E_FAIL;
}


HRESULT WebFrame::DOMWindow(_COM_Outptr_opt_ IDOMWindow** window)
{
    if (!window) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *window = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (WebCore::DOMWindow* coreWindow = coreFrame->document()->domWindow())
        *window = ::DOMWindow::createInstance(coreWindow);

    return *window ? S_OK : E_FAIL;
}

HRESULT WebFrame::frameElement(_COM_Outptr_opt_ IDOMHTMLElement** frameElement)
{
    if (!frameElement)
        return E_POINTER;

    *frameElement = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    COMPtr<IDOMElement> domElement(AdoptCOM, DOMElement::createInstance(coreFrame->ownerElement()));
    COMPtr<IDOMHTMLElement> htmlElement(Query, domElement);
    if (!htmlElement)
        return E_FAIL;
    return htmlElement.copyRefTo(frameElement);
}

HRESULT WebFrame::currentForm(_COM_Outptr_opt_ IDOMElement** currentForm)
{
    if (!currentForm) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *currentForm = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (HTMLFormElement* formElement = coreFrame->selection().currentForm())
        *currentForm = DOMElement::createInstance(formElement);

    return *currentForm ? S_OK : E_FAIL;
}

JSGlobalContextRef WebFrame::globalContext()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return nullptr;

    return toGlobalRef(coreFrame->script().globalObject(mainThreadNormalWorld())->globalExec());
}

JSGlobalContextRef WebFrame::globalContextForScriptWorld(IWebScriptWorld* iWorld)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return 0;

    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return 0;

    return toGlobalRef(coreFrame->script().globalObject(world->world())->globalExec());
}

HRESULT WebFrame::loadRequest(_In_opt_ IWebURLRequest* request)
{
    if (!request)
        return S_OK;

    COMPtr<WebMutableURLRequest> requestImpl;

    HRESULT hr = request->QueryInterface(&requestImpl);
    if (FAILED(hr))
        return hr;
 
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    coreFrame->loader().load(FrameLoadRequest(*coreFrame, requestImpl->resourceRequest(), ShouldOpenExternalURLsPolicy::ShouldNotAllow));
    return S_OK;
}

void WebFrame::loadData(Ref<WebCore::SharedBuffer>&& data, BSTR mimeType, BSTR textEncodingName, BSTR baseURL, BSTR failingURL)
{
    String mimeTypeString(mimeType, SysStringLen(mimeType));
    if (!mimeType)
        mimeTypeString = "text/html";

    String encodingString(textEncodingName, SysStringLen(textEncodingName));

    // FIXME: We should really be using MarshallingHelpers::BSTRToKURL here,
    // but that would turn a null BSTR into a null URL, and we crash inside of
    // WebCore if we use a null URL in constructing the ResourceRequest.
    URL baseCoreURL = URL(URL(), String(baseURL ? baseURL : L"", SysStringLen(baseURL)));

    URL failingCoreURL = MarshallingHelpers::BSTRToKURL(failingURL);

    ResourceRequest request(baseCoreURL);
    ResourceResponse response(URL(), mimeTypeString, data->size(), encodingString);
    SubstituteData substituteData(WTFMove(data), failingCoreURL, response, SubstituteData::SessionHistoryVisibility::Hidden);

    // This method is only called from IWebFrame methods, so don't ASSERT that the Frame pointer isn't null.
    if (Frame* coreFrame = core(this))
        coreFrame->loader().load(FrameLoadRequest(*coreFrame, request, ShouldOpenExternalURLsPolicy::ShouldNotAllow, substituteData));
}

HRESULT WebFrame::loadData(_In_opt_ IStream* data, _In_ BSTR mimeType, _In_ BSTR textEncodingName, _In_ BSTR url)
{
    auto sharedBuffer = SharedBuffer::create();

    STATSTG stat;
    if (SUCCEEDED(data->Stat(&stat, STATFLAG_NONAME))) {
        if (!stat.cbSize.HighPart && stat.cbSize.LowPart) {
            Vector<char> dataBuffer(stat.cbSize.LowPart);
            ULONG read;
            // FIXME: this does a needless copy, would be better to read right into the SharedBuffer
            // or adopt the Vector or something.
            if (SUCCEEDED(data->Read(dataBuffer.data(), static_cast<ULONG>(dataBuffer.size()), &read)))
                sharedBuffer->append(dataBuffer.data(), static_cast<int>(dataBuffer.size()));
        }
    }

    loadData(WTFMove(sharedBuffer), mimeType, textEncodingName, url, nullptr);
    return S_OK;
}

HRESULT WebFrame::loadPlainTextString(_In_ BSTR plainText, _In_ BSTR url)
{
    auto sharedBuffer = SharedBuffer::create(reinterpret_cast<char*>(plainText), sizeof(UChar) * SysStringLen(plainText));
    BString plainTextMimeType(TEXT("text/plain"), 10);
    BString utf16Encoding(TEXT("utf-16"), 6);
    loadData(WTFMove(sharedBuffer), plainTextMimeType, utf16Encoding, url, nullptr);
    return S_OK;
}

void WebFrame::loadHTMLString(_In_ BSTR htmlString, _In_ BSTR baseURL, _In_ BSTR unreachableURL)
{
    auto sharedBuffer = SharedBuffer::create(reinterpret_cast<char*>(htmlString), sizeof(UChar) * SysStringLen(htmlString));
    BString utf16Encoding(TEXT("utf-16"), 6);
    loadData(WTFMove(sharedBuffer), 0, utf16Encoding, baseURL, unreachableURL);
}

HRESULT WebFrame::loadHTMLString(_In_ BSTR htmlString, _In_ BSTR baseURL)
{
    loadHTMLString(htmlString, baseURL, nullptr);
    return S_OK;
}

HRESULT WebFrame::loadAlternateHTMLString(_In_ BSTR str, _In_ BSTR baseURL, _In_ BSTR unreachableURL)
{
    loadHTMLString(str, baseURL, unreachableURL);
    return S_OK;
}

HRESULT WebFrame::loadArchive(_In_opt_ IWebArchive* /*archive*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

static inline WebDataSource* getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : nullptr;
}

HRESULT WebFrame::dataSource(_COM_Outptr_opt_ IWebDataSource** source)
{
    if (!source) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *source = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    WebDataSource* webDataSource = getWebDataSource(coreFrame->loader().documentLoader());

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return *source ? S_OK : E_FAIL;
}

HRESULT WebFrame::provisionalDataSource(_COM_Outptr_opt_ IWebDataSource** source)
{
    if (!source) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *source = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    WebDataSource* webDataSource = getWebDataSource(coreFrame->loader().provisionalDocumentLoader());

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return *source ? S_OK : E_FAIL;
}

URL WebFrame::url() const
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return URL();

    return coreFrame->document()->url();
}

HRESULT WebFrame::stopLoading()
{
    if (Frame* coreFrame = core(this))
        coreFrame->loader().stopAllLoaders();
    return S_OK;
}

HRESULT WebFrame::reload()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    coreFrame->loader().reload();
    return S_OK;
}

HRESULT WebFrame::findFrameNamed(_In_ BSTR name, _COM_Outptr_opt_ IWebFrame** frame)
{
    if (!frame) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *frame = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    Frame* foundFrame = coreFrame->tree().find(AtomicString(name, SysStringLen(name)), *coreFrame);
    if (!foundFrame)
        return S_OK;

    WebFrame* foundWebFrame = kit(foundFrame);
    if (!foundWebFrame)
        return E_FAIL;

    return foundWebFrame->QueryInterface(IID_IWebFrame, (void**)frame);
}

HRESULT WebFrame::parentFrame(_COM_Outptr_opt_ IWebFrame** frame)
{
    if (!frame)
        return E_POINTER;

    *frame = nullptr;

    HRESULT hr = S_OK;
    if (Frame* coreFrame = core(this))
        if (WebFrame* webFrame = kit(coreFrame->tree().parent()))
            hr = webFrame->QueryInterface(IID_IWebFrame, (void**) frame);

    return hr;
}

class EnumChildFrames final : public IEnumVARIANT
{
public:
    EnumChildFrames(Frame* f)
        : m_frame(f), m_curChild(f ? f->tree().firstChild() : nullptr)
    {
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
    {
        *ppvObject = nullptr;
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumVARIANT))
            *ppvObject = this;
        else
            return E_NOINTERFACE;

        AddRef();
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        return ++m_refCount;
    }

    virtual ULONG STDMETHODCALLTYPE Release()
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
        ::VariantInit(rgVar);
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

        m_curChild = m_curChild->tree().nextSibling();
        if (pCeltFetched)
            *pCeltFetched = 1;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        if (!m_frame)
            return S_FALSE;
        for (unsigned i = 0; i < celt && m_curChild; i++)
            m_curChild = m_curChild->tree().nextSibling();
        return m_curChild ? S_OK : S_FALSE;
    }

    virtual HRESULT STDMETHODCALLTYPE Reset(void)
    {
        if (!m_frame)
            return S_FALSE;
        m_curChild = m_frame->tree().firstChild();
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**)
    {
        return E_NOTIMPL;
    }

private:
    ULONG m_refCount { 1 };
    Frame* m_frame;
    Frame* m_curChild;
};

HRESULT WebFrame::childFrames(_COM_Outptr_opt_ IEnumVARIANT** enumFrames)
{
    if (!enumFrames)
        return E_POINTER;

    *enumFrames = new EnumChildFrames(core(this));
    return S_OK;
}

// IWebFramePrivate ------------------------------------------------------

HRESULT WebFrame::renderTreeAsExternalRepresentation(BOOL forPrinting, _Deref_opt_out_ BSTR* result)
{
    if (!result)
        return E_POINTER;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    *result = BString(externalRepresentation(coreFrame, forPrinting ? RenderAsTextPrintingMode : RenderAsTextBehaviorNormal)).release();
    return S_OK;
}

HRESULT WebFrame::pageNumberForElementById(_In_ BSTR id, float pageWidthInPixels, float pageHeightInPixels, _Out_ int* pageNumber)
{
    // TODO: Please remove this function if not needed as this is LTC specific function
    // and has been moved to Internals.
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebFrame::numberOfPages(float pageWidthInPixels, float pageHeightInPixels, _Out_ int* pageCount)
{
    // TODO: Please remove this function if not needed as this is LTC specific function
    // and has been moved to Internals.
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebFrame::scrollOffset(_Out_ SIZE* offset)
{
    if (!offset) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    *offset = toIntSize(view->scrollPosition());
    return S_OK;
}

HRESULT WebFrame::layout()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    view->layoutContext().layout();
    return S_OK;
}

HRESULT WebFrame::firstLayoutDone(_Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    *result = coreFrame->loader().stateMachine().firstLayoutDone();
    return S_OK;
}

HRESULT WebFrame::pendingFrameUnloadEventCount(_Out_ UINT* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    *result = coreFrame->document()->domWindow()->pendingUnloadEventListeners();
    return S_OK;
}

HRESULT WebFrame::hasSpellingMarker(UINT from, UINT length, BOOL* result)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;
    *result = coreFrame->editor().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
    return S_OK;
}

HRESULT WebFrame::clearOpener()
{
    HRESULT hr = S_OK;
    if (Frame* coreFrame = core(this))
        coreFrame->loader().setOpener(nullptr);

    return hr;
}

HRESULT WebFrame::setTextDirection(_In_ BSTR direction)
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    String directionString(direction, SysStringLen(direction));
    if (directionString == "auto")
        coreFrame->editor().setBaseWritingDirection(WritingDirection::Natural);
    else if (directionString == "ltr")
        coreFrame->editor().setBaseWritingDirection(WritingDirection::LeftToRight);
    else if (directionString == "rtl")
        coreFrame->editor().setBaseWritingDirection(WritingDirection::RightToLeft);
    return S_OK;
}

// IWebDocumentText -----------------------------------------------------------

HRESULT WebFrame::supportsTextEncoding(_Out_ BOOL* result)
{
    *result = FALSE;
    return E_NOTIMPL;
}

HRESULT WebFrame::selectedString(_Deref_opt_out_ BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    String text = coreFrame->displayStringModifiedByEncoding(coreFrame->editor().selectedText());

    *result = BString(text).release();
    return S_OK;
}

HRESULT WebFrame::selectAll()
{
    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (!coreFrame->editor().command("SelectAll").execute())
        return E_FAIL;

    return S_OK;
}

HRESULT WebFrame::deselectAll()
{
    return E_NOTIMPL;
}

// WebFrame ---------------------------------------------------------------

Ref<Frame> WebFrame::createSubframeWithOwnerElement(IWebView* webView, Page* page, HTMLFrameOwnerElement* ownerElement)
{
    webView->QueryInterface(&d->webView);
    d->webView->Release(); // don't hold the extra ref

    HWND viewWindow;
    d->webView->viewWindow(&viewWindow);

    this->AddRef(); // We release this ref in frameLoaderDestroyed()
    auto frame = Frame::create(page, ownerElement, new WebFrameLoaderClient(this));
    d->frame = frame.ptr();
    return frame;
}

void WebFrame::initWithWebView(IWebView* webView, Page* page)
{
    webView->QueryInterface(&d->webView);
    d->webView->Release(); // don't hold the extra ref

    HWND viewWindow;
    d->webView->viewWindow(&viewWindow);

    this->AddRef(); // We release this ref in frameLoaderDestroyed()
    d->frame = &page->mainFrame();
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
        document->resolveStyle(WebCore::Document::ResolveStyleType::Rebuild);
}

HRESULT WebFrame::inViewSourceMode(BOOL* flag)
{
    return E_NOTIMPL;
}

HRESULT WebFrame::setInViewSourceMode(BOOL flag)
{
    return E_NOTIMPL;
}

HRESULT WebFrame::elementWithName(BSTR name, IDOMElement* form, IDOMElement** element)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement* formElement = formElementFromDOMElement(form);
    if (formElement) {
        AtomicString targetName((UChar*)name, SysStringLen(name));
        for (auto& associatedElement : formElement->copyAssociatedElementsVector()) {
            if (!is<HTMLFormControlElement>(associatedElement.get()))
                continue;
            auto& elt = downcast<HTMLFormControlElement>(associatedElement.get());
            // Skip option elements, other duds.
            if (elt.name() == targetName) {
                *element = DOMElement::createInstance(&elt);
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

HRESULT WebFrame::elementDoesAutoComplete(_In_opt_ IDOMElement *element, _Out_ BOOL* result)
{
    *result = false;
    if (!element)
        return E_INVALIDARG;

    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (!inputElement)
        *result = false;
    else
        *result = inputElement->isTextField() && !inputElement->isPasswordField() && inputElement->shouldAutocomplete();

    return S_OK;
}

HRESULT WebFrame::resumeAnimations()
{
    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    frame->animation().resumeAnimations();
    return S_OK;
}

HRESULT WebFrame::suspendAnimations()
{
    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    frame->animation().suspendAnimations();
    return S_OK;
}

HRESULT WebFrame::pauseAnimation(_In_ BSTR animationName, _In_opt_ IDOMNode* node, double secondsFromNow, _Out_ BOOL* animationWasRunning)
{
    if (!node || !animationWasRunning)
        return E_POINTER;

    *animationWasRunning = FALSE;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    COMPtr<DOMNode> domNode(Query, node);
    if (!domNode)
        return E_FAIL;

    *animationWasRunning = frame->animation().pauseAnimationAtTime(downcast<Element>(*domNode->node()), String(animationName, SysStringLen(animationName)), secondsFromNow);
    return S_OK;
}

HRESULT WebFrame::pauseTransition(_In_ BSTR propertyName, _In_opt_ IDOMNode* node, double secondsFromNow, _Out_ BOOL* transitionWasRunning)
{
    if (!node || !transitionWasRunning)
        return E_POINTER;

    *transitionWasRunning = FALSE;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    COMPtr<DOMNode> domNode(Query, node);
    if (!domNode)
        return E_FAIL;

    *transitionWasRunning = frame->animation().pauseTransitionAtTime(downcast<Element>(*domNode->node()), String(propertyName, SysStringLen(propertyName)), secondsFromNow);
    return S_OK;
}

HRESULT WebFrame::visibleContentRect(_Out_ RECT* rect)
{
    if (!rect)
        return E_POINTER;
    SetRectEmpty(rect);

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    FrameView* view = frame->view();
    if (!view)
        return E_FAIL;

    *rect = view->visibleContentRect();
    return S_OK;
}

HRESULT WebFrame::numberOfActiveAnimations(_Out_ UINT* number)
{
    if (!number)
        return E_POINTER;

    *number = 0;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    *number = frame->animation().numberOfActiveAnimations(frame->document());
    return S_OK;
}

HRESULT WebFrame::isDisplayingStandaloneImage(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    Document* document = frame->document();
    *result = document && document->isImageDocument();
    return S_OK;
}

HRESULT WebFrame::allowsFollowingLink(_In_ BSTR url, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = TRUE;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    *result = frame->document()->securityOrigin().canDisplay(MarshallingHelpers::BSTRToKURL(url));
    return S_OK;
}

HRESULT WebFrame::controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement* formElement = formElementFromDOMElement(form);
    if (!formElement)
        return E_FAIL;

    auto elements = formElement->copyAssociatedElementsVector();
    int inCount = *cControls;
    int count = (int) elements.size();
    *cControls = count;
    if (!controls)
        return S_OK;
    if (inCount < count)
        return E_FAIL;

    *cControls = 0;
    for (auto& element : elements) {
        if (element->isEnumeratable()) {
            // Skip option elements, other duds.
            controls[*cControls] = DOMElement::createInstance(&element->asHTMLElement());
            (*cControls)++;
        }
    }
    return S_OK;
}

HRESULT WebFrame::elementIsPassword(IDOMElement *element, bool *result)
{
    HTMLInputElement* inputElement = inputElementFromDOMElement(element);
    *result = inputElement && inputElement->isPasswordField();
    return S_OK;
}

HRESULT WebFrame::searchForLabelsBeforeElement(const BSTR* labels, unsigned cLabels, IDOMElement* beforeElement, unsigned* outResultDistance, BOOL* outResultIsInCellAbove, BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (outResultDistance)
        *outResultDistance = 0;
    if (outResultIsInCellAbove)
        *outResultIsInCellAbove = FALSE;
    *result = 0;

    if (!cLabels)
        return S_OK;
    if (cLabels < 1)
        return E_INVALIDARG;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    Vector<String> labelStrings(cLabels);
    for (int i=0; i<cLabels; i++)
        labelStrings[i] = String(labels[i], SysStringLen(labels[i]));
    Element *coreElement = elementFromDOMElement(beforeElement);
    if (!coreElement)
        return E_FAIL;

    size_t resultDistance;
    bool resultIsInCellAbove;
    String label = coreFrame->searchForLabelsBeforeElement(labelStrings, coreElement, &resultDistance, &resultIsInCellAbove);
    
    *result = BString(label).release();
    if (label.length() && !*result)
        return E_OUTOFMEMORY;
    if (outResultDistance)
        *outResultDistance = resultDistance;
    if (outResultIsInCellAbove)
        *outResultIsInCellAbove = resultIsInCellAbove;

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
        return E_UNEXPECTED;

    Vector<String> labelStrings(cLabels);
    for (int i=0; i<cLabels; i++)
        labelStrings[i] = String(labels[i], SysStringLen(labels[i]));
    Element *coreElement = elementFromDOMElement(againstElement);
    if (!coreElement)
        return E_FAIL;

    String label = coreFrame->matchLabelsAgainstElement(labelStrings, coreElement);
    
    *result = BString(label).release();
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
        BString mimeTypeBStr;
        if (SUCCEEDED(urlResponse->MIMEType(&mimeTypeBStr))) {
            String mimeType(mimeTypeBStr, SysStringLen(mimeTypeBStr));
            *result = mimeType == "text/html" || WebCore::MIMETypeRegistry::isXMLMIMEType(mimeType);
        }
    }
    return hr;
}

HRESULT WebFrame::layerTreeAsText(_Deref_out_opt_ BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;

    Frame* frame = core(this);
    if (!frame)
        return E_UNEXPECTED;

    String text = frame->layerTreeAsText();
    *result = BString(text).release();
    return S_OK;
}

void WebFrame::frameLoaderDestroyed()
{
    // The FrameLoader going away is equivalent to the Frame going away,
    // so we now need to clear our frame pointer.
    d->frame = 0;

    this->Release();
}

static IntRect printerRect(HDC printDC)
{
    return IntRect(0, 0, 
                   GetDeviceCaps(printDC, PHYSICALWIDTH)  - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETX),
                   GetDeviceCaps(printDC, PHYSICALHEIGHT) - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETY));
}

void WebFrame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot adjustViewSize)
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);
    coreFrame->setPrinting(printing, pageSize, originalPageSize, maximumShrinkRatio, adjustViewSize ? AdjustViewSize : DoNotAdjustViewSize);
}

HRESULT WebFrame::setInPrintingMode(BOOL value, _In_ HDC printDC)
{
    if (m_inPrintingMode == !!value)
        return S_OK;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (!coreFrame->document())
        return E_FAIL;

    m_inPrintingMode = !!value;

    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    FloatSize minLayoutSize(0.0, 0.0);
    FloatSize originalPageSize(0.0, 0.0);
    if (m_inPrintingMode && !coreFrame->document()->isFrameSet()) {
        if (!printDC) {
            ASSERT_NOT_REACHED();
            return E_POINTER;
        }

        const int desiredPixelsPerInch = 72;
        IntRect printRect = printerRect(printDC);
        int paperHorizontalPixelsPerInch = ::GetDeviceCaps(printDC, LOGPIXELSX);
        int paperVerticalPixelsPerInch = ::GetDeviceCaps(printDC, LOGPIXELSY);
        int paperWidth = printRect.width() * desiredPixelsPerInch / paperHorizontalPixelsPerInch;
        int paperHeight = printRect.height() * desiredPixelsPerInch / paperVerticalPixelsPerInch;
        originalPageSize = FloatSize(paperWidth, paperHeight);
        Frame* coreFrame = core(this);
        minLayoutSize = coreFrame->resizePageRectsKeepingRatio(originalPageSize, FloatSize(paperWidth * PrintingMinimumShrinkFactor, paperHeight * PrintingMinimumShrinkFactor));
    }

    setPrinting(m_inPrintingMode, minLayoutSize, originalPageSize, PrintingMaximumShrinkFactor / PrintingMinimumShrinkFactor, AdjustViewSize);

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
    if (headerHeight && SUCCEEDED(ui->webViewHeaderHeight(d->webView, &height)))
        *headerHeight = height;
    if (footerHeight && SUCCEEDED(ui->webViewFooterHeight(d->webView, &height)))
        *footerHeight = height;
}

IntRect WebFrame::printerMarginRect(HDC printDC)
{
    IntRect emptyRect(0, 0, 0, 0);

    COMPtr<IWebUIDelegate> ui;
    if (FAILED(d->webView->uiDelegate(&ui)))
        return emptyRect;

    RECT rect;
    if (FAILED(ui->webViewPrintingMarginRect(d->webView, &rect)))
        return emptyRect;

    rect.left = MulDiv(rect.left, ::GetDeviceCaps(printDC, LOGPIXELSX), 1000);
    rect.top = MulDiv(rect.top, ::GetDeviceCaps(printDC, LOGPIXELSY), 1000);
    rect.right = MulDiv(rect.right, ::GetDeviceCaps(printDC, LOGPIXELSX), 1000);
    rect.bottom = MulDiv(rect.bottom, ::GetDeviceCaps(printDC, LOGPIXELSY), 1000);

    return IntRect(rect.left, rect.top, (rect.right - rect.left), rect.bottom - rect.top);
}

const Vector<WebCore::IntRect>& WebFrame::computePageRects(HDC printDC)
{
    ASSERT(m_inPrintingMode);
    
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);
    ASSERT(coreFrame->document());

    if (!printDC)
        return m_pageRects;

    // adjust the page rect by the header and footer
    float headerHeight = 0, footerHeight = 0;
    headerAndFooterHeights(&headerHeight, &footerHeight);
    IntRect pageRect = printerRect(printDC);
    IntRect marginRect = printerMarginRect(printDC);
    IntRect adjustedRect = IntRect(
        pageRect.x() + marginRect.x(),
        pageRect.y() + marginRect.y(),
        pageRect.width() - marginRect.x() - marginRect.maxX(),
        pageRect.height() - marginRect.y() - marginRect.maxY());

    computePageRectsForFrame(coreFrame, adjustedRect, headerHeight, footerHeight, 1.0,m_pageRects, m_pageHeight);
    
    return m_pageRects;
}

HRESULT WebFrame::getPrintedPageCount(_In_ HDC printDC, _Out_ UINT *pageCount)
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
    if (!coreFrame)
        return E_UNEXPECTED;

    if (!coreFrame->document())
        return E_FAIL;

    const Vector<IntRect>& pages = computePageRects(printDC);
    *pageCount = (UINT) pages.size();
    
    return S_OK;
}

#if USE(CG)
void WebFrame::drawHeader(PlatformGraphicsContext* pctx, IWebUIDelegate* ui, const IntRect& pageRect, float headerHeight)
{
    int x = pageRect.x();
    int y = 0;
    RECT headerRect = {x, y, x+pageRect.width(), y+static_cast<int>(headerHeight)};
    ui->drawHeaderInRect(d->webView, &headerRect, reinterpret_cast<ULONG_PTR>(pctx));
}

void WebFrame::drawFooter(PlatformGraphicsContext* pctx, IWebUIDelegate* ui, const IntRect& pageRect, UINT page, UINT pageCount, float headerHeight, float footerHeight)
{
    int x = pageRect.x();
    int y = max((int)headerHeight+pageRect.height(), m_pageHeight-static_cast<int>(footerHeight));
    RECT footerRect = {x, y, x+pageRect.width(), y+static_cast<int>(footerHeight)};
    ui->drawFooterInRect(d->webView, &footerRect, reinterpret_cast<ULONG_PTR>(pctx), page + 1, pageCount);
}

void WebFrame::spoolPage(PlatformGraphicsContext* pctx, GraphicsContext& spoolCtx, HDC printDC, IWebUIDelegate* ui, float headerHeight, float footerHeight, UINT page, UINT pageCount)
{
    Frame* coreFrame = core(this);

    IntRect pageRect = m_pageRects[page];

    CGContextSaveGState(pctx);

    IntRect printRect = printerRect(printDC);
    CGRect mediaBox = CGRectMake(CGFloat(0),
                                 CGFloat(0),
                                 CGFloat(printRect.width()),
                                 CGFloat(printRect.height()));

    CGContextBeginPage(pctx, &mediaBox);

    CGFloat scale = static_cast<float>(mediaBox.size.width)/static_cast<float>(pageRect.width());
    CGAffineTransform ctm = CGContextGetBaseCTM(pctx);
    ctm = CGAffineTransformScale(ctm, -scale, -scale);
    ctm = CGAffineTransformTranslate(ctm, CGFloat(-pageRect.x()), CGFloat(-pageRect.y()+headerHeight)); // reserves space for header
    CGContextScaleCTM(pctx, scale, scale);
    CGContextTranslateCTM(pctx, CGFloat(-pageRect.x()), CGFloat(-pageRect.y()+headerHeight));   // reserves space for header
    CGContextSetBaseCTM(pctx, ctm);

    coreFrame->view()->paintContents(spoolCtx, pageRect);

    CGContextTranslateCTM(pctx, CGFloat(pageRect.x()), CGFloat(pageRect.y())-headerHeight);

    if (headerHeight)
        drawHeader(pctx, ui, pageRect, headerHeight);

    if (footerHeight)
        drawFooter(pctx, ui, pageRect, page, pageCount, headerHeight, footerHeight);

    CGContextEndPage(pctx);
    CGContextRestoreGState(pctx);
}
#elif USE(CAIRO)
static float scaleFactor(HDC printDC, const IntRect& marginRect, const IntRect& pageRect)
{
    const IntRect& printRect = printerRect(printDC);

    IntRect adjustedRect = IntRect(
        printRect.x() + marginRect.x(),
        printRect.y() + marginRect.y(),
        printRect.width() - marginRect.x() - marginRect.maxX(),
        printRect.height() - marginRect.y() - marginRect.maxY());

    float scale = static_cast<float>(adjustedRect.width()) / static_cast<float>(pageRect.width());
    if (!scale)
       scale = 1.0;

    return scale;
}

static HDC hdcFromContext(PlatformGraphicsContext* pctx)
{
    return cairo_win32_surface_get_dc(cairo_get_target(pctx->cr()));
}

void WebFrame::drawHeader(PlatformGraphicsContext* pctx, IWebUIDelegate* ui, const IntRect& pageRect, float headerHeight)
{
    HDC hdc = hdcFromContext(pctx);

    int x = pageRect.x();
    int y = 0;
    RECT headerRect = {x, y, x + pageRect.width(), y + static_cast<int>(headerHeight)};

    ui->drawHeaderInRect(d->webView, &headerRect, reinterpret_cast<ULONG_PTR>(hdc));
}

void WebFrame::drawFooter(PlatformGraphicsContext* pctx, IWebUIDelegate* ui, const IntRect& pageRect, UINT page, UINT pageCount, float headerHeight, float footerHeight)
{
    HDC hdc = hdcFromContext(pctx);
    
    int x = pageRect.x();
    int y = max(static_cast<int>(headerHeight) + pageRect.height(), m_pageHeight  -static_cast<int>(footerHeight));
    RECT footerRect = {x, y, x + pageRect.width(), y + static_cast<int>(footerHeight)};

    ui->drawFooterInRect(d->webView, &footerRect, reinterpret_cast<ULONG_PTR>(hdc), page+1, pageCount);
}

static XFORM buildXFORMFromCairo(HDC targetDC, cairo_t* previewContext)
{
    XFORM scaled;
    GetWorldTransform(targetDC, &scaled);

    cairo_matrix_t ctm;
    cairo_get_matrix(previewContext, &ctm);    
        
    // Scale to the preview screen bounds
    scaled.eM11 = ctm.xx;
    scaled.eM22 = ctm.yy;

    return scaled;
}

void WebFrame::spoolPage(PlatformGraphicsContext* pctx, GraphicsContext& spoolCtx, HDC printDC, IWebUIDelegate* ui, float headerHeight, float footerHeight, UINT page, UINT pageCount)
{
    Frame* coreFrame = core(this);

    const IntRect& pageRect = m_pageRects[page];
    const IntRect& marginRect = printerMarginRect(printDC);

    // In preview, the printDC is a placeholder, so just always use the HDC backing the graphics context.
    HDC hdc = hdcFromContext(pctx);

    spoolCtx.save();
    
    XFORM original, scaled;
    GetWorldTransform(hdc, &original);
    
    cairo_t* cr = pctx->cr();
    bool preview = (hdc != printDC);
    if (preview) {
        // If this is a preview, the Windows HDC was set to a non-scaled state so that Cairo will
        // draw correctly.  We need to retain the correct preview scale here for use when the Cairo
        // drawing completes so that we can scale our GDI-based header/footer calls. This is a
        // workaround for a bug in Cairo (see https://bugs.freedesktop.org/show_bug.cgi?id=28161)
        scaled = buildXFORMFromCairo(hdc, cr);
    }

    float scale = scaleFactor(printDC, marginRect, pageRect);
    
    IntRect cairoMarginRect(marginRect);
    cairoMarginRect.scale(1 / scale);

    // We cannot scale the display HDC because the print surface also scales fonts,
    // resulting in invalid printing (and print preview)
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, cairoMarginRect.x(), cairoMarginRect.y() + headerHeight);

    // Modify Cairo (only) to account for page position.
    cairo_translate(cr, -pageRect.x(), -pageRect.y());
    coreFrame->view()->paintContents(spoolCtx, pageRect);
    cairo_translate(cr, pageRect.x(), pageRect.y());
    
    if (preview) {
        // If this is a preview, the Windows HDC was set to a non-scaled state so that Cairo would
        // draw correctly.  We need to rescale the HDC to the correct preview scale so our GDI-based
        // header/footer calls will draw properly.  This is a workaround for a bug in Cairo.
        // (see https://bugs.freedesktop.org/show_bug.cgi?id=28161)
        SetWorldTransform(hdc, &scaled);
    }
    
    XFORM xform = TransformationMatrix().translate(marginRect.x(), marginRect.y()).scale(scale);
    ModifyWorldTransform(hdc, &xform, MWT_LEFTMULTIPLY);
   
    if (headerHeight)
        drawHeader(pctx, ui, pageRect, headerHeight);
    
    if (footerHeight)
        drawFooter(pctx, ui, pageRect, page, pageCount, headerHeight, footerHeight);

    SetWorldTransform(hdc, &original);

    cairo_show_page(cr);
    ASSERT(!cairo_status(cr));
    spoolCtx.restore();
}

static void setCairoTransformToPreviewHDC(cairo_t* previewCtx, HDC previewDC)
{
    XFORM passedCTM;
    GetWorldTransform(previewDC, &passedCTM);

    // Reset HDC WorldTransform to unscaled state.  Scaling must be
    // done in Cairo to avoid drawing errors.
    XFORM unscaledCTM = passedCTM;
    unscaledCTM.eM11 = 1.0;
    unscaledCTM.eM22 = 1.0;
        
    SetWorldTransform(previewDC, &unscaledCTM);

    // Make the Cairo transform match the information passed to WebKit
    // in the HDC's WorldTransform.
    cairo_matrix_t ctm = { passedCTM.eM11, passedCTM.eM12, passedCTM.eM21,
                           passedCTM.eM22, passedCTM.eDx, passedCTM.eDy };

    cairo_set_matrix(previewCtx, &ctm);    
}

#endif

HRESULT WebFrame::spoolPages(HDC printDC, UINT startPage, UINT endPage, void* ctx)
{
#if USE(CG)
    if (!printDC || !ctx) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }
#elif USE(CAIRO)
    if (!printDC) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }
    
    HDC targetDC = (ctx) ? (HDC)ctx : printDC;

    cairo_surface_t* printSurface = 0;
    if (ctx)
        printSurface = cairo_win32_surface_create(targetDC); // in-memory
    else
        printSurface = cairo_win32_printing_surface_create(targetDC); // metafile
    
    cairo_t* cr = cairo_create(printSurface);
    if (!cr) {
        cairo_surface_destroy(printSurface);    
        return E_FAIL;
    }

    PlatformContextCairo platformContext(cr);
    PlatformGraphicsContext* pctx = &platformContext;
    cairo_destroy(cr);

    if (ctx) {
        // If this is a preview, the Windows HDC was sent with scaling information.
        // Retrieve it and reset it so that it draws properly.  This is a workaround
        // for a bug in Cairo (see https://bugs.freedesktop.org/show_bug.cgi?id=28161)
        setCairoTransformToPreviewHDC(cr, targetDC);
    }
    
    cairo_surface_set_fallback_resolution(printSurface, 72.0, 72.0);
#endif

    if (!m_inPrintingMode) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (!coreFrame->document())
        return E_FAIL;

    UINT pageCount = (UINT) m_pageRects.size();
#if USE(CG)
    PlatformGraphicsContext* pctx = (PlatformGraphicsContext*)ctx;
#endif

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

    float headerHeight = 0, footerHeight = 0;
    headerAndFooterHeights(&headerHeight, &footerHeight);
#if USE(CG) || USE(CAIRO)
    GraphicsContext spoolCtx(pctx);
    spoolCtx.setShouldIncludeChildWindows(true);

    for (UINT ii = startPage; ii < endPage; ii++)
        spoolPage(pctx, spoolCtx, printDC, ui.get(), headerHeight, footerHeight, ii, pageCount);
#endif

#if USE(CAIRO)
    cairo_surface_finish(printSurface);
    ASSERT(!cairo_surface_status(printSurface));
    cairo_surface_destroy(printSurface);
#endif

    return S_OK;
}

HRESULT WebFrame::isFrameSet(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    if (!coreFrame->document())
        return E_FAIL;

    *result = coreFrame->document()->isFrameSet() ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebFrame::string(_Deref_opt_out_ BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    RefPtr<Range> allRange(rangeOfContents(*coreFrame->document()));
    String allString = plainText(allRange.get());
    *result = BString(allString).release();
    return S_OK;
}

HRESULT WebFrame::size(_Out_ SIZE* size)
{
    if (!size)
        return E_POINTER;
    size->cx = size->cy = 0;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;
    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;
    size->cx = view->width();
    size->cy = view->height();
    return S_OK;
}

HRESULT WebFrame::hasScrollBars(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    if (view->horizontalScrollbar() || view->verticalScrollbar())
        *result = TRUE;

    return S_OK;
}

HRESULT WebFrame::contentBounds(_Out_ RECT* result)
{
    if (!result)
        return E_POINTER;
    ::SetRectEmpty(result);

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    result->bottom = view->contentsHeight();
    result->right = view->contentsWidth();
    return S_OK;
}

HRESULT WebFrame::frameBounds(_Out_ RECT* result)
{
    if (!result)
        return E_POINTER;
    ::SetRectEmpty(result);

    Frame* coreFrame = core(this);
    if (!coreFrame)
        return E_UNEXPECTED;

    FrameView* view = coreFrame->view();
    if (!view)
        return E_FAIL;

    FloatRect bounds = view->visibleContentRectIncludingScrollbars();
    result->bottom = (LONG) bounds.height();
    result->right = (LONG) bounds.width();
    return S_OK;
}

HRESULT WebFrame::isDescendantOfFrame(_In_opt_ IWebFrame* ancestor, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;

    Frame* coreFrame = core(this);
    COMPtr<WebFrame> ancestorWebFrame(Query, ancestor);
    if (!ancestorWebFrame)
        return S_OK;

    *result = (coreFrame && coreFrame->tree().isDescendantOf(core(ancestorWebFrame.get()))) ? TRUE : FALSE;
    return S_OK;
}

HRESULT WebFrame::stringByEvaluatingJavaScriptInScriptWorld(IWebScriptWorld* iWorld, JSObjectRef globalObjectRef, BSTR script, BSTR* evaluationResult)
{
    if (!evaluationResult)
        return E_POINTER;
    *evaluationResult = 0;

    if (!iWorld)
        return E_POINTER;

    COMPtr<WebScriptWorld> world(Query, iWorld);
    if (!world)
        return E_INVALIDARG;

    Frame* coreFrame = core(this);
    String string = String(script, SysStringLen(script));

    // Start off with some guess at a frame and a global object, we'll try to do better...!
    JSDOMWindow* anyWorldGlobalObject = coreFrame->script().globalObject(mainThreadNormalWorld());

    // The global object is probably a proxy object? - if so, we know how to use this!
    JSC::JSObject* globalObjectObj = toJS(globalObjectRef);
    auto& vm = *globalObjectObj->vm();
    if (globalObjectObj->inherits<JSWindowProxy>(vm))
        anyWorldGlobalObject = JSC::jsDynamicCast<JSDOMWindow*>(vm, static_cast<JSWindowProxy*>(globalObjectObj)->window());

    if (!anyWorldGlobalObject)
        return E_INVALIDARG;

    // Get the frame frome the global object we've settled on.
    Frame* frame = anyWorldGlobalObject->wrapped().frame();
    ASSERT(frame->document());
    JSValue result = frame->script().executeScriptInWorld(world->world(), string, true);

    if (!frame) // In case the script removed our frame from the page.
        return S_OK;

    // This bizarre set of rules matches behavior from WebKit for Safari 2.0.
    // If you don't like it, use -[WebScriptObject evaluateWebScript:] or 
    // JSEvaluateScript instead, since they have less surprising semantics.
    if (!result || !result.isBoolean() && !result.isString() && !result.isNumber())
        return S_OK;

    JSC::ExecState* exec = anyWorldGlobalObject->globalExec();
    JSC::JSLockHolder lock(exec);
    String resultString = result.toWTFString(exec);
    *evaluationResult = BString(resultString).release();

    return S_OK;
}

void WebFrame::unmarkAllMisspellings()
{
    Frame* coreFrame = core(this);
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->markers().removeMarkers(DocumentMarker::Spelling);
    }
}

void WebFrame::unmarkAllBadGrammar()
{
    Frame* coreFrame = core(this);
    for (Frame* frame = coreFrame; frame; frame = frame->tree().traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->markers().removeMarkers(DocumentMarker::Grammar);
    }
}

WebView* WebFrame::webView() const
{
    return d->webView;
}

void WebFrame::setWebView(WebView* webView)
{
    d->webView = webView;
}

COMPtr<IAccessible> WebFrame::accessible() const
{
    Frame* coreFrame = core(this);
    ASSERT(coreFrame);

    Document* currentDocument = coreFrame->document();
    if (!currentDocument)
        m_accessible = 0;
    else if (!m_accessible || m_accessible->document() != currentDocument) {
        // Either we've never had a wrapper for this frame's top-level Document,
        // the Document renderer was destroyed and its wrapper was detached, or
        // the previous Document is in the page cache, and the current document
        // needs to be wrapped.
        m_accessible = new AccessibleDocument(currentDocument, webView()->viewWindow());
    }
    return m_accessible.get();
}

void WebFrame::updateBackground()
{
    Frame* coreFrame = core(this);

    if (!coreFrame || !coreFrame->view())
        return;

    coreFrame->view()->updateBackgroundRecursively(webView()->transparent());
}

// IWebFrame2
HRESULT WebFrame::isMainFrame(_Out_ BOOL* value)
{
    if (!value)
        return E_POINTER;

    Frame* coreFrame = core(this);
    *value = coreFrame->isMainFrame();

    return S_OK;
}
