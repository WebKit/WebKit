/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation. All rights reserved.

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
    static PassOwnPtr<GraphicsContext3DPrivate> create(GraphicsContext3D*, HostWindow*);
    ~GraphicsContext3DPrivate();

    bool createSurface(PageClientEfl*, bool);
    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback>);
#if USE(TEXTURE_MAPPER_GL)
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect&, const TransformationMatrix&, float) OVERRIDE;
#endif
#if USE(GRAPHICS_SURFACE)
    virtual IntSize platformLayerSize() const OVERRIDE;
    virtual uint32_t copyToGraphicsSurface() OVERRIDE;
    virtual GraphicsSurfaceToken graphicsSurfaceToken() const OVERRIDE;
    void didResizeCanvas(const IntSize&);
#endif
    bool makeContextCurrent() const;

private:
    GraphicsContext3DPrivate(GraphicsContext3D*, HostWindow*);
    bool initialize();
    bool prepareBuffer() const;
    void releaseResources();
#if USE(GRAPHICS_SURFACE)
    bool makeSharedContextCurrent() const;
#endif
    GraphicsContext3D* m_context;
    HostWindow* m_hostWindow;
    OwnPtr<GLPlatformContext> m_offScreenContext;
    OwnPtr<GLPlatformSurface> m_offScreenSurface;
#if USE(GRAPHICS_SURFACE)
    OwnPtr<GLPlatformContext> m_sharedContext;
    OwnPtr<GLPlatformSurface> m_sharedSurface;
    GraphicsSurfaceToken m_surfaceHandle;
#endif
    OwnPtr<GraphicsContext3D::ContextLostCallback> m_contextLostCallback;
    ListHashSet<GC3Denum> m_syntheticErrors;
    IntSize m_size;
};

} // namespace WebCore

#endif

#endif

