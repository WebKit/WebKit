/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "GLContext.h"
#include "GraphicsContext3D.h"
#include "PlatformLayer.h"

#if USE(TEXTURE_MAPPER)
#include "TextureMapperPlatformLayer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif

namespace WebCore {

class BitmapTextureGL;

class GraphicsContext3DPrivate
#if USE(TEXTURE_MAPPER)
    : public PlatformLayer
#endif
{
public:
    GraphicsContext3DPrivate(GraphicsContext3D*, GraphicsContext3D::RenderStyle);
    ~GraphicsContext3DPrivate();
    bool makeContextCurrent();
    PlatformGraphicsContext3D platformContext();

    GraphicsContext3D::RenderStyle renderStyle() { return m_renderStyle; }

#if USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<TextureMapperPlatformLayerProxy> proxy() const override;
    void swapBuffersIfNeeded() override;
#elif USE(TEXTURE_MAPPER)
    virtual void paintToTextureMapper(TextureMapper&, const FloatRect& target, const TransformationMatrix&, float opacity);
#endif

private:
    GraphicsContext3D* m_context;
    std::unique_ptr<GLContext> m_glContext;
    GraphicsContext3D::RenderStyle m_renderStyle;

#if USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<TextureMapperPlatformLayerProxy> m_platformLayerProxy;
    RefPtr<BitmapTextureGL> m_compositorTexture;
#endif
};

}

#endif // GraphicsContext3DPrivate_h
