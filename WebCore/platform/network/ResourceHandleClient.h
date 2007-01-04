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

#ifndef ResourceHandleClient_h
#define ResourceHandleClient_h

#include "Shared.h"
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>

#if USE(CFNETWORK)
#include <ConditionalMacros.h>
#include <CFNetwork/CFURLResponsePriv.h>
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSCachedURLResponse;
@class NSData;
@class NSURLAuthenticationChallenge;
@class NSURLCredential;
#else
class NSCachedURLResponse;
class NSData;
class NSURLAuthenticationChallenge;
class NSURLCredential;
#endif
#endif

#if PLATFORM(QT)
#include <QString>
#endif

namespace WebCore {

#if USE(CFNETWORK)
    typedef void* PlatformData; // unused for now
#elif PLATFORM(MAC)
    typedef NSData* PlatformData;
#elif PLATFORM(QT)
    class PlatformResponseQt : public Shared<PlatformResponseQt> {
    public:
        QString data;
        QString url;
    };

    typedef void* PlatformData;
    typedef RefPtr<PlatformResponseQt> PlatformResponse;
#else
    // Not sure what the strategy for this will be on other platforms.
    typedef struct PlatformDataStruct* PlatformData;
#endif

    class KURL;
    class ResourceHandle;
    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;

    class ResourceHandleClient {
    public:
        virtual ~ResourceHandleClient() { }

        // request may be modified
        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& redirectResponse) { }
    
        // void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&) { }
        // void didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&) { }

        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&) { }
        virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived) { }
        virtual void didFinishLoading(ResourceHandle*) { }
        virtual void didFail(ResourceHandle*, const ResourceError&) { }

        // cached response may be modified
        // void willCacheResponse(ResourceHandle*, CachedResourceResponse&) { }

#if PLATFORM(MAC)
        virtual void didReceiveAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *) { } 
        virtual void didCancelAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *) { } 
        
        virtual void willStopBufferingData(ResourceHandle*, const char*, int) { } 
        
        virtual NSCachedURLResponse *willCacheResponse(ResourceHandle*, NSCachedURLResponse *cachedResponse) { return cachedResponse; }

        virtual void receivedCredential(ResourceHandle*, NSURLAuthenticationChallenge *, NSURLCredential *) { } 
        virtual void receivedRequestToContinueWithoutCredential(ResourceHandle*, NSURLAuthenticationChallenge *) { } 
        virtual void receivedCancellation(ResourceHandle*, NSURLAuthenticationChallenge *) { } 
#endif
    };

}

#endif
