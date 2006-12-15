/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "Shared.h"
#include <wtf/RefPtr.h>
#include "ResourceHandleClient.h"

#if PLATFORM(MAC)

#include "RetainPtr.h"
#import <objc/objc.h>

#ifdef __OBJC__
@class NSCachedURLResponse;
@class NSError;
@class NSURLAuthenticationChallenge;
@class NSURLConnection;
@class NSURLRequest;
@class NSURLResponse;
#else
class NSCachedURLResponse;
class NSData;
class NSError;
class NSMutableData;
class NSObject;
class NSURL;
class NSURLAuthenticationChallenge;
class NSURLConnection;
class NSURLCredential;
class NSURLRequest;
class NSURLResponse;
#endif

#endif

namespace WebCore {

    class Frame;
    class FrameLoader;
    class ResourceHandle;
    
    class ResourceLoader : public Shared<ResourceLoader>, protected ResourceHandleClient {
    public:
        virtual ~ResourceLoader();

        void cancel();

#if PLATFORM(MAC)
        virtual bool load(NSURLRequest *);

        FrameLoader *frameLoader() const;

        virtual void cancel(NSError *);
        NSError *cancelledError();
#endif

        virtual void setDefersLoading(bool);

#if PLATFORM(MAC)
        void setIdentifier(id);
        id identifier() const { return m_identifier.get(); }

        virtual void releaseResources();
        NSURLResponse *response() const;

        virtual void addData(const char*, int, bool allAtOnce);
        virtual NSData *resourceData();
        void clearResourceData();

        virtual NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *redirectResponse);
        void didReceiveAuthenticationChallenge(NSURLAuthenticationChallenge *);
        void didCancelAuthenticationChallenge(NSURLAuthenticationChallenge *);
        virtual void didReceiveResponse(NSURLResponse *);
        virtual void didReceiveData(const char*, int, long long lengthReceived, bool allAtOnce);
        void willStopBufferingData(const char*, int);
        virtual void didFinishLoading();
        virtual void didFail(const ResourceError&);
        NSCachedURLResponse *willCacheResponse(NSCachedURLResponse *);

        void receivedCredential(NSURLAuthenticationChallenge *, NSURLCredential *);
        void receivedRequestToContinueWithoutCredential(NSURLAuthenticationChallenge *);
        void receivedCancellation(NSURLAuthenticationChallenge *);

#endif

        // ResourceHandleClient
        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& redirectResponse);
        
        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
        virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
        virtual void didFinishLoading(ResourceHandle*);
        virtual void didFail(ResourceHandle*, const ResourceError&);
        
#if PLATFORM(MAC)
        virtual void didReceiveAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { didReceiveAuthenticationChallenge(challenge); } 
        virtual void didCancelAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { didCancelAuthenticationChallenge(challenge); } 
        
        virtual void willStopBufferingData(ResourceHandle*, const char* data, int length) { willStopBufferingData(data, length); } 
        
        virtual NSCachedURLResponse *willCacheResponse(ResourceHandle*, NSCachedURLResponse *cachedResponse) { return willCacheResponse(cachedResponse); }
        
        virtual void receivedCredential(ResourceHandle*, NSURLAuthenticationChallenge *challenge, NSURLCredential *credential) { receivedCredential(challenge, credential); }
        virtual void receivedRequestToContinueWithoutCredential(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { receivedRequestToContinueWithoutCredential(challenge); } 
        virtual void receivedCancellation(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { receivedCancellation(challenge); }
#endif
        
        ResourceHandle* handle() const { return m_handle.get(); }

    protected:
        ResourceLoader(Frame*);

#if PLATFORM(MAC)
        virtual void didCancel(const ResourceError&);
        void didFinishLoadingOnePart();

        NSURLRequest *request() const { return m_request.get(); }
#endif
        bool reachedTerminalState() const { return m_reachedTerminalState; }
        bool cancelled() const { return m_cancelled; }
        bool defersLoading() const { return m_defersLoading; }

        RefPtr<ResourceHandle> m_handle;
        
    private:
#if PLATFORM(MAC)
        RetainPtr<NSURLRequest> m_request;
#endif

        bool m_reachedTerminalState;
        bool m_cancelled;
        bool m_calledDidFinishLoad;

protected:
        // FIXME: Once everything is made cross platform, these can be private instead of protected
        RefPtr<Frame> m_frame;
#if PLATFORM(MAC)
        RetainPtr<id> m_identifier;
        RetainPtr<NSURLResponse> m_response;
        NSURLAuthenticationChallenge *m_currentConnectionChallenge;
        RetainPtr<NSURLAuthenticationChallenge> m_currentWebChallenge;
        RetainPtr<NSURL> m_originalURL;
        RetainPtr<NSMutableData> m_resourceData;
#endif
        bool m_defersLoading;
    };

}

#endif
