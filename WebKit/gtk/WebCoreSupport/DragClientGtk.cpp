/*
 * Copyright (C) 2010 Igalia S.L.
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
#include "webkitprivate.h"
#include "webkitwebview.h"

#include <gtk/gtk.h>
#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_widget_get_window(widget) (widget)->window
#endif

using namespace WebCore;

namespace WebKit {

static gboolean buttonPressEvent(GtkWidget* widget, GdkEventButton* event, DragClient* client)
{
    client->setLastButtonPressEvent(gdk_event_copy(reinterpret_cast<GdkEvent*>(event)));
    return FALSE;
}

DragClient::DragClient(WebKitWebView* webView)
    : m_webView(webView)
    , m_startPos(0, 0)
{
    g_signal_connect(webView, "button-press-event", G_CALLBACK(buttonPressEvent), this);
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
    RefPtr<DataObjectGtk> dataObject = clipboardGtk->dataObject();
    GRefPtr<GtkTargetList> targetList(clipboardGtk->helper()->targetListForDataObject(dataObject.get()));

    // The DRT does not use the GTK+ event queue for mouse down and motion
    // events. Instead of using the gtk_get_current_event, we listen for mouse
    // button-down-event signals on the WebView and use those events instead.
    if (!m_lastButtonPressEvent)
        return;

    GdkDragContext* context = gtk_drag_begin(GTK_WIDGET(m_webView), targetList.get(), dragOperationToGdkDragActions(clipboard->sourceOperation()), 1, m_lastButtonPressEvent.get());
    setLastButtonPressEvent(0);

    if (!context)
        return;

    m_webView->priv->draggingDataObjects.set(context, dataObject);

    if (image)
        gtk_drag_set_icon_pixbuf(context, image, eventPos.x() - dragImageOrigin.x(), eventPos.y() - dragImageOrigin.y());
    else
        gtk_drag_set_icon_default(context);
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
