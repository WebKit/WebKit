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

#pragma once

#if USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO)
#include "CoordinatedPlatformLayerBuffer.h"

#if USE(GSTREAMER)
#include "GStreamerCommon.h"
#endif

namespace WebCore {

class GStreamerQuirksManager;

class CoordinatedPlatformLayerBufferHolePunch final : public CoordinatedPlatformLayerBuffer {
public:
    static std::unique_ptr<CoordinatedPlatformLayerBufferHolePunch> create(const IntSize&);
    explicit CoordinatedPlatformLayerBufferHolePunch(const IntSize&);
#if USE(GSTREAMER)
    static std::unique_ptr<CoordinatedPlatformLayerBufferHolePunch> create(const IntSize&, GstElement*, RefPtr<GStreamerQuirksManager>&&);
    CoordinatedPlatformLayerBufferHolePunch(const IntSize&, GstElement*, RefPtr<GStreamerQuirksManager>&&);
#endif
    virtual ~CoordinatedPlatformLayerBufferHolePunch();

private:
    void paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix& modelViewMatrix = TransformationMatrix(), float opacity = 1.0) override;

    void notifyVideoPosition(const FloatRect&, const TransformationMatrix&) override;
    void paintTransparentRectangle(TextureMapper&, const FloatRect&, const TransformationMatrix&) override;

#if USE(GSTREAMER)
    GRefPtr<GstElement> m_videoSink;
    RefPtr<GStreamerQuirksManager> m_quirksManager;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_COORDINATED_PLATFORM_LAYER_BUFFER_TYPE(CoordinatedPlatformLayerBufferHolePunch, Type::HolePunch)

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(VIDEO)
