/*
 * Copyright (C) Igalia S.L.
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
#include "DragClientGtk.h"

#include "ClipboardGtk.h"
#include "ClipboardUtilitiesGtk.h"
#include "DataObjectGtk.h"
#include "Document.h"
#include "DragController.h"
#include "Element.h"
#include "Frame.h"
#include "GOwnPtrGtk.h"
#include "GRefPtrGtk.h"
#include "GtkVersioning.h"
#include "NotImplemented.h"
#include "PasteboardHelper.h"
#include "RenderObject.h"
#include "webkitprivate.h"
#include "webkitwebview.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

static gboolean dragIconWindowExposeEventCallback(GtkWidget* widget, GdkEventExpose* event, DragClient* client)
{
    client->dragIconWindowExposeEvent(widget, event);
    return TRUE;
}

DragClient::DragClient(WebKitWebView* webView)
    : m_webView(webView)
    , m_startPos(0, 0)
    , m_dragIconWindow(gtk_window_new(GTK_WINDOW_POPUP))
{
    g_signal_connect(m_dragIconWindow.get(), "expose-event", G_CALLBACK(dragIconWindowExposeEventCallback), this);
}

DragClient::~DragClient()
{
    g_signal_handlers_disconnect_by_func(m_dragIconWindow.get(), (gpointer) dragIconWindowExposeEventCallback, this);
}

void DragClient::willPerformDragDestinationAction(DragDestinationAction, DragData*)
{
}

void DragClient::willPerformDragSourceAction(DragSourceAction, const IntPoint& startPos, Clipboard*)
{
    m_startPos = startPos;
}

DragDestinationAction DragClient::actionMaskForDrag(DragData*)
{
    notImplemented();
    return DragDestinationActionAny;
}

DragSourceAction DragClient::dragSourceActionMaskForPoint(const IntPoint&)
{
    notImplemented();
    return DragSourceActionAny;
}

void DragClient::startDrag(DragImageRef image, const IntPoint& dragImageOrigin, const IntPoint& eventPos, Clipboard* clipboard, Frame* frame, bool linkDrag)
{
    ClipboardGtk* clipboardGtk = reinterpret_cast<ClipboardGtk*>(clipboard);

    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));
    RefPtr<DataObjectGtk> dataObject = clipboardGtk->dataObject();
    PlatformRefPtr<GtkTargetList> targetList(clipboardGtk->helper()->targetListForDataObject(dataObject.get()));
    GOwnPtr<GdkEvent> currentEvent(gtk_get_current_event());

    GdkDragContext* context = gtk_drag_begin(GTK_WIDGET(m_webView), targetList.get(), dragOperationToGdkDragActions(clipboard->sourceOperation()), 1, currentEvent.get());
    webView->priv->draggingDataObjects.set(context, dataObject);

    // A drag starting should prevent a double-click from happening. This might
    // happen if a drag is followed very quickly by another click (like in the DRT).
    webView->priv->previousClickTime = 0;

    // This strategy originally comes from Chromium:
    // src/chrome/browser/gtk/tab_contents_drag_source.cc
    if (image) {
        m_dragImage = image;
        IntSize imageSize(cairo_image_surface_get_width(image), cairo_image_surface_get_height(image));
        gtk_window_resize(GTK_WINDOW(m_dragIconWindow.get()), imageSize.width(), imageSize.height());

        if (!gtk_widget_get_realized(m_dragIconWindow.get())) {
            GdkScreen* screen = gtk_widget_get_screen(m_dragIconWindow.get());
            GdkColormap* rgba = gdk_screen_get_rgba_colormap(screen);
            if (rgba)
                gtk_widget_set_colormap(m_dragIconWindow.get(), rgba);
        }

        IntSize origin = eventPos - dragImageOrigin;
        gtk_drag_set_icon_widget(context, m_dragIconWindow.get(),
                                 origin.width(), origin.height());
    } else
        gtk_drag_set_icon_default(context);
}

void DragClient::dragIconWindowExposeEvent(GtkWidget* widget, GdkEventExpose* event)
{
    PlatformRefPtr<cairo_t> context = adoptPlatformRef(gdk_cairo_create(event->window));
    cairo_rectangle(context.get(), 0, 0,
                    cairo_image_surface_get_width(m_dragImage.get()),
                    cairo_image_surface_get_height(m_dragImage.get()));
    cairo_set_operator(context.get(), CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context.get(), m_dragImage.get(), 0, 0);
    cairo_fill(context.get());
}

DragImageRef DragClient::createDragImageForLink(KURL&, const String&, Frame*)
{
    notImplemented();
    return 0;
}

void DragClient::dragControllerDestroyed()
{
    delete this;
}
}
