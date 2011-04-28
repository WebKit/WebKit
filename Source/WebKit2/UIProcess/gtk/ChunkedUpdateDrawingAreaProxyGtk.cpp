/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L
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
#include "ChunkedUpdateDrawingAreaProxy.h"

#include "RefPtrCairo.h"
#include "UpdateChunk.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebProcessProxy.h"
#include <WebCore/GtkVersioning.h>
#include <gdk/gdk.h>

using namespace WebCore;

namespace WebKit {

WebPageProxy* ChunkedUpdateDrawingAreaProxy::page()
{
    return webkitWebViewBaseGetPage(m_webView);
}

void ChunkedUpdateDrawingAreaProxy::ensureBackingStore()
{
    if (m_backingStoreImage)
        return;

    m_backingStoreImage = gdk_window_create_similar_surface(gtk_widget_get_window(GTK_WIDGET(m_webView)),
                                                            CAIRO_CONTENT_COLOR_ALPHA, size().width(), size().height());
}

void ChunkedUpdateDrawingAreaProxy::invalidateBackingStore()
{
    if (m_backingStoreImage) {
        cairo_surface_destroy(m_backingStoreImage);
        m_backingStoreImage = 0;
    }
}

bool ChunkedUpdateDrawingAreaProxy::platformPaint(const IntRect& rect, cairo_t* cr)
{
    if (!m_backingStoreImage)
        return false;

    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_source_surface(cr, m_backingStoreImage, 0, 0);
    cairo_fill(cr);

    return true;
}

void ChunkedUpdateDrawingAreaProxy::drawUpdateChunkIntoBackingStore(UpdateChunk* updateChunk)
{
    ensureBackingStore();

    RefPtr<cairo_surface_t> pixmap(updateChunk->createImage());
    if (cairo_surface_status(pixmap.get()) != CAIRO_STATUS_SUCCESS)
        return;

    const IntRect& updateChunkRect = updateChunk->rect();

    RefPtr<cairo_t> cr = cairo_create(m_backingStoreImage);
    cairo_set_source_surface(cr.get(), pixmap.get(), updateChunkRect.x(), updateChunkRect.y());
    cairo_paint(cr.get());

    gtk_widget_queue_draw_area(GTK_WIDGET(m_webView), updateChunkRect.x(), updateChunkRect.y(),
                               updateChunkRect.width(), updateChunkRect.height());
}

} // namespace WebKit
