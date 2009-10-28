/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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
#include "ResourceRequestCFNet.h"

#include "FormDataStreamCFNet.h"
#include "ResourceRequest.h"

#include <CFNetwork/CFURLRequestPriv.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

namespace WebCore {

typedef void (*CFURLRequestSetContentDispositionEncodingFallbackArrayFunction)(CFMutableURLRequestRef, CFArrayRef);
typedef CFArrayRef (*CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction)(CFURLRequestRef);

static HMODULE findCFNetworkModule()
{
#ifndef DEBUG_ALL
    return GetModuleHandleA("CFNetwork");
#else
    return GetModuleHandleA("CFNetwork_debug");
#endif
}

static CFURLRequestSetContentDispositionEncodingFallbackArrayFunction findCFURLRequestSetContentDispositionEncodingFallbackArrayFunction()
{
    return reinterpret_cast<CFURLRequestSetContentDispositionEncodingFallbackArrayFunction>(GetProcAddress(findCFNetworkModule(), "_CFURLRequestSetContentDispositionEncodingFallbackArray"));
}

static CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction findCFURLRequestCopyContentDispositionEncodingFallbackArrayFunction()
{
    return reinterpret_cast<CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction>(GetProcAddress(findCFNetworkModule(), "_CFURLRequestCopyContentDispositionEncodingFallbackArray"));
}

static void setContentDispositionEncodingFallbackArray(CFMutableURLRequestRef request, CFArrayRef fallbackArray)
{
    static CFURLRequestSetContentDispositionEncodingFallbackArrayFunction function = findCFURLRequestSetContentDispositionEncodingFallbackArrayFunction();
    if (function)
        function(request, fallbackArray);
}

static CFArrayRef copyContentDispositionEncodingFallbackArray(CFURLRequestRef request)
{
    static CFURLRequestCopyContentDispositionEncodingFallbackArrayFunction function = findCFURLRequestCopyContentDispositionEncodingFallbackArrayFunction();
    if (!function)
        return 0;
    return function(request);
}

CFURLRequestRef ResourceRequest::cfURLRequest() const
{
    updatePlatformRequest();

    return m_cfRequest.get();
}

static inline void setHeaderFields(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    // Remove existing headers first, as some of them may no longer be present in the map.
    RetainPtr<CFDictionaryRef> oldHeaderFields(AdoptCF, CFURLRequestCopyAllHTTPHeaderFields(request));
    CFIndex oldHeaderFieldCount = CFDictionaryGetCount(oldHeaderFields.get());
    Vector<CFStringRef> oldHeaderFieldNames(oldHeaderFieldCount);
    CFDictionaryGetKeysAndValues(oldHeaderFields.get(), reinterpret_cast<const void**>(&oldHeaderFieldNames[0]), 0);
    for (CFIndex i = 0; i < oldHeaderFieldCount; ++i)
        CFURLRequestSetHTTPHeaderFieldValue(request, oldHeaderFieldNames[i], 0);

    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        CFStringRef key = it->first.createCFString();
        CFStringRef value = it->second.createCFString();
        CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}

void ResourceRequest::doUpdatePlatformRequest()
{
    CFMutableURLRequestRef cfRequest;

    RetainPtr<CFURLRef> url(AdoptCF, ResourceRequest::url().createCFURL());
    RetainPtr<CFURLRef> firstPartyForCookies(AdoptCF, ResourceRequest::firstPartyForCookies().createCFURL());
    if (m_cfRequest) {
        cfRequest = CFURLRequestCreateMutableCopy(0, m_cfRequest.get());
        CFURLRequestSetURL(cfRequest, url.get());
        CFURLRequestSetMainDocumentURL(cfRequest, firstPartyForCookies.get());
        CFURLRequestSetCachePolicy(cfRequest, (CFURLRequestCachePolicy)cachePolicy());
        CFURLRequestSetTimeoutInterval(cfRequest, timeoutInterval());
    } else {
        cfRequest = CFURLRequestCreateMutable(0, url.get(), (CFURLRequestCachePolicy)cachePolicy(), timeoutInterval(), firstPartyForCookies.get());
    }

    RetainPtr<CFStringRef> requestMethod(AdoptCF, httpMethod().createCFString());
    CFURLRequestSetHTTPRequestMethod(cfRequest, requestMethod.get());

    setHeaderFields(cfRequest, httpHeaderFields());
    WebCore::setHTTPBody(cfRequest, httpBody());
    CFURLRequestSetShouldHandleHTTPCookies(cfRequest, allowCookies());

    unsigned fallbackCount = m_responseContentDispositionEncodingFallbackArray.size();
    RetainPtr<CFMutableArrayRef> encodingFallbacks(AdoptCF, CFArrayCreateMutable(kCFAllocatorDefault, fallbackCount, 0));
    for (unsigned i = 0; i != fallbackCount; ++i) {
        RetainPtr<CFStringRef> encodingName(AdoptCF, m_responseContentDispositionEncodingFallbackArray[i].createCFString());
        CFStringEncoding encoding = CFStringConvertIANACharSetNameToEncoding(encodingName.get());
        if (encoding != kCFStringEncodingInvalidId)
            CFArrayAppendValue(encodingFallbacks.get(), reinterpret_cast<const void*>(encoding));
    }
    setContentDispositionEncodingFallbackArray(cfRequest, encodingFallbacks.get());

    if (m_cfRequest) {
        RetainPtr<CFHTTPCookieStorageRef> cookieStorage(AdoptCF, CFURLRequestCopyHTTPCookieStorage(m_cfRequest.get()));
        if (cookieStorage)
            CFURLRequestSetHTTPCookieStorage(cfRequest, cookieStorage.get());
        CFURLRequestSetHTTPCookieStorageAcceptPolicy(cfRequest, CFURLRequestGetHTTPCookieStorageAcceptPolicy(m_cfRequest.get()));
        CFURLRequestSetSSLProperties(cfRequest, CFURLRequestGetSSLProperties(m_cfRequest.get()));
    }

    m_cfRequest.adoptCF(cfRequest);
}

void ResourceRequest::doUpdateResourceRequest()
{
    m_url = CFURLRequestGetURL(m_cfRequest.get());

    m_cachePolicy = (ResourceRequestCachePolicy)CFURLRequestGetCachePolicy(m_cfRequest.get());
    m_timeoutInterval = CFURLRequestGetTimeoutInterval(m_cfRequest.get());
    m_firstPartyForCookies = CFURLRequestGetMainDocumentURL(m_cfRequest.get());
    if (CFStringRef method = CFURLRequestCopyHTTPRequestMethod(m_cfRequest.get())) {
        m_httpMethod = method;
        CFRelease(method);
    }
    m_allowCookies = CFURLRequestShouldHandleHTTPCookies(m_cfRequest.get());

    m_httpHeaderFields.clear();
    if (CFDictionaryRef headers = CFURLRequestCopyAllHTTPHeaderFields(m_cfRequest.get())) {
        CFIndex headerCount = CFDictionaryGetCount(headers);
        Vector<const void*, 128> keys(headerCount);
        Vector<const void*, 128> values(headerCount);
        CFDictionaryGetKeysAndValues(headers, keys.data(), values.data());
        for (int i = 0; i < headerCount; ++i)
            m_httpHeaderFields.set((CFStringRef)keys[i], (CFStringRef)values[i]);
        CFRelease(headers);
    }

    m_responseContentDispositionEncodingFallbackArray.clear();
    RetainPtr<CFArrayRef> encodingFallbacks(AdoptCF, copyContentDispositionEncodingFallbackArray(m_cfRequest.get()));
    if (encodingFallbacks) {
        CFIndex count = CFArrayGetCount(encodingFallbacks.get());
        for (CFIndex i = 0; i < count; ++i) {
            CFStringEncoding encoding = reinterpret_cast<CFIndex>(CFArrayGetValueAtIndex(encodingFallbacks.get(), i));
            if (encoding != kCFStringEncodingInvalidId)
                m_responseContentDispositionEncodingFallbackArray.append(CFStringConvertEncodingToIANACharSetName(encoding));
        }
    }

    m_httpBody = httpBodyFromRequest(m_cfRequest.get());
}

unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    static const unsigned preferredConnectionCount = 6;
    return wkInitializeMaximumHTTPConnectionCountPerHost(preferredConnectionCount);
}

}
