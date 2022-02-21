/*
 * Copyright (C) 2021 Igalia S.L.
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
 */

// GtkEmojiChooser is private in GTK 3, so this goes through some hoops to
// obtain its type code via g_type_from_name(). The main issue is that the
// type needs to have been previously registered, which in practice means
// triggering a call to gtk_emoji_chooser_get_type(), also a hidden private
// function.
//
// One point in the public API where an emoji chooser can be explicitly
// triggered is the GtkEntry.insert_emoji vfunc: either at that point the
// GtkEmojiChooser used by the entry has been already created, or it will
// be created by calling it. Object instantiation needs the type to be
// registered, so after making sure at least one instance is created it
// is safe to use g_type_from_name().

#include "config.h"
#include "WebKitEmojiChooser.h"

#if GTK_CHECK_VERSION(3, 24, 0) && !USE(GTK4)

#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

GtkWidget* webkitEmojiChooserNew()
{
    static GType chooserType = ([] {
        GRefPtr<GtkWidget> entry = gtk_entry_new();
        gtk_entry_set_input_hints(GTK_ENTRY(entry.get()), GTK_INPUT_HINT_EMOJI);

        GtkEntryClass* entryClass = GTK_ENTRY_GET_CLASS(entry.get());
        entryClass->insert_emoji(GTK_ENTRY(entry.get()));

        // The emoji data is fetched in a delayed manner, so a direct call
        // to g_object_unref(entry) results in critical warning due to an
        // attempt to unref a yet unpopulated value. Arguably, GTK should do
        // a null-check, but this code needs to work even for older versions
        // of GTK which may be unpatched. Therefore, destroy the dummy entry
        // in a low priority idle source, after the source that fills the data
        // has been already dispatched.
        GRefPtr<GSource> source = adoptGRef(g_idle_source_new());
        g_source_set_callback(source.get(), [](void* data) {
            g_object_unref(data);
            return G_SOURCE_REMOVE;
        }, entry.leakRef(), nullptr);
        g_source_set_priority(source.get(), G_PRIORITY_LOW);
        g_source_attach(source.get(), RunLoop::main().mainContext());

        return g_type_from_name("GtkEmojiChooser");
    })();

    return GTK_WIDGET(g_object_new(chooserType, nullptr));
}

#endif // GTK_CHECK_VERSION(3, 24, 0) && !USE(GTK4)
