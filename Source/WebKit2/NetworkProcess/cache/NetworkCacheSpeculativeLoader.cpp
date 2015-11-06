/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
#include "NetworkCacheSpeculativeLoader.h"

#include "Logging.h"
#include "NetworkCacheSubresourcesEntry.h"
#include <WebCore/HysteresisActivity.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebKit {

namespace NetworkCache {

using namespace WebCore;

static const AtomicString& subresourcesType()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<const AtomicString> resource("subresources", AtomicString::ConstructFromLiteral);
    return resource;
}

static inline Key makeSubresourcesKey(const Key& resourceKey)
{
    return Key(resourceKey.partition(), subresourcesType(), resourceKey.range(), resourceKey.identifier());
}

class SpeculativeLoader::PendingFrameLoad {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PendingFrameLoad(const Key& mainResourceKey, std::function<void()> completionHandler)
        : m_mainResourceKey(mainResourceKey)
        , m_completionHandler(completionHandler)
        , m_loadHysteresisActivity([this](HysteresisState state) { if (state == HysteresisState::Stopped) m_completionHandler(); })
    { }

    void registerSubresource(const Key& subresourceKey)
    {
        ASSERT(RunLoop::isMain());
        m_subresourceKeys.add(subresourceKey);
        m_loadHysteresisActivity.impulse();
    }

    Optional<Storage::Record> encodeAsSubresourcesRecord()
    {
        ASSERT(RunLoop::isMain());
        if (m_subresourceKeys.isEmpty())
            return { };

        auto subresourcesStorageKey = makeSubresourcesKey(m_mainResourceKey);
        Vector<Key> subresourceKeys;
        copyToVector(m_subresourceKeys, subresourceKeys);

#if !LOG_DISABLED
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Saving to disk list of subresources for '%s':", m_mainResourceKey.identifier().utf8().data());
        for (auto& subresourceKey : subresourceKeys)
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) * Subresource: '%s'.", subresourceKey.identifier().utf8().data());
#endif

        return SubresourcesEntry(subresourcesStorageKey, WTF::move(subresourceKeys)).encodeAsStorageRecord();
    }

    void markAsCompleted()
    {
        ASSERT(RunLoop::isMain());
        m_completionHandler();
    }

private:
    Key m_mainResourceKey;
    HashSet<Key> m_subresourceKeys;
    std::function<void()> m_completionHandler;
    HysteresisActivity m_loadHysteresisActivity;
};

SpeculativeLoader::SpeculativeLoader(Storage& storage)
    : m_storage(storage)
{
}

SpeculativeLoader::~SpeculativeLoader()
{
}

void SpeculativeLoader::registerLoad(uint64_t webPageID, uint64_t webFrameID, const ResourceRequest& request, const Key& resourceKey)
{
    ASSERT(RunLoop::isMain());

    if (!request.url().protocolIsInHTTPFamily() || request.httpMethod() != "GET")
        return;

    auto frameKey = std::make_pair(webPageID, webFrameID);
    auto isMainResource = request.requester() == ResourceRequest::Requester::Main;
    if (isMainResource) {
        // Mark previous load in this frame as completed if necessary.
        if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameKey))
            pendingFrameLoad->markAsCompleted();

        // Start tracking loads in this frame.
        m_pendingFrameLoads.add(frameKey, std::make_unique<PendingFrameLoad>(resourceKey, [this, frameKey]() {
            auto frameLoad = m_pendingFrameLoads.take(frameKey);
            auto optionalRecord = frameLoad->encodeAsSubresourcesRecord();
            if (!optionalRecord)
                return;
            m_storage.store(optionalRecord.value(), [](const Data&) { });
        }));
        return;
    }

    if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameKey))
        pendingFrameLoad->registerSubresource(resourceKey);
}

} // namespace NetworkCache

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
