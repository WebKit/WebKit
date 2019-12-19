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

#include "config.h"
#include "WebKitInputMethodContextImplGtk.h"

#include <gtk/gtk.h>
#include <wtf/MathExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitInputMethodContextImplGtkPrivate {
    GRefPtr<GtkIMContext> context;
};

WEBKIT_DEFINE_TYPE(WebKitInputMethodContextImplGtk, webkit_input_method_context_impl_gtk, WEBKIT_TYPE_INPUT_METHOD_CONTEXT)

static void contextPreeditStartCallback(WebKitInputMethodContextImplGtk* context)
{
    g_signal_emit_by_name(context, "preedit-started", nullptr);
}

static void contextPreeditChangedCallback(WebKitInputMethodContextImplGtk* context)
{
    g_signal_emit_by_name(context, "preedit-changed", nullptr);
}

static void contextPreeditEndCallback(WebKitInputMethodContextImplGtk* context)
{
    g_signal_emit_by_name(context, "preedit-finished", nullptr);
}

static void contextCommitCallback(WebKitInputMethodContextImplGtk* context, const char* text)
{
    g_signal_emit_by_name(context, "committed", text, nullptr);
}

static void webkitInputMethodContextImplGtkConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_input_method_context_impl_gtk_parent_class)->constructed(object);

    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(object)->priv;
    priv->context = adoptGRef(gtk_im_multicontext_new());
    g_signal_connect_object(priv->context.get(), "preedit-start", G_CALLBACK(contextPreeditStartCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-changed", G_CALLBACK(contextPreeditChangedCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-end", G_CALLBACK(contextPreeditEndCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "commit", G_CALLBACK(contextCommitCallback), object, G_CONNECT_SWAPPED);
}

static void webkitInputMethodContextImplGtkSetEnablePreedit(WebKitInputMethodContext* context, gboolean enabled)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    gtk_im_context_set_use_preedit(priv->context.get(), enabled);
}

static void webkitInputMethodContextImplGtkGetPreedit(WebKitInputMethodContext* context, char** text, GList** underlines, guint* cursorOffset)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    PangoAttrList* attrList = nullptr;
    int offset;
    gtk_im_context_get_preedit_string(priv->context.get(), text, underlines ? &attrList : nullptr, &offset);

    if (underlines) {
        *underlines = nullptr;
        if (attrList) {
            PangoAttrIterator* iter = pango_attr_list_get_iterator(attrList);

            do {
                if (!pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE))
                    continue;

                int start, end;
                pango_attr_iterator_range(iter, &start, &end);

                auto* underline = webkit_input_method_underline_new(clampTo<unsigned>(start), clampTo<unsigned>(end));
                if (auto* colorAttribute = pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE_COLOR)) {
                    PangoColor* color = &(reinterpret_cast<PangoAttrColor*>(colorAttribute))->color;
                    GdkRGBA rgba = { color->red / 65535., color->green / 65535., color->blue / 65535., 1. };
                    webkit_input_method_underline_set_color(underline, &rgba);
                }

                *underlines = g_list_prepend(*underlines, underline);
            } while (pango_attr_iterator_next(iter));
        }
    }

    if (cursorOffset)
        *cursorOffset = clampTo<unsigned>(offset);
}

static gboolean webkitInputMethodContextImplGtkFilterKeyEvent(WebKitInputMethodContext* context, GdkEventKey* keyEvent)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    return gtk_im_context_filter_keypress(priv->context.get(), keyEvent);
}

static void webkitInputMethodContextImplGtkNotifyFocusIn(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    gtk_im_context_focus_in(priv->context.get());
}

static void webkitInputMethodContextImplGtkNotifyFocusOut(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    gtk_im_context_focus_out(priv->context.get());
}

static void webkitInputMethodContextImplGtkNotifyCursorArea(WebKitInputMethodContext* context, int x, int y, int width, int height)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    GdkRectangle cursorRect = { x, y, width, height };
    gtk_im_context_set_cursor_location(priv->context.get(), &cursorRect);
}

static void webkitInputMethodContextImplGtkReset(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    gtk_im_context_reset(priv->context.get());
}

static void webkit_input_method_context_impl_gtk_class_init(WebKitInputMethodContextImplGtkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitInputMethodContextImplGtkConstructed;

    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    imClass->set_enable_preedit = webkitInputMethodContextImplGtkSetEnablePreedit;
    imClass->get_preedit = webkitInputMethodContextImplGtkGetPreedit;
    imClass->filter_key_event = webkitInputMethodContextImplGtkFilterKeyEvent;
    imClass->notify_focus_in = webkitInputMethodContextImplGtkNotifyFocusIn;
    imClass->notify_focus_out = webkitInputMethodContextImplGtkNotifyFocusOut;
    imClass->notify_cursor_area = webkitInputMethodContextImplGtkNotifyCursorArea;
    imClass->reset = webkitInputMethodContextImplGtkReset;
}

WebKitInputMethodContext* webkitInputMethodContextImplGtkNew()
{
    return WEBKIT_INPUT_METHOD_CONTEXT(g_object_new(WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK, nullptr));
}

void webkitInputMethodContextImplGtkSetClientWindow(WebKitInputMethodContextImplGtk* context, GdkWindow* window)
{
    gtk_im_context_set_client_window(context->priv->context.get(), window);
}
