/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#if USE(3D_GRAPHICS) || USE(ACCELERATED_COMPOSITING)

#include "GraphicsContext3D.h"

#if USE(TEXTURE_MAPPER_GL)
#include <texmap/TextureMapperPlatformLayer.h>
#endif

#include "GLPlatformContext.h"
#include <wtf/PassOwnPtr.h>

class PageClientEfl;

namespace WebCore {
class GraphicsContext3DPrivate
#if USE(TEXTURE_MAPPER_GL)
        : public TextureMapperPlatformLayer
#endif
{
public:
    GraphicsContext3DPrivate(GraphicsContext3D*, HostWindow*, GraphicsContext3D::RenderStyle);
    ~GraphicsContext3DPrivate();

    bool createSurface(PageClientEfl*, bool);
    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback>  callBack);
#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer() const;
#endif
#if USE(TEXTURE_MAPPER_GL)
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect& target, const TransformationMatrix&, float opacity, BitmapTexture* mask);
#endif
#if USE(GRAPHICS_SURFACE)
    virtual uint32_t copyToGraphicsSurface();
    virtual GraphicsSurfaceToken graphicsSurfaceToken() const;
    void didResizeCanvas();
#endif
    bool makeContextCurrent();
    void releaseResources();
    GraphicsContext3D::Attributes m_attributes;
    GraphicsContext3D* m_context;
    HostWindow* m_hostWindow;
    OwnPtr<GLPlatformContext> m_platformContext;
    OwnPtr<GLPlatformSurface> m_platformSurface;
#if USE(GRAPHICS_SURFACE)
    GraphicsSurfaceToken m_surfaceHandle;
#endif
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
    ListHashSet<GC3Denum> m_syntheticErrors;
    bool m_pendingSurfaceResize;
};

} // namespace WebCore

#endif

#endif
