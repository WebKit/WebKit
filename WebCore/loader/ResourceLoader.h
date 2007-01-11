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

#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ResourceLoader.h"
#include "Shared.h"
#include "AuthenticationChallenge.h"
#include "KURL.h"

#include <wtf/Forward.h>

#if PLATFORM(MAC)

#include "RetainPtr.h"
#import <objc/objc.h>

#ifdef __OBJC__
@class NSCachedURLResponse;
@class NSURLAuthenticationChallenge;
#else
class NSCachedURLResponse;
class NSURLAuthenticationChallenge;
class NSURLCredential;
#endif

#else
// FIXME: Get rid of this once we don't use id in the loader
typedef void* id;
#endif

namespace WebCore {

    class Frame;
    class FrameLoader;
    class ResourceHandle;
    class SharedBuffer;
    
    class ResourceLoader : public Shared<ResourceLoader>, protected ResourceHandleClient {
    public:
        virtual ~ResourceLoader();

        void cancel();

        virtual bool load(const ResourceRequest&);

        FrameLoader *frameLoader() const;

        virtual void cancel(const ResourceError&);
        ResourceError cancelledError();

        virtual void setDefersLoading(bool);

#if PLATFORM(MAC)
        void setIdentifier(id);
        id identifier() const { return m_identifier.get(); }
#else
        void setIdentifier(id) { }
        id identifier() const { return 0; }
#endif

        virtual void releaseResources();
        const ResourceResponse& response() const;

        virtual void addData(const char*, int, bool allAtOnce);
        virtual PassRefPtr<SharedBuffer> resourceData();
        void clearResourceData();
        
        virtual void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse);
        virtual void didReceiveResponse(const ResourceResponse&);
        virtual void didReceiveData(const char*, int, long long lengthReceived, bool allAtOnce);
        void willStopBufferingData(const char*, int);
        virtual void didFinishLoading();
        virtual void didFail(const ResourceError&);
#if PLATFORM(MAC)
        NSCachedURLResponse *willCacheResponse(NSCachedURLResponse *);
#endif

        // ResourceHandleClient
        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& redirectResponse);        
        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
        virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
        virtual void didFinishLoading(ResourceHandle*);
        virtual void didFail(ResourceHandle*, const ResourceError&);
        virtual void willStopBufferingData(ResourceHandle*, const char* data, int length) { willStopBufferingData(data, length); } 

#if PLATFORM(MAC)
        virtual NSCachedURLResponse *willCacheResponse(ResourceHandle*, NSCachedURLResponse *cachedResponse) { return willCacheResponse(cachedResponse); }
        
        void didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge);
        void didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge);
        void receivedCredential(const AuthenticationChallenge&, const Credential&);
        void receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&);
        void receivedCancellation(const AuthenticationChallenge&);
        
        virtual void didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge) { didReceiveAuthenticationChallenge(challenge); } 
        virtual void didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge) { didCancelAuthenticationChallenge(challenge); } 
        virtual void receivedCredential(ResourceHandle*, const AuthenticationChallenge& challenge, const Credential& credential) { receivedCredential(challenge, credential); }
        virtual void receivedRequestToContinueWithoutCredential(ResourceHandle*, const AuthenticationChallenge& challenge) { receivedRequestToContinueWithoutCredential(challenge); } 
        virtual void receivedCancellation(ResourceHandle*, const AuthenticationChallenge& challenge) { receivedCancellation(challenge); }
#endif
        
        ResourceHandle* handle() const { return m_handle.get(); }

    protected:
        ResourceLoader(Frame*);

        virtual void didCancel(const ResourceError&);
        void didFinishLoadingOnePart();

        const ResourceRequest& request() const { return m_request; }
        void setRequest(const ResourceRequest& request) { m_request = request; }
        bool reachedTerminalState() const { return m_reachedTerminalState; }
        bool cancelled() const { return m_cancelled; }
        bool defersLoading() const { return m_defersLoading; }

        RefPtr<ResourceHandle> m_handle;

    private:
        ResourceRequest m_request;

        bool m_reachedTerminalState;
        bool m_cancelled;
        bool m_calledDidFinishLoad;

protected:
        // FIXME: Once everything is made cross platform, these can be private instead of protected
        RefPtr<Frame> m_frame;
        ResourceResponse m_response;
#if PLATFORM(MAC)
        RetainPtr<id> m_identifier;
        NSURLAuthenticationChallenge *m_currentMacChallenge;
#endif
        AuthenticationChallenge m_currentWebChallenge;

        KURL m_originalURL;
        RefPtr<SharedBuffer> m_resourceData;
        bool m_defersLoading;
    };

}

#endif
