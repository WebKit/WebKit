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
#include "CairoUtilities.h"
#include "CairoUtilitiesEfl.h"
#include "FrameView.h"
#include "GraphicsLayerTextureMapper.h"
#include "MainFrame.h"
#include "PlatformContextCairo.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "ewk_private.h"
#include "ewk_view_private.h"

const double compositingFrameRate = 60;

namespace WebCore {

AcceleratedCompositingContext::AcceleratedCompositingContext(Evas_Object* ewkView, Evas_Object* compositingObject)
    : m_view(ewkView)
    , m_compositingObject(compositingObject)
    , m_rootLayer(nullptr)
    , m_syncTimer(this, &AcceleratedCompositingContext::syncLayers)
    , m_isAccelerated(true)
{
    ASSERT(m_view);
    ASSERT(m_compositingObject);

    Evas* evas = evas_object_evas_get(m_view);
    const char* engine = ecore_evas_engine_name_get(ecore_evas_ecore_evas_get(evas));
    if (!strncmp(engine, "opengl_x11", strlen("opengl_x11"))) {
        m_evasGL = EflUniquePtr<Evas_GL>(evas_gl_new(evas_object_evas_get(m_view)));
        if (m_evasGL)
            m_evasGLContext = EvasGLContext::create(m_evasGL.get());
    }

    if (m_evasGLContext) {
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    } else {
        m_textureMapper = TextureMapper::create(TextureMapper::SoftwareMode);
        m_isAccelerated = false;
    }
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
}

void AcceleratedCompositingContext::syncLayers(Timer<AcceleratedCompositingContext>*)
{
    ewk_view_mark_for_sync(m_view);
}

void AcceleratedCompositingContext::resize(const IntSize& size)
{
    if (m_viewSize == size)
        return;

    m_viewSize = size;

    if (!m_isAccelerated)
        return;

    static Evas_GL_Config evasGLConfig = {
        EVAS_GL_RGBA_8888,
        EVAS_GL_DEPTH_BIT_8,
        EVAS_GL_STENCIL_NONE,
        EVAS_GL_OPTIONS_NONE,
        EVAS_GL_MULTISAMPLE_NONE
    };

    m_evasGLSurface = EvasGLSurface::create(m_evasGL.get(), &evasGLConfig, size);
    if (!m_evasGLSurface) {
        ERR("Failed to create a EvasGLSurface.");
        return;
    }

    Evas_Native_Surface nativeSurface;
    evas_gl_native_surface_get(m_evasGL.get(), m_evasGLSurface->surface(), &nativeSurface);
    evas_object_image_native_surface_set(m_compositingObject, &nativeSurface);

    if (!evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context())) {
        ERR("Failed to evas_gl_make_current.");
        return;
    }

    evas_gl_api_get(m_evasGL.get())->glViewport(0, 0, size.width(), size.height());
    evas_gl_api_get(m_evasGL.get())->glClearColor(1, 1, 1, 1);
    evas_gl_api_get(m_evasGL.get())->glClear(GL_COLOR_BUFFER_BIT);
}

void AcceleratedCompositingContext::flushAndRenderLayers()
{
    MainFrame& frame = EWKPrivate::corePage(m_view)->mainFrame();
    if (!frame.contentRenderer() || !frame.view())
        return;
    frame.view()->updateLayoutAndStyleIfNeededRecursive();

    if (!flushPendingLayerChanges())
        return;

    if (m_isAccelerated)
        paintToCurrentGLContext();
    else
        paintToGraphicsContext();
}

bool AcceleratedCompositingContext::flushPendingLayerChanges()
{
    if (m_rootLayer)
        m_rootLayer->flushCompositingStateForThisLayerOnly();
    return EWKPrivate::corePage(m_view)->mainFrame().view()->flushCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::paintToGraphicsContext()
{
    RefPtr<cairo_surface_t> surface = createSurfaceForImage(m_compositingObject);
    if (!surface)
        return;

    PlatformContextCairo platformContext(cairo_create(surface.get()));
    GraphicsContext context(&platformContext);
    m_textureMapper->setGraphicsContext(&context);

    compositeLayers();
}

void AcceleratedCompositingContext::paintToCurrentGLContext()
{
    if (!evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context()))
        return;

    evas_gl_api_get(m_evasGL.get())->glViewport(0, 0, m_viewSize.width(), m_viewSize.height());
    evas_gl_api_get(m_evasGL.get())->glClear(GL_COLOR_BUFFER_BIT);

    compositeLayers();
}

void AcceleratedCompositingContext::compositeLayers()
{
    TextureMapperLayer* currentRootLayer = toTextureMapperLayer(m_rootLayer);
    if (!currentRootLayer)
        return;

    currentRootLayer->setTextureMapper(m_textureMapper.get());
    currentRootLayer->applyAnimationsRecursively();

    m_textureMapper->beginPainting();
    m_textureMapper->beginClip(TransformationMatrix(), FloatRect(FloatPoint(), m_viewSize));
    currentRootLayer->paint();
    m_fpsCounter.updateFPSAndDisplay(m_textureMapper.get());
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (currentRootLayer->descendantsOrSelfHaveRunningAnimations() && !m_syncTimer.isActive())
        m_syncTimer.startOneShot(1 / compositingFrameRate);
}

void AcceleratedCompositingContext::setRootGraphicsLayer(GraphicsLayer* rootLayer)
{
    m_rootLayer = rootLayer;

    if (!m_syncTimer.isActive())
        m_syncTimer.startOneShot(0);
}

void AcceleratedCompositingContext::extractImageData(Evas_Object* buffer, const IntRect& rect)
{
    ASSERT(buffer);

    Evas_Coord width, height;
    evas_object_image_size_get(buffer, &width, &height);

    ASSERT(rect.width() == width);
    ASSERT(rect.height() == height);

    RefPtr<cairo_surface_t> imageData;
    if (m_isAccelerated) {
        imageData = getImageDataGL(rect);
        flipImageSurfaceVertically(imageData.get());
    } else
        imageData = getImageData(rect);

    evas_object_image_data_copy_set(buffer, static_cast<void*>(cairo_image_surface_get_data(imageData.get())));
}

PassRefPtr<cairo_surface_t> AcceleratedCompositingContext::getImageData(const IntRect& rect)
{
    RefPtr<cairo_surface_t> source = createSurfaceForImage(m_compositingObject);
    evas_object_image_data_set(m_compositingObject, static_cast<void*>(cairo_image_surface_get_data(source.get())));

    if (m_viewSize == rect.size())
        return source.release();

    RefPtr<cairo_surface_t> destination = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, rect.width(), rect.height()));
    copyRectFromOneSurfaceToAnother(source.get(), destination.get(), -IntSize(rect.x(), rect.y()), IntRect(IntPoint(), rect.size()), IntSize(), CAIRO_OPERATOR_SOURCE);
    return destination.release();
}

PassRefPtr<cairo_surface_t> AcceleratedCompositingContext::getImageDataGL(const IntRect& rect)
{
    if (!evas_gl_make_current(m_evasGL.get(), m_evasGLSurface->surface(), m_evasGLContext->context()))
        return nullptr;

    RefPtr<cairo_surface_t> source = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, rect.width(), rect.height()));
    uint8_t* sourcePtr = static_cast<uint8_t*>(cairo_image_surface_get_data(source.get()));

    GraphicsContext3D* context3D = static_cast<TextureMapperGL*>(m_textureMapper.get())->graphicsContext3D();
    context3D->readPixels(rect.x(), rect.y(), rect.width(), rect.height(), GraphicsContext3D::BGRA, GraphicsContext3D::UNSIGNED_BYTE, sourcePtr);
    return source.release();
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
