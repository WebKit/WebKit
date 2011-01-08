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
#include "GtkVersioning.h"
#include "Page.h"
#include "Pasteboard.h"
#include "TextResourceDecoder.h"
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebCore {

static GdkAtom textPlainAtom;
static GdkAtom markupAtom;
static GdkAtom netscapeURLAtom;
static GdkAtom uriListAtom;
static String gMarkupPrefix;

static void removeMarkupPrefix(String& markup)
{

    // The markup prefix is not harmful, but we remove it from the string anyway, so that
    // we can have consistent results with other ports during the layout tests.
    if (markup.startsWith(gMarkupPrefix))
        markup.remove(0, gMarkupPrefix.length());
}

static void initGdkAtoms()
{
    static gboolean initialized = FALSE;

    if (initialized)
        return;

    initialized = TRUE;

    textPlainAtom = gdk_atom_intern("text/plain;charset=utf-8", FALSE);
    markupAtom = gdk_atom_intern("text/html", FALSE);
    netscapeURLAtom = gdk_atom_intern("_NETSCAPE_URL", FALSE);
    uriListAtom = gdk_atom_intern("text/uri-list", FALSE);
    gMarkupPrefix = "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">";
}

PasteboardHelper::PasteboardHelper()
    : m_targetList(gtk_target_list_new(0, 0))
{
    initGdkAtoms();
}

PasteboardHelper::~PasteboardHelper()
{
    gtk_target_list_unref(m_targetList);
}

void PasteboardHelper::initializeTargetList()
{
    gtk_target_list_add_text_targets(m_targetList, getIdForTargetType(TargetTypeText));
    gtk_target_list_add(m_targetList, markupAtom, 0, getIdForTargetType(TargetTypeMarkup));
    gtk_target_list_add_uri_targets(m_targetList, getIdForTargetType(TargetTypeURIList));
    gtk_target_list_add(m_targetList, netscapeURLAtom, 0, getIdForTargetType(TargetTypeNetscapeURL));
    gtk_target_list_add_image_targets(m_targetList, getIdForTargetType(TargetTypeImage), TRUE);
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

static String selectionDataToUTF8String(GtkSelectionData* data)
{
    // g_strndup guards against selection data that is not null-terminated.
    GOwnPtr<gchar> markupString(g_strndup(reinterpret_cast<const char*>(gtk_selection_data_get_data(data)), gtk_selection_data_get_length(data)));
    return String::fromUTF8(markupString.get());
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

    if (gtk_clipboard_wait_is_target_available(clipboard, markupAtom)) {
        if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, markupAtom)) {
            String markup(selectionDataToUTF8String(data));
            removeMarkupPrefix(markup);
            dataObject->setMarkup(markup);
            gtk_selection_data_free(data);
        }
    }

    if (gtk_clipboard_wait_is_target_available(clipboard, uriListAtom)) {
        if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, uriListAtom)) {
            dataObject->setURIList(selectionDataToUTF8String(data));
            gtk_selection_data_free(data);
        }
    }
}

void PasteboardHelper::fillSelectionData(GtkSelectionData* selectionData, guint info, DataObjectGtk* dataObject)
{
    if (info == getIdForTargetType(TargetTypeText))
        gtk_selection_data_set_text(selectionData, dataObject->text().utf8().data(), -1);

    else if (info == getIdForTargetType(TargetTypeMarkup)) {
        // Some Linux applications refuse to accept pasted markup unless it is
        // prefixed by a content-type meta tag.
        CString markup = (gMarkupPrefix + dataObject->markup()).utf8();
        gtk_selection_data_set(selectionData, markupAtom, 8,
            reinterpret_cast<const guchar*>(markup.data()), markup.length() + 1);

    } else if (info == getIdForTargetType(TargetTypeURIList)) {
        CString uriList = dataObject->uriList().utf8();
        gtk_selection_data_set(selectionData, uriListAtom, 8,
            reinterpret_cast<const guchar*>(uriList.data()), uriList.length() + 1);

    } else if (info == getIdForTargetType(TargetTypeNetscapeURL) && dataObject->hasURL()) {
        String url(dataObject->url());
        String result(url);
        result.append("\n");

        if (dataObject->hasText())
            result.append(dataObject->text());
        else
            result.append(url);

        GOwnPtr<gchar> resultData(g_strdup(result.utf8().data()));
        gtk_selection_data_set(selectionData, netscapeURLAtom, 8,
            reinterpret_cast<const guchar*>(resultData.get()), strlen(resultData.get()) + 1);

    } else if (info == getIdForTargetType(TargetTypeImage))
        gtk_selection_data_set_pixbuf(selectionData, dataObject->image());
}

GtkTargetList* PasteboardHelper::targetListForDataObject(DataObjectGtk* dataObject)
{
    GtkTargetList* list = gtk_target_list_new(0, 0);

    if (dataObject->hasText())
        gtk_target_list_add_text_targets(list, getIdForTargetType(TargetTypeText));

    if (dataObject->hasMarkup())
        gtk_target_list_add(list, markupAtom, 0, getIdForTargetType(TargetTypeMarkup));

    if (dataObject->hasURIList()) {
        gtk_target_list_add_uri_targets(list, getIdForTargetType(TargetTypeURIList));
        gtk_target_list_add(list, netscapeURLAtom, 0, getIdForTargetType(TargetTypeNetscapeURL));
    }

    if (dataObject->hasImage())
        gtk_target_list_add_image_targets(list, getIdForTargetType(TargetTypeImage), TRUE);

    return list;
}

void PasteboardHelper::fillDataObjectFromDropData(GtkSelectionData* data, guint info, DataObjectGtk* dataObject)
{
    if (!gtk_selection_data_get_data(data))
        return;

    GdkAtom target = gtk_selection_data_get_target(data);
    if (target == textPlainAtom)
        dataObject->setText(selectionDataToUTF8String(data));
    else if (target == markupAtom) {
        String markup(selectionDataToUTF8String(data));
        removeMarkupPrefix(markup);
        dataObject->setMarkup(markup);
    } else if (target == uriListAtom) {
        dataObject->setURIList(selectionDataToUTF8String(data));
    } else if (target == netscapeURLAtom) {
        String urlWithLabel(selectionDataToUTF8String(data));
        Vector<String> pieces;
        urlWithLabel.split("\n", pieces);

        // Give preference to text/uri-list here, as it can hold more
        // than one URI but still take  the label if there is one.
        if (!dataObject->hasURIList())
            dataObject->setURIList(pieces[0]);
        if (pieces.size() > 1)
            dataObject->setText(pieces[1]);
    }
}

Vector<GdkAtom> PasteboardHelper::dropAtomsForContext(GtkWidget* widget, GdkDragContext* context)
{
    // Always search for these common atoms.
    Vector<GdkAtom> dropAtoms;
    dropAtoms.append(textPlainAtom);
    dropAtoms.append(markupAtom);
    dropAtoms.append(uriListAtom);
    dropAtoms.append(netscapeURLAtom);

    // For images, try to find the most applicable image type.
    GRefPtr<GtkTargetList> list(gtk_target_list_new(0, 0));
    gtk_target_list_add_image_targets(list.get(), getIdForTargetType(TargetTypeImage), TRUE);
    GdkAtom atom = gtk_drag_dest_find_target(widget, context, list.get());
    if (atom != GDK_NONE)
        dropAtoms.append(atom);

    return dropAtoms;
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
        gtk_clipboard_set_can_store(clipboard, 0, 0);

        settingClipboardDataObject = 0;

    } else
        gtk_clipboard_clear(clipboard);

    if (table)
        gtk_target_table_free(table, numberOfTargets);
    gtk_target_list_unref(list);
}

}

