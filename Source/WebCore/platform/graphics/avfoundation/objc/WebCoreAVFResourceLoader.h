/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAssetResourceLoadingRequest;

namespace WebCore {

class CachedResourceMediaLoader;
class DataURLResourceMediaLoader;
class MediaPlayerPrivateAVFoundationObjC;
class PlatformResourceMediaLoader;
class ResourceError;
class ResourceResponse;
class SharedBuffer;

class WebCoreAVFResourceLoader : public RefCounted<WebCoreAVFResourceLoader> {
    WTF_MAKE_NONCOPYABLE(WebCoreAVFResourceLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WebCoreAVFResourceLoader> create(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *);
    virtual ~WebCoreAVFResourceLoader();

    void startLoading();
    void stopLoading();
    void invalidate();

private:
    WebCoreAVFResourceLoader(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest *);

    friend class CachedResourceMediaLoader;
    friend class DataURLResourceMediaLoader;
    friend class PlatformResourceMediaLoader;

    void responseReceived(const ResourceResponse&);
    void loadFailed(const ResourceError&);
    void loadFinished();
    void newDataStoredInSharedBuffer(SharedBuffer&);

    MediaPlayerPrivateAVFoundationObjC* m_parent;
    RetainPtr<AVAssetResourceLoadingRequest> m_avRequest;
    std::unique_ptr<DataURLResourceMediaLoader> m_dataURLMediaLoader;
    std::unique_ptr<CachedResourceMediaLoader> m_resourceMediaLoader;
    WeakPtr<PlatformResourceMediaLoader> m_platformMediaLoader;
    size_t m_responseOffset { 0 };
};

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
