/*
 * Copyright (C) 2006-2018 Apple Inc.  All rights reserved.
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

#include "WebURLResponse.h"

#include "WebKitDLL.h"
#include "WebKit.h"

#include "COMPropertyBag.h"
#include "MarshallingHelpers.h"

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if USE(CFURLCONNECTION)
#include <pal/spi/win/CFNetworkSPIWin.h>
#endif

#include <WebCore/BString.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wchar.h>
#include <wtf/URL.h>

using namespace WebCore;

static String localizedShortDescriptionForStatusCode(int statusCode)
{
    String result;
    if (statusCode < 100 || statusCode >= 600)
        result = WEB_UI_STRING("server error", "HTTP result code string");
    else if (statusCode >= 100 && statusCode <= 199) {
        switch (statusCode) {
            case 100:
                result = WEB_UI_STRING("continue", "HTTP result code string");
                break;
            case 101:
                result = WEB_UI_STRING("switching protocols", "HTTP result code string");
                break;
            default:
                result = WEB_UI_STRING("informational", "HTTP result code string");
                break;
        }
    } else if (statusCode >= 200 && statusCode <= 299) {
        switch (statusCode) {
            case 200:
                result = WEB_UI_STRING("no error", "HTTP result code string");
                break;
            case 201:
                result = WEB_UI_STRING("created", "HTTP result code string");
                break;
            case 202:
                result = WEB_UI_STRING("accepted", "HTTP result code string");
                break;
            case 203:
                result = WEB_UI_STRING("non-authoritative information", "HTTP result code string");
                break;
            case 204:
                result = WEB_UI_STRING("no content", "HTTP result code string");
                break;
            case 205:
                result = WEB_UI_STRING("reset content", "HTTP result code string");
                break;
            case 206:
                result = WEB_UI_STRING("partial content", "HTTP result code string");
                break;
            default:
                result = WEB_UI_STRING("success", "HTTP result code string");
                break;
        } 
    } else if (statusCode >= 300 && statusCode <= 399) {
        switch (statusCode) {
            case 300:
                result = WEB_UI_STRING("multiple choices", "HTTP result code string");
                break;
            case 301:
                result = WEB_UI_STRING("moved permanently", "HTTP result code string");
                break;
            case 302:
                result = WEB_UI_STRING("found", "HTTP result code string");
                break;
            case 303:
                result = WEB_UI_STRING("see other", "HTTP result code string");
                break;
            case 304:
                result = WEB_UI_STRING("not modified", "HTTP result code string");
                break;
            case 305:
                result = WEB_UI_STRING("needs proxy", "HTTP result code string");
                break;
            case 307:
                result = WEB_UI_STRING("temporarily redirected", "HTTP result code string");
                break;
            case 306:   // 306 status code unused in HTTP
            default:
                result = WEB_UI_STRING("redirected", "HTTP result code string");
                break;
        }
    } else if (statusCode >= 400 && statusCode <= 499) {
        switch (statusCode) {
            case 400:
                result = WEB_UI_STRING("bad request", "HTTP result code string");
                break;
            case 401:
                result = WEB_UI_STRING("unauthorized", "HTTP result code string");
                break;
            case 402:
                result = WEB_UI_STRING("payment required", "HTTP result code string");
                break;
            case 403:
                result = WEB_UI_STRING("forbidden", "HTTP result code string");
                break;
            case 404:
                result = WEB_UI_STRING("not found", "HTTP result code string");
                break;
            case 405:
                result = WEB_UI_STRING("method not allowed", "HTTP result code string");
                break;
            case 406:
                result = WEB_UI_STRING("unacceptable", "HTTP result code string");
                break;
            case 407:
                result = WEB_UI_STRING("proxy authentication required", "HTTP result code string");
                break;
            case 408:
                result = WEB_UI_STRING("request timed out", "HTTP result code string");
                break;
            case 409:
                result = WEB_UI_STRING("conflict", "HTTP result code string");
                break;
            case 410:
                result = WEB_UI_STRING("no longer exists", "HTTP result code string");
                break;
            case 411:
                result = WEB_UI_STRING("length required", "HTTP result code string");
                break;
            case 412:
                result = WEB_UI_STRING("precondition failed", "HTTP result code string");
                break;
            case 413:
                result = WEB_UI_STRING("request too large", "HTTP result code string");
                break;
            case 414:
                result = WEB_UI_STRING("requested URL too long", "HTTP result code string");
                break;
            case 415:
                result = WEB_UI_STRING("unsupported media type", "HTTP result code string");
                break;
            case 416:
                result = WEB_UI_STRING("requested range not satisfiable", "HTTP result code string");
                break;
            case 417:
                result = WEB_UI_STRING("expectation failed", "HTTP result code string");
                break;
            default:
                result = WEB_UI_STRING("client error", "HTTP result code string");
                break;
        }
    } else if (statusCode >= 500 && statusCode <= 599) {
        switch (statusCode) {
            case 500:
                result = WEB_UI_STRING("internal server error", "HTTP result code string");
                break;
            case 501:
                result = WEB_UI_STRING("unimplemented", "HTTP result code string");
                break;
            case 502:
                result = WEB_UI_STRING("bad gateway", "HTTP result code string");
                break;
            case 503:
                result = WEB_UI_STRING("service unavailable", "HTTP result code string");
                break;
            case 504:
                result = WEB_UI_STRING("gateway timed out", "HTTP result code string");
                break;
            case 505:
                result = WEB_UI_STRING("unsupported version", "HTTP result code string");
                break;
            default:
                result = WEB_UI_STRING("server error", "HTTP result code string");
                break;
        }
    }
    return result;
}

// IWebURLResponse ----------------------------------------------------------------

WebURLResponse::WebURLResponse()
{
    gClassCount++;
    gClassNameCount().add("WebURLResponse"_s);
}

WebURLResponse::~WebURLResponse()
{
    gClassCount--;
    gClassNameCount().remove("WebURLResponse"_s);
}

WebURLResponse* WebURLResponse::createInstance()
{
    WebURLResponse* instance = new WebURLResponse();
    // fake an http response - so it has the IWebHTTPURLResponse interface
    instance->m_response = ResourceResponse(WTF::URL("http://"_s), String(), 0, String());
    instance->AddRef();
    return instance;
}

WebURLResponse* WebURLResponse::createInstance(const ResourceResponse& response)
{
    if (response.isNull())
        return 0;

    WebURLResponse* instance = new WebURLResponse();
    instance->AddRef();
    instance->m_response = response;

    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebURLResponse::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebURLResponse*>(this);
    else if (IsEqualGUID(riid, __uuidof(this)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IWebURLResponse))
        *ppvObject = static_cast<IWebURLResponse*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLResponsePrivate))
        *ppvObject = static_cast<IWebURLResponsePrivate*>(this);
    else if (m_response.isInHTTPFamily() && IsEqualGUID(riid, IID_IWebHTTPURLResponse))
        *ppvObject = static_cast<IWebHTTPURLResponse*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebURLResponse::AddRef()
{
    return ++m_refCount;
}

ULONG WebURLResponse::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLResponse --------------------------------------------------------------------

HRESULT WebURLResponse::expectedContentLength(_Out_ long long* result)
{
    if (!result)
        return E_POINTER;
    *result = m_response.expectedContentLength();
    return S_OK;
}

HRESULT WebURLResponse::initWithURL(_In_ BSTR url, _In_ BSTR mimeType, int expectedContentLength, _In_ BSTR textEncodingName)
{
    m_response = ResourceResponse(MarshallingHelpers::BSTRToKURL(url), String(mimeType), expectedContentLength, String(textEncodingName));
    return S_OK;
}

HRESULT WebURLResponse::MIMEType(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    BString mimeType(m_response.mimeType());
    *result = mimeType.release();
    if (!m_response.mimeType().isNull() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT WebURLResponse::suggestedFilename(__deref_opt_out BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = nullptr;

    if (m_response.url().isEmpty())
        return E_FAIL;

    *result = BString(m_response.suggestedFilename()).release();
    return S_OK;
}

HRESULT WebURLResponse::textEncodingName(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_INVALIDARG;

    BString textEncodingName(m_response.textEncodingName());
    *result = textEncodingName.release();
    if (!m_response.textEncodingName().isNull() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT WebURLResponse::URL(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_INVALIDARG;

    BString url(m_response.url().string());
    *result = url.release();
    if (!m_response.url().isEmpty() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

// IWebHTTPURLResponse --------------------------------------------------------

HRESULT WebURLResponse::allHeaderFields(_COM_Outptr_opt_ IPropertyBag** headerFields)
{
    ASSERT(m_response.isInHTTPFamily());

    if (!headerFields)
        return E_POINTER;

    HashMap<String, String, ASCIICaseInsensitiveHash> fields;
    for (const auto& keyValuePair : m_response.httpHeaderFields())
        fields.add(keyValuePair.key, keyValuePair.value);

    *headerFields = COMPropertyBag<String, String, ASCIICaseInsensitiveHash>::adopt(fields);
    return S_OK;
}

HRESULT WebURLResponse::localizedStringForStatusCode(int statusCode, __deref_opt_out BSTR* statusString)
{
    ASSERT(m_response.isInHTTPFamily());
    if (!statusString)
        return E_POINTER;

    *statusString = nullptr;
    const String& statusText = localizedShortDescriptionForStatusCode(statusCode);
    if (!statusText)
        return E_FAIL;
    *statusString = BString(statusText).release();
    return S_OK;
}

HRESULT WebURLResponse::statusCode(_Out_ int* statusCode)
{
    ASSERT(m_response.isInHTTPFamily());
    if (statusCode)
        *statusCode = m_response.httpStatusCode();
    return S_OK;
}

HRESULT WebURLResponse::isAttachment(_Out_ BOOL* attachment)
{
    if (!attachment)
        return E_POINTER;
    *attachment = m_response.isAttachment();
    return S_OK;
}

HRESULT WebURLResponse::sslPeerCertificate(_Out_ ULONG_PTR* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;

#if USE(CFURLCONNECTION)
    CFDictionaryRef dict = certificateDictionary();
    if (!dict)
        return E_FAIL;
    const void* data = ResourceError::getSSLPeerCertificateDataBytePtr(dict);
    if (!data)
        return E_FAIL;
    *result = reinterpret_cast<ULONG_PTR>(const_cast<void*>(data));
#endif

    return *result ? S_OK : E_FAIL;
}

// WebURLResponse -------------------------------------------------------------

HRESULT WebURLResponse::suggestedFileExtension(BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    if (m_response.mimeType().isEmpty())
        return E_FAIL;

    BString mimeType(m_response.mimeType());
    HKEY key;
    LONG err = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("MIME\\Database\\Content Type"), 0, KEY_QUERY_VALUE, &key);
    if (!err) {
        HKEY subKey;
        err = RegOpenKeyEx(key, mimeType, 0, KEY_QUERY_VALUE, &subKey);
        if (!err) {
            DWORD keyType = REG_SZ;
            WCHAR extension[MAX_PATH];
            DWORD keySize = sizeof(extension)/sizeof(extension[0]);
            err = RegQueryValueEx(subKey, TEXT("Extension"), 0, &keyType, (LPBYTE)extension, &keySize);
            if (!err && keyType != REG_SZ)
                err = ERROR_INVALID_DATA;
            if (err) {
                // fallback handlers
                if (!wcscmp(mimeType, L"text/html")) {
                    wcscpy(extension, L".html");
                    err = 0;
                } else if (!wcscmp(mimeType, L"application/xhtml+xml")) {
                    wcscpy(extension, L".xhtml");
                    err = 0;
                } else if (!wcscmp(mimeType, L"image/svg+xml")) {
                    wcscpy(extension, L".svg");
                    err = 0;
                }
            }
            if (!err) {
                *result = SysAllocString(extension);
                if (!*result)
                    err = ERROR_OUTOFMEMORY;
            }
            RegCloseKey(subKey);
        }
        RegCloseKey(key);
    }

    return HRESULT_FROM_WIN32(err);
}

const ResourceResponse& WebURLResponse::resourceResponse() const
{
    return m_response;
}

#if USE(CFURLCONNECTION)
CFDictionaryRef WebURLResponse::certificateDictionary() const
{
    if (m_SSLCertificateInfo)
        return m_SSLCertificateInfo.get();

    CFURLResponseRef cfResponse = m_response.cfURLResponse();
    if (!cfResponse)
        return nullptr;
    m_SSLCertificateInfo = _CFURLResponseGetSSLCertificateContext(cfResponse);
    return m_SSLCertificateInfo.get();
}
#endif
