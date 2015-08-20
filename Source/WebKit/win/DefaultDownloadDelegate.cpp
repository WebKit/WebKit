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
#include "DefaultDownloadDelegate.h"

#include "MarshallingHelpers.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
#include <WebCore/COMPtr.h>
#include <wtf/text/CString.h>

#include <shlobj.h>
#include <wchar.h>

#include <WebCore/BString.h>

using namespace WebCore;


// DefaultDownloadDelegate ----------------------------------------------------------------

DefaultDownloadDelegate::DefaultDownloadDelegate()
{
    gClassCount++;
    gClassNameCount().add("DefaultDownloadDelegate");
}

DefaultDownloadDelegate::~DefaultDownloadDelegate()
{
    gClassCount--;
    gClassNameCount().remove("DefaultDownloadDelegate");
    HashSet<IWebDownload*>::iterator i = m_downloads.begin();
    for (;i != m_downloads.end(); ++i)
        (*i)->Release();
}

DefaultDownloadDelegate* DefaultDownloadDelegate::sharedInstance()
{
    static COMPtr<DefaultDownloadDelegate> shared;
    if (!shared)
        shared.adoptRef(DefaultDownloadDelegate::createInstance());
    return shared.get();
}

DefaultDownloadDelegate* DefaultDownloadDelegate::createInstance()
{
    DefaultDownloadDelegate* instance = new DefaultDownloadDelegate();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT DefaultDownloadDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownloadDelegate))
        *ppvObject = static_cast<IWebDownloadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG DefaultDownloadDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG DefaultDownloadDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT DefaultDownloadDelegate::decideDestinationWithSuggestedFilename(_In_opt_ IWebDownload* download, _In_ BSTR filename)
{
    LOG(Download, "DefaultDownloadDelegate %p - decideDestinationWithSuggestedFilename %s", download, String(filename, SysStringLen(filename)).ascii().data());

    if (!download)
        return E_POINTER;

    WCHAR pathChars[MAX_PATH];
    if (FAILED(SHGetFolderPath(0, CSIDL_DESKTOPDIRECTORY  | CSIDL_FLAG_CREATE, 0, 0, pathChars))) {
        if (FAILED(download->setDestination(filename, true))) {
            LOG_ERROR("Failed to set destination on file");
            return E_FAIL;
        }  
        return S_OK;
    }

    size_t fullLength = wcslen(pathChars) + SysStringLen(filename) + 2;
    BSTR full = SysAllocStringLen(0, (UINT)fullLength);
    if (!full)
        return E_OUTOFMEMORY;

    wcscpy_s(full, fullLength, pathChars);
    wcscat_s(full, fullLength, L"\\");
    wcscat_s(full, fullLength, filename);
    BString fullPath;
    fullPath.adoptBSTR(full);

#ifndef NDEBUG
    String debug((BSTR)fullPath, SysStringLen(BSTR(fullPath)));
    LOG(Download, "Setting path to %s", debug.ascii().data());
#endif

    if (FAILED(download->setDestination(fullPath, true))) {
        LOG_ERROR("Failed to set destination on file");
        return E_FAIL;
    }
    return S_OK;
}
HRESULT DefaultDownloadDelegate::didCancelAuthenticationChallenge(_In_opt_ IWebDownload* download, _In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    LOG(Download, "DefaultDownloadDelegate %p - didCancelAuthenticationChallenge %p", download, challenge);
    download = nullptr;
    challenge = nullptr;
    return S_OK;
}
HRESULT DefaultDownloadDelegate::didCreateDestination(_In_opt_ IWebDownload* download, _In_ BSTR destination)
{
    LOG(Download, "DefaultDownloadDelegate %p - didCreateDestination %s", download, String(destination, SysStringLen(destination)).ascii().data());
    download = nullptr;
    destination = nullptr;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didReceiveAuthenticationChallenge(_In_opt_ IWebDownload* download, _In_opt_ IWebURLAuthenticationChallenge* challenge)
{
    LOG(Download, "DefaultDownloadDelegate %p - didReceiveAuthenticationChallenge %p", download, challenge);
    download = nullptr;
    challenge = nullptr;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didReceiveDataOfLength(_In_opt_ IWebDownload* download, unsigned length)
{
    LOG(Download, "DefaultDownloadDelegate %p - didReceiveDataOfLength %i", download, length);
    download = nullptr;
    length = 0;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didReceiveResponse(_In_opt_ IWebDownload* download, _In_opt_ IWebURLResponse* response)
{
    LOG(Download, "DefaultDownloadDelegate %p - didReceiveResponse %p", download, response);
    download = nullptr;
    response = 0;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::shouldDecodeSourceDataOfMIMEType(_In_opt_ IWebDownload* download, _In_ BSTR encodingType, _Out_ BOOL* shouldDecode)
{
    LOG(Download, "DefaultDownloadDelegate %p - shouldDecodeSourceDataOfMIMEType %s", download, String(encodingType, SysStringLen(encodingType)).ascii().data());
    if (!shouldDecode)
        return E_POINTER;
    download = nullptr;
    encodingType = nullptr;
    *shouldDecode = false;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::willResumeWithResponse(_In_opt_ IWebDownload* download, _In_opt_ IWebURLResponse* response, long long fromByte)
{
    LOG(Download, "DefaultDownloadDelegate %p - willResumeWithResponse %p, %q", download, response, fromByte);
    download = nullptr;
    response = nullptr;
    fromByte = 0;
    return S_OK;
}

HRESULT DefaultDownloadDelegate::willSendRequest(_In_opt_ IWebDownload* download, _In_opt_ IWebMutableURLRequest* request,
    _In_opt_ IWebURLResponse* redirectResponse, _COM_Outptr_opt_ IWebMutableURLRequest** finalRequest)
{
    LOG(Download, "DefaultDownloadDelegate %p - willSendRequest %p %p", download, request, redirectResponse);
    if (!finalRequest)
        return E_POINTER;

    download = nullptr;
    redirectResponse = nullptr;
    *finalRequest = request;
    (*finalRequest)->AddRef();
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didBegin(_In_opt_ IWebDownload* download)
{
    LOG(Download, "DefaultDownloadDelegate %p - didBegin", download);
    registerDownload(download);
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didFinish(_In_opt_ IWebDownload* download)
{
    LOG(Download, "DefaultDownloadDelegate %p - didFinish", download);
    unregisterDownload(download);
    return S_OK;
}

HRESULT DefaultDownloadDelegate::didFailWithError(_In_opt_ IWebDownload* download, _In_opt_ IWebError* error)
{
    LOG(Download, "DefaultDownloadDelegate %p - didFailWithError %p", download, error);
    unregisterDownload(download);
    error = nullptr;
    return S_OK;
}

void DefaultDownloadDelegate::registerDownload(IWebDownload* download)
{
    if (m_downloads.contains(download))
        return;
    download->AddRef();
    m_downloads.add(download);
}

void DefaultDownloadDelegate::unregisterDownload(IWebDownload* download)
{
    if (m_downloads.contains(download)) {
        download->Release();
        m_downloads.remove(download);
    }
}
