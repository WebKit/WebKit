/*
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 24, 0)

G_BEGIN_DECLS

#define WEBKIT_TYPE_EMOJI_CHOOSER            (webkit_emoji_chooser_get_type())
#define WEBKIT_EMOJI_CHOOSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_EMOJI_CHOOSER, WebKitEmojiChooser))
#define WEBKIT_IS_EMOJI_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_EMOJI_CHOOSER))
#define WEBKIT_EMOJI_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_EMOJI_CHOOSER, WebKitEmojiChooserClass))
#define WEBKIT_IS_EMOJI_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_EMOJI_CHOOSER))
#define WEBKIT_EMOJI_CHOOSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_EMOJI_CHOOSER, WebKitEmojiChooserClass))

typedef struct _WebKitEmojiChooser        WebKitEmojiChooser;
typedef struct _WebKitEmojiChooserClass   WebKitEmojiChooserClass;
typedef struct _WebKitEmojiChooserPrivate WebKitEmojiChooserPrivate;

struct _WebKitEmojiChooser {
    GtkPopover parent;

    WebKitEmojiChooserPrivate* priv;
};

struct _WebKitEmojiChooserClass {
    GtkPopoverClass parentClass;
};

GType webkit_emoji_chooser_get_type();
GtkWidget* webkitEmojiChooserNew();

G_END_DECLS

#endif // GTK_CHECK_VERSION(3, 24, 0)
