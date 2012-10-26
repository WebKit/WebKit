/*
    Copyright (C) 2012 Samsung Electronics

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

#include "GraphicsContext3D.h"

#if USE(TEXTURE_MAPPER_GL)
#include <texmap/TextureMapperPlatformLayer.h>
#endif

#if USE(GRAPHICS_SURFACE)
#include "GraphicsSurface.h"
#endif

typedef struct _Evas_GL               Evas_GL;
typedef struct _Evas_GL_Surface       Evas_GL_Surface;
typedef struct _Evas_GL_Context       Evas_GL_Context;
typedef struct _Evas_GL_Config        Evas_GL_Config;
typedef struct _Evas_GL_API           Evas_GL_API;

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

    PlatformGraphicsContext3D platformGraphicsContext3D() const;

#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer() const;
#endif
#if USE(TEXTURE_MAPPER_GL)
    virtual void paintToTextureMapper(TextureMapper*, const FloatRect& target, const TransformationMatrix&, float opacity, BitmapTexture* mask);
#endif
#if USE(GRAPHICS_SURFACE)
    virtual uint32_t copyToGraphicsSurface();
    virtual GraphicsSurfaceToken graphicsSurfaceToken() const;
    void createGraphicsSurfaces(const IntSize&);
#endif

    bool makeContextCurrent();
    bool createSurface(PageClientEfl*, bool renderDirectlyToEvasGLObject);
    void setCurrentGLContext(void*, void*);

    GraphicsContext3D::Attributes m_attributes;
    GraphicsContext3D* m_context;
    HostWindow* m_hostWindow;
#if USE(GRAPHICS_SURFACE)
    GraphicsSurface::Flags m_surfaceFlags;
    RefPtr<GraphicsSurface> m_graphicsSurface;
#endif

    ListHashSet<GC3Denum> m_syntheticErrors;

    OwnPtr<Ecore_Evas> m_ecoreEvas;
    Evas_GL* m_evasGL;
    Evas_GL_Context* m_evasGLContext;
    Evas_GL_Surface* m_evasGLSurface;
    void* m_glContext;
    void* m_glSurface;
    Evas_GL_API* m_api;
    GraphicsContext3D::RenderStyle m_renderStyle;
};

} // namespace WebCore

#endif // GraphicsLayerEfl_h
