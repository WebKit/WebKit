/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEInputMethodContextWaylandV1.h"

#include "GRefPtrWPE.h"
#include "WPEDisplayWaylandPrivate.h"
#include "WPEViewWayland.h"
#include "text-input-unstable-v1-client-protocol.h"
#include <algorithm>
#include <cstdlib>
#include <wayland-client-protocol.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <xkbcommon/xkbcommon.h>

typedef struct _TextInputV1Global TextInputV1Global;

struct _TextInputV1Global {
    struct zwp_text_input_v1* textInput;

    WPEInputMethodContext* current;

    bool focused;

    uint32_t serial;
};

struct _WPEIMContextWaylandV1Private {
    struct {
        GUniquePtr<char> text;
        GList* underlines;
        int32_t cursorIndex;
    } preedit;

    struct {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    } cursorRect;

    struct {
        GUniquePtr<char> text;
        uint32_t cursorIndex;
        uint32_t anchorIndex;
    } surrounding;

    struct {
        int32_t index;
        uint32_t length;
    } pendingSurroundingDelete;

    struct {
        xkb_mod_mask_t shiftMask;
        xkb_mod_mask_t altMask;
        xkb_mod_mask_t controlMask;
    } modifiers;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEIMContextWaylandV1, wpe_im_context_wayland_v1, WPE_TYPE_INPUT_METHOD_CONTEXT, WPEInputMethodContext)

static TextInputV1Global* textInputV1GetGlobalByDisplay(WPEDisplayWayland* /*display*/);

static TextInputV1Global* textInputV1GetGlobalByContext(WPEIMContextWaylandV1* self)
{
    if (wpe_input_method_context_get_view(WPE_INPUT_METHOD_CONTEXT(self)) == nullptr)
        return nullptr;

    auto* display = wpe_input_method_context_get_display(WPE_INPUT_METHOD_CONTEXT(self));
    auto* global = textInputV1GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));
    if (global->current != WPE_INPUT_METHOD_CONTEXT(self))
        return nullptr;
    if (global->textInput == nullptr)
        return nullptr;

    return global;
}

static uint32_t toTextInputV1Hints(WPEInputHints inputHints, WPEInputPurpose purpose)
{
    uint32_t hints = 0;

    if (inputHints & WPE_INPUT_HINT_WORD_COMPLETION)
        hints |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION;
    if (inputHints & WPE_INPUT_HINT_LOWERCASE)
        hints |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_CHARS)
        hints |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_WORDS)
        hints |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_SENTENCES)
        hints |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION;

    if (purpose == WPE_INPUT_PURPOSE_PIN || purpose == WPE_INPUT_PURPOSE_PASSWORD)
        hints |= (ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT | ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA);

    return hints;
}

static uint32_t toTextInputV1Purpose(WPEInputPurpose purpose)
{
    switch (purpose) {
    case WPE_INPUT_PURPOSE_FREE_FORM:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case WPE_INPUT_PURPOSE_ALPHA:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA;
    case WPE_INPUT_PURPOSE_DIGITS:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS;
    case WPE_INPUT_PURPOSE_NUMBER:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER;
    case WPE_INPUT_PURPOSE_PHONE:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE;
    case WPE_INPUT_PURPOSE_URL:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL;
    case WPE_INPUT_PURPOSE_EMAIL:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL;
    case WPE_INPUT_PURPOSE_NAME:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME;
    case WPE_INPUT_PURPOSE_PASSWORD:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD;
    case WPE_INPUT_PURPOSE_TERMINAL:
        return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL;
    default:
        g_assert_not_reached();
    }

    return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
}

static char* truncateSurroundingTextIfeeded(const char *text, uint32_t *cursorIndex, uint32_t *anchorIndex)
{
#define MAX_LEN 4000
    unsigned len = strlen(text);

    if (len < MAX_LEN)
        return nullptr;

    const char* start;
    const char* end;
    if (*cursorIndex < MAX_LEN && *anchorIndex < MAX_LEN) {
        start = text;
        end = &text[MAX_LEN];
    } else if (*cursorIndex > len - MAX_LEN && *anchorIndex > len - MAX_LEN) {
        start = &text[len - MAX_LEN];
        end = &text[len];
    } else {
        unsigned selectionLen = std::abs(static_cast<int>(*cursorIndex - *anchorIndex));
        if (selectionLen > MAX_LEN) {
            /* This is unsupported, let's just ignore the selection. */
            if (*cursorIndex < MAX_LEN) {
                start = text;
                end = &text[MAX_LEN];
            } else if (*cursorIndex > len - MAX_LEN) {
                start = &text[len - MAX_LEN];
                end = &text[len];
            } else {
                start = &text[std::max(0u, *cursorIndex - (MAX_LEN / 2))];
                end = &text[std::min(static_cast<unsigned>(MAX_LEN), *cursorIndex + (MAX_LEN / 2))];
            }
        } else {
            unsigned mid = std::min(*cursorIndex, *anchorIndex) + (selectionLen / 2);
            start = &text[std::max(0u, (mid - MAX_LEN) / 2)];
            end = &text[std::min(static_cast<unsigned>(MAX_LEN), (mid + MAX_LEN / 2))];
        }
    }

    if (start != text)
        start = g_utf8_next_char(start);
    if (end != &text[len])
        end = g_utf8_find_prev_char(text, end);

    *cursorIndex -= start - text;
    *anchorIndex -= start - text;

    return g_strndup(start, end - start);
#undef MAX_LEN
}

static void textInputV1SetCursorRectangle(WPEIMContextWaylandV1* context)
{
    auto* global = textInputV1GetGlobalByContext(context);
    if (global == nullptr)
        return;

    zwp_text_input_v1_set_cursor_rectangle(global->textInput,
        context->priv->cursorRect.x,
        context->priv->cursorRect.y,
        context->priv->cursorRect.width,
        context->priv->cursorRect.height);
}

static void textInputV1SetSurroundingText(WPEIMContextWaylandV1* context)
{
    auto* global = textInputV1GetGlobalByContext(context);
    if (global == nullptr)
        return;

    if (!context->priv->surrounding.text)
        return;

    uint32_t cursorIndex = context->priv->surrounding.cursorIndex;
    uint32_t anchorIndex = context->priv->surrounding.anchorIndex;
    GUniquePtr<char> truncatedText(
        truncateSurroundingTextIfeeded(context->priv->surrounding.text.get(),
        &cursorIndex,
        &anchorIndex));

    zwp_text_input_v1_set_surrounding_text(global->textInput,
        truncatedText ? truncatedText.get() : context->priv->surrounding.text.get(),
        cursorIndex, anchorIndex);
}

static void textInputV1SetContentType(WPEIMContextWaylandV1* context)
{
    WPEInputHints hints;
    WPEInputPurpose purpose;

    auto* global = textInputV1GetGlobalByContext(context);
    if (global == nullptr)
        return;

    g_object_get(context,
        "input-hints", &hints,
        "input-purpose", &purpose,
        nullptr);

    zwp_text_input_v1_set_content_type(global->textInput,
        toTextInputV1Hints(hints, purpose),
        toTextInputV1Purpose(purpose));
}

static void textInputV1CommitState(WPEIMContextWaylandV1* context)
{
    auto* global = textInputV1GetGlobalByContext(context);
    if (global == nullptr)
        return;

    global->serial++;

    zwp_text_input_v1_commit_state(global->textInput, global->serial);
}

static void textInputV1UpdateState(WPEIMContextWaylandV1* context)
{
    auto* global = textInputV1GetGlobalByContext(context);
    if (global == nullptr)
        return;

    textInputV1SetSurroundingText(context);
    textInputV1SetContentType(context);
    textInputV1SetCursorRectangle(context);
    textInputV1CommitState(context);
}

static xkb_mod_mask_t keysymModifiersGetMask(struct wl_array *map, const char *name)
{
    xkb_mod_index_t index = 0;
    const char* p = static_cast<const char *>(map->data);

    while (p < static_cast<const char *>(p + map->size)) {
        if (!strcmp (p, name))
            return 1 << index;

        index++;
        p += strlen(p) + 1;
    }

    return XKB_MOD_INVALID;
}

static const struct zwp_text_input_v1_listener textInputListenerV1 = {
    // enter
    [](void* data, struct zwp_text_input_v1* /*text_input*/, struct wl_surface* /*surface*/)
    {
        auto* global = static_cast<TextInputV1Global*>(data);

        global->focused = true;

        textInputV1UpdateState(WPE_IM_CONTEXT_WAYLAND_V1(global->current));
    },
    // leave
    [](void *data, struct zwp_text_input_v1* /*text_input*/)
    {
        auto* global = static_cast<TextInputV1Global*>(data);

        global->focused = false;
    },
    // modifiers_map
    [](void* data, struct zwp_text_input_v1* /*text_input*/, struct wl_array* map)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        priv->modifiers.shiftMask = keysymModifiersGetMask(map, XKB_MOD_NAME_SHIFT);
        priv->modifiers.altMask = keysymModifiersGetMask(map, XKB_MOD_NAME_ALT);
        priv->modifiers.controlMask = keysymModifiersGetMask(map, XKB_MOD_NAME_CTRL);
    },
    // input_panel_state
    [](void* /*data*/, struct zwp_text_input_v1* /*text_input*/, uint32_t /*state*/)
    {
        // NOOP
    },
    // preedit_string
    [](void* data, struct zwp_text_input_v1* /*text_input*/, uint32_t serial, const char* text, const char* /*commit*/)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        bool valid = global->serial == serial;
        if (valid && !priv->preedit.text)
            g_signal_emit_by_name(global->current, "preedit-started");

        priv->preedit.text.reset(g_strdup(text));
        if (valid)
            g_signal_emit_by_name(global->current, "preedit-changed");
    },
    // preedit_styling
    [](void* data, struct zwp_text_input_v1* /*text_input*/, uint32_t index, uint32_t length, uint32_t style)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        if (style == ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE)
            length = 0;

        auto* underline = wpe_input_method_underline_new(index, index + length);
        switch (style) {
        case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT: {
            WPEColor color = { 1., 0, 0, 1. };
            wpe_input_method_underline_set_color(underline, &color);
            break;
        }
        case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT: {
            WPEColor color = { 1., 1., 0., 1. };
            wpe_input_method_underline_set_color(underline, &color);
            break;
        }
        case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE: {
            WPEColor color = { 0., 0., 1., 1. };
            wpe_input_method_underline_set_color(underline, &color);
            break;
        }
        case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE: {
            WPEColor color = { 0.3, 0.3, 0.3, 1. };
            wpe_input_method_underline_set_color(underline, &color);
            break;
        }
        }

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        priv->preedit.underlines = g_list_append(priv->preedit.underlines, underline);
    },
    // preedit_cursor
    [](void* data, struct zwp_text_input_v1* /*text_input*/, int32_t index)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        priv->preedit.cursorIndex = index;
    },
    // commit_string
    [](void* data, struct zwp_text_input_v1* /*text_input*/, uint32_t serial, const char* text)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        bool valid = global->serial == serial;
        if (valid && priv->preedit.text) {
            priv->preedit.text.reset(nullptr);
            g_signal_emit_by_name(global->current, "preedit-changed");
            g_signal_emit_by_name(global->current, "preedit-finished");
        } else
            priv->preedit.text.reset(nullptr);

        if (valid && priv->surrounding.text && priv->pendingSurroundingDelete.length > 0) {
            char* pos = priv->surrounding.text.get() + priv->surrounding.cursorIndex;
            glong charIndex = g_utf8_pointer_to_offset(priv->surrounding.text.get(), pos);
            pos += priv->pendingSurroundingDelete.index;
            glong charStart = g_utf8_pointer_to_offset(priv->surrounding.text.get(), pos);
            pos += priv->pendingSurroundingDelete.length;
            glong charEnd = g_utf8_pointer_to_offset(priv->surrounding.text.get(), pos);

            g_signal_emit_by_name(global->current, "delete-surrounding", charStart - charIndex, charEnd - charStart);
        }
        priv->pendingSurroundingDelete.index = 0;
        priv->pendingSurroundingDelete.length = 0;

        if (valid && text)
            g_signal_emit_by_name(global->current, "committed", text);
    },
    // cursor_position
    [](void* /*data*/, struct zwp_text_input_v1* /*text_input*/, int32_t /*index*/, int32_t /*anchor*/)
    {
        // NOOP
    },
    // delete_surrounding_text
    [](void* data, struct zwp_text_input_v1* /*text_input*/, int32_t index, uint32_t length)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        priv->pendingSurroundingDelete.index = index;
        priv->pendingSurroundingDelete.length = length;
    },
    // keysym
    [](void* data, struct zwp_text_input_v1* /*text_input*/, uint32_t /*serial*/, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
    {
        auto* global = static_cast<TextInputV1Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V1(global->current)->priv;
        auto* view = wpe_input_method_context_get_view(global->current);

        uint32_t wpe_modifiers = 0;
        if (modifiers & priv->modifiers.shiftMask)
            wpe_modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
        if (modifiers & priv->modifiers.altMask)
            wpe_modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
        if (modifiers & priv->modifiers.controlMask)
            wpe_modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;

        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_keyboard_new(state ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, view,
            WPE_INPUT_SOURCE_KEYBOARD, time, static_cast<WPEModifiers>(wpe_modifiers), 0, sym));
        wpe_view_event(view, event.get());
    },
    // language
    [](void* /*data*/, struct zwp_text_input_v1* /*text_input*/, uint32_t /*serial*/, const char* /*language*/)
    {
        // NOOP
    },
    // text_direction
    [](void* /*data*/, struct zwp_text_input_v1* /*text_input*/, uint32_t /*serial*/, uint32_t /*direction*/)
    {
        // NOOP
    }
};

static void wpeIMContextWaylandV1GlobalFree(gpointer data)
{
    auto* global = static_cast<TextInputV1Global*>(data);
    g_free(global);
}

static TextInputV1Global* textInputV1GetGlobalByDisplay(WPEDisplayWayland *display)
{
    auto* global = static_cast<TextInputV1Global*>(g_object_get_data(G_OBJECT(display), "text-input-v1-global"));
    if (global != nullptr)
        return global;

    global = g_new0(TextInputV1Global, 1);
    global->textInput = wpeDisplayWaylandGetTextInputV1(display);

    if (global->textInput)
        zwp_text_input_v1_add_listener(global->textInput, &textInputListenerV1, global);

    g_object_set_data_full(G_OBJECT(display),
        "text-input-v1-global",
        global,
        wpeIMContextWaylandV1GlobalFree);
    return global;
}

static void wpeIMContextWaylandV1PurposeOrHintsChanged(WPEInputMethodContext *context)
{
    textInputV1SetContentType(WPE_IM_CONTEXT_WAYLAND_V1(context));
    textInputV1CommitState(WPE_IM_CONTEXT_WAYLAND_V1(context));
}

static void wpeIMContextWaylandV1Constructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_im_context_wayland_v1_parent_class)->constructed(object);

    auto* context = WPE_INPUT_METHOD_CONTEXT(object);
    auto* wpeContext = WPE_IM_CONTEXT_WAYLAND_V1(context);

    g_signal_connect_swapped(context, "notify::input-purpose", G_CALLBACK(wpeIMContextWaylandV1PurposeOrHintsChanged), wpeContext);
    g_signal_connect_swapped(context, "notify::input-hints", G_CALLBACK(wpeIMContextWaylandV1PurposeOrHintsChanged), wpeContext);
}

static void wpeIMContextWaylandV1Dispose(GObject* object)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(object);

    wpe_input_method_context_focus_out(WPE_INPUT_METHOD_CONTEXT(self));

    G_OBJECT_CLASS(wpe_im_context_wayland_v1_parent_class)->dispose(object);
}

static void wpeIMContextWaylandV1GetPreeditString(WPEInputMethodContext* context, char** text, GList** underlines, guint* cursorOffset)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(context);

    if (text != nullptr)
        *text = self->priv->preedit.text ? g_strdup(self->priv->preedit.text.get()) : g_strdup("");

    if (underlines != nullptr)
        *underlines = self->priv->preedit.underlines;
    else
        g_list_free_full(self->priv->preedit.underlines, g_object_unref);
    self->priv->preedit.underlines = nullptr;

    if (cursorOffset)
        *cursorOffset = self->priv->preedit.cursorIndex;
}

static void wpeIMContextWaylandV1FocusIn(WPEInputMethodContext* context)
{
    auto* view = wpe_input_method_context_get_view(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* seat = wpeDisplayWaylandGetSeat(WPE_DISPLAY_WAYLAND(display));
    auto* global = textInputV1GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));
    if (global->current == context)
        return;

    if (view == nullptr)
        return;

    global->current = context;
    if (global->textInput == nullptr)
        return;

    WPEInputHints hints = wpe_input_method_context_get_input_hints(context);
    if (!(hints & WPE_INPUT_HINT_INHIBIT_OSK)) {
        zwp_text_input_v1_show_input_panel(global->textInput);
        zwp_text_input_v1_activate(global->textInput,
            seat->seat(),
            wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view)));
    }
}

static void wpeIMContextWaylandV1FocusOut(WPEInputMethodContext *context)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* seat = wpeDisplayWaylandGetSeat(WPE_DISPLAY_WAYLAND(display));
    auto* global = textInputV1GetGlobalByContext(self);
    if (global == nullptr)
        return;

    if (global->focused)
        zwp_text_input_v1_deactivate(global->textInput, seat->seat());

    global->current = nullptr;
}

static void wpeIMContextWaylandV1SetCursorArea(WPEInputMethodContext* context, int x, int y, int width, int height)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* global = textInputV1GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));
    if (self->priv->cursorRect.x == x && self->priv->cursorRect.y == y && self->priv->cursorRect.width == width && self->priv->cursorRect.height == height)
        return;

    self->priv->cursorRect.x = x;
    self->priv->cursorRect.y = y;
    self->priv->cursorRect.width = width;
    self->priv->cursorRect.height = height;

    if (global->current == context) {
        textInputV1SetCursorRectangle(self);
        textInputV1CommitState(self);
    }
}

static void wpeIMContextWaylandV1SetSurrounding(WPEInputMethodContext* context, const char* text, guint length, guint cursorIndex, guint selectionIndex)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* global = textInputV1GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));

    self->priv->surrounding.text.reset(g_strndup(text, length));
    self->priv->surrounding.cursorIndex = cursorIndex;
    self->priv->surrounding.anchorIndex = selectionIndex;

    if (global->current == context) {
        textInputV1SetSurroundingText(self);
        textInputV1CommitState(self);
    }
}

static void wpeIMContextWaylandV1Reset(WPEInputMethodContext *context)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V1(context);
    auto* global = textInputV1GetGlobalByContext(self);
    if (global->current == nullptr)
        return;

    if (global->current == context) {
        zwp_text_input_v1_reset(global->textInput);
        textInputV1UpdateState(self);
    }
}

static void wpe_im_context_wayland_v1_class_init(WPEIMContextWaylandV1Class* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = wpeIMContextWaylandV1Constructed;
    objectClass->dispose = wpeIMContextWaylandV1Dispose;

    WPEInputMethodContextClass* imContextClass = WPE_INPUT_METHOD_CONTEXT_CLASS(klass);
    imContextClass->get_preedit_string = wpeIMContextWaylandV1GetPreeditString;
    imContextClass->focus_in = wpeIMContextWaylandV1FocusIn;
    imContextClass->focus_out = wpeIMContextWaylandV1FocusOut;
    imContextClass->set_cursor_area = wpeIMContextWaylandV1SetCursorArea;
    imContextClass->set_surrounding = wpeIMContextWaylandV1SetSurrounding;
    imContextClass->reset = wpeIMContextWaylandV1Reset;
}

WPEInputMethodContext* wpe_im_context_wayland_v1_new(WPEDisplayWayland* display, WPEView* view)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);
    g_return_val_if_fail(WPE_IS_VIEW_WAYLAND(view), nullptr);

    textInputV1GetGlobalByDisplay(display); // ensure creation of listener
    return WPE_INPUT_METHOD_CONTEXT(g_object_new(WPE_TYPE_IM_CONTEXT_WAYLAND_V1, "view", view, nullptr));
}
