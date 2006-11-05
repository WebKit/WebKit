// -*- mode: c++; c-basic-offset: 4 -*-
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
#include "ResourceRequestCFNet.h"

#include "FormDataStreamCFNet.h"
#include "ResourceRequest.h"

#include <CFNetwork/CFURLRequestPriv.h>

namespace WebCore {
    
    void getResourceRequest(ResourceRequest& request, CFURLRequestRef cfRequest)
    {
        request = ResourceRequest(CFURLRequestGetURL(cfRequest));

        request.setCachePolicy((ResourceRequestCachePolicy)CFURLRequestGetCachePolicy(cfRequest));
        request.setTimeoutInterval(CFURLRequestGetTimeoutInterval(cfRequest));
        request.setMainDocumentURL(KURL(CFURLRequestGetMainDocumentURL(cfRequest)));
        if (CFStringRef method = CFURLRequestCopyHTTPRequestMethod(cfRequest)) {
            request.setHTTPMethod(method);
            CFRelease(method);
        }
        request.setAllowHTTPCookies(CFURLRequestShouldHandleHTTPCookies(cfRequest));

        if (CFDictionaryRef headers = CFURLRequestCopyAllHTTPHeaderFields(cfRequest)) {
            CFIndex headerCount = CFDictionaryGetCount(headers);
            Vector<const void*, 128> keys(headerCount);
            Vector<const void*, 128> values(headerCount);
            CFDictionaryGetKeysAndValues(headers, keys.data(), values.data());
            for (int i = 0; i < headerCount; ++i)
                request.setHTTPHeaderField((CFStringRef)keys[i], (CFStringRef)values[i]);
            CFRelease(headers);
        }
       

        if (CFDataRef bodyData = CFURLRequestCopyHTTPRequestBody(cfRequest)) {
            request.setHTTPBody(FormData(CFDataGetBytePtr(bodyData), CFDataGetLength(bodyData)));
            CFRelease(bodyData);
        } else if (CFReadStreamRef bodyStream = CFURLRequestCopyHTTPRequestBodyStream(cfRequest)) {
            if (const FormData* formData = httpBodyFromStream(bodyStream))
                request.setHTTPBody(*formData);
            CFRelease(bodyStream);
        }
        // FIXME: what to do about arbitrary body streams?
    }

    static void addHeadersFromHashMap(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
    {
        if (!requestHeaders.size())
            return;
        
        HTTPHeaderMap::const_iterator end = requestHeaders.end();
        for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
            CFStringRef key = it->first.createCFString();
            CFStringRef value = it->second.createCFString();
            CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
            CFRelease(key);
            CFRelease(value);
        }
    }

    CFURLRequestRef cfURLRequest(const ResourceRequest& request)
    {
        CFURLRef url = request.url().createCFURL();
        CFURLRef mainDocumentURL = request.mainDocumentURL().createCFURL();

        CFMutableURLRequestRef cfRequest = CFURLRequestCreateMutable(0, url, (CFURLRequestCachePolicy)request.cachePolicy(), request.timeoutInterval(), mainDocumentURL);

        CFRelease(url);
        CFRelease(mainDocumentURL);

        CFStringRef requestMethod = request.httpMethod().createCFString();
        CFURLRequestSetHTTPRequestMethod(cfRequest, requestMethod);
        CFRelease(requestMethod);

        addHeadersFromHashMap(cfRequest, request.httpHeaderFields());
        setHTTPBody(cfRequest, request.httpBody());
        CFURLRequestSetShouldHandleHTTPCookies(cfRequest, request.allowHTTPCookies());

        return cfRequest;
    }

}
