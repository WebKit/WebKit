/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)

#include "ImageBufferCairoSurfaceBackend.h"
#include <wtf/IsoMalloc.h>

#if USE(NICOSIA)
#include "NicosiaContentLayerTextureMapperImpl.h"
#else
#include "PlatformLayer.h"
#endif

namespace WebCore {

class ImageBufferCairoGLSurfaceBackend
#if USE(NICOSIA)
    : public Nicosia::ContentLayerTextureMapperImpl::Client
#else
    : public PlatformLayer
#endif
    , public ImageBufferCairoSurfaceBackend {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferCairoGLSurfaceBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferCairoGLSurfaceBackend);
public:
    static std::unique_ptr<ImageBufferCairoGLSurfaceBackend> create(const FloatSize&, float resolutionScale, ColorSpace, const HostWindow*);
    static std::unique_ptr<ImageBufferCairoGLSurfaceBackend> create(const FloatSize&, const GraphicsContext&);

    ~ImageBufferCairoGLSurfaceBackend();

    PlatformLayer* platformLayer() const override;
    bool copyToPlatformTexture(GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) override;

private:
    ImageBufferCairoGLSurfaceBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace, RefPtr<cairo_surface_t>&&);

#if USE(COORDINATED_GRAPHICS)
    void createCompositorBuffer();
    void swapBuffersIfNeeded() override;
#if !USE(NICOSIA)
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const override { return m_platformLayerProxy.copyRef(); }
#endif
#else
    void paintToTextureMapper(TextureMapper&, const FloatRect& target, const TransformationMatrix&, float opacity);
#endif

#if USE(COORDINATED_GRAPHICS)
#if USE(NICOSIA)
    RefPtr<Nicosia::ContentLayer> m_nicosiaLayer;
#else
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
#endif
    uint32_t m_texture;
    RefPtr<cairo_surface_t> m_compositorSurface;
    RefPtr<cairo_t> m_compositorCr;
    uint32_t m_compositorTexture { 0 };
#endif
    uint32_t m_texture { 0 };
};

} // namespace WebCore

#endif // USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)
