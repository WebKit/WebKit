/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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

#include <WebCore/DeferredFetchIdentifier.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/LoadSchedulingMode.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceLoadPriority.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/ObjectIdentifier.h>

namespace WebCore {

class CachedResource;
class ContentSecurityPolicy;
class FrameLoader;
class HTTPHeaderMap;
class LocalFrame;
class NetscapePlugInStreamLoader;
class NetscapePlugInStreamLoaderClient;
struct NetworkTransactionInformation;
class NetworkLoadMetrics;
class Page;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class FragmentedSharedBuffer;
class SubresourceLoader;

struct FetchOptions;
class SecurityOriginData;

class WEBCORE_EXPORT LoaderStrategy : public CanMakeCheckedPtr<LoaderStrategy> {
    WTF_MAKE_TZONE_ALLOCATED(LoaderStrategy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LoaderStrategy);
public:
    virtual void loadResource(LocalFrame&, CachedResource&, ResourceRequest&&, const ResourceLoaderOptions&, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&&) = 0;
    virtual void loadResourceSynchronously(FrameLoader&, ResourceLoaderIdentifier, const ResourceRequest&, ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&, ResourceError&, ResourceResponse&, Vector<uint8_t>& data) = 0;
    virtual void pageLoadCompleted(Page&) = 0;
    virtual void browsingContextRemoved(LocalFrame&) = 0;

    virtual void remove(ResourceLoader*) = 0;
    virtual void setDefersLoading(ResourceLoader&, bool) = 0;
    virtual void crossOriginRedirectReceived(ResourceLoader*, const URL& redirectURL) = 0;

    virtual void servePendingRequests(ResourceLoadPriority minimumPriority = ResourceLoadPriority::VeryLow) = 0;
    virtual void suspendPendingRequests() = 0;
    virtual void resumePendingRequests() = 0;

    virtual void setResourceLoadSchedulingMode(Page&, LoadSchedulingMode);
    virtual void prioritizeResourceLoads(const Vector<Ref<SubresourceLoader>>&);

    virtual bool startKeepAliveLoadForWebKitLegacy(FrameLoader&, const ResourceRequest&, const ResourceLoaderOptions&, CompletionHandler<void(const ResourceError&, const ResourceResponse&)>&&);

    virtual std::pair<std::optional<DeferredFetchIdentifier>, uint64_t> reserveDeferredFetchQuota(LocalFrame&, FrameIdentifier, const SecurityOriginData&, uint64_t, uint64_t);
    virtual void releaseDeferredFetchQuota(DeferredFetchIdentifier);

    // Hands a deferred fetch to the network process so that it can be sent even once the
    // queueing document is gone. Returns false if the port has no network process, in which
    // case the caller sends the request itself while the document is still alive.
    virtual bool addDeferredFetch(LocalFrame&, DeferredFetchIdentifier, const ResourceRequest&, const ResourceLoaderOptions&);
    virtual void sendDeferredFetch(DeferredFetchIdentifier);
    virtual void removeDeferredFetch(DeferredFetchIdentifier);

    using PreconnectCompletionHandler = Function<void(const ResourceError&)>;
    enum class ShouldPreconnectAsFirstParty : bool { No, Yes };
    virtual void preconnectTo(FrameLoader&, ResourceRequest&&, StoredCredentialsPolicy, ShouldPreconnectAsFirstParty, PreconnectCompletionHandler&&) = 0;

    virtual void setCaptureExtraNetworkLoadMetricsEnabled(bool) = 0;

    virtual bool isOnLine() const = 0;
    virtual void addOnlineStateChangeListener(Function<void(bool)>&&) = 0;

    virtual bool shouldPerformSecurityChecks() const { return false; }
    virtual bool havePerformedSecurityChecks(const ResourceResponse&) const { return false; }

    virtual ResourceResponse responseFromResourceLoadIdentifier(ResourceLoaderIdentifier);
    virtual Vector<NetworkTransactionInformation> intermediateLoadInformationFromResourceLoadIdentifier(ResourceLoaderIdentifier);
    virtual NetworkLoadMetrics networkMetricsFromResourceLoadIdentifier(ResourceLoaderIdentifier);

    virtual void isResourceLoadFinished(CachedResource&, CompletionHandler<void(bool)>&& callback) = 0;

    // Used for testing only.
    virtual Vector<ResourceLoaderIdentifier> ongoingLoads() const { return { }; }

    virtual ResourceError cancelledError(const ResourceRequest&) const = 0;
    virtual ResourceError blockedError(const ResourceRequest&) const = 0;
    virtual bool isBlockedError(const ResourceError&) const = 0;
    virtual ResourceError blockedByContentBlockerError(const ResourceRequest&) const = 0;
    virtual ResourceError cannotShowURLError(const ResourceRequest&) const = 0;
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) const = 0;
#if ENABLE(CONTENT_FILTERING)
    virtual ResourceError blockedByContentFilterError(const ResourceRequest&) const = 0;
#endif
    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) const = 0;
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) const = 0;
    virtual ResourceError httpsUpgradeRedirectLoopError(const ResourceRequest&) const = 0;
    virtual ResourceError httpNavigationWithHTTPSOnlyError(const ResourceRequest&) const = 0;
    virtual bool isHttpNavigationWithHTTPSOnlyError(const ResourceError&) const = 0;
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) const = 0;

protected:
    virtual ~LoaderStrategy();
};

} // namespace WebCore
