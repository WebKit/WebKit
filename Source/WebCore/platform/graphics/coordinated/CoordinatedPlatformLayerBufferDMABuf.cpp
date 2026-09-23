/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferDMABuf.h"

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER) && USE(GBM)
#include "DMABufBuffer.h"
#include "GLFence.h"

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, AlphaMode alphaMode, Origin origin, std::unique_ptr<GLFence>&& fence, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    if (!threadSafeGrContext)
        return nullptr;
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), alphaMode, origin, WTF::move(fence), threadSafeGrContext);
}

std::unique_ptr<CoordinatedPlatformLayerBufferDMABuf> CoordinatedPlatformLayerBufferDMABuf::create(Ref<DMABufBuffer>&& dmabuf, AlphaMode alphaMode, Origin origin, UnixFileDescriptor&& fenceFD, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    if (!threadSafeGrContext)
        return nullptr;
    return makeUnique<CoordinatedPlatformLayerBufferDMABuf>(WTF::move(dmabuf), alphaMode, origin, WTF::move(fenceFD), threadSafeGrContext);
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, AlphaMode alphaMode, Origin origin, std::unique_ptr<GLFence>&& fence, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, alphaMode)
    , m_dmabuf(WTF::move(dmabuf))
{
    initializeSkiaImage(threadSafeGrContext, origin, WTF::move(fence), { });
}

CoordinatedPlatformLayerBufferDMABuf::CoordinatedPlatformLayerBufferDMABuf(Ref<DMABufBuffer>&& dmabuf, AlphaMode alphaMode, Origin origin, UnixFileDescriptor&& fenceFD, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : CoordinatedPlatformLayerBuffer(Type::DMABuf, dmabuf->attributes().size, alphaMode)
    , m_dmabuf(WTF::move(dmabuf))
{
    initializeSkiaImage(threadSafeGrContext, origin, nullptr, WTF::move(fenceFD));
}

CoordinatedPlatformLayerBufferDMABuf::~CoordinatedPlatformLayerBufferDMABuf() = default;

void CoordinatedPlatformLayerBufferDMABuf::initializeSkiaImage(const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext, Origin origin, std::unique_ptr<GLFence>&& fence, UnixFileDescriptor&& fenceFD)
{
    m_image = m_dmabuf->createPromiseImage(threadSafeGrContext, kRGBA_8888_SkColorType, toSkiaAlphaType(m_alphaMode), toSkiaOrigin(origin), WTF::move(fence), WTF::move(fenceFD));
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(GBM)
