/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContentSecurityPolicy.h"
#include "CrossOriginPreflightChecker.h"
#include "LoaderMalloc.h"
#include "ResourceLoaderIdentifier.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "ThreadableLoader.h"
#include <wtf/WeakPtr.h>

namespace WebCore {
    class CachedRawResource;
    class ContentSecurityPolicy;
    class Document;
    class ThreadableLoaderClient;
    class WeakPtrImplWithEventTargetData;

    class DocumentThreadableLoader : public RefCounted<DocumentThreadableLoader>, public ThreadableLoader, public CachedRawResourceClient {
        WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Loader);
    public:
        static void loadResourceSynchronously(Document&, ResourceRequest&&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&, std::optional<CrossOriginEmbedderPolicy>&&);
        static void loadResourceSynchronously(Document&, ResourceRequest&&, ThreadableLoaderClient&, const ThreadableLoaderOptions&);

        enum class ShouldLogError : bool { No, Yes };
        static RefPtr<DocumentThreadableLoader> create(Document&, ThreadableLoaderClient&, ResourceRequest&&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&, std::optional<CrossOriginEmbedderPolicy>&&, String&& referrer, ShouldLogError);
        static RefPtr<DocumentThreadableLoader> create(Document&, ThreadableLoaderClient&, ResourceRequest&&, const ThreadableLoaderOptions&, String&& referrer = String());

        virtual ~DocumentThreadableLoader();

        void cancel() override;
        virtual void setDefersLoading(bool);
        void computeIsDone() final;
        void clearClient() { m_client = nullptr; }

        friend CrossOriginPreflightChecker;
        friend class InspectorInstrumentation;
        friend class InspectorNetworkAgent;

        using RefCounted<DocumentThreadableLoader>::ref;
        using RefCounted<DocumentThreadableLoader>::deref;

    protected:
        void refThreadableLoader() override { ref(); }
        void derefThreadableLoader() override { deref(); }

    private:
        enum BlockingBehavior {
            LoadSynchronously,
            LoadAsynchronously
        };

        DocumentThreadableLoader(Document&, ThreadableLoaderClient&, BlockingBehavior, ResourceRequest&&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&, std::optional<CrossOriginEmbedderPolicy>&&, String&&, ShouldLogError);

        void clearResource();

        // CachedRawResourceClient
        void dataSent(CachedResource&, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
        void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) override;
        void dataReceived(CachedResource&, const SharedBuffer&) override;
        void redirectReceived(CachedResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&) override;
        void finishedTimingForWorkerLoad(CachedResource&, const ResourceTiming&) override;
        void finishedTimingForWorkerLoad(const ResourceTiming&);
        void notifyFinished(CachedResource&, const NetworkLoadMetrics&) override;

        void didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse&);
        void didReceiveData(ResourceLoaderIdentifier, const SharedBuffer&);
        void didFinishLoading(ResourceLoaderIdentifier, const NetworkLoadMetrics&);
        void didFail(ResourceLoaderIdentifier, const ResourceError&);
        void makeCrossOriginAccessRequest(ResourceRequest&&);
        void makeSimpleCrossOriginAccessRequest(ResourceRequest&&);
        void makeCrossOriginAccessRequestWithPreflight(ResourceRequest&&);
        void preflightSuccess(ResourceRequest&&);
        void preflightFailure(ResourceLoaderIdentifier, const ResourceError&);

        void loadRequest(ResourceRequest&&, SecurityCheckPolicy);
        bool isAllowedRedirect(const URL&);
        bool isAllowedByContentSecurityPolicy(const URL&, ContentSecurityPolicy::RedirectResponseReceived, const URL& preRedirectURL = URL());
        bool isResponseAllowedByContentSecurityPolicy(const ResourceResponse&);

        SecurityOrigin& securityOrigin() const;
        Ref<SecurityOrigin> protectedSecurityOrigin() const;
        const ContentSecurityPolicy& contentSecurityPolicy() const;
        CheckedRef<const ContentSecurityPolicy> checkedContentSecurityPolicy() const;
        const CrossOriginEmbedderPolicy& crossOriginEmbedderPolicy() const;

        Document& document() { return *m_document; }
        Ref<Document> protectedDocument();

        const ThreadableLoaderOptions& options() const { return m_options; }
        const String& referrer() const { return m_referrer; }
        bool isLoading() { return m_resource || m_preflightChecker; }

        void reportRedirectionWithBadScheme(const URL&);
        void reportContentSecurityPolicyError(const URL&);
        void reportCrossOriginResourceSharingError(const URL&);
        void reportIntegrityMetadataError(const CachedResource&, const String& expectedMetadata);
        void logErrorAndFail(const ResourceError&);

        bool shouldSetHTTPHeadersToKeep() const;
        bool checkURLSchemeAsCORSEnabled(const URL&);

        CachedResourceHandle<CachedRawResource> protectedResource() const;

        CachedResourceHandle<CachedRawResource> m_resource;
        ThreadableLoaderClient* m_client; // FIXME: Use a smart pointer.
        WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
        ThreadableLoaderOptions m_options;
        bool m_responsesCanBeOpaque { true };
        RefPtr<SecurityOrigin> m_origin;
        String m_referrer;
        bool m_sameOriginRequest;
        bool m_simpleRequest;
        bool m_async;
        bool m_delayCallbacksForIntegrityCheck;
        std::unique_ptr<ContentSecurityPolicy> m_contentSecurityPolicy;
        std::optional<CrossOriginEmbedderPolicy> m_crossOriginEmbedderPolicy;
        std::optional<CrossOriginPreflightChecker> m_preflightChecker;
        std::optional<HTTPHeaderMap> m_originalHeaders;
        URL m_responseURL;

        ShouldLogError m_shouldLogError;
        std::optional<ResourceRequest> m_bypassingPreflightForServiceWorkerRequest;
    };

} // namespace WebCore
