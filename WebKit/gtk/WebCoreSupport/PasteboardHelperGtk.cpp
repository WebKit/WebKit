/*
 *  Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 *  Copyright (C) 2009 Holger Hans Peter Freyther
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
#include "PasteboardHelperGtk.h"

#include "Frame.h"
#include "webkitwebframe.h"
#include "webkitwebview.h"
#include "webkitprivate.h"

#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

GtkClipboard* PasteboardHelperGtk::getCurrentTarget(Frame* frame) const
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));

    if (webkit_web_view_use_primary_for_paste(webView))
        return getPrimary(frame);
    else
        return getClipboard(frame);
}

GtkClipboard* PasteboardHelperGtk::getClipboard(Frame* frame) const
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));
    return gtk_widget_get_clipboard(GTK_WIDGET (webView),
                                    GDK_SELECTION_CLIPBOARD);
}

GtkClipboard* PasteboardHelperGtk::getPrimary(Frame* frame) const
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));
    return gtk_widget_get_clipboard(GTK_WIDGET (webView),
                                    GDK_SELECTION_PRIMARY);
}

GtkTargetList* PasteboardHelperGtk::getCopyTargetList(Frame* frame) const
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));
    return webkit_web_view_get_copy_target_list(webView);
}

GtkTargetList* PasteboardHelperGtk::getPasteTargetList(Frame* frame) const
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(kit(frame));
    return webkit_web_view_get_paste_target_list(webView);
}

}
