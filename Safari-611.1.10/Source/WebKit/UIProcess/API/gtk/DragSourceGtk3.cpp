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

#if ENABLE(DRAG_SUPPORT) && !USE(GTK4)

#include "WebKitWebViewBasePrivate.h"
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/PasteboardCustomData.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

enum DragTargetType { Markup, Text, Image, URIList, NetscapeURL, SmartPaste, Custom };

DragSource::DragSource(GtkWidget* webView)
    : m_webView(webView)
{
    g_signal_connect(m_webView, "drag-data-get", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, GtkSelectionData* data, guint info, guint, gpointer userData) {
        auto& drag = *static_cast<DragSource*>(userData);
        if (drag.m_drag.get() != context)
            return;

        switch (info) {
        case DragTargetType::Text:
            gtk_selection_data_set_text(data, drag.m_selectionData->text().utf8().data(), -1);
            break;
        case DragTargetType::Markup: {
            CString markup = drag.m_selectionData->markup().utf8();
            gtk_selection_data_set(data, gdk_atom_intern_static_string("text/html"), 8, reinterpret_cast<const guchar*>(markup.data()), markup.length());
            break;
        }
        case DragTargetType::URIList: {
            CString uriList = drag.m_selectionData->uriList().utf8();
            gtk_selection_data_set(data, gdk_atom_intern_static_string("text/uri-list"), 8, reinterpret_cast<const guchar*>(uriList.data()), uriList.length());
            break;
        }
        case DragTargetType::NetscapeURL: {
            CString urlString = drag.m_selectionData->url().string().utf8();
            GUniquePtr<gchar> url(g_strdup_printf("%s\n%s", urlString.data(), drag.m_selectionData->hasText() ? drag.m_selectionData->text().utf8().data() : urlString.data()));
            gtk_selection_data_set(data, gdk_atom_intern_static_string("_NETSCAPE_URL"), 8, reinterpret_cast<const guchar*>(url.get()), strlen(url.get()));
            break;
        }
        case DragTargetType::Image: {
            GRefPtr<GdkPixbuf> pixbuf = adoptGRef(drag.m_selectionData->image()->getGdkPixbuf());
            gtk_selection_data_set_pixbuf(data, pixbuf.get());
            break;
        }
        case DragTargetType::SmartPaste:
            gtk_selection_data_set_text(data, "", -1);
            break;
        case DragTargetType::Custom: {
            auto* buffer = drag.m_selectionData->customData();
            gtk_selection_data_set(data, gdk_atom_intern_static_string(PasteboardCustomData::gtkType()), 8, reinterpret_cast<const guchar*>(buffer->data()), buffer->size());
            break;
        }
        }
    }), this);

    g_signal_connect(m_webView, "drag-end", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gpointer userData) {
        auto& drag = *static_cast<DragSource*>(userData);
        if (drag.m_drag.get() != context)
            return;
        if (!drag.m_selectionData)
            return;

        drag.m_selectionData = WTF::nullopt;
        drag.m_drag = nullptr;

        GdkDevice* device = gdk_drag_context_get_device(context);
        int x = 0;
        int y = 0;
        gdk_device_get_window_at_position(device, &x, &y);
        int xRoot = 0;
        int yRoot = 0;
        gdk_device_get_position(device, nullptr, &xRoot, &yRoot);

        auto* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(drag.m_webView));
        ASSERT(page);
        page->dragEnded({ x, y }, { xRoot, yRoot }, gdkDragActionToDragOperation(gdk_drag_context_get_selected_action(context)));
    }), this);
}

DragSource::~DragSource()
{
    g_signal_handlers_disconnect_by_data(m_webView, this);
}

void DragSource::begin(SelectionData&& selectionData, OptionSet<DragOperation> operationMask, RefPtr<ShareableBitmap>&& image)
{
    if (m_drag) {
        gtk_drag_cancel(m_drag.get());
        m_drag = nullptr;
    }

    m_selectionData = WTFMove(selectionData);

    GRefPtr<GtkTargetList> list = adoptGRef(gtk_target_list_new(nullptr, 0));
    if (m_selectionData->hasText())
        gtk_target_list_add_text_targets(list.get(), DragTargetType::Text);
    if (m_selectionData->hasMarkup())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("text/html"), 0, DragTargetType::Markup);
    if (m_selectionData->hasURIList())
        gtk_target_list_add_uri_targets(list.get(), DragTargetType::URIList);
    if (m_selectionData->hasURL())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("_NETSCAPE_URL"), 0, DragTargetType::NetscapeURL);
    if (m_selectionData->hasImage())
        gtk_target_list_add_image_targets(list.get(), DragTargetType::Image, TRUE);
    if (m_selectionData->canSmartReplace())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string("application/vnd.webkitgtk.smartpaste"), 0, DragTargetType::SmartPaste);
    if (m_selectionData->hasCustomData())
        gtk_target_list_add(list.get(), gdk_atom_intern_static_string(PasteboardCustomData::gtkType()), 0, DragTargetType::Custom);

    m_drag = gtk_drag_begin_with_coordinates(m_webView, list.get(), dragOperationToGdkDragActions(operationMask), GDK_BUTTON_PRIMARY, nullptr, -1, -1);
    if (image) {
        RefPtr<cairo_surface_t> imageSurface(image->createCairoSurface());
        // Use the center of the drag image as hotspot.
        cairo_surface_set_device_offset(imageSurface.get(), -cairo_image_surface_get_width(imageSurface.get()) / 2, -cairo_image_surface_get_height(imageSurface.get()) / 2);
        gtk_drag_set_icon_surface(m_drag.get(), imageSurface.get());
    } else
        gtk_drag_set_icon_default(m_drag.get());
}

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT) && !USE(GTK4)
