/*
* Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
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

#include "stdafx.h"
#include "ResourceLoadDelegate.h"

#include "Common.h"
#include "WebKitLegacyBrowserWindow.h"
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <comip.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <string>
#include <wininet.h>

HRESULT ResourceLoadDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebResourceLoadDelegate*>(this);
    else if (IsEqualIID(riid, IID_IWebResourceLoadDelegate))
        *ppvObject = static_cast<IWebResourceLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG ResourceLoadDelegate::AddRef()
{
    return m_client->AddRef();
}

ULONG ResourceLoadDelegate::Release()
{
    return m_client->Release();
}

HRESULT ResourceLoadDelegate::identifierForInitialRequest(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _In_opt_ IWebDataSource*, unsigned long identifier)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::willSendRequest(_In_opt_ IWebView*, unsigned long, _In_opt_ IWebURLRequest*, _In_opt_ IWebURLResponse*, _In_opt_ IWebDataSource*, _COM_Outptr_opt_ IWebURLRequest** result)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::didReceiveAuthenticationChallenge(_In_opt_ IWebView*, unsigned long, _In_opt_ IWebURLAuthenticationChallenge* challenge, _In_opt_ IWebDataSource*)
{
    COMPtr<IWebURLAuthenticationChallengeSender> sender;
    if (!challenge || FAILED(challenge->sender(&sender)))
        return E_FAIL;

    COMPtr<IWebURLProtectionSpace> protectionSpace;
    if (FAILED(challenge->protectionSpace(&protectionSpace)))
        return E_FAIL;

    BSTR realm;
    if (FAILED(protectionSpace->realm(&realm)))
        return E_FAIL;

    if (auto credential = askCredential(m_client->hwnd(), std::wstring(realm))) {
        COMPtr<IWebURLCredential> wkCredential;
        if (FAILED(WebKitCreateInstance(CLSID_WebURLCredential, 0, IID_IWebURLCredential, (void**)&wkCredential)))
            return E_FAIL;
        wkCredential->initWithUser(_bstr_t(credential->username.c_str()), _bstr_t(credential->password.c_str()), WebURLCredentialPersistenceForSession);

        sender->useCredential(wkCredential.get(), challenge);
        return S_OK;
    }

    return E_FAIL;
}

HRESULT ResourceLoadDelegate::didCancelAuthenticationChallenge(_In_opt_ IWebView*, unsigned long, _In_opt_ IWebURLAuthenticationChallenge*, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::didReceiveResponse(_In_opt_ IWebView*, unsigned long, _In_opt_ IWebURLResponse*, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::didReceiveContentLength(_In_opt_ IWebView*, unsigned long, UINT, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::didFinishLoadingFromDataSource(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::didFailLoadingWithError(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebError*, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}

HRESULT ResourceLoadDelegate::plugInFailedWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebDataSource*)
{
    return E_NOTIMPL;
}
