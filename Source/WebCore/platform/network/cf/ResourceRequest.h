/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#pragma once

#include "ResourceRequestBase.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSCachedURLResponse;
OBJC_CLASS NSURLRequest;

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
typedef const struct _CFURLRequest* CFURLRequestRef;
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

namespace WebCore {

class ResourceRequest : public ResourceRequestBase {
public:
    ResourceRequest(const String& url) 
        : ResourceRequestBase(URL({ }, url), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(const URL& url) 
        : ResourceRequestBase(url, ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(const URL& url, const String& referrer, ResourceRequestCachePolicy policy = ResourceRequestCachePolicy::UseProtocolCachePolicy)
        : ResourceRequestBase(url, policy)
    {
        setHTTPReferrer(referrer);
    }
    
    ResourceRequest()
        : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }
    
#if USE(CFURLCONNECTION)
    ResourceRequest(CFURLRequestRef cfRequest)
        : ResourceRequestBase()
        , m_cfRequest(cfRequest)
    {
    }
#else
    ResourceRequest(NSURLRequest *nsRequest)
        : ResourceRequestBase()
        , m_nsRequest(nsRequest)
    {
    }
#endif

    WEBCORE_EXPORT void updateFromDelegatePreservingOldProperties(const ResourceRequest&);

#if PLATFORM(COCOA)
    bool encodingRequiresPlatformData() const { return m_httpBody || m_nsRequest; }
    WEBCORE_EXPORT NSURLRequest *nsURLRequest(HTTPBodyUpdatePolicy) const;

    WEBCORE_EXPORT static CFStringRef isUserInitiatedKey();
#endif

#if PLATFORM(COCOA) || USE(CFURLCONNECTION)
    WEBCORE_EXPORT CFURLRequestRef cfURLRequest(HTTPBodyUpdatePolicy) const;
    void setStorageSession(CFURLStorageSessionRef);
#endif

    WEBCORE_EXPORT static bool httpPipeliningEnabled();
    WEBCORE_EXPORT static void setHTTPPipeliningEnabled(bool);

    static bool resourcePrioritiesEnabled();

private:
    friend class ResourceRequestBase;

    void doUpdatePlatformRequest();
    void doUpdateResourceRequest();
    void doUpdatePlatformHTTPBody();
    void doUpdateResourceHTTPBody();

    void doPlatformSetAsIsolatedCopy(const ResourceRequest&);

#if USE(CFURLCONNECTION)
    RetainPtr<CFURLRequestRef> m_cfRequest;
#endif
#if PLATFORM(COCOA)
    RetainPtr<NSURLRequest> m_nsRequest;
#endif

    static bool s_httpPipeliningEnabled;
};

inline bool ResourceRequest::resourcePrioritiesEnabled()
{
#if PLATFORM(MAC)
    return true;
#elif PLATFORM(IOS_FAMILY)
    return true;
#elif PLATFORM(WIN)
    return false;
#endif
}

#if PLATFORM(COCOA)
NSURLRequest *copyRequestWithStorageSession(CFURLStorageSessionRef, NSURLRequest *);
WEBCORE_EXPORT NSCachedURLResponse *cachedResponseForRequest(CFURLStorageSessionRef, NSURLRequest *);
#endif

} // namespace WebCore
