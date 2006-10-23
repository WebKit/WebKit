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

#ifndef ResourceLoaderClient_h
#define ResourceLoaderClient_h

#include <wtf/Platform.h>
#if USE(CFNETWORK)
#include <ConditionalMacros.h>
#include <CFNetwork/CFURLResponsePriv.h>
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSData;
@class NSURLResponse;
#else
class NSData;
class NSURLResponse;
#endif
#endif

#if PLATFORM(QT)
#include <QString>
#include <wtf/RefPtr.h>
#endif

#include "Shared.h"

namespace WebCore {

#if USE(CFNETWORK)
    typedef void* PlatformData; // unused for now
    typedef CFURLResponseRef PlatformResponse;
#elif PLATFORM(MAC)
    typedef NSData* PlatformData;
    typedef NSURLResponse* PlatformResponse;
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
    typedef struct PlatformResponseStruct* PlatformResponse;
#endif

    class KURL;
    class ResourceLoader;

    class ResourceLoaderClient {
    public:
        virtual ~ResourceLoaderClient() { }

        // request may be modified
        // void willSendRequest(ResourceLoader*, Request&, const ResourceResonse& redirectResponse) { }
    
        // void didReceiveAuthenticationChallenge(ResourceLoader*, const AuthenticationChallenge&) { }
        // void didCancelAuthenticationChallenge(ResourceLoader*, const AuthenticationChallenge&) { }

        // void didReceiveResponse(ResourceLoader*, const ResourceResponse&) { }
        virtual void didReceiveData(ResourceLoader*, const char*, int) { }
        virtual void didFinishLoading(ResourceLoader*) { }
        // void didFailWithError(ResourceError*) { }

        // cached response may be modified
        // void willCacheResponse(ResourceLoader*, CachedResourceResponse&) { }

        // old-style methods
        virtual void receivedRedirect(ResourceLoader*, const KURL&) { }
        virtual void receivedResponse(ResourceLoader*, PlatformResponse) { }
        virtual void receivedAllData(ResourceLoader*, PlatformData) { }
    };

}

#endif
