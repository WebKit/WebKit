/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WebKitInputMethodContextImplWPE.h"

#if ENABLE(WPE_PLATFORM)

#include <wpe/wpe-platform.h>
#include <wtf/MathExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitInputMethodContextImplWPEPrivate {
    GRefPtr<WPEInputMethodContext> context;
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitInputMethodContextImplWPE, webkit_input_method_context_impl_wpe, WEBKIT_TYPE_INPUT_METHOD_CONTEXT, WebKitInputMethodContext)

static WPEInputPurpose toWPEInputPurpose(WebKitInputPurpose purpose)
{
    switch (purpose) {
    case WEBKIT_INPUT_PURPOSE_FREE_FORM:
        return WPE_INPUT_PURPOSE_FREE_FORM;
    case WEBKIT_INPUT_PURPOSE_DIGITS:
        return WPE_INPUT_PURPOSE_DIGITS;
    case WEBKIT_INPUT_PURPOSE_NUMBER:
        return WPE_INPUT_PURPOSE_NUMBER;
    case WEBKIT_INPUT_PURPOSE_PHONE:
        return WPE_INPUT_PURPOSE_PHONE;
    case WEBKIT_INPUT_PURPOSE_URL:
        return WPE_INPUT_PURPOSE_URL;
    case WEBKIT_INPUT_PURPOSE_EMAIL:
        return WPE_INPUT_PURPOSE_EMAIL;
    case WEBKIT_INPUT_PURPOSE_PASSWORD:
        return WPE_INPUT_PURPOSE_PASSWORD;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static WPEInputHints toWPEInputHints(WebKitInputHints hints)
{
    unsigned wpeHints = 0;
    if (hints & WEBKIT_INPUT_HINT_SPELLCHECK)
        wpeHints |= WPE_INPUT_HINT_SPELLCHECK;
    if (hints & WEBKIT_INPUT_HINT_LOWERCASE)
        wpeHints |= WPE_INPUT_HINT_LOWERCASE;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_CHARS)
        wpeHints |= WPE_INPUT_HINT_UPPERCASE_CHARS;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_WORDS)
        wpeHints |= WPE_INPUT_HINT_UPPERCASE_WORDS;
    if (hints & WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES)
        wpeHints |= WPE_INPUT_HINT_UPPERCASE_SENTENCES;
    if (hints & WEBKIT_INPUT_HINT_INHIBIT_OSK)
        wpeHints |= WPE_INPUT_HINT_INHIBIT_OSK;
    return static_cast<WPEInputHints>(wpeHints);
}

static void inputPurposeChangedCallback(WebKitInputMethodContextImplWPE* context)
{
    g_object_set(context->priv->context.get(), "input-purpose", toWPEInputPurpose(webkit_input_method_context_get_input_purpose(WEBKIT_INPUT_METHOD_CONTEXT(context))), nullptr);
}

static void inputHintsChangedCallback(WebKitInputMethodContextImplWPE* context)
{
    g_object_set(context->priv->context.get(), "input-hints", toWPEInputHints(webkit_input_method_context_get_input_hints(WEBKIT_INPUT_METHOD_CONTEXT(context))), nullptr);
}

static void wpeIMContextPreeditStartCallback(WebKitInputMethodContextImplWPE* context)
{
    g_signal_emit_by_name(context, "preedit-started", nullptr);
}

static void wpeIMContextPreeditChangedCallback(WebKitInputMethodContextImplWPE* context)
{
    g_signal_emit_by_name(context, "preedit-changed", nullptr);
}

static void wpeIMContextPreeditEndCallback(WebKitInputMethodContextImplWPE* context)
{
    g_signal_emit_by_name(context, "preedit-finished", nullptr);
}

static void wpeIMContextCommitCallback(WebKitInputMethodContextImplWPE* context, const char* text)
{
    g_signal_emit_by_name(context, "committed", text, nullptr);
}

static void wpeIMContextDeleteSurrounding(WebKitInputMethodContextImplWPE* context, gint offset, guint characterCount)
{
    g_signal_emit_by_name(context, "delete-surrounding", offset, characterCount, nullptr);
}

static void webkitInputMethodContextImplWPEGetPreedit(WebKitInputMethodContext* context, char** text, GList** underlines, guint* cursorOffset)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    GList* wpeUnderlines = nullptr;
    guint offset;
    wpe_input_method_context_get_preedit_string(priv->context.get(), text, underlines ? &wpeUnderlines : nullptr, &offset);

    if (underlines) {
        *underlines = nullptr;

        if (wpeUnderlines) {
            for (auto* it = wpeUnderlines; it; it = g_list_next(it)) {
                auto* wpeUnderline = static_cast<WPEInputMethodUnderline*>(it->data);
                auto* wpeColor = wpe_input_method_underline_get_color(wpeUnderline);
                auto* underline = webkit_input_method_underline_new(
                    clampTo<unsigned>(wpe_input_method_underline_get_start_offset(wpeUnderline)),
                    clampTo<unsigned>(wpe_input_method_underline_get_end_offset(wpeUnderline)));
                WebKitColor color = { wpeColor->red, wpeColor->green, wpeColor->blue, wpeColor->alpha };
                webkit_input_method_underline_set_color(underline, &color);

                *underlines = g_list_prepend(*underlines, underline);
            }
            g_list_free_full(wpeUnderlines, reinterpret_cast<GDestroyNotify>(wpe_input_method_underline_free));
        }
    }

    if (cursorOffset)
        *cursorOffset = clampTo<unsigned>(offset);
}

static gboolean webkitInputMethodContextImplWPEFilterKeyEvent(WebKitInputMethodContext* context, void* keyEvent)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    auto* wpeEvent = static_cast<WPEEvent*>(keyEvent);
    return wpe_input_method_context_filter_key_event(priv->context.get(), wpeEvent);
}

static void webkitInputMethodContextImplWPENotifyFocusIn(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    wpe_input_method_context_focus_in(priv->context.get());
}

static void webkitInputMethodContextImplWPENotifyFocusOut(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    wpe_input_method_context_focus_out(priv->context.get());
}

static void webkitInputMethodContextImplWPENotifyCursorArea(WebKitInputMethodContext* context, int x, int y, int width, int height)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    wpe_input_method_context_set_cursor_area(priv->context.get(), x, y, width, height);
}

static void webkitInputMethodContextImplWPENotifySurrounding(WebKitInputMethodContext* context, const gchar* text, unsigned length, unsigned cursorIndex, unsigned selectionIndex)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    wpe_input_method_context_set_surrounding(priv->context.get(), text, length, cursorIndex, selectionIndex);
}

static void webkitInputMethodContextImplWPEReset(WebKitInputMethodContext* context)
{
    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    wpe_input_method_context_reset(priv->context.get());
}

static void webkit_input_method_context_impl_wpe_class_init(WebKitInputMethodContextImplWPEClass* klass)
{
    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    imClass->get_preedit = webkitInputMethodContextImplWPEGetPreedit;
    imClass->filter_key_event = webkitInputMethodContextImplWPEFilterKeyEvent;
    imClass->notify_focus_in = webkitInputMethodContextImplWPENotifyFocusIn;
    imClass->notify_focus_out = webkitInputMethodContextImplWPENotifyFocusOut;
    imClass->notify_cursor_area = webkitInputMethodContextImplWPENotifyCursorArea;
    imClass->notify_surrounding = webkitInputMethodContextImplWPENotifySurrounding;
    imClass->reset = webkitInputMethodContextImplWPEReset;
}

WebKitInputMethodContext* webkitInputMethodContextImplWPENew(WPEView *view)
{
    auto* context = WEBKIT_INPUT_METHOD_CONTEXT(g_object_new(WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_WPE, nullptr));

    g_signal_connect_swapped(context, "notify::input-purpose", G_CALLBACK(inputPurposeChangedCallback), context);
    g_signal_connect_swapped(context, "notify::input-hints", G_CALLBACK(inputHintsChangedCallback), context);

    auto* priv = WEBKIT_INPUT_METHOD_CONTEXT_IMPL_WPE(context)->priv;
    priv->context = adoptGRef(wpe_input_method_context_new(view) );

    g_signal_connect_object(priv->context.get(), "preedit-started", G_CALLBACK(wpeIMContextPreeditStartCallback), context, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-changed", G_CALLBACK(wpeIMContextPreeditChangedCallback), context, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "preedit-finished", G_CALLBACK(wpeIMContextPreeditEndCallback), context, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "committed", G_CALLBACK(wpeIMContextCommitCallback), context, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->context.get(), "delete-surrounding", G_CALLBACK(wpeIMContextDeleteSurrounding), context, G_CONNECT_SWAPPED);

    return context;
}

#endif // ENABLE(WPE_PLATFORM)
