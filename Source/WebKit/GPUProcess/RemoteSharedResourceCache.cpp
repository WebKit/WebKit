/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)
#include "RemoteSharedResourceCache.h"

namespace WebKit {
using namespace WebCore;

// Per GPU process limit of accelerated image buffers.
// These consume limited global OS resources.
constexpr size_t acceleratedImageBufferGlobalLimit = 30000;

// Per GPU process count of current accelerated image buffers.
static std::atomic<size_t> acceleratedImageBufferGlobalCount;

// Per WebContent process limit of accelerated image buffers.
constexpr size_t acceleratedImageBufferLimit = 5000;

// Per WebContent process limit of accelerated image buffers for 2d context.
// It is most common to leak 2d contexts.
constexpr size_t acceleratedImageBuffer2DContextLimit = 1000;

constexpr Seconds defaultRemoteSharedResourceCacheTimeout = 15_s;

Ref<RemoteSharedResourceCache> RemoteSharedResourceCache::create()
{
    return adoptRef(*new RemoteSharedResourceCache());
}

RemoteSharedResourceCache::RemoteSharedResourceCache() = default;

RemoteSharedResourceCache::~RemoteSharedResourceCache() = default;

void RemoteSharedResourceCache::addSerializedImageBuffer(RenderingResourceIdentifier identifier, Ref<ImageBuffer> imageBuffer)
{
    m_serializedImageBuffers.add({ identifier, 0 }, WTFMove(imageBuffer));
}

RefPtr<ImageBuffer> RemoteSharedResourceCache::takeSerializedImageBuffer(RenderingResourceIdentifier identifier)
{
    return m_serializedImageBuffers.take({ { identifier, 0 }, 0 }, defaultRemoteSharedResourceCacheTimeout);
}

void RemoteSharedResourceCache::releaseSerializedImageBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    m_serializedImageBuffers.remove({ { identifier, 0 }, 0 });
}

void RemoteSharedResourceCache::didAddAcceleratedImageBuffer()
{
    ++acceleratedImageBufferGlobalCount;
    ++m_acceleratedImageBufferCount;
}

void RemoteSharedResourceCache::didTakeAcceleratedImageBuffer()
{
    --acceleratedImageBufferGlobalCount;
    --m_acceleratedImageBufferCount;
}

WebCore::RenderingMode RemoteSharedResourceCache::adjustAcceleratedImageBufferRenderingMode(RenderingPurpose renderingPurpose) const
{
    // These are naturally racy, but the limits are heuristic in nature.
    if (acceleratedImageBufferGlobalCount >= acceleratedImageBufferGlobalLimit)
        return RenderingMode::Unaccelerated;
    if (m_acceleratedImageBufferCount >= acceleratedImageBufferLimit)
        return RenderingMode::Unaccelerated;
    if (renderingPurpose == RenderingPurpose::Canvas && m_acceleratedImageBufferCount >= acceleratedImageBuffer2DContextLimit)
        return RenderingMode::Unaccelerated;
    return RenderingMode::Accelerated;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
