/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_FILTERING)

#include <WebCore/CachedResourceHandle.h>
#include <WebCore/LoaderMalloc.h>
#include <WebCore/PlatformContentFilter.h>
#include <WebCore/ResourceError.h>
#include <functional>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class CachedRawResource;
class ContentFilterClient;
class DocumentLoader;
class ResourceRequest;
class ResourceResponse;
class SubstituteData;

class ContentFilter : public RefCounted<ContentFilter> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ContentFilter, Loader);
    WTF_MAKE_NONCOPYABLE(ContentFilter);
public:
    template <typename T> static void addType() { types().append(type<T>()); }

    WEBCORE_EXPORT static RefPtr<ContentFilter> create(ContentFilterClient&);
    WEBCORE_EXPORT ~ContentFilter();

    static constexpr ASCIILiteral urlScheme() { return "x-apple-content-filter"_s; }

    WEBCORE_EXPORT void startFilteringMainResource(const URL&);
    void startFilteringMainResource(CachedRawResource&);
    WEBCORE_EXPORT void stopFilteringMainResource();

    WEBCORE_EXPORT bool continueAfterWillSendRequest(ResourceRequest&, const ResourceResponse&);
    WEBCORE_EXPORT void continueAfterWillSendRequest(ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&);
    WEBCORE_EXPORT bool continueAfterResponseReceived(const ResourceResponse&);
    enum class FromDocumentLoader : bool { No, Yes };
    WEBCORE_EXPORT bool continueAfterDataReceived(const SharedBuffer&, FromDocumentLoader = FromDocumentLoader::No);
    WEBCORE_EXPORT bool continueAfterNotifyFinished(const URL& resourceURL);
    bool continueAfterNotifyFinished(CachedResource&);

    static bool continueAfterSubstituteDataRequest(const DocumentLoader& activeLoader, const SubstituteData&);
    bool willHandleProvisionalLoadFailure(const ResourceError&) const;
    WEBCORE_EXPORT void handleProvisionalLoadFailure(const ResourceError&);

    const ResourceError& blockedError() const { return m_blockedError; }
    void setBlockedError(const ResourceError& error) { m_blockedError = error; }
    bool isAllowed() const { return m_state == State::Allowed; }
    bool responseReceived() const { return m_responseReceived; }

    WEBCORE_EXPORT static const URL& blockedPageURL();

#if HAVE(AUDIT_TOKEN)
    WEBCORE_EXPORT void setHostProcessAuditToken(const std::optional<audit_token_t>&);
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
    static bool isWebContentRestrictionsUnblockURL(const URL&);
#endif

private:
    using State = PlatformContentFilter::State;

    class ContentFilterCallbackAggregator : public ThreadSafeRefCounted<ContentFilterCallbackAggregator> {
    public:
        static auto create(ContentFilter& contentFilter, const ResourceRequest& request, CompletionHandler<void(ResourceRequest&&)>&& callback) { return adoptRef(*new ContentFilterCallbackAggregator(contentFilter, request, WTF::move(callback))); }

        ~ContentFilterCallbackAggregator();

        void didReceivePlatformContentFilterDecision(PlatformContentFilter&, String&&);

    private:
        ContentFilterCallbackAggregator(ContentFilter&, const ResourceRequest&, CompletionHandler<void(ResourceRequest&&)>&&);

        RefPtr<ContentFilter> m_contentFilter;
        ResourceRequest m_request;
        CompletionHandler<void(ResourceRequest&&)> m_callback;
        unsigned m_numberOfFiltersAllowed { 0 };
        bool m_isBlocked { false };
    };

    struct Type {
        Function<Ref<PlatformContentFilter>(const PlatformContentFilter::FilterParameters&)> create;
    };
    template <typename T> static Type type();
    WEBCORE_EXPORT static Vector<Type>& types();

    using Container = Vector<Ref<PlatformContentFilter>>;
    ContentFilter(Container&&, ContentFilterClient&);

    template <typename Function> void forEachContentFilterUntilBlocked(Function&&);
    void didDecide(State);
    void deliverResourceData(const SharedBuffer&);
    void deliverStoredResourceData();

    URL url();

    Container m_contentFilters;
    WeakPtr<ContentFilterClient> m_client;
    URL m_mainResourceURL;
    struct ResourceDataItem {
        RefPtr<const SharedBuffer> buffer;
    };
    Vector<ResourceDataItem> m_buffers;
    CachedResourceHandle<CachedRawResource> m_mainResource;
    ThreadSafeWeakPtr<const PlatformContentFilter> m_blockingContentFilter;
    State m_state { State::Stopped };
    ResourceError m_blockedError;
    bool m_isLoadingBlockedPage { false };
    bool m_responseReceived { false };
};

template <typename T>
ContentFilter::Type ContentFilter::type()
{
    static_assert(std::is_base_of<PlatformContentFilter, T>::value, "Type must be a PlatformContentFilter.");
    return { T::create };
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
