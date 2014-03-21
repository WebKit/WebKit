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

#include "config.h"

#if USE(TEXTURE_MAPPER_GL)

#include "AcceleratedCompositingContextEfl.h"
#include "GraphicsLayerTextureMapper.h"
#include "MainFrame.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "ewk_view_private.h"

const double compositingFrameRate = 60;

namespace WebCore {

AcceleratedCompositingContext::AcceleratedCompositingContext(Evas_Object* ewkView, Evas_Object* compositingObject)
    : m_view(ewkView)
    , m_compositingObject(compositingObject)
    , m_syncTimer(this, &AcceleratedCompositingContext::syncLayers)
{
    ASSERT(m_view);
    ASSERT(m_compositingObject);
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
}

bool AcceleratedCompositingContext::initialize()
{
    m_evasGL = EflUniquePtr<Evas_GL>(evas_gl_new(evas_object_evas_get(m_view)));
    if (!m_evasGL)
        return false;

    m_evasGLContext = EvasGLContext::create(m_evasGL.get());
    if (!m_evasGLContext)
        return false;

    Evas_Coord width = 0;
    Evas_Coord height = 0;
    evas_object_geometry_get(m_view, 0, 0, &width, &height);

    IntSize webViewSize(width, height);
    if (webViewSize.isEmpty())
        return false;

    return resize(webViewSize);
}

void AcceleratedCompositingContext::syncLayers(Timer<AcceleratedCompositingContext>*)
{
    ewk_view_mark_for_sync(m_view);
}

bool AcceleratedCompositingContext::resize(const IntSize& size)
{
    static Evas_GL_Config evasGLConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    m_evasGLSurface = EvasGLSurface::create(m_evasGL.get(), &evasGLConfig, size);
    if (!m_evasGLSurface)
        return false;

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(m_evasGL.get(), m_evasGLSurface->surface(), &nativeSurface);
    evas_object_image_native_surface_set(m_compositingObject, &nativeSurface);
    return true;
}

bool AcceleratedCompositingContext::canComposite()
{
    return m_rootGraphicsLayer && m_textureMapper;
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    if (!canComposite())
        return;

    MainFrame& frame = EWKPrivate::corePage(m_view)->mainFrame();
    if (!frame.contentRenderer() || !frame.view())
        return;
    frame.view()->updateLayoutAndStyleIfNeededRecursive();

    if (!canComposite())
        return;

    if (!evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context()))
        return;

    if (!flushPendingLayerChanges())
        return;

    compositeLayersToContext();

    if (toTextureMapperLayer(m_rootGraphicsLayer.get())->descendantsOrSelfHaveRunningAnimations() && !m_syncTimer.isActive())
        m_syncTimer.startOneShot(1 / compositingFrameRate);
}

bool AcceleratedCompositingContext::flushPendingLayerChanges()
{
    m_rootGraphicsLayer->flushCompositingStateForThisLayerOnly();
    return EWKPrivate::corePage(m_view)->mainFrame().view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::compositeLayersToContext()
{
    Evas_Coord width = 0;
    Evas_Coord height = 0;
    evas_object_geometry_get(m_view, 0, 0, &width, &height);

    evas_gl_api_get(m_evasGL.get())->glViewport(0, 0, width, height);
    evas_gl_api_get(m_evasGL.get())->glClear(GL_COLOR_BUFFER_BIT);

    m_textureMapper->beginPainting();
    m_textureMapper->beginClip(TransformationMatrix(), FloatRect(0, 0, width, height));
    toTextureMapperLayer(m_rootGraphicsLayer.get())->paint();
    m_fpsCounter.updateFPSAndDisplay(m_textureMapper.get());
    m_textureMapper->endClip();
    m_textureMapper->endPainting();
}

void AcceleratedCompositingContext::attachRootGraphicsLayer(GraphicsLayer* rootLayer)
{
    if (!rootLayer) {
        m_rootGraphicsLayer = nullptr;
        return;
    }

    if (!m_textureMapper) {
        evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context());
        m_textureMapper = TextureMapperGL::create();
    }

    m_rootGraphicsLayer = GraphicsLayer::create(0, 0);
    m_rootGraphicsLayer->addChild(rootLayer);
    m_rootGraphicsLayer->setDrawsContent(false);
    m_rootGraphicsLayer->setMasksToBounds(false);
    m_rootGraphicsLayer->setSize(IntSize(1, 1));

    toTextureMapperLayer(m_rootGraphicsLayer.get())->setTextureMapper(m_textureMapper.get());

    m_rootGraphicsLayer->flushCompositingStateForThisLayerOnly();
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
