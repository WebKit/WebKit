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
#include "WPEInputMethodContextWaylandV3.h"

#include "WPEDisplayWaylandPrivate.h"
#include "WPEViewWayland.h"

#include "text-input-unstable-v3-client-protocol.h"

#include <algorithm>
#include <cstdlib>
#include <wayland-client-protocol.h>
#include <wtf/glib/WTFGType.h>
#include <xkbcommon/xkbcommon.h>

typedef struct _TextInputV3Global TextInputV3Global;
typedef struct _Preedit Preedit;

struct _TextInputV3Global {
    struct zwp_text_input_v3* textInput;

    WPEInputMethodContext* current;

    bool focused;

    uint32_t serial;
};

struct _Preedit {
    GUniquePtr<char> text;
    int32_t cursorBegin;
    int32_t cursorEnd;

    _Preedit& operator=(Preedit& other)
    {
        if (this != &other) {
            text.reset(g_strdup(other.text.get()));
            cursorBegin = other.cursorBegin;
            cursorEnd = other.cursorEnd;
        }
        return *this;
    }
};

struct _WPEIMContextWaylandV3Private {
    Preedit pendingPreedit;
    Preedit currentPreedit;

    GUniquePtr<char> pendingCommit;

    struct {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    } cursorRect;

    struct {
        GUniquePtr<char> text;
        int32_t cursorIndex;
        int32_t anchorIndex;
    } surrounding;

    enum zwp_text_input_v3_change_cause textChangeCause;

    struct {
        uint32_t beforeLength;
        uint32_t afterLength;
    } pendingSurroundingDelete;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEIMContextWaylandV3, wpe_im_context_wayland_v3, WPE_TYPE_INPUT_METHOD_CONTEXT, WPEInputMethodContext)

static TextInputV3Global* textInputV3GetGlobalByDisplay(WPEDisplayWayland*);

static TextInputV3Global* textInputV3GetGlobalByContext(WPEIMContextWaylandV3* self)
{
    if (wpe_input_method_context_get_view(WPE_INPUT_METHOD_CONTEXT(self)) == nullptr)
        return nullptr;

    auto* display = wpe_input_method_context_get_display(WPE_INPUT_METHOD_CONTEXT(self));
    auto* global = textInputV3GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));
    if (global->current != WPE_INPUT_METHOD_CONTEXT(self))
        return nullptr;
    if (global->textInput == nullptr)
        return nullptr;

    return global;
}

static uint32_t toTextInputV3Hints(WPEInputHints inputHints, WPEInputPurpose purpose)
{
    uint32_t hints = 0;

    if (inputHints & WPE_INPUT_HINT_SPELLCHECK)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
    if (inputHints & WPE_INPUT_HINT_WORD_COMPLETION)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
    if (inputHints & WPE_INPUT_HINT_LOWERCASE)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_CHARS)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_WORDS)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE;
    if (inputHints & WPE_INPUT_HINT_UPPERCASE_SENTENCES)
        hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;

    if (purpose == WPE_INPUT_PURPOSE_PIN || purpose == WPE_INPUT_PURPOSE_PASSWORD)
        hints |= (ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT | ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA);

    return hints;
}

static uint32_t toTextInputV3Purpose(WPEInputPurpose purpose)
{
    switch (purpose) {
    case WPE_INPUT_PURPOSE_FREE_FORM:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case WPE_INPUT_PURPOSE_ALPHA:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA;
    case WPE_INPUT_PURPOSE_DIGITS:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS;
    case WPE_INPUT_PURPOSE_NUMBER:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case WPE_INPUT_PURPOSE_PHONE:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case WPE_INPUT_PURPOSE_URL:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case WPE_INPUT_PURPOSE_EMAIL:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case WPE_INPUT_PURPOSE_NAME:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;
    case WPE_INPUT_PURPOSE_PASSWORD:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case WPE_INPUT_PURPOSE_PIN:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN;
    case WPE_INPUT_PURPOSE_TERMINAL:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL;
    default:
        g_assert_not_reached();
    }

    return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
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

static void textInputV3SetCursorRectangle(WPEIMContextWaylandV3* context)
{
    auto* global = textInputV3GetGlobalByContext(context);
    if (global == nullptr)
        return;

    zwp_text_input_v3_set_cursor_rectangle(global->textInput,
        context->priv->cursorRect.x,
        context->priv->cursorRect.y,
        context->priv->cursorRect.width,
        context->priv->cursorRect.height);
}

static void textInputV3SetSurroundingText(WPEIMContextWaylandV3* context)
{
    auto* global = textInputV3GetGlobalByContext(context);
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

    zwp_text_input_v3_set_surrounding_text(global->textInput,
        truncatedText ? truncatedText.get() : context->priv->surrounding.text.get(),
        cursorIndex, anchorIndex);
    zwp_text_input_v3_set_text_change_cause(global->textInput,
        context->priv->textChangeCause);
}

static void textInputV3SetContentType(WPEIMContextWaylandV3* context)
{
    WPEInputHints hints;
    WPEInputPurpose purpose;

    auto* global = textInputV3GetGlobalByContext(context);
    if (global == nullptr)
        return;

    g_object_get(context,
        "input-hints", &hints,
        "input-purpose", &purpose,
        nullptr);

    zwp_text_input_v3_set_content_type(global->textInput,
        toTextInputV3Hints(hints, purpose),
        toTextInputV3Purpose(purpose));
}

static void textInputV3CommitState(WPEIMContextWaylandV3* context)
{
    auto* global = textInputV3GetGlobalByContext(context);
    if (global == nullptr)
        return;

    global->serial++;
    zwp_text_input_v3_commit(global->textInput);
    context->priv->textChangeCause = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
}

static void textInputV3UpdateState(WPEIMContextWaylandV3* context, enum zwp_text_input_v3_change_cause cause)
{
    auto* global = textInputV3GetGlobalByContext(context);
    if (global == nullptr)
        return;

    context->priv->textChangeCause = cause;

    textInputV3SetSurroundingText(context);
    textInputV3SetContentType(context);
    textInputV3SetCursorRectangle(context);
    textInputV3CommitState(context);
}

static void textInputV3PreeditApply(TextInputV3Global* global, uint32_t serial)
{
    auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;

    bool valid = global->serial == serial;
    bool stateChanged = (priv->currentPreedit.text != nullptr) != (priv->pendingPreedit.text != nullptr);
    if (valid && stateChanged && !priv->currentPreedit.text)
        g_signal_emit_by_name(global->current, "preedit-started");

    priv->currentPreedit = priv->pendingPreedit;
    priv->pendingPreedit.text.reset(nullptr);
    priv->pendingPreedit.cursorBegin = priv->pendingPreedit.cursorEnd = 0;

    if (valid)
        g_signal_emit_by_name(global->current, "preedit-changed");

    if (valid && stateChanged && !priv->currentPreedit.text)
        g_signal_emit_by_name(global->current, "preedit-finished");
}

static void textInputV3CommitApply(TextInputV3Global* global, uint32_t serial)
{
    auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;

    bool valid = global->serial == serial;
    if (valid && priv->pendingCommit)
        g_signal_emit_by_name(global->current, "committed", priv->pendingCommit.get());
    g_clear_pointer(&priv->pendingCommit, g_free);
}

static void textInputV3DeleteSurroundingTextApply(TextInputV3Global* global, uint32_t serial)
{
    auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;

    bool valid = global->serial == serial;
    if ((valid && priv->pendingSurroundingDelete.beforeLength) || priv->pendingSurroundingDelete.afterLength) {
        g_signal_emit_by_name(global->current,
            "delete-surrounding",
            -priv->pendingSurroundingDelete.beforeLength,
            priv->pendingSurroundingDelete.beforeLength +
            priv->pendingSurroundingDelete.afterLength);
    }
    priv->pendingSurroundingDelete.beforeLength = 0;
    priv->pendingSurroundingDelete.afterLength = 0;
}

static void textInputV3Enable(WPEIMContextWaylandV3* context, TextInputV3Global* global)
{
    zwp_text_input_v3_enable(global->textInput);
    textInputV3UpdateState(context, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);

    // Mutter only pops up the OSK on >1 consecutive zwp_text_input_v3_enable() calls
    WPEInputHints hints;
    g_object_get(context, "input-hints", &hints, nullptr);
    if (!(hints & WPE_INPUT_HINT_INHIBIT_OSK)) {
        zwp_text_input_v3_enable(global->textInput);
        textInputV3CommitState(context);
    }
}

static void textInputV3Disable(WPEIMContextWaylandV3* context, TextInputV3Global* global)
{
    zwp_text_input_v3_disable(global->textInput);
    textInputV3CommitState(context);
}


static const struct zwp_text_input_v3_listener textInputListenerV3 = {
    // enter
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, struct wl_surface* /*surface*/)
    {
        auto* global = static_cast<TextInputV3Global*>(data);

        global->focused = true;

        if (global->current)
            textInputV3Enable(WPE_IM_CONTEXT_WAYLAND_V3(global->current), global);
    },
    // leave
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, struct wl_surface* /*surface*/)
    {
        auto* global = static_cast<TextInputV3Global*>(data);

        global->focused = false;

        if (global->current)
            textInputV3Disable(WPE_IM_CONTEXT_WAYLAND_V3(global->current), global);
    },
    // preedit_string
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, const char* text, int32_t cursorBegin, int32_t cursorEnd)
    {
        auto* global = static_cast<TextInputV3Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;
        Preedit* pendingPreedit = &priv->pendingPreedit;
        g_clear_pointer(&pendingPreedit->text, g_free);
        pendingPreedit->text.reset(g_strdup(text));
        pendingPreedit->cursorBegin = cursorBegin;
        pendingPreedit->cursorEnd = cursorEnd;
    },
    // commit_string
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, const char* text)
    {
        auto* global = static_cast<TextInputV3Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;
        g_clear_pointer(&priv->pendingCommit, g_free);
        priv->pendingCommit.reset(g_strdup(text));
    },
    // delete_surrounding_text
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, uint32_t beforeLength, uint32_t afterLength)
    {
        auto* global = static_cast<TextInputV3Global*>(data);
        if (global->current == nullptr)
            return;

        auto* priv = WPE_IM_CONTEXT_WAYLAND_V3(global->current)->priv;
        priv->pendingSurroundingDelete.beforeLength = beforeLength;
        priv->pendingSurroundingDelete.afterLength = afterLength;
    },
    // done
    [](void* data, struct zwp_text_input_v3* /*zwp_text_input_v3*/, uint32_t serial)
    {
        auto* global = static_cast<TextInputV3Global*>(data);
        if (global->current == nullptr)
            return;

        auto* context = WPE_IM_CONTEXT_WAYLAND_V3(global->current);

        bool hasPendingPreedit = g_strcmp0(context->priv->pendingPreedit.text.get(), context->priv->currentPreedit.text.get());
        bool hasPendingCommit = context->priv->pendingCommit != nullptr;
        bool updateIm = (hasPendingPreedit || hasPendingCommit);

        textInputV3DeleteSurroundingTextApply(global, serial);
        textInputV3CommitApply(global, serial);
        textInputV3PreeditApply(global, serial);

        if (updateIm && global->serial == serial)
            textInputV3UpdateState(context, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
    }
};

static void wpeIMContextWaylandV3GlobalFree(gpointer data)
{
    auto* global = static_cast<TextInputV3Global*>(data);
    g_free(global);
}

static TextInputV3Global* textInputV3GetGlobalByDisplay(WPEDisplayWayland *display)
{
    auto* global = static_cast<TextInputV3Global*>(g_object_get_data(G_OBJECT(display), "text-input-v3-global"));
    if (global != nullptr)
        return global;

    global = g_new0(TextInputV3Global, 1);
    global->textInput = wpeDisplayWaylandGetTextInputV3(display);

    if (global->textInput)
        zwp_text_input_v3_add_listener(global->textInput, &textInputListenerV3, global);

    g_object_set_data_full(G_OBJECT(display),
        "text-input-v3-global",
        global,
        wpeIMContextWaylandV3GlobalFree);
    return global;
}

static void wpeIMContextWaylandV3PurposeOrHintsChanged(WPEInputMethodContext *context)
{
    textInputV3SetContentType(WPE_IM_CONTEXT_WAYLAND_V3(context));
    textInputV3CommitState(WPE_IM_CONTEXT_WAYLAND_V3(context));
}

static void wpeIMContextWaylandV3Constructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_im_context_wayland_v3_parent_class)->constructed(object);

    auto* context = WPE_INPUT_METHOD_CONTEXT(object);
    auto* wpeContext = WPE_IM_CONTEXT_WAYLAND_V3(context);

    g_signal_connect_swapped(context, "notify::input-purpose", G_CALLBACK(wpeIMContextWaylandV3PurposeOrHintsChanged), wpeContext);
    g_signal_connect_swapped(context, "notify::input-hints", G_CALLBACK(wpeIMContextWaylandV3PurposeOrHintsChanged), wpeContext);
}

static void wpeIMContextWaylandV3Dispose(GObject* object)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(object);

    wpe_input_method_context_focus_out(WPE_INPUT_METHOD_CONTEXT(self));

    G_OBJECT_CLASS(wpe_im_context_wayland_v3_parent_class)->dispose(object);
}

static void wpeIMContextWaylandV3GetPreeditString(WPEInputMethodContext* context, char** text, GList** underlines, guint* cursorOffset)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(context);

    if (text != nullptr)
        *text = self->priv->currentPreedit.text ? g_strdup(self->priv->currentPreedit.text.get()) : g_strdup("");

    if (underlines != nullptr) {
        *underlines = nullptr;
        if (self->priv->currentPreedit.cursorBegin != self->priv->currentPreedit.cursorEnd) {
            *underlines =
                g_list_prepend(*underlines, wpe_input_method_underline_new(self->priv->currentPreedit.cursorBegin,
                    self->priv->currentPreedit.cursorEnd));
        }
    };

    if (cursorOffset)
        *cursorOffset = self->priv->currentPreedit.cursorBegin;
}

static void wpeIMContextWaylandV3FocusIn(WPEInputMethodContext* context)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* global = textInputV3GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));
    if (global->current == context)
        return;

    if (wpe_input_method_context_get_view(context) == nullptr)
        return;

    global->current = context;
    if (global->textInput == nullptr)
        return;

    if (global->focused)
        textInputV3Enable(self, global);
}

static void wpeIMContextWaylandV3FocusOut(WPEInputMethodContext *context)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(context);
    auto* global = textInputV3GetGlobalByContext(self);
    if (global == nullptr)
        return;

    if (global->focused)
        textInputV3Disable(self, global);

    global->current = nullptr;
}

static void wpeIMContextWaylandV3SetCursorArea(WPEInputMethodContext* context, int x, int y, int width, int height)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* global = textInputV3GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));

    if (self->priv->cursorRect.x == x && self->priv->cursorRect.y == y && self->priv->cursorRect.width == width && self->priv->cursorRect.height == height)
        return;

    self->priv->cursorRect.x = x;
    self->priv->cursorRect.y = y;
    self->priv->cursorRect.width = width;
    self->priv->cursorRect.height = height;

    if (global->current == context) {
        textInputV3SetCursorRectangle(self);
        textInputV3CommitState(self);
    }
}

static void wpeIMContextWaylandV3SetSurrounding(WPEInputMethodContext* context, const char* text, guint length, guint cursorIndex, guint selectionIndex)
{
    auto* self = WPE_IM_CONTEXT_WAYLAND_V3(context);
    auto* display = wpe_input_method_context_get_display(context);
    auto* global = textInputV3GetGlobalByDisplay(WPE_DISPLAY_WAYLAND(display));

    g_clear_pointer(&self->priv->surrounding.text, g_free);
    self->priv->surrounding.text.reset(g_strndup(text, length));
    self->priv->surrounding.cursorIndex = cursorIndex;
    self->priv->surrounding.anchorIndex = selectionIndex;

    if (global->current == context) {
        textInputV3SetSurroundingText(self);
        textInputV3CommitState(self);
    }
}

static void wpeIMContextWaylandV3Reset(WPEInputMethodContext *context)
{
    textInputV3UpdateState(WPE_IM_CONTEXT_WAYLAND_V3(context), ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
}

static void wpe_im_context_wayland_v3_class_init(WPEIMContextWaylandV3Class* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = wpeIMContextWaylandV3Constructed;
    objectClass->dispose = wpeIMContextWaylandV3Dispose;

    WPEInputMethodContextClass* imContextClass = WPE_INPUT_METHOD_CONTEXT_CLASS(klass);
    imContextClass->get_preedit_string = wpeIMContextWaylandV3GetPreeditString;
    imContextClass->focus_in = wpeIMContextWaylandV3FocusIn;
    imContextClass->focus_out = wpeIMContextWaylandV3FocusOut;
    imContextClass->set_cursor_area = wpeIMContextWaylandV3SetCursorArea;
    imContextClass->set_surrounding = wpeIMContextWaylandV3SetSurrounding;
    imContextClass->reset = wpeIMContextWaylandV3Reset;
}

WPEInputMethodContext* wpe_im_context_wayland_v3_new(WPEDisplayWayland* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);

    textInputV3GetGlobalByDisplay(display); // ensure creation of listener
    return WPE_INPUT_METHOD_CONTEXT(g_object_new(WPE_TYPE_IM_CONTEXT_WAYLAND_V3, nullptr));
}
