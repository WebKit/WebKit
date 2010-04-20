/*
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
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
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebCore {

static GdkAtom gdkMarkupAtom = gdk_atom_intern("text/html", FALSE);

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

void PasteboardHelper::fillSelectionData(GtkSelectionData* selectionData, guint info, DataObjectGtk* dataObject)
{
    if (info == getIdForTargetType(TargetTypeText))
        gtk_selection_data_set_text(selectionData, dataObject->text().utf8().data(), -1);
    else if (info == getIdForTargetType(TargetTypeMarkup)) {
        GOwnPtr<gchar> markup(g_strdup(dataObject->markup().utf8().data()));
        gtk_selection_data_set(selectionData, selectionData->target, 8,
                               reinterpret_cast<const guchar*>(markup.get()),
                               strlen(markup.get()));
    }
}

GtkTargetList* PasteboardHelper::targetListForDataObject(DataObjectGtk* dataObject)
{
    GtkTargetList* list = gtk_target_list_new(0, 0);

    if (dataObject->hasText())
        gtk_target_list_add_text_targets(list, getIdForTargetType(TargetTypeText));

    if (dataObject->hasMarkup())
        gtk_target_list_add(list, gdkMarkupAtom, 0, getIdForTargetType(TargetTypeMarkup));

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

