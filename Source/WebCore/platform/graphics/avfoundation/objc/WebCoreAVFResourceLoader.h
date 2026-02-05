/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVAssetResourceLoadingRequest;

namespace WebCore {

class CachedResourceMediaLoader;
class DataURLResourceMediaLoader;
class FragmentedSharedBuffer;
class MediaPlayerPrivateAVFoundationObjC;
class ParsedContentRange;
class PlatformMediaResourceLoader;
class PlatformResourceMediaLoader;
class ResourceError;
class ResourceResponse;

class WebCoreAVFResourceLoader : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebCoreAVFResourceLoader> {
    WTF_MAKE_TZONE_ALLOCATED(WebCoreAVFResourceLoader);
    WTF_MAKE_NONCOPYABLE(WebCoreAVFResourceLoader);
public:
    static Ref<WebCoreAVFResourceLoader> create(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest*, Ref<PlatformMediaResourceLoader>&&, GuaranteedSerialFunctionDispatcher&);
    virtual ~WebCoreAVFResourceLoader();

    void startLoading();
    void stopLoading();

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t logIdentifier) { m_logIdentifier = logIdentifier; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
#endif

private:
    WebCoreAVFResourceLoader(MediaPlayerPrivateAVFoundationObjC* parent, AVAssetResourceLoadingRequest*, Ref<PlatformMediaResourceLoader>&&, GuaranteedSerialFunctionDispatcher&);

    friend class CachedResourceMediaLoader;
    friend class DataURLResourceMediaLoader;
    friend class PlatformResourceMediaLoader;

    // Return true if stopLoading() got called, indicating that no further processing should occur.
    bool responseReceived(const String&, int, const ParsedContentRange&, size_t);
    bool newDataStoredInSharedBuffer(const FragmentedSharedBuffer&);

    void startLoadingImpl();
    void loadFailed(const ResourceError&);
    void loadFinished();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger.get(); }
    Ref<const Logger> protectedLogger() const { return logger(); }
    ASCIILiteral logClassName() const { return "WebCoreAVFResourceLoader"_s; }
    WTFLogChannel& logChannel() const;
#endif

    ThreadSafeWeakPtr<MediaPlayerPrivateAVFoundationObjC> m_parent;
    RetainPtr<AVAssetResourceLoadingRequest> m_avRequest;
    RefPtr<DataURLResourceMediaLoader> m_dataURLMediaLoader;
    RefPtr<PlatformResourceMediaLoader> m_resourceMediaLoader;
    const Ref<PlatformMediaResourceLoader> m_platformMediaLoader;
    bool m_isBlob { false };
    size_t m_responseOffset { 0 };
    int64_t m_requestedLength { 0 };
    int64_t m_requestedOffset { 0 };
    int64_t m_currentOffset { 0 };

    const Ref<GuaranteedSerialFunctionDispatcher> m_targetDispatcher;
    std::optional<MonotonicTime> m_loadStartTime;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier;
#endif
};

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
