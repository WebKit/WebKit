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
#include "FrameView.h"
#include "GraphicsLayerTextureMapper.h"
#include "HostWindow.h"
#include "MainFrame.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "ewk_view_private.h"

namespace WebCore {

PassOwnPtr<AcceleratedCompositingContext> AcceleratedCompositingContext::create(HostWindow* hostWindow)
{
    OwnPtr<AcceleratedCompositingContext> context = adoptPtr(new AcceleratedCompositingContext);
    if (!context->initialize(hostWindow))
        return nullptr;

    return context.release();
}

AcceleratedCompositingContext::AcceleratedCompositingContext()
    : m_view(0)
    , m_rootTextureMapperLayer(0)
{
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
}

bool AcceleratedCompositingContext::initialize(HostWindow* hostWindow)
{
    m_view = hostWindow->platformPageClient();
    if (!m_view)
        return false;

    m_evasGL = adoptPtr(evas_gl_new(evas_object_evas_get(m_view)));
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

    return true;
}

void AcceleratedCompositingContext::syncLayersNow()
{
    if (m_rootGraphicsLayer)
        m_rootGraphicsLayer->flushCompositingStateForThisLayerOnly();

    EWKPrivate::corePage(m_view)->mainFrame().view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::renderLayers()
{
    if (!m_rootGraphicsLayer)
        return;

    if (!evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context()))
        return;

    Evas_Coord width = 0;
    Evas_Coord height = 0;
    evas_object_geometry_get(m_view, 0, 0, &width, &height);
    evas_gl_api_get(m_evasGL.get())->glViewport(0, 0, width, height);

    m_textureMapper->beginPainting();
    m_rootTextureMapperLayer->paint();
    m_fpsCounter.updateFPSAndDisplay(m_textureMapper.get());
    m_textureMapper->endPainting();
}

void AcceleratedCompositingContext::attachRootGraphicsLayer(GraphicsLayer* rootLayer)
{
    if (!rootLayer) {
        m_rootGraphicsLayer = nullptr;
        m_rootTextureMapperLayer = 0;
        return;
    }

    m_rootGraphicsLayer = WebCore::GraphicsLayer::create(0, 0);
    m_rootTextureMapperLayer = toTextureMapperLayer(m_rootGraphicsLayer.get());
    m_rootGraphicsLayer->addChild(rootLayer);
    m_rootGraphicsLayer->setDrawsContent(false);
    m_rootGraphicsLayer->setMasksToBounds(false);
    m_rootGraphicsLayer->setSize(WebCore::IntSize(1, 1));

    m_textureMapper = TextureMapperGL::create();
    m_rootTextureMapperLayer->setTextureMapper(m_textureMapper.get());

    m_rootGraphicsLayer->flushCompositingStateForThisLayerOnly();
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
