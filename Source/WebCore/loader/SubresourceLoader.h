/*
 * Copyright (C) 2005, 2006, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef SubresourceLoader_h
#define SubresourceLoader_h

#include "FrameLoaderTypes.h"
#include "ResourceLoader.h"

#include <wtf/text/WTFString.h>
 
namespace WebCore {

class CachedResource;
class CachedResourceLoader;
class Document;
class PageActivityAssertionToken;
class ResourceRequest;

class SubresourceLoader final : public ResourceLoader {
public:
    static PassRefPtr<SubresourceLoader> create(Frame*, CachedResource*, const ResourceRequest&, const ResourceLoaderOptions&);

    virtual ~SubresourceLoader();

    void cancelIfNotFinishing();
    virtual bool isSubresourceLoader() override;
    CachedResource* cachedResource();

#if PLATFORM(IOS)
    virtual bool startLoading() override;

    // FIXME: What is an "iOS" original request? Why is it necessary?
    virtual const ResourceRequest& iOSOriginalRequest() const override { return m_iOSOriginalRequest; }
#endif

private:
    SubresourceLoader(Frame*, CachedResource*, const ResourceLoaderOptions&);

    virtual bool init(const ResourceRequest&) override;

    virtual void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse) override;
    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    virtual void didReceiveResponse(const ResourceResponse&) override;
    virtual void didReceiveData(const char*, unsigned, long long encodedDataLength, DataPayloadType) override;
    virtual void didReceiveBuffer(PassRefPtr<SharedBuffer>, long long encodedDataLength, DataPayloadType) override;
    virtual void didFinishLoading(double finishTime) override;
    virtual void didFail(const ResourceError&) override;
    virtual void willCancel(const ResourceError&) override;
    virtual void didCancel(const ResourceError&) override;

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    virtual bool supportsDataArray() override { return true; }
    virtual void didReceiveDataArray(CFArrayRef) override;
#endif
    virtual void releaseResources() override;

#if USE(SOUP)
    virtual char* getOrCreateReadBuffer(size_t requestedSize, size_t& actualSize) override;
#endif

    bool checkForHTTPStatusCodeError();

    void didReceiveDataOrBuffer(const char*, int, PassRefPtr<SharedBuffer>, long long encodedDataLength, DataPayloadType);

    void notifyDone();

    enum SubresourceLoaderState {
        Uninitialized,
        Initialized,
        Finishing,
#if PLATFORM(IOS)
        CancelledWhileInitializing
#endif
    };

    class RequestCountTracker {
    public:
        RequestCountTracker(CachedResourceLoader*, CachedResource*);
        ~RequestCountTracker();
    private:
        CachedResourceLoader* m_cachedResourceLoader;
        CachedResource* m_resource;
    };

#if PLATFORM(IOS)
    ResourceRequest m_iOSOriginalRequest;
#endif
    CachedResource* m_resource;
    bool m_loadingMultipartContent;
    SubresourceLoaderState m_state;
    OwnPtr<RequestCountTracker> m_requestCountTracker;
};

}

#endif // SubresourceLoader_h
