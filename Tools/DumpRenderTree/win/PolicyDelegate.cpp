/*
 * Copyright (C) 2007, 2009, 2014-2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "PolicyDelegate.h"

#include "DumpRenderTree.h"
#include "TestRunner.h"
#include <comutil.h>
#include <string>

using std::wstring;

static wstring dumpPath(IDOMNode* node)
{
    ASSERT(node);

    wstring result;

    _bstr_t name;
    if (FAILED(node->nodeName(&name.GetBSTR())))
        return result;
    result.assign(name, name.length());

    COMPtr<IDOMNode> parent;
    if (SUCCEEDED(node->parentNode(&parent)))
        result += TEXT(" > ") + dumpPath(parent.get());

    return result;
}

PolicyDelegate::PolicyDelegate()
{
}

// IUnknown
HRESULT PolicyDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebPolicyDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebPolicyDelegate))
        *ppvObject = static_cast<IWebPolicyDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG PolicyDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG PolicyDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

HRESULT PolicyDelegate::decidePolicyForNavigationAction(_In_opt_ IWebView*, _In_opt_ IPropertyBag* actionInformation,
    _In_opt_ IWebURLRequest* request, _In_opt_ IWebFrame* frame, _In_opt_ IWebPolicyDecisionListener* listener)
{
    _bstr_t url;
    request->URL(&url.GetBSTR());
    wstring wurl = urlSuitableForTestResult(wstring(url, url.length()));

    int navType = 0;
    _variant_t var;
    if (SUCCEEDED(actionInformation->Read(WebActionNavigationTypeKey, &var.GetVARIANT(), nullptr))) {
        V_VT(&var) = VT_I4;
        navType = V_I4(&var);
    }

    LPCTSTR typeDescription;
    switch (navType) {
        case WebNavigationTypeLinkClicked:
            typeDescription = TEXT("link clicked");
            break;
        case WebNavigationTypeFormSubmitted:
            typeDescription = TEXT("form submitted");
            break;
        case WebNavigationTypeBackForward:
            typeDescription = TEXT("back/forward");
            break;
        case WebNavigationTypeReload:
            typeDescription = TEXT("reload");
            break;
        case WebNavigationTypeFormResubmitted:
            typeDescription = TEXT("form resubmitted");
            break;
        case WebNavigationTypeOther:
            typeDescription = TEXT("other");
            break;
        default:
            typeDescription = TEXT("illegal value");
    }

    wstring message = TEXT("Policy delegate: attempt to load ") + wurl + TEXT(" with navigation type '") + typeDescription + TEXT("'");

    _variant_t actionElementVar;
    if (SUCCEEDED(actionInformation->Read(WebActionElementKey, &actionElementVar, nullptr))) {
        COMPtr<IPropertyBag> actionElement(Query, V_UNKNOWN(&actionElementVar));
        _variant_t originatingNodeVar;
        if (SUCCEEDED(actionElement->Read(WebElementDOMNodeKey, &originatingNodeVar, nullptr))) {
            COMPtr<IDOMNode> originatingNode(Query, V_UNKNOWN(&originatingNodeVar));
            message += TEXT(" originating from ") + dumpPath(originatingNode.get());
        }
    }

    fprintf(testResult, "%S\n", message.c_str());

    if (m_permissiveDelegate)
        listener->use();
    else
        listener->ignore();

    if (m_controllerToNotifyDone) {
        m_controllerToNotifyDone->notifyDone();
        m_controllerToNotifyDone = nullptr;
    }

    return S_OK;
}


HRESULT PolicyDelegate::unableToImplementPolicyWithError(_In_opt_ IWebView*, _In_opt_ IWebError* error, _In_opt_ IWebFrame* frame)
{
    _bstr_t domainStr;
    error->domain(&domainStr.GetBSTR());

    int code;
    error->code(&code);
    
    _bstr_t frameName;
    frame->name(&frameName.GetBSTR());
    
    fprintf(testResult, "Policy delegate: unable to implement policy with error domain '%S', error code %d, in frame '%S'\n", static_cast<wchar_t*>(domainStr), code, static_cast<TCHAR*>(frameName));
    
    return S_OK;
}
