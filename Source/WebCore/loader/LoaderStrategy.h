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

#ifndef LoaderStrategy_h
#define LoaderStrategy_h

#include "ResourceHandleTypes.h"
#include "ResourceLoadPriority.h"
#include "ResourceLoaderOptions.h"
#include <wtf/Vector.h>

namespace WebCore {

class BlobRegistry;
class CachedResource;
class Frame;
class NetscapePlugInStreamLoader;
class NetscapePlugInStreamLoaderClient;
class NetworkingContext;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SubresourceLoader;
class URL;

class WEBCORE_EXPORT LoaderStrategy {
public:
    virtual RefPtr<SubresourceLoader> loadResource(Frame&, CachedResource&, const ResourceRequest&, const ResourceLoaderOptions&) = 0;
    virtual void loadResourceSynchronously(NetworkingContext*, unsigned long identifier, const ResourceRequest&, StoredCredentials, ClientCredentialPolicy, ResourceError&, ResourceResponse&, Vector<char>& data) = 0;

    virtual void remove(ResourceLoader*) = 0;
    virtual void setDefersLoading(ResourceLoader*, bool) = 0;
    virtual void crossOriginRedirectReceived(ResourceLoader*, const URL& redirectURL) = 0;

    virtual void servePendingRequests(ResourceLoadPriority minimumPriority = ResourceLoadPriority::VeryLow) = 0;
    virtual void suspendPendingRequests() = 0;
    virtual void resumePendingRequests() = 0;

    virtual void createPingHandle(NetworkingContext*, ResourceRequest&, bool shouldUseCredentialStorage, bool shouldFollowRedirects) = 0;

protected:
    virtual ~LoaderStrategy();
};

} // namespace WebCore

#endif // LoaderStrategy_h
