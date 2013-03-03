/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef MainResourceLoader_h
#define MainResourceLoader_h

#include "CachedRawResource.h"
#include "CachedResourceHandle.h"
#include "FrameLoaderTypes.h"
#include "ResourceLoader.h"
#include "SubstituteData.h"
#include <wtf/Forward.h>

#if HAVE(RUNLOOP_TIMER)
#include "RunLoopTimer.h"
#else
#include "Timer.h"
#endif

namespace WebCore {

class FormState;
class ResourceRequest;
    
#if USE(CONTENT_FILTERING)
class ContentFilter;
#endif

class MainResourceLoader : public RefCounted<MainResourceLoader>, public CachedRawResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<MainResourceLoader> create(DocumentLoader*);
    virtual ~MainResourceLoader();

    void load(const ResourceRequest&, const SubstituteData&);
    void cancel();
    void cancel(const ResourceError&);
    ResourceLoader* loader() const;
    PassRefPtr<ResourceBuffer> resourceData();

    void setDefersLoading(bool);
    void setDataBufferingPolicy(DataBufferingPolicy);

#if HAVE(RUNLOOP_TIMER)
    typedef RunLoopTimer<MainResourceLoader> MainResourceLoaderTimer;
#else
    typedef Timer<MainResourceLoader> MainResourceLoaderTimer;
#endif

    unsigned long identifier() const;
    bool isLoadingMultipartContent() const { return m_loadingMultipartContent; }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    explicit MainResourceLoader(DocumentLoader*);

    virtual void redirectReceived(CachedResource*, ResourceRequest&, const ResourceResponse&) OVERRIDE;
    virtual void responseReceived(CachedResource*, const ResourceResponse&) OVERRIDE;
    virtual void dataReceived(CachedResource*, const char* data, int dataLength) OVERRIDE;
    virtual void notifyFinished(CachedResource*) OVERRIDE;

    void willSendRequest(ResourceRequest&, const ResourceResponse& redirectResponse);
    void didFinishLoading(double finishTime);
    void handleSubstituteDataLoadSoon(const ResourceRequest&);
    void handleSubstituteDataLoadNow(MainResourceLoaderTimer*);

    void startDataLoadTimer();

    void receivedError(const ResourceError&);
    ResourceError interruptedForPolicyChangeError() const;
    void stopLoadingForPolicyChange();
    bool isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse);

    static void callContinueAfterNavigationPolicy(void*, const ResourceRequest&, PassRefPtr<FormState>, bool shouldContinue);
    void continueAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue);

    static void callContinueAfterContentPolicy(void*, PolicyAction);
    void continueAfterContentPolicy(PolicyAction);
    void continueAfterContentPolicy(PolicyAction, const ResourceResponse&);
    
#if PLATFORM(QT)
    void substituteMIMETypeFromPluginDatabase(const ResourceResponse&);
#endif

    FrameLoader* frameLoader() const;
    DocumentLoader* documentLoader() const { return m_documentLoader.get(); }

    const ResourceRequest& request() const;
    void clearResource();

    bool defersLoading() const;

    CachedResourceHandle<CachedRawResource> m_resource;

    ResourceRequest m_initialRequest;
    SubstituteData m_substituteData;
    ResourceResponse m_response;

    MainResourceLoaderTimer m_dataLoadTimer;
    RefPtr<DocumentLoader> m_documentLoader;

    bool m_loadingMultipartContent;
    bool m_waitingForContentPolicy;
    double m_timeOfLastDataReceived;
    unsigned long m_identifierForLoadWithoutResourceLoader;

#if USE(CONTENT_FILTERING)
    RefPtr<ContentFilter> m_contentFilter;
#endif
};

}

#endif
