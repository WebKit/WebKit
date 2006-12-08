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

#ifndef SubresourceLoader_H_
#define SubresourceLoader_H_
 
#include "ResourceHandleClient.h"
#include "ResourceLoader.h"
#include <wtf/PassRefPtr.h>
 
#ifndef __OBJC__
class NSArray;
class NSDictionary;
class NSMutableURLRequest;
#endif
 
namespace WebCore {

    class FormData;
    class String;
    class ResourceHandle;
    class ResourceRequest;
    class SubresourceLoaderClient;
    
    class SubresourceLoader : public ResourceLoader, ResourceHandleClient {
    public:
        static PassRefPtr<SubresourceLoader> create(Frame*, SubresourceLoaderClient*, const ResourceRequest&);
        
        virtual ~SubresourceLoader();

        void stopLoading();
        
#if PLATFORM(MAC)
        // FIXME: These should go away once ResourceLoader uses ResourceHandle.
        virtual bool load(NSURLRequest *);
        virtual NSData *resourceData();
        virtual void setDefersLoading(bool);             
        
        virtual NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *redirectResponse);
        virtual void didReceiveResponse(NSURLResponse *);
        virtual void didReceiveData(NSData *, long long lengthReceived, bool allAtOnce);
        virtual void didFinishLoading();
        virtual void didFail(NSError *);
        
        // FIXME: This function is here because we want ResourceHandleClient to be privately inherited, but 
        // ResourceHandle needs to be passed a ResourceHandleClient.
        ResourceHandleClient* loaderAsResourceHandleClient() { return this; }
#endif

        // ResourceHandleClient
        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& redirectResponse);
        
        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
        virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
        virtual void didFinishLoading(ResourceHandle*);
        virtual void didFail(ResourceHandle*, const ResourceError&);

#if PLATFORM(MAC)
        virtual void didReceiveAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { ResourceLoader::didReceiveAuthenticationChallenge(challenge); } 
        virtual void didCancelAuthenticationChallenge(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { ResourceLoader::didCancelAuthenticationChallenge(challenge); } 
        
        virtual void willStopBufferingData(ResourceHandle*, NSData *data) { ResourceLoader::willStopBufferingData(data); } 
        
        virtual NSCachedURLResponse *willCacheResponse(ResourceHandle*, NSCachedURLResponse *cachedResponse) { return ResourceLoader::willCacheResponse(cachedResponse); }
        
        virtual void receivedCredential(ResourceHandle*, NSURLAuthenticationChallenge *challenge, NSURLCredential *credential) { ResourceLoader::receivedCredential(challenge, credential); }
        virtual void receivedRequestToContinueWithoutCredential(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { ResourceLoader::receivedRequestToContinueWithoutCredential(challenge); } 
        virtual void receivedCancellation(ResourceHandle*, NSURLAuthenticationChallenge *challenge) { ResourceLoader::receivedCancellation(challenge); }
#endif
        
        ResourceHandle* handle() const { return m_handle.get(); }
    private:
        SubresourceLoader(Frame*, SubresourceLoaderClient*);

#if PLATFORM(MAC)
        virtual void didCancel(NSError *);
#endif
        SubresourceLoaderClient* m_client;
        RefPtr<ResourceHandle> m_handle;
        bool m_loadingMultipartContent;
    };

}

#endif // SubresourceLoader_H_
