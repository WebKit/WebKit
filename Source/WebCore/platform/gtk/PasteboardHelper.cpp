/*
 * Copyright (C) 2010 Martin Robinson <mrobinson@webkit.org>
 * Copyright (C) Igalia S.L.
 * Copyright (C) 2013 Collabora Ltd.
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

#include "DataObjectGtk.h"
#include "GtkVersioning.h"
#include "Pasteboard.h"
#include "TextResourceDecoder.h"
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

static GdkAtom textPlainAtom;
static GdkAtom markupAtom;
static GdkAtom netscapeURLAtom;
static GdkAtom uriListAtom;
static GdkAtom smartPasteAtom;
static GdkAtom unknownAtom;

static const String gMarkupPrefix = ASCIILiteral("<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">");

static void removeMarkupPrefix(String& markup)
{
    // The markup prefix is not harmful, but we remove it from the string anyway, so that
    // we can have consistent results with other ports during the layout tests.
    if (markup.startsWith(gMarkupPrefix))
        markup.remove(0, gMarkupPrefix.length());
}

PasteboardHelper& PasteboardHelper::singleton()
{
    static PasteboardHelper helper;
    return helper;
}

PasteboardHelper::PasteboardHelper()
    : m_targetList(adoptGRef(gtk_target_list_new(nullptr, 0)))
{
    textPlainAtom = gdk_atom_intern_static_string("text/plain;charset=utf-8");
    markupAtom = gdk_atom_intern_static_string("text/html");
    netscapeURLAtom = gdk_atom_intern_static_string("_NETSCAPE_URL");
    uriListAtom = gdk_atom_intern_static_string("text/uri-list");
    smartPasteAtom = gdk_atom_intern_static_string("application/vnd.webkitgtk.smartpaste");
    unknownAtom = gdk_atom_intern_static_string("application/vnd.webkitgtk.unknown");

    gtk_target_list_add_text_targets(m_targetList.get(), PasteboardHelper::TargetTypeText);
    gtk_target_list_add(m_targetList.get(), markupAtom, 0, PasteboardHelper::TargetTypeMarkup);
    gtk_target_list_add_uri_targets(m_targetList.get(), PasteboardHelper::TargetTypeURIList);
    gtk_target_list_add(m_targetList.get(), netscapeURLAtom, 0, PasteboardHelper::TargetTypeNetscapeURL);
    gtk_target_list_add_image_targets(m_targetList.get(), PasteboardHelper::TargetTypeImage, TRUE);
    gtk_target_list_add(m_targetList.get(), unknownAtom, 0, PasteboardHelper::TargetTypeUnknown);
}

PasteboardHelper::~PasteboardHelper()
{
}

GtkTargetList* PasteboardHelper::targetList() const
{
    return m_targetList.get();
}

static String selectionDataToUTF8String(GtkSelectionData* data)
{
    // g_strndup guards against selection data that is not null-terminated.
    GUniquePtr<gchar> markupString(g_strndup(reinterpret_cast<const char*>(gtk_selection_data_get_data(data)), gtk_selection_data_get_length(data)));
    return String::fromUTF8(markupString.get());
}

void PasteboardHelper::getClipboardContents(GtkClipboard* clipboard, DataObjectGtk& dataObject)
{
    if (gtk_clipboard_wait_is_text_available(clipboard)) {
        GUniquePtr<gchar> textData(gtk_clipboard_wait_for_text(clipboard));
        if (textData)
            dataObject.setText(String::fromUTF8(textData.get()));
    }

    if (gtk_clipboard_wait_is_target_available(clipboard, markupAtom)) {
        if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, markupAtom)) {
            String markup(selectionDataToUTF8String(data));
            removeMarkupPrefix(markup);
            dataObject.setMarkup(markup);
            gtk_selection_data_free(data);
        }
    }

    if (gtk_clipboard_wait_is_target_available(clipboard, uriListAtom)) {
        if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, uriListAtom)) {
            dataObject.setURIList(selectionDataToUTF8String(data));
            gtk_selection_data_free(data);
        }
    }

    dataObject.setCanSmartReplace(gtk_clipboard_wait_is_target_available(clipboard, smartPasteAtom));
}

void PasteboardHelper::fillSelectionData(GtkSelectionData* selectionData, guint info, const DataObjectGtk& dataObject)
{
    if (info == TargetTypeText)
        gtk_selection_data_set_text(selectionData, dataObject.text().utf8().data(), -1);

    else if (info == TargetTypeMarkup) {
        // Some Linux applications refuse to accept pasted markup unless it is
        // prefixed by a content-type meta tag.
        CString markup = String(gMarkupPrefix + dataObject.markup()).utf8();
        gtk_selection_data_set(selectionData, markupAtom, 8,
            reinterpret_cast<const guchar*>(markup.data()), markup.length());

    } else if (info == TargetTypeURIList) {
        CString uriList = dataObject.uriList().utf8();
        gtk_selection_data_set(selectionData, uriListAtom, 8,
            reinterpret_cast<const guchar*>(uriList.data()), uriList.length());

    } else if (info == TargetTypeNetscapeURL && dataObject.hasURL()) {
        String url(dataObject.url());
        String result(url);
        result.append("\n");

        if (dataObject.hasText())
            result.append(dataObject.text());
        else
            result.append(url);

        GUniquePtr<gchar> resultData(g_strdup(result.utf8().data()));
        gtk_selection_data_set(selectionData, netscapeURLAtom, 8,
            reinterpret_cast<const guchar*>(resultData.get()), strlen(resultData.get()));

    } else if (info == TargetTypeImage)
        gtk_selection_data_set_pixbuf(selectionData, dataObject.image());

    else if (info == TargetTypeSmartPaste)
        gtk_selection_data_set_text(selectionData, "", -1);

    else if (info == TargetTypeUnknown) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

        for (auto& it : dataObject.unknownTypes()) {
            GUniquePtr<gchar> dictItem(g_strdup_printf("{'%s', '%s'}", it.key.utf8().data(), it.value.utf8().data()));
            g_variant_builder_add_parsed(&builder, dictItem.get());
        }

        GRefPtr<GVariant> variant = g_variant_builder_end(&builder);
        GUniquePtr<gchar> serializedVariant(g_variant_print(variant.get(), TRUE));
        gtk_selection_data_set(selectionData, unknownAtom, 1, reinterpret_cast<const guchar*>(serializedVariant.get()), strlen(serializedVariant.get()));
    }
}

GRefPtr<GtkTargetList> PasteboardHelper::targetListForDataObject(const DataObjectGtk& dataObject)
{
    GRefPtr<GtkTargetList> list = adoptGRef(gtk_target_list_new(nullptr, 0));

    if (dataObject.hasText())
        gtk_target_list_add_text_targets(list.get(), TargetTypeText);

    if (dataObject.hasMarkup())
        gtk_target_list_add(list.get(), markupAtom, 0, TargetTypeMarkup);

    if (dataObject.hasURIList()) {
        gtk_target_list_add_uri_targets(list.get(), TargetTypeURIList);
        gtk_target_list_add(list.get(), netscapeURLAtom, 0, TargetTypeNetscapeURL);
    }

    if (dataObject.hasImage())
        gtk_target_list_add_image_targets(list.get(), TargetTypeImage, TRUE);

    if (dataObject.hasUnknownTypeData())
        gtk_target_list_add(list.get(), unknownAtom, 0, TargetTypeUnknown);

    if (dataObject.canSmartReplace())
        gtk_target_list_add(list.get(), smartPasteAtom, 0, TargetTypeSmartPaste);

    return list;
}

void PasteboardHelper::fillDataObjectFromDropData(GtkSelectionData* data, guint /* info */, DataObjectGtk& dataObject)
{
    if (!gtk_selection_data_get_data(data))
        return;

    GdkAtom target = gtk_selection_data_get_target(data);
    if (target == textPlainAtom)
        dataObject.setText(selectionDataToUTF8String(data));
    else if (target == markupAtom) {
        String markup(selectionDataToUTF8String(data));
        removeMarkupPrefix(markup);
        dataObject.setMarkup(markup);
    } else if (target == uriListAtom) {
        dataObject.setURIList(selectionDataToUTF8String(data));
    } else if (target == netscapeURLAtom) {
        String urlWithLabel(selectionDataToUTF8String(data));
        Vector<String> pieces;
        urlWithLabel.split('\n', pieces);

        // Give preference to text/uri-list here, as it can hold more
        // than one URI but still take  the label if there is one.
        if (!dataObject.hasURIList())
            dataObject.setURIList(pieces[0]);
        if (pieces.size() > 1)
            dataObject.setText(pieces[1]);
    } else if (target == unknownAtom) {
        GRefPtr<GVariant> variant = g_variant_new_parsed(reinterpret_cast<const char*>(gtk_selection_data_get_data(data)));

        GUniqueOutPtr<gchar> key;
        GUniqueOutPtr<gchar> value;
        GVariantIter iter;

        g_variant_iter_init(&iter, variant.get());
        while (g_variant_iter_next(&iter, "{ss}", &key.outPtr(), &value.outPtr()))
            dataObject.setUnknownTypeData(key.get(), value.get());
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
    dropAtoms.append(unknownAtom);

    // For images, try to find the most applicable image type.
    GRefPtr<GtkTargetList> list = adoptGRef(gtk_target_list_new(0, 0));
    gtk_target_list_add_image_targets(list.get(), TargetTypeImage, TRUE);
    GdkAtom atom = gtk_drag_dest_find_target(widget, context, list.get());
    if (atom != GDK_NONE)
        dropAtoms.append(atom);

    return dropAtoms;
}

static DataObjectGtk* settingClipboardDataObject = 0;

struct ClipboardSetData {
    ClipboardSetData(DataObjectGtk* dataObject, std::function<void()>&& selectionClearedCallback)
        : dataObject(dataObject)
        , selectionClearedCallback(WTFMove(selectionClearedCallback))
    {
    }

    ~ClipboardSetData()
    {
    }

    RefPtr<DataObjectGtk> dataObject;
    std::function<void()> selectionClearedCallback;
};

static void getClipboardContentsCallback(GtkClipboard*, GtkSelectionData *selectionData, guint info, gpointer userData)
{
    auto* data = static_cast<ClipboardSetData*>(userData);
    PasteboardHelper::singleton().fillSelectionData(selectionData, info, *data->dataObject);
}

static void clearClipboardContentsCallback(GtkClipboard*, gpointer userData)
{
    std::unique_ptr<ClipboardSetData> data(static_cast<ClipboardSetData*>(userData));

    // Only clear the DataObject for this clipboard if we are not currently setting it.
    if (data->dataObject.get() != settingClipboardDataObject)
        data->dataObject->clearAll();

    if (data->selectionClearedCallback)
        data->selectionClearedCallback();
}

void PasteboardHelper::writeClipboardContents(GtkClipboard* clipboard, const DataObjectGtk& dataObject, std::function<void()>&& primarySelectionCleared)
{
    GRefPtr<GtkTargetList> list = targetListForDataObject(dataObject);

    int numberOfTargets;
    GtkTargetEntry* table = gtk_target_table_new_from_list(list.get(), &numberOfTargets);

    if (numberOfTargets > 0 && table) {
        settingClipboardDataObject = const_cast<DataObjectGtk*>(&dataObject);

        auto data = std::make_unique<ClipboardSetData>(settingClipboardDataObject, WTFMove(primarySelectionCleared));
        if (gtk_clipboard_set_with_data(clipboard, table, numberOfTargets, getClipboardContentsCallback, clearClipboardContentsCallback, data.get())) {
            gtk_clipboard_set_can_store(clipboard, nullptr, 0);
            // When gtk_clipboard_set_with_data() succeeds clearClipboardContentsCallback takes the ownership of data, so we leak it here.
            data.release();
        }

        settingClipboardDataObject = nullptr;
    } else
        gtk_clipboard_clear(clipboard);

    if (table)
        gtk_target_table_free(table, numberOfTargets);
}

}

