/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragSource.h"

#if ENABLE(DRAG_SUPPORT) && USE(GTK4)

#include "WebKitWebViewBasePrivate.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/PasteboardCustomData.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

DragSource::DragSource(GtkWidget* webView)
    : m_webView(webView)
{
}

DragSource::~DragSource()
{
}

void DragSource::begin(SelectionData&& selectionData, OptionSet<DragOperation> operationMask, RefPtr<ShareableBitmap>&& image)
{
    if (m_drag) {
        gdk_drag_drop_done(m_drag.get(), FALSE);
        m_drag = nullptr;
    }

    m_selectionData = WTFMove(selectionData);

    Vector<GdkContentProvider*> providers;
    if (m_selectionData->hasMarkup()) {
        CString markup = m_selectionData->markup().utf8();
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(markup.data(), markup.length()));
        providers.append(gdk_content_provider_new_for_bytes("text/html", bytes.get()));
    }

    if (m_selectionData->hasURIList()) {
        CString uriList = m_selectionData->uriList().utf8();
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(uriList.data(), uriList.length()));
        providers.append(gdk_content_provider_new_for_bytes("text/uri-list", bytes.get()));
    }

    if (m_selectionData->hasURL()) {
        CString urlString = m_selectionData->url().string().utf8();
        gchar* url = g_strdup_printf("%s\n%s", urlString.data(), m_selectionData->hasText() ? m_selectionData->text().utf8().data() : urlString.data());
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_take(url, strlen(url)));
        providers.append(gdk_content_provider_new_for_bytes("_NETSCAPE_URL", bytes.get()));
    }

    if (m_selectionData->hasImage()) {
        GRefPtr<GdkPixbuf> pixbuf = adoptGRef(m_selectionData->image()->getGdkPixbuf());
        providers.append(gdk_content_provider_new_typed(GDK_TYPE_PIXBUF, pixbuf.get()));
    }

    if (m_selectionData->hasText())
        providers.append(gdk_content_provider_new_typed(G_TYPE_STRING, m_selectionData->text().utf8().data()));

    if (m_selectionData->canSmartReplace()) {
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(nullptr, 0));
        providers.append(gdk_content_provider_new_for_bytes("application/vnd.webkitgtk.smartpaste", bytes.get()));
    }

    if (m_selectionData->hasCustomData()) {
        GRefPtr<GBytes> bytes = m_selectionData->customData()->createGBytes();
        providers.append(gdk_content_provider_new_for_bytes(PasteboardCustomData::gtkType(), bytes.get()));
    }

    auto* surface = gtk_native_get_surface(gtk_widget_get_native(m_webView));
    auto* device = gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(m_webView)));
    GRefPtr<GdkContentProvider> provider = adoptGRef(gdk_content_provider_new_union(providers.data(), providers.size()));
    m_drag = adoptGRef(gdk_drag_begin(surface, device, provider.get(), dragOperationToGdkDragActions(operationMask), 0, 0));
    g_signal_connect(m_drag.get(), "dnd-finished", G_CALLBACK(+[](GdkDrag* gtkDrag, gpointer userData) {
        auto& drag = *static_cast<DragSource*>(userData);
        if (drag.m_drag.get() != gtkDrag)
            return;

        drag.m_selectionData = WTF::nullopt;
        drag.m_drag = nullptr;

        GdkDevice* device = gdk_drag_get_device(gtkDrag);
        double x = 0;
        double y = 0;
        gdk_device_get_surface_at_position(device, &x, &y);

        auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(drag.m_webView));
        ASSERT(page);

        IntPoint point(x, y);
        page->dragEnded(point, point, gdkDragActionToDragOperation(gdk_drag_get_selected_action(gtkDrag)));
    }), this);

    g_signal_connect(m_drag.get(), "cancel", G_CALLBACK(+[](GdkDrag* gtkDrag, GdkDragCancelReason, gpointer userData) {
        auto& drag = *static_cast<DragSource*>(userData);
        if (drag.m_drag.get() != gtkDrag)
            return;

        drag.m_selectionData = WTF::nullopt;
        drag.m_drag = nullptr;
    }), this);

    auto* dragIcon = gtk_drag_icon_get_for_drag(m_drag.get());
    RefPtr<Image> iconImage = image ? image->createImage() : nullptr;
    if (iconImage) {
        if (GRefPtr<GdkTexture> texture = adoptGRef(iconImage->gdkTexture())) {
            gdk_drag_set_hotspot(m_drag.get(), -gdk_texture_get_width(texture.get()) / 2, -gdk_texture_get_height(texture.get()) / 2);
            auto* picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture.get()));
            gtk_drag_icon_set_child(GTK_DRAG_ICON(dragIcon), picture);
            return;
        }
    }

    gdk_drag_set_hotspot(m_drag.get(), -2, -2);
    auto* child = gtk_image_new_from_icon_name("text-x-generic");
    gtk_image_set_icon_size(GTK_IMAGE(child), GTK_ICON_SIZE_LARGE);
    gtk_drag_icon_set_child(GTK_DRAG_ICON(dragIcon), child);
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT) && USE(GTK4)
