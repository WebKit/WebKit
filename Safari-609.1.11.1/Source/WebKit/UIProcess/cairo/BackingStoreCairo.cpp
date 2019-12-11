/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011,2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BackingStore.h"

#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPageProxy.h"
#include <WebCore/BackingStoreBackendCairoImpl.h>
#include <WebCore/CairoUtilities.h>
#include <WebCore/GraphicsContextImplCairo.h>
#include <WebCore/PlatformContextCairo.h>
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>

#if PLATFORM(GTK) && PLATFORM(X11) && defined(GDK_WINDOWING_X11)
#include <WebCore/BackingStoreBackendCairoX11.h>
#include <WebCore/PlatformDisplayX11.h>
#include <gdk/gdkx.h>
#endif

namespace WebKit {
using namespace WebCore;

std::unique_ptr<BackingStoreBackendCairo> BackingStore::createBackend()
{
#if PLATFORM(GTK) && PLATFORM(X11) && defined(GDK_WINDOWING_X11)
    const auto& sharedDisplay = PlatformDisplay::sharedDisplay();
    if (is<PlatformDisplayX11>(sharedDisplay)) {
        GdkVisual* visual = gtk_widget_get_visual(m_webPageProxy.viewWidget());
        GdkScreen* screen = gdk_visual_get_screen(visual);
        ASSERT(downcast<PlatformDisplayX11>(sharedDisplay).native() == GDK_SCREEN_XDISPLAY(screen));
        return makeUnique<BackingStoreBackendCairoX11>(GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
            GDK_VISUAL_XVISUAL(visual), gdk_visual_get_depth(visual), m_size, m_deviceScaleFactor);
    }
#endif

    return makeUnique<BackingStoreBackendCairoImpl>(m_size, m_deviceScaleFactor);
}

void BackingStore::paint(cairo_t* context, const IntRect& rect)
{
    ASSERT(m_backend);

    cairo_save(context);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, m_backend->surface(), 0, 0);
    cairo_rectangle(context, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(context);
    cairo_restore(context);
}

void BackingStore::incorporateUpdate(ShareableBitmap* bitmap, const UpdateInfo& updateInfo)
{
    if (!m_backend)
        m_backend = createBackend();

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    // Paint all update rects.
    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();
    RefPtr<cairo_t> cairoContext = adoptRef(cairo_create(m_backend->surface()));
    GraphicsContext graphicsContext(GraphicsContextImplCairo::createFactory(cairoContext.get()));

    // When m_webPageProxy.drawsBackground() is false, bitmap contains transparent parts as a background of the webpage.
    // For such case, bitmap must be drawn using CompositeCopy to overwrite the existing surface.
    graphicsContext.setCompositeOperation(WebCore::CompositeCopy);

    for (const auto& updateRect : updateInfo.updateRects) {
        IntRect srcRect = updateRect;
        srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());
        bitmap->paint(graphicsContext, deviceScaleFactor(), updateRect.location(), srcRect);
    }
}

void BackingStore::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (scrollOffset.isZero())
        return;

    ASSERT(m_backend);
    m_backend->scroll(scrollRect, scrollOffset);
}

} // namespace WebKit
