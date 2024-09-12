/*
 * Copyright (C) 2015, 2024 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferHolePunch.h"

#if USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO)
#include "IntRect.h"
#include "TextureMapper.h"

#if USE(GSTREAMER)
#include "GStreamerQuirks.h"
#endif

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferHolePunch> CoordinatedPlatformLayerBufferHolePunch::create(const IntSize& size)
{
    return makeUnique<CoordinatedPlatformLayerBufferHolePunch>(size);
}

CoordinatedPlatformLayerBufferHolePunch::CoordinatedPlatformLayerBufferHolePunch(const IntSize& size)
    : CoordinatedPlatformLayerBuffer(Type::HolePunch, size, { TextureMapperFlags::ShouldNotBlend }, nullptr)
{
}

#if USE(GSTREAMER)
std::unique_ptr<CoordinatedPlatformLayerBufferHolePunch> CoordinatedPlatformLayerBufferHolePunch::create(const IntSize& size, GstElement* videoSink, RefPtr<GStreamerQuirksManager>&& quirksManager)
{
    ASSERT(videoSink && quirksManager);
    return makeUnique<CoordinatedPlatformLayerBufferHolePunch>(size, videoSink, WTFMove(quirksManager));
}

CoordinatedPlatformLayerBufferHolePunch::CoordinatedPlatformLayerBufferHolePunch(const IntSize& size, GstElement* videoSink, RefPtr<GStreamerQuirksManager>&& quirksManager)
    : CoordinatedPlatformLayerBuffer(Type::HolePunch, size, { TextureMapperFlags::ShouldNotBlend }, nullptr)
    , m_videoSink(videoSink)
    , m_quirksManager(quirksManager)
{
}
#endif

CoordinatedPlatformLayerBufferHolePunch::~CoordinatedPlatformLayerBufferHolePunch() = default;

void CoordinatedPlatformLayerBufferHolePunch::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float)
{
#if USE(GSTREAMER)
    if (m_videoSink && m_quirksManager)
        m_quirksManager->setHolePunchVideoRectangle(m_videoSink.get(), enclosingIntRect(modelViewMatrix.mapRect(targetRect)));
#endif
    textureMapper.drawSolidColor(targetRect, modelViewMatrix, Color::transparentBlack, false);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO)
