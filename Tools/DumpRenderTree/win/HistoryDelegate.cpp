/*
 * Copyright (C) 2009, 2014-2015 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "HistoryDelegate.h"

#include "DumpRenderTree.h"
#include "DumpRenderTreeWin.h"
#include "TestRunner.h"
#include <WebKitLegacy/WebKit.h>
#include <comutil.h>
#include <string>

using std::wstring;

static inline wstring wstringFromBSTR(BSTR str)
{
    return wstring(str, ::SysStringLen(str));
}

HistoryDelegate::HistoryDelegate()
{
}

HistoryDelegate::~HistoryDelegate()
{
}

// IUnknown
HRESULT HistoryDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistoryDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryDelegate))
        *ppvObject = static_cast<IWebHistoryDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG HistoryDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG HistoryDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistoryDelegate
HRESULT HistoryDelegate::didNavigateWithNavigationData(_In_opt_ IWebView* webView, _In_opt_ IWebNavigationData* navigationData, _In_opt_ IWebFrame* webFrame)
{
    if (!gTestRunner->dumpHistoryDelegateCallbacks())
        return S_OK;

    _bstr_t urlBSTR;
    if (FAILED(navigationData->url(&urlBSTR.GetBSTR())))
        return E_FAIL;
    wstring url;
    if (urlBSTR.length())
        url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));

    _bstr_t titleBSTR;
    if (FAILED(navigationData->title(&titleBSTR.GetBSTR())))
        return E_FAIL;

    if (!static_cast<char*>(titleBSTR))
        titleBSTR = L"";

    COMPtr<IWebURLRequest> request;
    if (FAILED(navigationData->originalRequest(&request)))
        return E_FAIL;

    _bstr_t httpMethodBSTR;
    if (FAILED(request->HTTPMethod(&httpMethodBSTR.GetBSTR())))
        return E_FAIL;

    if (!static_cast<char*>(httpMethodBSTR))
        httpMethodBSTR = L"";

    COMPtr<IWebURLResponse> response;
    if (FAILED(navigationData->response(&response)))
        return E_FAIL;

    COMPtr<IWebHTTPURLResponse> httpResponse;
    if (FAILED(response->QueryInterface(&httpResponse)))
        return E_FAIL;

    int statusCode = 0;
    if (FAILED(httpResponse->statusCode(&statusCode)))
        return E_FAIL;

    BOOL hasSubstituteData;
    if (FAILED(navigationData->hasSubstituteData(&hasSubstituteData)))
        return E_FAIL;

    _bstr_t clientRedirectSourceBSTR;
    if (FAILED(navigationData->clientRedirectSource(&clientRedirectSourceBSTR.GetBSTR())))
        return E_FAIL;

    if (!static_cast<char*>(clientRedirectSourceBSTR))
        clientRedirectSourceBSTR = L"";

    bool hasClientRedirect = clientRedirectSourceBSTR.length();
    wstring redirectSource;
    if (clientRedirectSourceBSTR.length())
        redirectSource = urlSuitableForTestResult(wstringFromBSTR(clientRedirectSourceBSTR));

    bool wasFailure = hasSubstituteData || (httpResponse && statusCode >= 400);
        
    fprintf(testResult, "WebView navigated to url \"%S\" with title \"%s\" with HTTP equivalent method \"%s\".  The navigation was %s and was %s%S.\n", 
        url.c_str(), 
        static_cast<char*>(titleBSTR),
        static_cast<char*>(httpMethodBSTR),
        wasFailure ? "a failure" : "successful", 
        hasClientRedirect ? "a client redirect from " : "not a client redirect", 
        redirectSource.c_str());

    return S_OK;
}

HRESULT HistoryDelegate::didPerformClientRedirectFromURL(_In_opt_ IWebView*, _In_ BSTR sourceURL, _In_ BSTR destinationURL, _In_opt_ IWebFrame*)
{
    if (!gTestRunner->dumpHistoryDelegateCallbacks())
        return S_OK;

    wstring source;
    if (sourceURL)
        source = urlSuitableForTestResult(wstringFromBSTR(sourceURL));
    
    wstring destination;
    if (destinationURL)
        destination = urlSuitableForTestResult(wstringFromBSTR(destinationURL));

    fprintf(testResult, "WebView performed a client redirect from \"%S\" to \"%S\".\n", source.c_str(), destination.c_str());
    return S_OK;
}
    
HRESULT HistoryDelegate::didPerformServerRedirectFromURL(_In_opt_ IWebView* webView, _In_ BSTR sourceURL, _In_ BSTR destinationURL, _In_opt_ IWebFrame* webFrame)
{
    if (!gTestRunner->dumpHistoryDelegateCallbacks())
        return S_OK;

    wstring source;
    if (sourceURL)
        source = urlSuitableForTestResult(wstringFromBSTR(sourceURL));
    
    wstring destination;
    if (destinationURL)
        destination = urlSuitableForTestResult(wstringFromBSTR(destinationURL));

    fprintf(testResult, "WebView performed a server redirect from \"%S\" to \"%S\".\n", source.c_str(), destination.c_str());
    return S_OK;
}

HRESULT HistoryDelegate::updateHistoryTitle(_In_opt_ IWebView* webView, _In_ BSTR titleBSTR, _In_ BSTR urlBSTR)
{
    if (!gTestRunner->dumpHistoryDelegateCallbacks())
        return S_OK;
    
    wstring url;
    if (urlBSTR)
        url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));

    fprintf(testResult, "WebView updated the title for history URL \"%S\" to \"%S\".\n", url.c_str(), titleBSTR ? titleBSTR : L"");
    return S_OK;
}
    
HRESULT HistoryDelegate::populateVisitedLinksForWebView(_In_opt_ IWebView* webView)
{
    if (!gTestRunner->dumpHistoryDelegateCallbacks())
        return S_OK;

    _bstr_t urlBSTR;
    if (FAILED(webView->mainFrameURL(&urlBSTR.GetBSTR())))
        return E_FAIL;

    wstring url;
    if (urlBSTR.length())
        url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));

    if (gTestRunner->dumpVisitedLinksCallback())
        fprintf(testResult, "Asked to populate visited links for WebView \"%S\"\n", url.c_str());

    return S_OK;
}
