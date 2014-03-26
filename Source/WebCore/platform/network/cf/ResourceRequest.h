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
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
        {
        }

        ResourceRequest(const URL& url) 
            : ResourceRequestBase(url, UseProtocolCachePolicy)
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
        {
        }

        ResourceRequest(const URL& url, const String& referrer, ResourceRequestCachePolicy policy = UseProtocolCachePolicy)
            : ResourceRequestBase(url, policy)
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
        {
            setHTTPReferrer(referrer);
        }
        
        ResourceRequest()
            : ResourceRequestBase(URL(), UseProtocolCachePolicy)
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
        {
        }
        
#if USE(CFNETWORK)
#if PLATFORM(COCOA)
        ResourceRequest(NSURLRequest *);
        void updateNSURLRequest();
#endif

        ResourceRequest(CFURLRequestRef cfRequest)
            : ResourceRequestBase()
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
            , m_cfRequest(cfRequest)
        {
#if PLATFORM(COCOA)
            updateNSURLRequest();
#endif
        }
#else
        ResourceRequest(NSURLRequest *nsRequest)
            : ResourceRequestBase()
#if PLATFORM(IOS)
            , m_mainResourceRequest(false)
#endif
            , m_nsRequest(nsRequest)
        {
        }
#endif

        void updateFromDelegatePreservingOldProperties(const ResourceRequest&);

#if PLATFORM(MAC)
        void applyWebArchiveHackForMail();
#endif
#if PLATFORM(COCOA)
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

#if PLATFORM(COCOA)
        static bool useQuickLookResourceCachingQuirks();
#endif

#if PLATFORM(IOS)
        void setMainResourceRequest(bool isMainResourceRequest) const { m_mainResourceRequest = isMainResourceRequest; }
        bool isMainResourceRequest() const { return m_mainResourceRequest; }

    private:
        mutable bool m_mainResourceRequest;
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

} // namespace WebCore

#endif // ResourceRequest_h
