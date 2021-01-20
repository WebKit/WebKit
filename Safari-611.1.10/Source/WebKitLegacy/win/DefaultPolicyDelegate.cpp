/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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
#include "DefaultPolicyDelegate.h"

#include <comutil.h>
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

// FIXME: move this enum to a separate header file when other code begins to use it.
typedef enum WebExtraNavigationType {
    WebNavigationTypePlugInRequest = WebNavigationTypeOther + 1
} WebExtraNavigationType;

// DefaultPolicyDelegate ----------------------------------------------------------------

DefaultPolicyDelegate::DefaultPolicyDelegate()
{
    gClassCount++;
    gClassNameCount().add("DefaultPolicyDelegate");
}

DefaultPolicyDelegate::~DefaultPolicyDelegate()
{
    gClassCount--;
    gClassNameCount().remove("DefaultPolicyDelegate");
}

DefaultPolicyDelegate* DefaultPolicyDelegate::sharedInstance()
{
    static COMPtr<DefaultPolicyDelegate> shared;
    if (!shared)
        shared.adoptRef(DefaultPolicyDelegate::createInstance());
    return shared.get();
}

DefaultPolicyDelegate* DefaultPolicyDelegate::createInstance()
{
    DefaultPolicyDelegate* instance = new DefaultPolicyDelegate();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT DefaultPolicyDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IWebPolicyDelegate))
        *ppvObject = static_cast<IWebPolicyDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG DefaultPolicyDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG DefaultPolicyDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT DefaultPolicyDelegate::decidePolicyForNavigationAction(_In_opt_ IWebView* webView, _In_opt_ IPropertyBag* actionInformation,
    _In_opt_ IWebURLRequest* request, _In_opt_ IWebFrame* /*frame*/, _In_opt_ IWebPolicyDecisionListener* listener)
{
    int navType = 0;
    _variant_t var;
    if (SUCCEEDED(actionInformation->Read(WebActionNavigationTypeKey, &var.GetVARIANT(), nullptr))) {
        var.ChangeType(VT_I4, nullptr);
        navType = V_I4(&var);
    }
    COMPtr<IWebViewPrivate2> wvPrivate(Query, webView);
    if (wvPrivate) {
        BOOL canHandleRequest = FALSE;
        if (SUCCEEDED(wvPrivate->canHandleRequest(request, &canHandleRequest)) && canHandleRequest)
            listener->use();
        else if (navType == WebNavigationTypePlugInRequest)
            listener->use();
        else {
            BString url;
            // A file URL shouldn't fall through to here, but if it did,
            // it would be a security risk to open it.
            if (SUCCEEDED(request->URL(&url)) && !String(url, SysStringLen(url)).startsWith("file:")) {
                // FIXME: Open the URL not by means of a webframe, but by handing it over to the system and letting it determine how to open that particular URL scheme.  See documentation for [NSWorkspace openURL]
                ;
            }
            listener->ignore();
        }
    }
    return S_OK;
}

HRESULT DefaultPolicyDelegate::decidePolicyForNewWindowAction(_In_opt_ IWebView* /*webView*/, _In_opt_ IPropertyBag* /*actionInformation*/,
    _In_opt_ IWebURLRequest* /*request*/, _In_ BSTR /*frameName*/, _In_opt_ IWebPolicyDecisionListener* listener)
{
    if (!listener)
        return E_POINTER;

    listener->use();
    return S_OK;
}

HRESULT DefaultPolicyDelegate::decidePolicyForMIMEType(_In_opt_ IWebView* webView, _In_ BSTR type, _In_opt_ IWebURLRequest* request,
    _In_opt_ IWebFrame* /*frame*/, _In_opt_ IWebPolicyDecisionListener* listener)
{
    BOOL canShowMIMEType;
    if (FAILED(webView->canShowMIMEType(type, &canShowMIMEType)))
        canShowMIMEType = FALSE;

    BString url;
    request->URL(&url);

    if (String(url, SysStringLen(url)).startsWith("file:")) {
        BOOL isDirectory = FALSE;
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (GetFileAttributesEx(url, GetFileExInfoStandard, &attrs))
            isDirectory = !!(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

        if (isDirectory)
            listener->ignore();
        else if(canShowMIMEType)
            listener->use();
        else
            listener->ignore();
    } else if (canShowMIMEType)
        listener->use();
    else
        listener->ignore();
    return S_OK;
}

HRESULT DefaultPolicyDelegate::unableToImplementPolicyWithError(_In_opt_ IWebView* /*webView*/, _In_opt_ IWebError* error, _In_opt_ IWebFrame* frame)
{
    if (!error || !frame)
        return E_POINTER;

    BString errorStr;
    error->localizedDescription(&errorStr);

    BString frameName;
    frame->name(&frameName);

    LOG_ERROR("called unableToImplementPolicyWithError:%S inFrame:%S", errorStr ? static_cast<BSTR>(errorStr) : TEXT(""), frameName ? static_cast<BSTR>(frameName) : TEXT(""));

    return S_OK;
}
