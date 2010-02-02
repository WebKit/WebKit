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

#include "DataObjectGtk.h"
#include "FocusController.h"
#include "Frame.h"
#include <gtk/gtk.h>
#include "webkitwebframe.h"
#include "webkitwebview.h"
#include "webkitprivate.h"

using namespace WebCore;

namespace WebKit {

static GdkAtom gdkMarkupAtom = gdk_atom_intern("text/html", FALSE);

PasteboardHelperGtk::PasteboardHelperGtk()
    : m_targetList(gtk_target_list_new(0, 0))
{
    gtk_target_list_add_text_targets(m_targetList, WEBKIT_WEB_VIEW_TARGET_INFO_TEXT);
    gtk_target_list_add(m_targetList, gdkMarkupAtom, 0, WEBKIT_WEB_VIEW_TARGET_INFO_HTML);
}

PasteboardHelperGtk::~PasteboardHelperGtk()
{
    gtk_target_list_unref(m_targetList);
}

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

GtkTargetList* PasteboardHelperGtk::targetList() const
{
    return m_targetList;
}

gint PasteboardHelperGtk::getWebViewTargetInfoHtml() const
{
    return WEBKIT_WEB_VIEW_TARGET_INFO_HTML;
}

static void fillSelectionData(GtkSelectionData* selectionData, guint info, DataObjectGtk* dataObject)
{
    if (info == WEBKIT_WEB_VIEW_TARGET_INFO_TEXT)
        gtk_selection_data_set_text(selectionData, dataObject->text().utf8().data(), -1);
    else if (info == WEBKIT_WEB_VIEW_TARGET_INFO_HTML) {
        GOwnPtr<gchar> markup(g_strdup(dataObject->markup().utf8().data()));
        gtk_selection_data_set(selectionData, selectionData->target, 8,
                               reinterpret_cast<const guchar*>(markup.get()),
                               strlen(markup.get()));
    }
}

static GtkTargetList* targetListForDataObject(DataObjectGtk* dataObject)
{
    GtkTargetList* list = gtk_target_list_new(0, 0);

    if (dataObject->hasText())
        gtk_target_list_add_text_targets(list, WEBKIT_WEB_VIEW_TARGET_INFO_TEXT);

    if (dataObject->hasMarkup())
        gtk_target_list_add(list, gdkMarkupAtom, 0, WEBKIT_WEB_VIEW_TARGET_INFO_HTML);

    return list;
}

static DataObjectGtk* settingClipboardDataObject = 0;
static gpointer settingClipboardData = 0;
static void getClipboardContentsCallback(GtkClipboard* clipboard, GtkSelectionData *selectionData, guint info, gpointer data)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    ASSERT(dataObject);
    fillSelectionData(selectionData, info, dataObject);
}

static void clearClipboardContentsCallback(GtkClipboard* clipboard, gpointer data)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    ASSERT(dataObject);

    // Only clear the DataObject for this clipboard if we are not currently setting it.
    if (dataObject != settingClipboardDataObject)
        dataObject->clear();

    // Only collapse the selection if this is an X11 primary clipboard
    // and we aren't currently setting the clipboard for this WebView.
    if (!data || data == settingClipboardData)
        return;

    WebKitWebView* webView = reinterpret_cast<WebKitWebView*>(data);
    WebCore::Page* corePage = core(webView);

    if (!corePage || !corePage->focusController()) {
        g_object_unref(webView);
        return;
    }

    Frame* frame = corePage->focusController()->focusedOrMainFrame();

    // Collapse the selection without clearing it
    ASSERT(frame);
    frame->selection()->setBase(frame->selection()->extent(), frame->selection()->affinity());

    g_object_unref(webView);
}

void PasteboardHelperGtk::writeClipboardContents(GtkClipboard* clipboard, gpointer data)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    GtkTargetList* list = targetListForDataObject(dataObject);

    int numberOfTargets;
    GtkTargetEntry* table = gtk_target_table_new_from_list(list, &numberOfTargets);

    if (numberOfTargets > 0 && table) {
        settingClipboardDataObject = dataObject;
        settingClipboardData = data;

        // Protect the web view from being destroyed before one of the clipboard callbacks
        // is called. Balanced in both getClipboardContentsCallback and
        // clearClipboardContentsCallback.
        WebKitWebView* webView = static_cast<WebKitWebView*>(data);
        g_object_ref(webView);

        gboolean succeeded = gtk_clipboard_set_with_data(clipboard, table, numberOfTargets,
                                                         getClipboardContentsCallback,
                                                         clearClipboardContentsCallback, data);
        if (!succeeded)
            g_object_unref(webView);

        settingClipboardDataObject = 0;
        settingClipboardData = 0;
    } else
        gtk_clipboard_clear(clipboard);

    if (table)
        gtk_target_table_free(table, numberOfTargets);

    gtk_target_list_unref(list);
}

}
