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
#include "WebDownload.h"

#include "DefaultDownloadDelegate.h"
#include "MarshallingHelpers.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLCredential.h"
#include "WebURLResponse.h"
#include <wtf/text/CString.h>

#include <WebCore/BString.h>
#include <WebCore/DownloadBundle.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/StdLibExtras.h>

using namespace WebCore;

// WebDownload ----------------------------------------------------------------

WebDownload::WebDownload()
{
    gClassCount++;
    gClassNameCount().add("WebDownload");
}

WebDownload::~WebDownload()
{
    LOG(Download, "WebDownload - Destroying download (%p)", this);
    cancel();
    gClassCount--;
    gClassNameCount().remove("WebDownload");
}

WebDownload* WebDownload::createInstance()
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    return instance;
}

WebDownload* WebDownload::createInstance(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    instance->init(handle, request, response, delegate);
    return instance;
}

WebDownload* WebDownload::createInstance(const URL& url, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    instance->init(url, delegate);
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebDownload::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownload))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLAuthenticationChallengeSender))
        *ppvObject = static_cast<IWebURLAuthenticationChallengeSender*>(this);
    else if (IsEqualGUID(riid, CLSID_WebDownload))
        *ppvObject = static_cast<WebDownload*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebDownload::AddRef()
{
    return ++m_refCount;
}

ULONG WebDownload::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebDownload -------------------------------------------------------------------

HRESULT WebDownload::canResumeDownloadDecodedWithEncodingMIMEType(_In_ BSTR, _Out_ BOOL*)
{
    notImplemented();
    return E_FAIL;
}

HRESULT WebDownload::bundlePathForTargetPath(_In_ BSTR targetPath, __deref_out_opt BSTR* bundlePath)
{
    if (!targetPath)
        return E_INVALIDARG;

    String bundle(targetPath, SysStringLen(targetPath));
    if (bundle.isEmpty())
        return E_INVALIDARG;

    if (bundle[bundle.length()-1] == '/')
        bundle.truncate(1);

    bundle.append(DownloadBundle::fileExtension());
    *bundlePath = BString(bundle).release();
    if (!*bundlePath)
       return E_FAIL;
    return S_OK;
}

HRESULT WebDownload::request(_COM_Outptr_opt_ IWebURLRequest** request)
{
    if (!request)
        return E_POINTER;

    *request = m_request.get();
    if (*request)
        (*request)->AddRef();

    return S_OK;
}
