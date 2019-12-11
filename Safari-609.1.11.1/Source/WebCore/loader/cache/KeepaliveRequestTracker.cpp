/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "KeepaliveRequestTracker.h"

namespace WebCore {

const uint64_t maxInflightKeepaliveBytes { 65536 }; // 64 kibibytes as per Fetch specification.

KeepaliveRequestTracker::~KeepaliveRequestTracker()
{
    auto inflightRequests = WTFMove(m_inflightKeepaliveRequests);
    for (auto& resource : inflightRequests)
        resource->removeClient(*this);
}

bool KeepaliveRequestTracker::tryRegisterRequest(CachedResource& resource)
{
    ASSERT(resource.options().keepAlive);
    auto* body = resource.resourceRequest().httpBody();
    if (!body)
        return true;

    uint64_t bodySize = body->lengthInBytes();
    if (m_inflightKeepaliveBytes + bodySize > maxInflightKeepaliveBytes)
        return false;

    registerRequest(resource);
    return true;
}

void KeepaliveRequestTracker::registerRequest(CachedResource& resource)
{
    ASSERT(resource.options().keepAlive);
    auto* body = resource.resourceRequest().httpBody();
    if (!body)
        return;
    ASSERT(!m_inflightKeepaliveRequests.contains(&resource));
    m_inflightKeepaliveRequests.append(&resource);
    m_inflightKeepaliveBytes += body->lengthInBytes();
    ASSERT(m_inflightKeepaliveBytes <= maxInflightKeepaliveBytes);

    resource.addClient(*this);
}

void KeepaliveRequestTracker::responseReceived(CachedResource& resource, const ResourceResponse&, CompletionHandler<void()>&& completionHandler)
{
    // Per Fetch specification, allocated quota should be returned before the promise is resolved,
    // which is when the response is received.
    unregisterRequest(resource);

    if (completionHandler)
        completionHandler();
}

void KeepaliveRequestTracker::notifyFinished(CachedResource& resource)
{
    unregisterRequest(resource);
}

void KeepaliveRequestTracker::unregisterRequest(CachedResource& resource)
{
    ASSERT(resource.options().keepAlive);
    resource.removeClient(*this);
    bool wasRemoved = m_inflightKeepaliveRequests.removeFirst(&resource);
    ASSERT_UNUSED(wasRemoved, wasRemoved);
    m_inflightKeepaliveBytes -= resource.resourceRequest().httpBody()->lengthInBytes();
    ASSERT(m_inflightKeepaliveBytes <= maxInflightKeepaliveBytes);
}

}
