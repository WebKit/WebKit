/*
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
 * Copyright (C) Igalia S.L.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include "config.h"
#include "PasteboardHelper.h"

#include "Chrome.h"
#include "DataObjectGtk.h"
#include "Frame.h"
#include "Page.h"
#include "Pasteboard.h"
#include "TextResourceDecoder.h"
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebCore {

static GdkAtom gdkMarkupAtom = gdk_atom_intern("text/html", FALSE);
static GdkAtom netscapeURLAtom = gdk_atom_intern("_NETSCAPE_URL", FALSE);
static GdkAtom uriListAtom = gdk_atom_intern("text/uri-list", FALSE);

PasteboardHelper::PasteboardHelper()
    : m_targetList(gtk_target_list_new(0, 0))
{
}

PasteboardHelper::~PasteboardHelper()
{
    gtk_target_list_unref(m_targetList);
}


void PasteboardHelper::initializeTargetList()
{
    gtk_target_list_add_text_targets(m_targetList, getIdForTargetType(TargetTypeText));
    gtk_target_list_add(m_targetList, gdkMarkupAtom, 0, getIdForTargetType(TargetTypeMarkup));
    gtk_target_list_add_uri_targets(m_targetList, getIdForTargetType(TargetTypeURIList));
    gtk_target_list_add(m_targetList, netscapeURLAtom, 0, getIdForTargetType(TargetTypeNetscapeURL));
}

static inline GtkWidget* widgetFromFrame(Frame* frame)
{
    ASSERT(frame);
    Page* page = frame->page();
    ASSERT(page);
    Chrome* chrome = page->chrome();
    ASSERT(chrome);
    PlatformPageClient client = chrome->platformPageClient();
    ASSERT(client);
    return client;
}

GtkClipboard* PasteboardHelper::getCurrentClipboard(Frame* frame)
{
    if (usePrimarySelectionClipboard(widgetFromFrame(frame)))
        return getPrimarySelectionClipboard(frame);
    return getClipboard(frame);
}

GtkClipboard* PasteboardHelper::getClipboard(Frame* frame) const
{
    return gtk_widget_get_clipboard(widgetFromFrame(frame), GDK_SELECTION_CLIPBOARD);
}

GtkClipboard* PasteboardHelper::getPrimarySelectionClipboard(Frame* frame) const
{
    return gtk_widget_get_clipboard(widgetFromFrame(frame), GDK_SELECTION_PRIMARY);
}

GtkTargetList* PasteboardHelper::targetList() const
{
    return m_targetList;
}

static Vector<KURL> urisToKURLVector(gchar** uris)
{
    ASSERT(uris);

    Vector<KURL> uriList;
    for (int i = 0; *(uris + i); i++)
        uriList.append(KURL(KURL(), *(uris + i)));

    return uriList;
}

void PasteboardHelper::getClipboardContents(GtkClipboard* clipboard)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    ASSERT(dataObject);

    if (gtk_clipboard_wait_is_text_available(clipboard)) {
        GOwnPtr<gchar> textData(gtk_clipboard_wait_for_text(clipboard));
        if (textData)
            dataObject->setText(String::fromUTF8(textData.get()));
    }

    if (gtk_clipboard_wait_is_target_available(clipboard, gdkMarkupAtom)) {
       if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, gdkMarkupAtom)) {
          RefPtr<TextResourceDecoder> decoder(TextResourceDecoder::create("text/plain", "UTF-8", true));
          String markup(decoder->decode(reinterpret_cast<char*>(data->data), data->length));
          markup += decoder->flush();
          dataObject->setMarkup(markup);
          gtk_selection_data_free(data);
       }
    }

    if (gtk_clipboard_wait_is_target_available(clipboard, uriListAtom)) {
        if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, uriListAtom)) {
            gchar** uris = gtk_selection_data_get_uris(data);
            if (uris) {
                dataObject->setURIList(urisToKURLVector(uris));
                g_strfreev(uris);
            }
            gtk_selection_data_free(data);
        }
    }
}

void PasteboardHelper::fillSelectionData(GtkSelectionData* selectionData, guint info, DataObjectGtk* dataObject)
{
    if (info == getIdForTargetType(TargetTypeText))
        gtk_selection_data_set_text(selectionData, dataObject->text().utf8().data(), -1);

    else if (info == getIdForTargetType(TargetTypeMarkup)) {
        GOwnPtr<gchar> markup(g_strdup(dataObject->markup().utf8().data()));
        gtk_selection_data_set(selectionData, selectionData->target, 8,
            reinterpret_cast<const guchar*>(markup.get()), strlen(markup.get()));

    } else if (info == getIdForTargetType(TargetTypeURIList)) {
        Vector<KURL> uriList(dataObject->uriList());
        gchar** uris = g_new0(gchar*, uriList.size() + 1);
        for (size_t i = 0; i < uriList.size(); i++)
            uris[i] = g_strdup(uriList[i].string().utf8().data());

        gtk_selection_data_set_uris(selectionData, uris);
        g_strfreev(uris);

    } else if (info == getIdForTargetType(TargetTypeNetscapeURL) && dataObject->hasURL()) {
        String url(dataObject->url());
        String result(url);
        result.append("\n");

        if (dataObject->hasText())
            result.append(dataObject->text());
        else
            result.append(url);

        GOwnPtr<gchar> resultData(g_strdup(result.utf8().data()));
        gtk_selection_data_set(selectionData, selectionData->target, 8,
            reinterpret_cast<const guchar*>(resultData.get()), strlen(resultData.get()));

    } else if (info == getIdForTargetType(TargetTypeImage))
        gtk_selection_data_set_pixbuf(selectionData, dataObject->image());
}

GtkTargetList* PasteboardHelper::targetListForDataObject(DataObjectGtk* dataObject)
{
    GtkTargetList* list = gtk_target_list_new(0, 0);

    if (dataObject->hasText())
        gtk_target_list_add_text_targets(list, getIdForTargetType(TargetTypeText));

    if (dataObject->hasMarkup())
        gtk_target_list_add(list, gdkMarkupAtom, 0, getIdForTargetType(TargetTypeMarkup));

    if (dataObject->hasURIList()) {
        gtk_target_list_add_uri_targets(list, getIdForTargetType(TargetTypeURIList));
        gtk_target_list_add(list, netscapeURLAtom, 0, getIdForTargetType(TargetTypeNetscapeURL));
    }

    if (dataObject->hasImage())
        gtk_target_list_add_image_targets(list, getIdForTargetType(TargetTypeImage), TRUE);

    return list;
}

static DataObjectGtk* settingClipboardDataObject = 0;

static void getClipboardContentsCallback(GtkClipboard* clipboard, GtkSelectionData *selectionData, guint info, gpointer data)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    ASSERT(dataObject);
    Pasteboard::generalPasteboard()->helper()->fillSelectionData(selectionData, info, dataObject);
}

static void clearClipboardContentsCallback(GtkClipboard* clipboard, gpointer data)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    ASSERT(dataObject);

    // Only clear the DataObject for this clipboard if we are not currently setting it.
    if (dataObject != settingClipboardDataObject)
        dataObject->clear();

    if (!data)
        return;

    GClosure* callback = static_cast<GClosure*>(data);
    GValue firstArgument = {0, {{0}}};
    g_value_init(&firstArgument, G_TYPE_POINTER);
    g_value_set_pointer(&firstArgument, clipboard);
    g_closure_invoke(callback, 0, 1, &firstArgument, 0);
    g_closure_unref(callback);
}

void PasteboardHelper::writeClipboardContents(GtkClipboard* clipboard, GClosure* callback)
{
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    GtkTargetList* list = targetListForDataObject(dataObject);

    int numberOfTargets;
    GtkTargetEntry* table = gtk_target_table_new_from_list(list, &numberOfTargets);

    if (numberOfTargets > 0 && table) {
        settingClipboardDataObject = dataObject;

        gtk_clipboard_set_with_data(clipboard, table, numberOfTargets,
            getClipboardContentsCallback, clearClipboardContentsCallback, callback);

        settingClipboardDataObject = 0;

    } else
        gtk_clipboard_clear(clipboard);

    if (table)
        gtk_target_table_free(table, numberOfTargets);
    gtk_target_list_unref(list);
}

}

