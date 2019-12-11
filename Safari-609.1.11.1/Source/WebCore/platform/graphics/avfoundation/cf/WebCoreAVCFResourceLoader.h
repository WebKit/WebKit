/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_LOADER_DELEGATE)

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

typedef struct OpaqueAVCFAssetResourceLoadingRequest* AVCFAssetResourceLoadingRequestRef;

namespace WebCore {

class CachedRawResource;
class CachedResourceLoader;
class MediaPlayerPrivateAVFoundationCF;

class WebCoreAVCFResourceLoader : public RefCounted<WebCoreAVCFResourceLoader>, CachedRawResourceClient {
    WTF_MAKE_NONCOPYABLE(WebCoreAVCFResourceLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WebCoreAVCFResourceLoader> create(MediaPlayerPrivateAVFoundationCF* parent, AVCFAssetResourceLoadingRequestRef);
    virtual ~WebCoreAVCFResourceLoader();

    void startLoading();
    void stopLoading();
    void invalidate();

    CachedRawResource* resource();

private:
    // CachedRawResourceClient
    void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) override;
    void dataReceived(CachedResource&, const char*, int) override;
    void notifyFinished(CachedResource&) override;

    void fulfillRequestWithResource(CachedResource&);

    WebCoreAVCFResourceLoader(MediaPlayerPrivateAVFoundationCF* parent, AVCFAssetResourceLoadingRequestRef);
    MediaPlayerPrivateAVFoundationCF* m_parent;
    RetainPtr<AVCFAssetResourceLoadingRequestRef> m_avRequest;
    CachedResourceHandle<CachedRawResource> m_resource;
};

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
