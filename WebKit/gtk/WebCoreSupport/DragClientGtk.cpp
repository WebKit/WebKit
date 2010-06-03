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
#include "GRefPtrGtk.h"
#include "NotImplemented.h"
#include "PasteboardHelper.h"
#include "RenderObject.h"
#include "webkitprivate.h"
#include "webkitwebview.h"

#include <gtk/gtk.h>
#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_widget_get_window(widget) (widget)->window
#endif

using namespace WebCore;

namespace WebKit {

DragClient::DragClient(WebKitWebView* webView)
    : m_webView(webView)
    , m_startPos(0, 0)
{
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

    GRefPtr<GtkTargetList> targetList(clipboardGtk->helper()->targetListForDataObject(dataObject.get()));
    GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
    // This will be decremented by gdk_event_free() below.
    event->button.window = static_cast<GdkWindow*>(g_object_ref(gtk_widget_get_window(GTK_WIDGET(m_webView))));
    event->button.time = GDK_CURRENT_TIME;

    GdkDragContext* context = gtk_drag_begin(GTK_WIDGET(m_webView), targetList.get(), dragOperationToGdkDragActions(clipboard->sourceOperation()), 1, event);
    webView->priv->draggingDataObjects.set(context, dataObject);

    if (image)
        gtk_drag_set_icon_pixbuf(context, image, eventPos.x() - dragImageOrigin.x(), eventPos.y() - dragImageOrigin.y());
    else
        gtk_drag_set_icon_default(context);

    gdk_event_free(event);
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
