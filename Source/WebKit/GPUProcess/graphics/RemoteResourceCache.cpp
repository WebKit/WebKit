/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteResourceCache.h"

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

void RemoteResourceCache::cacheImageBuffer(Ref<ImageBuffer>&& imageBuffer)
{
    auto renderingResourceIdentifier = imageBuffer->renderingResourceIdentifier();
    m_imageBuffers.add(renderingResourceIdentifier, WTFMove(imageBuffer));

    ensureResourceUseCounter(renderingResourceIdentifier);
}

ImageBuffer* RemoteResourceCache::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return m_imageBuffers.get(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheNativeImage(Ref<NativeImage>&& image)
{
    auto renderingResourceIdentifier = image->renderingResourceIdentifier();
    m_nativeImages.add(renderingResourceIdentifier, WTFMove(image));

    ensureResourceUseCounter(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheFont(Ref<Font>&& font)
{
    auto renderingResourceIdentifier = font->renderingResourceIdentifier();
    m_fonts.add(renderingResourceIdentifier, WTFMove(font));

    ensureResourceUseCounter(renderingResourceIdentifier);
}

void RemoteResourceCache::ensureResourceUseCounter(RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto result = m_resourceUseCounters.add(renderingResourceIdentifier, ResourceUseCounter { });
    if (!result.isNewEntry) {
        auto& state = result.iterator->value.state;
        // FIXME: We should consider making this assertion MESSAGE_CHECK the web process.
        ASSERT(state == ResourceState::ToBeDeleted);
        state = ResourceState::Alive;
    }
}

void RemoteResourceCache::deleteAllFonts()
{
    m_fonts.clear();
}

bool RemoteResourceCache::maybeRemoveResource(RenderingResourceIdentifier renderingResourceIdentifier, ResourceUseCountersMap::iterator& iterator)
{
    auto& value = iterator->value;
    if (value.state == ResourceState::Alive || value.useOrPendingCount < 0)
        return true;

    if (value.useOrPendingCount) {
        // This means we've hit a situation that's something like the following:
        //
        // The WP sends:
        // 1. Send cache resource message
        // 2. Use resource 2x
        // 3. Send release resource message with useCount = 2
        // 4. Send cache resource message again for the same resource
        // 5. Use resource 4 times
        // 6. Send release resource message with useCount = 4
        //
        // The GPUP sees:
        // 1. The cache resource message
        // 2. Use resource 2x
        // 3. Use resource 3x more times (from step 5 above)
        // 4. Release resource message with useCount = 2  <== This part right here
        // 5. ... more stuff ...
        //
        // In this situation, we've got a head-start on a future frame's rendering commands,
        // using a resource which was cached from a previous frame. (This is not a problem.)
        // Therefore, we know that, in the future, there's going to be another cache resource
        // message which will want us to cache this exact same resource that we already have
        // cached.
        //
        // So, let's just not remove the resource, just in case there are _more_ rendering
        // commands we can get a head start on before the cache resource message eventually
        // gets delivered. The eventual cache resource command is smart enough to realize that
        // the thing it's supposed to be caching is already in the cache, and will correctly
        // do nothing.

        return true;
    }

    m_resourceUseCounters.remove(iterator);

    if (m_imageBuffers.remove(renderingResourceIdentifier))
        return true;
    if (m_nativeImages.remove(renderingResourceIdentifier))
        return true;
    if (m_fonts.remove(renderingResourceIdentifier))
        return true;

    // Caching the remote resource should have happened before releasing it.
    return false;
}

void RemoteResourceCache::recordResourceUse(RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto iterator = m_resourceUseCounters.find(renderingResourceIdentifier);

    ASSERT(iterator != m_resourceUseCounters.end());

    ResourceUseCounter& useCounter = iterator->value;
    ++useCounter.useOrPendingCount;

    maybeRemoveResource(renderingResourceIdentifier, iterator);
}

bool RemoteResourceCache::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier, uint64_t useCount)
{
    auto iterator = m_resourceUseCounters.find(renderingResourceIdentifier);
    if (iterator == m_resourceUseCounters.end())
        return false;
    ResourceUseCounter& useCounter = iterator->value;
    useCounter.state = ResourceState::ToBeDeleted;
    useCounter.useOrPendingCount -= useCount;
    return maybeRemoveResource(renderingResourceIdentifier, iterator);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
