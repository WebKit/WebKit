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
#include "TextureMapperLayer.h"
#include "webkitwebviewprivate.h"
#include <GL/gl.h>
#include <cairo.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#if defined(GDK_WINDOWING_X11)
#define Region XRegion
#define Font XFont
#define Cursor XCursor
#define Screen XScreen
#include <gdk/gdkx.h>
#endif

using namespace WebCore;

namespace WebKit {

AcceleratedCompositingContext::AcceleratedCompositingContext(WebKitWebView* webView)
    : m_webView(webView)
    , m_syncTimerCallbackId(0)
    , m_rootTextureMapperLayer(0)
{
}

AcceleratedCompositingContext::~AcceleratedCompositingContext()
{
    if (m_syncTimerCallbackId)
        g_source_remove(m_syncTimerCallbackId);
}

bool AcceleratedCompositingContext::enabled()
{
    return m_rootTextureMapperLayer && m_textureMapper;
}

GLContext* AcceleratedCompositingContext::glContext()
{
    if (m_context)
        return m_context.get();

#if defined(GDK_WINDOWING_X11)
    // FIXME: Gracefully account for situations where we do not have a realized window.
    GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(m_webView));
    if (gdkWindow && gdk_window_has_native(gdkWindow))
        m_context = GLContext::createContextForWindow(GDK_WINDOW_XID(gdkWindow), GLContext::sharingContext());
#endif

    return m_context.get();
}

bool AcceleratedCompositingContext::renderLayersToWindow(const IntRect& clipRect)
{
    if (!enabled())
        return false;

    GLContext* context = glContext();
    if (!context)
        return false;

    if (!context->makeContextCurrent())
        return false;

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    glViewport(0, 0, allocation.width, allocation.height);

    m_textureMapper->beginPainting();
    m_rootTextureMapperLayer->paint();
    m_textureMapper->endPainting();

    context->swapBuffers();
    return true;
}

void AcceleratedCompositingContext::attachRootGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer) {
        m_rootGraphicsLayer.clear();
        m_rootTextureMapperLayer = 0;
        m_context.clear();
        return;
    }

    m_rootGraphicsLayer = GraphicsLayer::create(this);
    m_rootTextureMapperLayer = toTextureMapperLayer(m_rootGraphicsLayer.get());
    m_rootGraphicsLayer->addChild(graphicsLayer);
    m_rootGraphicsLayer->setDrawsContent(true);
    m_rootGraphicsLayer->setMasksToBounds(false);
    m_rootGraphicsLayer->setNeedsDisplay();
    m_rootGraphicsLayer->setSize(core(m_webView)->mainFrame()->view()->frameRect().size());

    GLContext* context = glContext();
    if (!context)
        return;

    // The context needs to be active when creating the texture mapper. It's fine to
    // avoid calling swapBuffers here, because it will just initialize shaders.
    if (!context->makeContextCurrent())
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(m_webView), &allocation);
    glViewport(0, 0, allocation.width, allocation.height);

    m_textureMapper = TextureMapperGL::create();
    m_rootTextureMapperLayer->setTextureMapper(m_textureMapper.get());
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
    m_rootGraphicsLayer->syncCompositingStateForThisLayerOnly();
}

static gboolean syncLayersTimeoutCallback(AcceleratedCompositingContext* context)
{
    context->syncLayersTimeout();
    return FALSE;
}

void AcceleratedCompositingContext::markForSync()
{
    if (m_syncTimerCallbackId)
        return;

    // We use a GLib timer because otherwise GTK+ event handling during
    // dragging can starve WebCore timers, which have a lower priority.
    m_syncTimerCallbackId = g_timeout_add_full(GDK_PRIORITY_EVENTS, 0, reinterpret_cast<GSourceFunc>(syncLayersTimeoutCallback), this, 0);
}

void AcceleratedCompositingContext::syncLayersNow()
{
    if (m_rootGraphicsLayer)
        m_rootGraphicsLayer->syncCompositingStateForThisLayerOnly();

    core(m_webView)->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void AcceleratedCompositingContext::syncLayersTimeout()
{
    m_syncTimerCallbackId = 0;
    syncLayersNow();
    if (!m_rootGraphicsLayer)
        return;

    renderLayersToWindow(IntRect());

    if (toTextureMapperLayer(m_rootGraphicsLayer.get())->descendantsOrSelfHaveRunningAnimations())
        m_syncTimerCallbackId = g_timeout_add_full(GDK_PRIORITY_EVENTS, 1000.0 / 60.0, reinterpret_cast<GSourceFunc>(syncLayersTimeoutCallback), this, 0);
}

void AcceleratedCompositingContext::notifyAnimationStarted(const GraphicsLayer*, double time)
{

}
void AcceleratedCompositingContext::notifySyncRequired(const GraphicsLayer*)
{

}

void AcceleratedCompositingContext::paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& rectToPaint)
{
    cairo_t* cr = context.platformContext()->cr();
    copyRectFromCairoSurfaceToContext(m_webView->priv->backingStore->cairoSurface(), cr,
                                      IntSize(), rectToPaint);
}

bool AcceleratedCompositingContext::showDebugBorders(const GraphicsLayer*) const
{
    return false;
}

bool AcceleratedCompositingContext::showRepaintCounter(const GraphicsLayer*) const
{
    return false;
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER_GL)
