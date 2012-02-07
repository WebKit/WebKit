/*
 * Copyright (C) 2012 Igalia, S.L.
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
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "AcceleratedCompositingContext.h"

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL)

#include "CairoUtilities.h"
#include "Chrome.h"
#include "ChromeClientGtk.h"
#include "Frame.h"
#include "FrameView.h"
#include "PlatformContextCairo.h"
#include "TextureMapperGL.h"
#include "TextureMapperNode.h"
#include "webkitwebviewprivate.h"
#include <GL/gl.h>
#include <cairo.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

AcceleratedCompositingContext::AcceleratedCompositingContext(WebKitWebView* webView)
    : m_webView(webView)
    , m_syncTimer(this, &AcceleratedCompositingContext::syncLayersTimeout)
    , m_initialized(false)
    , m_rootTextureMapperNode(0)
{
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{

}

void AcceleratedCompositingContext::initializeIfNecessary()
{
    if (m_initialized)
        return;

    m_initialized = true;

    // The GTK+ docs say that we can fail to create a native window.
    // FIXME: We should fall back to the ImageBuffer TextureMapper when it exists.
    if (!m_webView->priv->hasNativeWindow)
        return;

    m_context = WebCore::WindowGLContext::createContextWithGdkWindow(gtk_widget_get_window(GTK_WIDGET(m_webView)));
}

bool AcceleratedCompositingContext::enabled()
{
    return m_rootTextureMapperNode && m_textureMapper;
}


bool AcceleratedCompositingContext::renderLayersToWindow(const IntRect& clipRect)
{
    if (!enabled())
        return false;

    // We initialize the context lazily here so that we know that the GdkWindow realized.
    initializeIfNecessary();
    if (!m_context)
        return false;

    m_context->startDrawing();

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    glViewport(0, 0, allocation.width, allocation.height);

    m_textureMapper->beginPainting();
    m_rootTextureMapperNode->paint();
    m_textureMapper->endPainting();

    m_context->finishDrawing();
    return true;
}

void AcceleratedCompositingContext::attachRootGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer) {
        m_rootGraphicsLayer.clear();
        m_rootTextureMapperNode = 0;
        return;
    }

    m_rootGraphicsLayer = GraphicsLayer::create(this);
    m_rootTextureMapperNode = toTextureMapperNode(m_rootGraphicsLayer.get());
    m_rootGraphicsLayer->addChild(graphicsLayer);
    m_rootGraphicsLayer->setDrawsContent(true);
    m_rootGraphicsLayer->setMasksToBounds(false);
    m_rootGraphicsLayer->setNeedsDisplay();
    m_rootGraphicsLayer->setSize(core(m_webView)->mainFrame()->view()->frameRect().size());

    // We initialize the context lazily here so that we know that the GdkWindow realized.
    initializeIfNecessary();
    if (!m_context)
        return;

    // The context needs to be active when creating the texture mapper. It's fine to
    // avoid calling endDrawing here, because it will just initialize shaders.
    m_context->startDrawing();

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    glViewport(0, 0, allocation.width, allocation.height);

    m_textureMapper = TextureMapperGL::create();
    m_rootTextureMapperNode->setTextureMapper(m_textureMapper.get());
    m_rootGraphicsLayer->syncCompositingStateForThisLayerOnly();
}

void AcceleratedCompositingContext::scheduleRootLayerRepaint(const IntRect& rect)
{
    if (!m_rootGraphicsLayer)
        return;
    if (rect.isEmpty()) {
        m_rootGraphicsLayer->setNeedsDisplay();
        return;
    }
    m_rootGraphicsLayer->setNeedsDisplayInRect(rect);
}

void AcceleratedCompositingContext::resizeRootLayer(const IntSize& size)
{
    if (!m_rootGraphicsLayer)
        return;
    m_rootGraphicsLayer->setSize(size);
    m_rootGraphicsLayer->setNeedsDisplay();
}

void AcceleratedCompositingContext::markForSync()
{
    if (m_syncTimer.isActive())
        return;
    m_syncTimer.startOneShot(0);
}

void AcceleratedCompositingContext::syncLayersNow()
{
    if (m_rootGraphicsLayer)
        m_rootGraphicsLayer->syncCompositingStateForThisLayerOnly();

    core(m_webView)->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::syncLayersTimeout(Timer<AcceleratedCompositingContext>*)
{
    syncLayersNow();
    if (!m_rootGraphicsLayer)
        return;

    renderLayersToWindow(IntRect());

    if (toTextureMapperNode(m_rootGraphicsLayer.get())->descendantsOrSelfHaveRunningAnimations())
        m_syncTimer.startOneShot(1.0 / 60.0);
}

void AcceleratedCompositingContext::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{

}
void AcceleratedCompositingContext::notifySyncRequired(const WebCore::GraphicsLayer*)
{

}

void AcceleratedCompositingContext::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext& context, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& rectToPaint)
{
    cairo_t* cr = context.platformContext()->cr();
    copyRectFromCairoSurfaceToContext(m_webView->priv->backingStore->cairoSurface(), cr,
                                      IntSize(), rectToPaint);
}

bool AcceleratedCompositingContext::showDebugBorders(const WebCore::GraphicsLayer*) const
{
    return false;
}

bool AcceleratedCompositingContext::showRepaintCounter(const WebCore::GraphicsLayer*) const
{
    return false;
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL)
