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

#include "config.h"
#include "WebKitDLL.h"

#include "IWebURLResponse.h"
#include "WebMutableURLRequest.h"
#include "WebFrame.h"
#include "WebView.h"
#include "WebDataSource.h"
#include "WebHistoryItem.h"

#pragma warning( push, 0 )
#include "Cache.h"
#include "Document.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "RenderFrame.h"
#include "cairo.h"
#include "cairo-win32.h"
#include "TransferJob.h"
#include "TransferJobWin.h"
#pragma warning(pop)

using namespace WebCore;

// WebFramePrivate ------------------------------------------------------------

class WebFrame::WebFramePrivate {
public:
    WebFramePrivate() { }
    ~WebFramePrivate() { }

    RefPtr<Frame> frame;
    RefPtr<FrameView> frameView;
    WebView* webView;
};

// WebFrame ----------------------------------------------------------------

WebFrame::WebFrame()
: m_refCount(0)
, d(new WebFrame::WebFramePrivate)
, m_dataSource(0)
, m_provisionalDataSource(0)
, m_quickRedirectComing(false)
{
    gClassCount++;
}

WebFrame::~WebFrame()
{
    delete d->frame->page();
    delete d;
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
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame))
        *ppvObject = static_cast<IWebFrame*>(this);
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

HRESULT STDMETHODCALLTYPE WebFrame::initWithName( 
    /* [in] */ BSTR /*name*/,
    /* [in] */ IWebFrameView* /*view*/,
    /* [in] */ IWebView* webView)
{
    if (webView)
        webView->AddRef();

    d->webView = static_cast<WebView*>(webView);
    HWND windowHandle;
    HRESULT hr = d->webView->viewWindow(&windowHandle);
    if (FAILED(hr))
        return hr;

    Page* page = new Page();
    Frame* frame = new FrameWin(page, 0, this);

    // FIXME: This is one-time initialization, but it gets the value of the setting from the
    // current WebView. That's a mismatch and not good!
    static bool initializedObjectCacheSize = false;
    if (!initializedObjectCacheSize) {
        Cache::setSize(getObjectCacheSize());
        initializedObjectCacheSize = true;
    }

    d->frame = frame;
    frame->deref(); // Frames are created with a refcount of 1.  Release this ref, since we've assigned it to a RefPtr
    page->setMainFrame(frame);
    FrameView* frameView = new FrameView(frame);
    d->frameView = frameView;
    frameView->deref(); // FrameViews are created with a refcount of 1.  Release this ref, since we've assigned it to a RefPtr
    d->frame->setView(frameView);
    d->frameView->setWindowHandle(windowHandle);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::name( 
    /* [retval][out] */ BSTR* /*frameName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::webView( 
    /* [retval][out] */ IWebView* /*view*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameView( 
    /* [retval][out] */ IWebFrameView* /*view*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::DOMDocument( 
    /* [retval][out] */ IDOMDocument** /*document*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameElement( 
    /* [retval][out] */ IDOMHTMLElement* /*frameElement*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadRequest( 
    /* [in] */ IWebURLRequest* request)
{
    HRESULT hr = S_OK;

    if (m_provisionalDataSource) {
        m_provisionalDataSource->Release();
        m_provisionalDataSource = 0;
        // FIXME - cancel the outstanding request?
    }

    m_provisionalDataSource = WebDataSource::createInstance(this);
    hr = m_provisionalDataSource->initWithRequest(request);

    return hr;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadData( 
    /* [in] */ IStream* /*data*/,
    /* [in] */ BSTR /*mimeType*/,
    /* [in] */ BSTR /*textEncodingName*/,
    /* [in] */ BSTR /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadHTMLString( 
    /* [in] */ BSTR /*string*/,
    /* [in] */ BSTR /*baseURL*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadAlternateHTMLString( 
    /* [in] */ BSTR /*str*/,
    /* [in] */ BSTR /*baseURL*/,
    /* [in] */ BSTR /*unreachableURL*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadArchive( 
    /* [in] */ IWebArchive* /*archive*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::dataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (m_dataSource)
        m_dataSource->AddRef();
    *source = m_dataSource;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::provisionalDataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (m_provisionalDataSource)
        m_provisionalDataSource->AddRef();
    *source = m_provisionalDataSource;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::stopLoading( void)
{
//    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::reload( void)
{
    if (!d->frame->url().url().startsWith("javascript:", false))
        d->frame->scheduleLocationChange(d->frame->url().url(), d->frame->referrer(), true/*lock history*/, true/*userGesture*/);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::findFrameNamed( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IWebFrame** /*frame*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::parentFrame( 
    /* [retval][out] */ IWebFrame** /*frame*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::childFrames( 
    /* [out] */ int* /*frameCount*/,
    /* [retval][out] */ IWebFrame*** /*frames*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// WebFrame ---------------------------------------------------------------

void WebFrame::paint()
{
    d->frameView->layout();

    HWND windowHandle;
    if (FAILED(d->webView->viewWindow(&windowHandle)))
        return;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(windowHandle, &ps);
    cairo_surface_t* finalSurface = cairo_win32_surface_create(hdc);
    cairo_surface_t* surface = cairo_win32_surface_create_with_dib(CAIRO_FORMAT_ARGB32,  
                                                            ps.rcPaint.right - ps.rcPaint.left,  
                                                            ps.rcPaint.bottom - ps.rcPaint.top); 

    cairo_t* context = cairo_create(surface);
    GraphicsContext gc(context);
    
    IntRect documentDirtyRect = ps.rcPaint;
    documentDirtyRect.move(d->frameView->contentsX(), d->frameView->contentsY());

    HDC surfaceDC = cairo_win32_surface_get_dc(surface);
    SaveDC(surfaceDC);
    cairo_translate(context, -d->frameView->contentsX() - ps.rcPaint.left, 
                             -d->frameView->contentsY() - ps.rcPaint.top);
    d->frame->paint(&gc, documentDirtyRect);
    RestoreDC(surfaceDC, -1);

    cairo_destroy(context);
    context = cairo_create(finalSurface);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, surface, ps.rcPaint.left, ps.rcPaint.top);
    cairo_rectangle(context, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
    cairo_fill(context);
    cairo_destroy(context);

    cairo_surface_destroy(surface);
    cairo_surface_destroy(finalSurface);

    EndPaint(windowHandle, &ps);
}

WebCore::Frame* WebFrame::impl()
{
    return d->frame.get();
}

HRESULT WebFrame::loadDataSource(WebDataSource* dataSource)
{
    HRESULT hr = S_OK;
    BSTR url = 0;
    BSTR method = 0;

    IWebMutableURLRequest* request;
    hr = dataSource->request(&request);
    if (SUCCEEDED(hr)) {
        HRESULT hr = request->URL(&url);
        if (SUCCEEDED(hr)) {
            hr = request->HTTPMethod(&method);
            if (SUCCEEDED(hr)) {
                KURL kurl(DeprecatedString((QChar*)url, SysStringLen(url)));
                d->frame->didOpenURL(kurl);
                d->frame->begin(kurl);
                String methodString(method, SysStringLen(method));
                WebCore::TransferJob* job;
                const FormData* formData = 0;
                if (!wcscmp(method, TEXT("GET"))) {
                    WebMutableURLRequest* requestImpl = static_cast<WebMutableURLRequest*>(request);
                    formData = requestImpl->formData();
                }
                if (formData)
                    job = new TransferJob(this, methodString, kurl, *formData);
                else
                    job = new TransferJob(this, methodString, kurl);
                job->start(d->frame->document()->docLoader());
                IWebFrameLoadDelegate* frameLoadDelegate;
                if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
                    frameLoadDelegate->didStartProvisionalLoadForFrame(d->webView, this);
                    frameLoadDelegate->Release();
                }
            }
        }
        request->Release();
    }

    SysFreeString(url);
    SysFreeString(method);

    return hr;
}

bool WebFrame::loading()
{
    return !!m_provisionalDataSource;
}

HRESULT WebFrame::goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType)
{
    IWebBackForwardList* backForwardList;
    HRESULT hr = d->webView->backForwardList(&backForwardList);
    if (SUCCEEDED(hr)) {
        backForwardList->goToItem(item);
        backForwardList->Release();
        hr = loadItem(item, withLoadType);
    }
    return hr;
}

HRESULT WebFrame::loadItem(IWebHistoryItem* item, WebFrameLoadType withLoadType)
{
    m_loadType = withLoadType;

    BSTR urlBStr;
    HRESULT hr = item->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    hr = request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);
    if (SUCCEEDED(hr))
        loadRequest(request);

    SysFreeString(urlBStr);
    request->Release();

    return hr;
}

static unsigned long long WebSystemMainMemory()
{
    MEMORYSTATUSEX statex;
    
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    return statex.ullTotalPhys;
}

int WebFrame::getObjectCacheSize()
{
    const int WebKitObjectCacheSizePreferenceKey = 16777216; // FIXME!

    unsigned long long memSize = WebSystemMainMemory();
    int cacheSize = WebKitObjectCacheSizePreferenceKey;
    int multiplier = 1;
    if (memSize >= 1024 * 1024 * 1000 /*1024*/)
        multiplier = 4;
    else if (memSize >= 512 * 1024 * 1024)
        multiplier = 2;

    return cacheSize * multiplier;
}

// TransferJobClient

void WebFrame::receivedRedirect(TransferJob*, const KURL&)
{
    //FIXME
}

void WebFrame::receivedResponse(TransferJob*, PlatformResponse)
{
    if (m_provisionalDataSource) {
        m_dataSource = m_provisionalDataSource;
        m_provisionalDataSource = 0;
    }
}

void WebFrame::receivedData(TransferJob*, const char* data, int length)
{
    d->frame->write(data, length);
}

void WebFrame::receivedAllData(TransferJob* /*job*/)
{
    m_quickRedirectComing = false;
    m_loadType = WebFrameLoadTypeStandard;
}

void WebFrame::receivedAllData(TransferJob* job, PlatformData data)
{
    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {

        if (data->loaded) {
            frameLoadDelegate->didFinishLoadForFrame(d->webView, this);

            if (m_loadType != WebFrameLoadTypeBack && m_loadType != WebFrameLoadTypeForward && m_loadType != WebFrameLoadTypeIndexedBackForward && !m_quickRedirectComing) {
                DeprecatedString urlStr = job->url().url();
                BSTR urlBStr = SysAllocStringLen((LPCOLESTR)urlStr.unicode(), urlStr.length());
                WebHistoryItem* historyItem = WebHistoryItem::createInstance();
                if (SUCCEEDED(historyItem->initWithURLString(urlBStr, 0/*FIXME*/))) {
                    IWebBackForwardList* backForwardList;
                    if (SUCCEEDED(d->webView->backForwardList(&backForwardList))) {
                        backForwardList->addItem(historyItem);
                        backForwardList->Release();
                    }
                    historyItem->Release();
                }
            }
        }
        else {
            frameLoadDelegate->didFailLoadWithError(d->webView, 0/*FIXME*/, this);
            m_quickRedirectComing = false;
            m_loadType = WebFrameLoadTypeStandard;
        }

        frameLoadDelegate->Release();
    }

    d->frame->end();
}

// FrameWinClient

void WebFrame::openURL(const DeprecatedString& url, bool lockHistory)
{
    DeprecatedString terminatedURL(url);
    terminatedURL.append('\0');

    m_quickRedirectComing = lockHistory;
    BSTR urlBStr = SysAllocString((LPCTSTR)terminatedURL.unicode());
    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    if (SUCCEEDED(request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0)))
        loadRequest(request);
    SysFreeString(urlBStr);
    request->Release();
}

void WebFrame::submitForm(const String& method, const KURL& url, const FormData* submitFormData)
{
    // FIXME: This is a dumb implementation, doesn't handle subframes, etc.

    DeprecatedString terminatedURL(url.url());
    terminatedURL.append('\0');
    DeprecatedString terminatedMethod(method.deprecatedString());
    terminatedMethod.append('\0');

    m_quickRedirectComing = false;
    m_loadType = WebFrameLoadTypeStandard;
    BSTR urlBStr = SysAllocString((LPCTSTR)terminatedURL.unicode());
    BSTR methodBStr = SysAllocString((LPCTSTR)terminatedMethod.unicode());
    WebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    if (SUCCEEDED(request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0))) {
        request->setHTTPMethod(methodBStr);
        request->setFormData(submitFormData);
        loadRequest(request);
    }
    SysFreeString(urlBStr);
    SysFreeString(methodBStr);
    request->Release();
}

void WebFrame::setTitle(const String& title)
{
    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        BSTR titleBStr = SysAllocStringLen(title.characters(), title.length());
        frameLoadDelegate->didReceiveTitle(d->webView, titleBStr, this);
        SysFreeString(titleBStr);
        frameLoadDelegate->Release();
    }
}

void WebFrame::setStatusText(const String& statusText)
{
    IWebUIDelegate* uiDelegate;
    if (SUCCEEDED(d->webView->uiDelegate(&uiDelegate))) {
        BSTR statusBStr = SysAllocStringLen(statusText.characters(), statusText.length());
        uiDelegate->setStatusText(d->webView, statusBStr);
        SysFreeString(statusBStr);
        uiDelegate->Release();
    }
}