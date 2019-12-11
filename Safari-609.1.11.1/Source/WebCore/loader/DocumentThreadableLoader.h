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
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "ThreadableLoader.h"

namespace WebCore {
    class CachedRawResource;
    class ContentSecurityPolicy;
    class Document;
    class ThreadableLoaderClient;

    class DocumentThreadableLoader : public RefCounted<DocumentThreadableLoader>, public ThreadableLoader, private CachedRawResourceClient  {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static void loadResourceSynchronously(Document&, ResourceRequest&&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&);
        static void loadResourceSynchronously(Document&, ResourceRequest&&, ThreadableLoaderClient&, const ThreadableLoaderOptions&);

        enum class ShouldLogError { No, Yes };
        static RefPtr<DocumentThreadableLoader> create(Document&, ThreadableLoaderClient&, ResourceRequest&&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&, String&& referrer, ShouldLogError);
        static RefPtr<DocumentThreadableLoader> create(Document&, ThreadableLoaderClient&, ResourceRequest&&, const ThreadableLoaderOptions&, String&& referrer = String());

        virtual ~DocumentThreadableLoader();

        void cancel() override;
        virtual void setDefersLoading(bool);

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

        DocumentThreadableLoader(Document&, ThreadableLoaderClient&, BlockingBehavior, ResourceRequest&&, const ThreadableLoaderOptions&, RefPtr<SecurityOrigin>&&, std::unique_ptr<ContentSecurityPolicy>&&, String&&, ShouldLogError);

        void clearResource();

        // CachedRawResourceClient
        void dataSent(CachedResource&, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
        void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) override;
        void dataReceived(CachedResource&, const char* data, int dataLength) override;
        void redirectReceived(CachedResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&) override;
        void finishedTimingForWorkerLoad(CachedResource&, const ResourceTiming&) override;
        void finishedTimingForWorkerLoad(const ResourceTiming&);
        void notifyFinished(CachedResource&) override;

        void didReceiveResponse(unsigned long identifier, const ResourceResponse&);
        void didReceiveData(unsigned long identifier, const char* data, int dataLength);
        void didFinishLoading(unsigned long identifier);
        void didFail(unsigned long identifier, const ResourceError&);
        void makeCrossOriginAccessRequest(ResourceRequest&&);
        void makeSimpleCrossOriginAccessRequest(ResourceRequest&&);
        void makeCrossOriginAccessRequestWithPreflight(ResourceRequest&&);
        void preflightSuccess(ResourceRequest&&);
        void preflightFailure(unsigned long identifier, const ResourceError&);

        void loadRequest(ResourceRequest&&, SecurityCheckPolicy);
        bool isAllowedRedirect(const URL&);
        bool isAllowedByContentSecurityPolicy(const URL&, ContentSecurityPolicy::RedirectResponseReceived);

        SecurityOrigin& securityOrigin() const;
        const ContentSecurityPolicy& contentSecurityPolicy() const;

        Document& document() { return m_document; }
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

        CachedResourceHandle<CachedRawResource> m_resource;
        ThreadableLoaderClient* m_client;
        Document& m_document;
        ThreadableLoaderOptions m_options;
        RefPtr<SecurityOrigin> m_origin;
        String m_referrer;
        bool m_sameOriginRequest;
        bool m_simpleRequest;
        bool m_async;
        bool m_delayCallbacksForIntegrityCheck;
        std::unique_ptr<ContentSecurityPolicy> m_contentSecurityPolicy;
        Optional<CrossOriginPreflightChecker> m_preflightChecker;
        Optional<HTTPHeaderMap> m_originalHeaders;

        ShouldLogError m_shouldLogError;
#if ENABLE(SERVICE_WORKER)
        Optional<ResourceRequest> m_bypassingPreflightForServiceWorkerRequest;
#endif
    };

} // namespace WebCore
