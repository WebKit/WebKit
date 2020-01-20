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
#include <wtf/text/CString.h>

struct _WebKitInputMethodContextImplGtkPrivate {
    GRefPtr<GtkIMContext> context;
    CString surroundingText;
    unsigned surroundingCursorIndex;
};

WEBKIT_DEFINE_TYPE(WebKitInputMethodContextImplGtk, webkit_input_method_context_impl_gtk, WEBKIT_TYPE_INPUT_METHOD_CONTEXT)

static GtkInputPurpose toGtkInputPurpose(WebKitInputPurpose purpose)
{
    switch (purpose) {
    case WEBKIT_INPUT_PURPOSE_FREE_FORM:
        return GTK_INPUT_PURPOSE_FREE_FORM;
    case WEBKIT_INPUT_PURPOSE_DIGITS:
        return GTK_INPUT_PURPOSE_DIGITS;
    case WEBKIT_INPUT_PURPOSE_NUMBER:
        return GTK_INPUT_PURPOSE_NUMBER;
    case WEBKIT_INPUT_PURPOSE_PHONE:
        return GTK_INPUT_PURPOSE_PHONE;
    case WEBKIT_INPUT_PURPOSE_URL:
        return GTK_INPUT_PURPOSE_URL;
    case WEBKIT_INPUT_PURPOSE_EMAIL:
        return GTK_INPUT_PURPOSE_EMAIL;
    case WEBKIT_INPUT_PURPOSE_PASSWORD:
        return GTK_INPUT_PURPOSE_PASSWORD;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static GtkInputHints toGtkInputHints(WebKitInputHints hints)
{
    unsigned gtkHints = 0;
    if (hints & WEBKIT_INPUT_HINT_SPELLCHECK)
        gtkHints |= GTK_INPUT_HINT_SPELLCHECK;
    if (hints & WEBKIT_INPUT_HINT_LOWERCASE)
        gtkHints |= GTK_INPUT_HINT_LOWERCASE;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_CHARS)
        gtkHints |= GTK_INPUT_HINT_UPPERCASE_CHARS;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_WORDS)
        gtkHints |= GTK_INPUT_HINT_UPPERCASE_WORDS;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES)
        gtkHints |= GTK_INPUT_HINT_UPPERCASE_SENTENCES;
    if (hints & WEBKIT_INPUT_HINT_INHIBIT_OSK)
        gtkHints |= GTK_INPUT_HINT_INHIBIT_OSK;
    return static_cast<GtkInputHints>(gtkHints);
}

static void inputPurposeChangedCallback(WebKitInputMethodContextImplGtk* context)
{
    g_object_set(context->priv->context.get(), "input-purpose", toGtkInputPurpose(webkit_input_method_context_get_input_purpose(WEBKIT_INPUT_METHOD_CONTEXT(context))), nullptr);
}

static void inputHintsChangedCallback(WebKitInputMethodContextImplGtk* context)
{
    g_object_set(context->priv->context.get(), "input-hints", toGtkInputHints(webkit_input_method_context_get_input_hints(WEBKIT_INPUT_METHOD_CONTEXT(context))), nullptr);
}

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

static gboolean contextRetrieveSurrounding(WebKitInputMethodContextImplGtk* context)
{
    auto* priv = context->priv;
    gtk_im_context_set_surrounding(priv->context.get(), priv->surroundingText.data(), priv->surroundingText.length(), priv->surroundingCursorIndex);
    return TRUE;
}

static void webkitInputMethodContextImplGtkConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_input_method_context_impl_gtk_parent_class)->constructed(object);

    g_signal_connect_swapped(object, "notify::input-purpose", G_CALLBACK(inputPurposeChangedCallback), object);
    g_signal_connect_swapped(object, "notify::input-hints", G_CALLBACK(inputHintsChangedCallback), object);

    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(object)->priv;
    priv->context = adoptGRef(gtk_im_multicontext_new());
    g_signal_connect_object(priv->context.get(), "preedit-start", G_CALLBACK(contextPreeditStartCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-changed", G_CALLBACK(contextPreeditChangedCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-end", G_CALLBACK(contextPreeditEndCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "commit", G_CALLBACK(contextCommitCallback), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "retrieve-surrounding", G_CALLBACK(contextRetrieveSurrounding), object, G_CONNECT_SWAPPED);
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

static void webkitInputMethodContextImplGtkNotifySurrounding(WebKitInputMethodContext* context, const gchar* text, unsigned length, unsigned cursorIndex, unsigned)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(context)->priv;
    priv->surroundingText = { text, length };
    priv->surroundingCursorIndex = cursorIndex;
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
    imClass->notify_surrounding = webkitInputMethodContextImplGtkNotifySurrounding;
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
