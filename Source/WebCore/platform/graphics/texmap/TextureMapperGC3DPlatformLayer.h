/*
 * Copyright (C) 2011, 2012, 2017 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#pragma once

#if ENABLE(GRAPHICS_CONTEXT_3D) && USE(TEXTURE_MAPPER)

#include "GraphicsContext3D.h"
#include "PlatformLayer.h"
#include "TextureMapperPlatformLayer.h"
#include "TextureMapperPlatformLayerProxy.h"

namespace WebCore {

class BitmapTextureGL;
class GLContext;
class TextureMapperPlatformLayerProxy;

class TextureMapperGC3DPlatformLayer : public PlatformLayer {
public:
    TextureMapperGC3DPlatformLayer(GraphicsContext3D&, GraphicsContext3D::RenderStyle);
    virtual ~TextureMapperGC3DPlatformLayer();

    bool makeContextCurrent();
    PlatformGraphicsContext3D platformContext();
    GraphicsContext3D::RenderStyle renderStyle() { return m_renderStyle; }

#if USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const override;
    void swapBuffersIfNeeded() override;
#else
    virtual void paintToTextureMapper(TextureMapper&, const FloatRect& target, const TransformationMatrix&, float opacity);
#endif

private:
    GraphicsContext3D& m_context;
    std::unique_ptr<GLContext> m_glContext;
    GraphicsContext3D::RenderStyle m_renderStyle;

#if USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
    RefPtr<BitmapTextureGL> m_compositorTexture;
#endif
};

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D) && USE(TEXTURE_MAPPER)
