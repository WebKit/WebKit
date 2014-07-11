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

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include "ResourceRequestBase.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSURLRequest;

#if PLATFORM(COCOA) || USE(CFNETWORK)
typedef const struct _CFURLRequest* CFURLRequestRef;
typedef const struct __CFURLStorageSession* CFURLStorageSessionRef;
#endif

namespace WebCore {

    class ResourceRequest : public ResourceRequestBase {
    public:
        ResourceRequest(const String& url) 
            : ResourceRequestBase(URL(ParsedURLString, url), UseProtocolCachePolicy)
        {
        }

        ResourceRequest(const URL& url) 
            : ResourceRequestBase(url, UseProtocolCachePolicy)
        {
        }

        ResourceRequest(const URL& url, const String& referrer, ResourceRequestCachePolicy policy = UseProtocolCachePolicy)
            : ResourceRequestBase(url, policy)
        {
            setHTTPReferrer(referrer);
        }
        
        ResourceRequest()
            : ResourceRequestBase(URL(), UseProtocolCachePolicy)
        {
        }
        
#if USE(CFNETWORK)
#if PLATFORM(COCOA)
        ResourceRequest(NSURLRequest *);
        void updateNSURLRequest();
        void clearOrUpdateNSURLRequest();
#endif

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

        void updateFromDelegatePreservingOldProperties(const ResourceRequest&);

#if PLATFORM(MAC)
        void applyWebArchiveHackForMail();
#endif
#if PLATFORM(COCOA)
#if USE(CFNETWORK)
        bool encodingRequiresPlatformData() const { return m_httpBody || m_cfRequest; }
#else
        bool encodingRequiresPlatformData() const { return m_httpBody || m_nsRequest; }
#endif
        NSURLRequest *nsURLRequest(HTTPBodyUpdatePolicy) const;
#endif

#if ENABLE(CACHE_PARTITIONING)
        static String partitionName(const String& domain);
        const String& cachePartition() const { return m_cachePartition.isNull() ? emptyString() : m_cachePartition; }
        void setCachePartition(const String& cachePartition) { m_cachePartition = partitionName(cachePartition); }
#endif

#if PLATFORM(COCOA) || USE(CFNETWORK)
        CFURLRequestRef cfURLRequest(HTTPBodyUpdatePolicy) const;
        void setStorageSession(CFURLStorageSessionRef);
#endif

        static bool httpPipeliningEnabled();
        static void setHTTPPipeliningEnabled(bool);

        static bool resourcePrioritiesEnabled();

#if PLATFORM(COCOA)
        static bool useQuickLookResourceCachingQuirks();
#endif

#if PLATFORM(IOS)
        // FIXME: deprecatedIsMainResourceRequest() does not return the correct value if the ResourceRequest has been
        // deserialized from an IPC message. As a result this function can only be relied on when networking is not in a
        // separate process.
        void deprecatedSetMainResourceRequest(bool isMainResourceRequest) const { m_mainResourceRequest = isMainResourceRequest; }
        bool deprecatedIsMainResourceRequest() const { return m_mainResourceRequest; }

    private:
        mutable bool m_mainResourceRequest = false;
#endif

    private:
        friend class ResourceRequestBase;

        void doUpdatePlatformRequest();
        void doUpdateResourceRequest();
        void doUpdatePlatformHTTPBody();
        void doUpdateResourceHTTPBody();

        PassOwnPtr<CrossThreadResourceRequestData> doPlatformCopyData(PassOwnPtr<CrossThreadResourceRequestData>) const;
        void doPlatformAdopt(PassOwnPtr<CrossThreadResourceRequestData>);

#if USE(CFNETWORK)
        RetainPtr<CFURLRequestRef> m_cfRequest;
#endif
#if PLATFORM(COCOA)
        RetainPtr<NSURLRequest> m_nsRequest;
#endif
#if ENABLE(CACHE_PARTITIONING)
        String m_cachePartition;
#endif

        static bool s_httpPipeliningEnabled;
    };

    struct CrossThreadResourceRequestData : public CrossThreadResourceRequestDataBase {
#if ENABLE(CACHE_PARTITIONING)
        String m_cachePartition;
#endif
    };

    inline bool ResourceRequest::resourcePrioritiesEnabled()
    {
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 10100
    return true;
#elif PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 10100
    // See <rdar://problem/16518595>, <rdar://problem/17168793> for issues we had before OS X 10.10.
    // HTTP Pipelining could be enabled for experiments, but there is no point in doing so on old OS versions,
    // and that can't work well because of the above issues.
    ASSERT(!httpPipeliningEnabled());
    return false;
#elif PLATFORM(IOS)
    return true;
#elif PLATFORM(WIN)
    return false;
#endif
    }


} // namespace WebCore

#endif // ResourceRequest_h
