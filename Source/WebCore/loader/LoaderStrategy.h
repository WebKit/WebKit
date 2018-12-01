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

#include "FetchOptions.h"
#include "ResourceLoadPriority.h"
#include "ResourceLoaderOptions.h"
#include "StoredCredentialsPolicy.h"
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/SHA1.h>

namespace WebCore {

class CachedResource;
class ContentSecurityPolicy;
class Frame;
class FrameLoader;
class HTTPHeaderMap;
class NetscapePlugInStreamLoader;
class NetscapePlugInStreamLoaderClient;
struct NetworkTransactionInformation;
class NetworkLoadMetrics;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SharedBuffer;
class SubresourceLoader;

struct FetchOptions;

class WEBCORE_EXPORT LoaderStrategy {
public:
    virtual void loadResource(Frame&, CachedResource&, ResourceRequest&&, const ResourceLoaderOptions&, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&&) = 0;
    virtual void loadResourceSynchronously(FrameLoader&, unsigned long identifier, const ResourceRequest&, ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&, ResourceError&, ResourceResponse&, Vector<char>& data) = 0;
    virtual void pageLoadCompleted(uint64_t webPageID) = 0;

    virtual void remove(ResourceLoader*) = 0;
    virtual void setDefersLoading(ResourceLoader&, bool) = 0;
    virtual void crossOriginRedirectReceived(ResourceLoader*, const URL& redirectURL) = 0;

    virtual void servePendingRequests(ResourceLoadPriority minimumPriority = ResourceLoadPriority::VeryLow) = 0;
    virtual void suspendPendingRequests() = 0;
    virtual void resumePendingRequests() = 0;

    using PingLoadCompletionHandler = WTF::Function<void(const ResourceError&, const ResourceResponse&)>;
    virtual void startPingLoad(Frame&, ResourceRequest&, const HTTPHeaderMap& originalRequestHeaders, const FetchOptions&, PingLoadCompletionHandler&& = { }) = 0;

    using PreconnectCompletionHandler = WTF::Function<void(const ResourceError&)>;
    virtual void preconnectTo(FrameLoader&, const URL&, StoredCredentialsPolicy, PreconnectCompletionHandler&&) = 0;

    virtual void storeDerivedDataToCache(const SHA1::Digest& bodyKey, const String& type, const String& partition, WebCore::SharedBuffer&) = 0;

    virtual void setCaptureExtraNetworkLoadMetricsEnabled(bool) = 0;

    virtual bool isOnLine() const = 0;
    virtual void addOnlineStateChangeListener(WTF::Function<void(bool)>&&) = 0;

    virtual bool shouldPerformSecurityChecks() const { return false; }
    virtual bool havePerformedSecurityChecks(const ResourceResponse&) const { return false; }

    virtual ResourceResponse responseFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier);
    virtual Vector<NetworkTransactionInformation> intermediateLoadInformationFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier);
    virtual NetworkLoadMetrics networkMetricsFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier);

    // Used for testing only.
    virtual Vector<uint64_t> ongoingLoads() const { return { }; }

protected:
    virtual ~LoaderStrategy();
};

} // namespace WebCore
