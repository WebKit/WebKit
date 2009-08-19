/*
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

#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "RenderObject.h"
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
    notImplemented();
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

void DragClient::startDrag(DragImageRef image, const IntPoint& dragImageOrigin, const IntPoint& eventPos, Clipboard*, Frame* frame, bool linkDrag)
{
    Element* targetElement = frame->document()->elementFromPoint(m_startPos.x(), m_startPos.y());
    bool imageDrag = false;

    if (targetElement)
        imageDrag = targetElement->renderer()->isImage();

    GdkAtom textHtml = gdk_atom_intern_static_string("text/html");
    GdkAtom netscapeUrl = gdk_atom_intern_static_string("_NETSCAPE_URL");

    GtkTargetList* targetList = gtk_target_list_new(NULL, 0);
    gtk_target_list_add(targetList, textHtml, 0, WEBKIT_WEB_VIEW_TARGET_INFO_HTML);
    gtk_target_list_add_text_targets(targetList, WEBKIT_WEB_VIEW_TARGET_INFO_TEXT);

    if (linkDrag || imageDrag) {
        gtk_target_list_add(targetList, netscapeUrl, 0, WEBKIT_WEB_VIEW_TARGET_INFO_NETSCAPE_URL);
        gtk_target_list_add_uri_targets(targetList, WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST);
    }

    if (imageDrag)
        gtk_target_list_add_image_targets(targetList, WEBKIT_WEB_VIEW_TARGET_INFO_IMAGE, false);

    GdkDragAction dragAction = GDK_ACTION_COPY;
    if (linkDrag) {
        dragAction = GDK_ACTION_LINK;
        if (imageDrag)
            dragAction = (GdkDragAction)(dragAction | GDK_ACTION_COPY);
    }

    GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
    reinterpret_cast<GdkEventButton*>(event)->window = gtk_widget_get_window(GTK_WIDGET(m_webView));
    reinterpret_cast<GdkEventButton*>(event)->time = GDK_CURRENT_TIME;

    GdkDragContext* context = gtk_drag_begin(GTK_WIDGET(m_webView),
                                             targetList, dragAction, 1, event);
    g_object_ref(context);

    if (image)
        gtk_drag_set_icon_pixbuf(context, image, eventPos.x() - dragImageOrigin.x(), eventPos.y() - dragImageOrigin.y());
    else
        gtk_drag_set_icon_default(context);

    gtk_target_list_unref(targetList);
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
