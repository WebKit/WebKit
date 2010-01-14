/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "ResourceLoadDelegate.h"

#include "DumpRenderTree.h"
#include "LayoutTestController.h"
#include <comutil.h>
#include <WebKit/WebKitCOMAPI.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <sstream>


using std::wstring;
using std::wiostream;

static inline wstring wstringFromBSTR(BSTR str)
{
    return wstring(str, ::SysStringLen(str));
}

wstring wstringFromInt(int i)
{
    std::wostringstream ss;
    ss << i;
    return ss.str();
}

typedef HashMap<unsigned long, wstring> IdentifierMap;

IdentifierMap& urlMap()
{
    static IdentifierMap urlMap;

    return urlMap;
}

static wstring descriptionSuitableForTestResult(unsigned long identifier)
{
    IdentifierMap::iterator it = urlMap().find(identifier);
    
    if (it == urlMap().end())
        return L"<unknown>";

    return urlSuitableForTestResult(it->second);
}

static wstring descriptionSuitableForTestResult(IWebURLRequest* request)
{
    if (!request)
        return L"(null)";

    BSTR urlBSTR;
    if (FAILED(request->URL(&urlBSTR)))
        return wstring();
    
    wstring url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));
    ::SysFreeString(urlBSTR);
    
    BSTR mainDocumentURLBSTR;
    if (FAILED(request->mainDocumentURL(&mainDocumentURLBSTR)))
        return wstring();
    
    wstring mainDocumentURL = urlSuitableForTestResult(wstringFromBSTR(mainDocumentURLBSTR));
    ::SysFreeString(mainDocumentURLBSTR);
    
    BSTR httpMethodBSTR;
    if (FAILED(request->HTTPMethod(&httpMethodBSTR)))
        return wstring();
    
    wstring httpMethod = wstringFromBSTR(httpMethodBSTR);
    ::SysFreeString(httpMethodBSTR);

    return L"<NSURLRequest URL " + url + L", main document URL " + mainDocumentURL + L", http method " + httpMethod + L">";
}

static wstring descriptionSuitableForTestResult(IWebURLResponse* response)
{
    if (!response)
        return L"(null)";

    BSTR urlBSTR;
    if (FAILED(response->URL(&urlBSTR)))
        return wstring();
    
    wstring url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));
    ::SysFreeString(urlBSTR);

    int statusCode = 0;
    COMPtr<IWebHTTPURLResponse> httpResponse;
    if (response && SUCCEEDED(response->QueryInterface(&httpResponse)))
        httpResponse->statusCode(&statusCode);
    
    return L"<NSURLResponse " + url + L", http status code " + wstringFromInt(statusCode) + L">";
}

static wstring descriptionSuitableForTestResult(IWebError* error, unsigned long identifier)
{
    wstring result = L"<NSError ";

    BSTR domainSTR;
    if (FAILED(error->domain(&domainSTR)))
        return wstring();

    wstring domain = wstringFromBSTR(domainSTR);
    ::SysFreeString(domainSTR);

    int code;
    if (FAILED(error->code(&code)))
        return wstring();

    if (domain == L"CFURLErrorDomain") {
        domain = L"NSURLErrorDomain";

        // Convert kCFURLErrorUnknown to NSURLErrorUnknown
        if (code == -998)
            code = -1;
    } else if (domain == L"kCFErrorDomainWinSock") {
        domain = L"NSURLErrorDomain";

        // Convert the winsock error code to an NSURLError code.
        if (code == WSAEADDRNOTAVAIL)
            code = -1004; // NSURLErrorCannotConnectToHose;
    }

    result += L"domain " + domain;
    result += L", code " + wstringFromInt(code);

    BSTR failingURLSTR;
    if (FAILED(error->failingURL(&failingURLSTR)))
        return wstring();

    wstring failingURL;
    
    // If the error doesn't have a failing URL, we fake one by using the URL the resource had 
    // at creation time. This seems to work fine for now.
    // See <rdar://problem/5064234> CFErrors should have failingURL key.
    if (failingURLSTR)
        failingURL = wstringFromBSTR(failingURLSTR);
    else
        failingURL = descriptionSuitableForTestResult(identifier);

    ::SysFreeString(failingURLSTR);

    result += L", failing URL \"" + urlSuitableForTestResult(failingURL) + L"\">";

    return result;
}

ResourceLoadDelegate::ResourceLoadDelegate()
    : m_refCount(1)
{
}

ResourceLoadDelegate::~ResourceLoadDelegate()
{
}

HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebResourceLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebResourceLoadDelegate))
        *ppvObject = static_cast<IWebResourceLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE ResourceLoadDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE ResourceLoadDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::identifierForInitialRequest( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebURLRequest* request,
    /* [in] */ IWebDataSource* dataSource,
    /* [in] */ unsigned long identifier)
{ 
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        BSTR urlStr;
        if (FAILED(request->URL(&urlStr)))
            return E_FAIL;

        urlMap().set(identifier, wstringFromBSTR(urlStr));
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::willSendRequest( 
    /* [in] */ IWebView* webView,
    /* [in] */ unsigned long identifier,
    /* [in] */ IWebURLRequest* request,
    /* [in] */ IWebURLResponse* redirectResponse,
    /* [in] */ IWebDataSource* dataSource,
    /* [retval][out] */ IWebURLRequest **newRequest)
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        printf("%S - willSendRequest %S redirectResponse %S\n", 
            descriptionSuitableForTestResult(identifier).c_str(),
            descriptionSuitableForTestResult(request).c_str(),
            descriptionSuitableForTestResult(redirectResponse).c_str());
    }

    if (!done && gLayoutTestController->willSendRequestReturnsNull()) {
        *newRequest = 0;
        return S_OK;
    }

    if (!done && gLayoutTestController->willSendRequestReturnsNullOnRedirect() && redirectResponse) {
        printf("Returning null for this redirect\n");
        *newRequest = 0;
        return S_OK;
    }

    request->AddRef();
    *newRequest = request;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::didReceiveAuthenticationChallenge( 
    /* [in] */ IWebView *webView,
    /* [in] */ unsigned long identifier,
    /* [in] */ IWebURLAuthenticationChallenge *challenge,
    /* [in] */ IWebDataSource *dataSource)
{
    if (!gLayoutTestController->handlesAuthenticationChallenges())
        return E_FAIL;
    
    const char* user = gLayoutTestController->authenticationUsername().c_str();
    const char* password = gLayoutTestController->authenticationPassword().c_str();

    printf("%S - didReceiveAuthenticationChallenge - Responding with %s:%s\n", descriptionSuitableForTestResult(identifier).c_str(), user, password);
    
    COMPtr<IWebURLAuthenticationChallengeSender> sender;
    if (!challenge || FAILED(challenge->sender(&sender)))
        return E_FAIL;
        
    COMPtr<IWebURLCredential> credential;
    if (FAILED(WebKitCreateInstance(CLSID_WebURLCredential, 0, IID_IWebURLCredential, (void**)&credential)))
        return E_FAIL;
    credential->initWithUser(_bstr_t(user), _bstr_t(password), WebURLCredentialPersistenceForSession);

    sender->useCredential(credential.get(), challenge);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::didReceiveResponse(
    /* [in] */ IWebView* webView, 
    /* [in] */ unsigned long identifier, 
    /* [in] */ IWebURLResponse* response, 
    /* [in] */ IWebDataSource* dataSource)
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        printf("%S - didReceiveResponse %S\n",
            descriptionSuitableForTestResult(identifier).c_str(),
            descriptionSuitableForTestResult(response).c_str());
    }
    if (!done && gLayoutTestController->dumpResourceResponseMIMETypes()) {
        BSTR mimeTypeBSTR;
        if (FAILED(response->MIMEType(&mimeTypeBSTR)))
            E_FAIL;
    
        wstring mimeType = wstringFromBSTR(mimeTypeBSTR);
        ::SysFreeString(mimeTypeBSTR);

        BSTR urlBSTR;
        if (FAILED(response->URL(&urlBSTR)))
            E_FAIL;
    
        wstring url = urlSuitableForTestResult(wstringFromBSTR(urlBSTR));
        ::SysFreeString(urlBSTR);

        printf("%S has MIME type %S\n", url.c_str(), mimeType.c_str());
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::didFinishLoadingFromDataSource( 
    /* [in] */ IWebView* webView,
    /* [in] */ unsigned long identifier,
    /* [in] */ IWebDataSource* dataSource)
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        printf("%S - didFinishLoading\n",
            descriptionSuitableForTestResult(identifier).c_str()),
       urlMap().remove(identifier);
    }

   return S_OK;
}
        
HRESULT STDMETHODCALLTYPE ResourceLoadDelegate::didFailLoadingWithError( 
    /* [in] */ IWebView* webView,
    /* [in] */ unsigned long identifier,
    /* [in] */ IWebError* error,
    /* [in] */ IWebDataSource* dataSource)
{
    if (!done && gLayoutTestController->dumpResourceLoadCallbacks()) {
        printf("%S - didFailLoadingWithError: %S\n", 
            descriptionSuitableForTestResult(identifier).c_str(),
            descriptionSuitableForTestResult(error, identifier).c_str());
        urlMap().remove(identifier);
    }

    return S_OK;
}
