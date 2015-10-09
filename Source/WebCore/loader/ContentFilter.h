/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef ContentFilter_h
#define ContentFilter_h

#if ENABLE(CONTENT_FILTERING)

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include <functional>
#include <wtf/Vector.h>

namespace WebCore {

class CachedRawResource;
class ContentFilterUnblockHandler;
class DocumentLoader;
class PlatformContentFilter;
class SharedBuffer;

class ContentFilter final : private CachedRawResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ContentFilter);

public:
    template <typename T> static void addType() { types().append(type<T>()); }

    static std::unique_ptr<ContentFilter> createIfEnabled(DocumentLoader&);
    ~ContentFilter() override;

    static const char* urlScheme() { return "x-apple-content-filter"; }

    void willSendRequest(ResourceRequest&, const ResourceResponse&);
    void startFilteringMainResource(CachedRawResource&);

    enum class State {
        Initialized,
        Filtering,
        Allowed,
        Blocked
    };
    State state() const { return m_state; }
    ContentFilterUnblockHandler unblockHandler() const;
    Ref<SharedBuffer> replacementData() const;
    String unblockRequestDeniedScript() const;

private:
    struct Type {
        const std::function<bool()> enabled;
        const std::function<std::unique_ptr<PlatformContentFilter>()> create;
    };
    template <typename T> static Type type();
    WEBCORE_EXPORT static Vector<Type>& types();

    using Container = Vector<std::unique_ptr<PlatformContentFilter>>;
    friend std::unique_ptr<ContentFilter> std::make_unique<ContentFilter>(Container&&, DocumentLoader&);
    ContentFilter(Container, DocumentLoader&);

    // CachedRawResourceClient
    void responseReceived(CachedResource*, const ResourceResponse&) override;
    void dataReceived(CachedResource*, const char* data, int length) override;
    void redirectReceived(CachedResource*, ResourceRequest&, const ResourceResponse&) override;

    // CachedResourceClient
    void notifyFinished(CachedResource*) override;

    void forEachContentFilterUntilBlocked(std::function<void(PlatformContentFilter&)>);
    void didDecide(State);
    void deliverResourceData(CachedResource&);

    const Container m_contentFilters;
    DocumentLoader& m_documentLoader;
    CachedResourceHandle<CachedRawResource> m_mainResource;
    PlatformContentFilter* m_blockingContentFilter { nullptr };
    State m_state { State::Initialized };
};

template <typename T>
ContentFilter::Type ContentFilter::type()
{
    static_assert(std::is_base_of<PlatformContentFilter, T>::value, "Type must be a PlatformContentFilter.");
    return { T::enabled, T::create };
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)

#endif // ContentFilter_h
