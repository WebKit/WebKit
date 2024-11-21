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
#include "WPEInputMethodContext.h"

#include "WPEDisplayPrivate.h"
#include "WPEEnumTypes.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEInputMethodUnderline {
    _WPEInputMethodUnderline(unsigned startOffset, unsigned endOffset)
        : startOffset(startOffset)
        , endOffset(endOffset)
    {
    }

    guint startOffset { 0 };
    guint endOffset { 0 };
    WPEColor color { 0., 0., 0., 1. };
};

/**
 * WPEInputMethodUnderline:
 *
 * Range of text in an preedit string to be shown underlined.
 */

G_DEFINE_BOXED_TYPE(WPEInputMethodUnderline, wpe_input_method_underline, wpe_input_method_underline_copy, wpe_input_method_underline_free)

/**
 * wpe_input_method_underline_new:
 * @start_offset: the start offset in preedit string
 * @end_offset: the end offset in preedit string
 *
 * Create a new #WPEInputMethodUnderline for the given range in preedit string
 *
 * Returns: (transfer full): A newly created #WPEInputMethodUnderline
 */
WPEInputMethodUnderline* wpe_input_method_underline_new(guint startOffset, guint endOffset)
{
    auto* underline = static_cast<WPEInputMethodUnderline*>(fastMalloc(sizeof(WPEInputMethodUnderline)));
    new (underline) WPEInputMethodUnderline(startOffset, endOffset);
    return underline;
}

/**
 * wpe_input_method_underline_copy:
 * @underline: a #WPEInputMethodUnderline
 *
 * Make a copy of the #WPEInputMethodUnderline.
 *
 * Returns: (transfer full): A copy of passed in #WPEInputMethodUnderline
 */
WPEInputMethodUnderline* wpe_input_method_underline_copy(WPEInputMethodUnderline* underline)
{
    g_return_val_if_fail(underline, nullptr);

    auto* copyUnderline = static_cast<WPEInputMethodUnderline*>(fastMalloc(sizeof(WPEInputMethodUnderline)));
    new (copyUnderline) WPEInputMethodUnderline(underline->startOffset, underline->endOffset);
    return copyUnderline;
}

/**
 * wpe_input_method_underline_free:
 * @underline: A #WPEInputMethodUnderline
 *
 * Free the #WPEInputMethodUnderline.
 */
void wpe_input_method_underline_free(WPEInputMethodUnderline* underline)
{
    g_return_if_fail(underline);

    underline->~WPEInputMethodUnderline();
    fastFree(underline);
}

/**
 * wpe_input_method_underline_get_start_offset:
 * @underline: a #WPEInputMethodUnderline
 *
 * Returns: the underline start offset
 */
guint wpe_input_method_underline_get_start_offset(WPEInputMethodUnderline* underline)
{
    g_return_val_if_fail(underline, 0);

    return underline->startOffset;
}

/**
 * wpe_input_method_underline_get_end_offset:
 * @underline: a #WPEInputMethodUnderline
 *
 * Returns: the underline end offset
 */
guint wpe_input_method_underline_get_end_offset(WPEInputMethodUnderline* underline)
{
    g_return_val_if_fail(underline, 0);

    return underline->endOffset;
}

/**
 * wpe_input_method_underline_set_color:
 * @underline: a #WPEInputMethodUnderline
 * @color: (nullable): a #WPEColor or %NULL
 *
 * Set the color of the underline. If @color is %NULL the foreground text color will be used
 * for the underline too.
 */
void wpe_input_method_underline_set_color(WPEInputMethodUnderline* underline, const WPEColor* color)
{
    if (!color) {
        underline->color.red = color->red;
        underline->color.green = color->green;
        underline->color.blue = color->blue;
        underline->color.alpha = color->alpha;
    }
}

/**
 * wpe_input_method_underline_get_color:
 * @underline: a #WPEInputMethodUnderline
 *
 * Returns: the underline color.
 */
const WPEColor* wpe_input_method_underline_get_color(WPEInputMethodUnderline* underline)
{
    g_return_val_if_fail(underline, nullptr);

    return &underline->color;
}

struct _WPEInputMethodContextPrivate {
    WPEView* view;
    WPEInputPurpose purpose;
    WPEInputHints hints;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEInputMethodContext, wpe_input_method_context, G_TYPE_OBJECT)

enum {
    PREEDIT_STARTED,
    PREEDIT_CHANGED,
    PREEDIT_FINISHED,
    COMMITTED,
    DELETE_SURROUNDING,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_INPUT_PURPOSE,
    PROP_INPUT_HINTS,
    N_PROPERTIES
};

static std::array<unsigned, LAST_SIGNAL> signals;
static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void wpeInputMethodContextSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* ime = WPE_INPUT_METHOD_CONTEXT(object);

    switch (propId) {
    case PROP_INPUT_PURPOSE:
        if (ime->priv->purpose != g_value_get_enum(value)) {
            ime->priv->purpose = static_cast<WPEInputPurpose>(g_value_get_enum(value));
            g_object_notify_by_pspec(object, paramSpec);
        }
        break;
    case PROP_INPUT_HINTS:
        if (ime->priv->hints != g_value_get_flags(value))  {
            ime->priv->hints =  static_cast<WPEInputHints>(g_value_get_flags(value));
            g_object_notify_by_pspec(object, paramSpec);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeInputMethodContextGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* ime = WPE_INPUT_METHOD_CONTEXT(object);

    switch (propId) {
    case PROP_INPUT_PURPOSE:
        g_value_set_enum(value, ime->priv->purpose);
        break;
    case PROP_INPUT_HINTS:
        g_value_set_flags(value, ime->priv->hints);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpe_input_method_context_class_init(WPEInputMethodContextClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->set_property = wpeInputMethodContextSetProperty;
    objectClass->get_property = wpeInputMethodContextGetProperty;

    /**
     * WPEInputMethodContext:input-purpose:
     *
     * The purpose of the text field that the #WPEInputMethodContext is connected to.
     *
     * This property can be used by on-screen keyboards and other input
     * methods to adjust their behaviour.
     */
    sObjProperties[PROP_INPUT_PURPOSE] =
        g_param_spec_enum(
            "input-purpose",
            nullptr, nullptr,
            WPE_TYPE_INPUT_PURPOSE,
            WPE_INPUT_PURPOSE_FREE_FORM,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

    /**
     * WPEInputMethodContext:input-hints:
     *
     * Additional hints that allow input methods to fine-tune
     * their behaviour.
     */
    sObjProperties[PROP_INPUT_HINTS] =
        g_param_spec_flags(
            "input-hints",
            nullptr, nullptr,
            WPE_TYPE_INPUT_HINTS,
            WPE_INPUT_HINT_NONE,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());

     /**
     * WPEInputMethodContextClass::preedit-started:
     * @context: the #WPEInputMethodContextClass on which the signal is emitted
     *
     * Emitted when a new preediting sequence starts.
     */
    signals[PREEDIT_STARTED] = g_signal_new(
        "preedit-started",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEInputMethodContext::preedit-changed:
     * @context: the #WPEInputMethodContext on which the signal is emitted
     *
     * Emitted whenever the preedit sequence currently being entered has changed.
     * It is also emitted at the end of a preedit sequence, in which case
     * wpe_input_method_context_get_preedit() returns the empty string.
     */
    signals[PREEDIT_CHANGED] = g_signal_new(
        "preedit-changed",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEInputMethodContext::preedit-finished:
     * @context: the #WPEInputMethodContext on which the signal is emitted
     *
     * Emitted when a preediting sequence has been completed or canceled.
     */
    signals[PREEDIT_FINISHED] = g_signal_new(
        "preedit-finished",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEInputMethodContext::committed:
     * @context: the #WPEInputMethodContext on which the signal is emitted
     * @text: the string result
     *
     * Emitted when a complete input sequence has been entered by the user.
     * This can be a single character immediately after a key press or the
     * final result of preediting.
     */
    signals[COMMITTED] = g_signal_new(
        "committed",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        G_TYPE_STRING);

    /**
     * WPEInputMethodContext::delete-surrounding:
     * @context: the #WPEInputMethodContext on which the signal is emitted
     * @offset: the character offset from the cursor position of the text to be deleted.
     * @n_chars: the number of characters to be deleted
     *
     * Emitted when the input method wants to delete the context surrounding the cursor.
     * If @offset is a negative value, it means a position before the cursor.
     */
    signals[DELETE_SURROUNDING] = g_signal_new(
        "delete-surrounding",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        G_TYPE_INT,
        G_TYPE_UINT);
}

/**
 * wpe_input_method_context_new:
 * @view: a #WPEView
 *
 * Create a new #WPEInputMethodContext for @view
 *
 * Returns: (transfer full): a #WPEInputMethodContext
 */
WPEInputMethodContext* wpe_input_method_context_new(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    auto display = wpe_view_get_display(view);
    auto imeContext = wpeDisplayCreateInputMethodContext(display);
    imeContext->priv->view = view;
    return imeContext;
}

/**
 * wpe_input_method_context_get_view:
 * @context: a #WPEInputMethodContext
 *
 * Get the #WPEView of @context
 *
 * Returns: (transfer none): a #WPEView
 */

WPEView* wpe_input_method_context_get_view(WPEInputMethodContext   *context)
{
    g_return_val_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context), nullptr);

    return context->priv->view;
}

/**
 * wpe_input_method_context_get_display:
 * @context: a #WPEInputMethodContext
 *
 * Get the #WPEDisplay of @context
 *
 * Returns: (transfer none): a #WPEDisplay
 */
WPEDisplay* wpe_input_method_context_get_display(WPEInputMethodContext* context)
{
    g_return_val_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context), nullptr);

    return wpe_view_get_display(context->priv->view);
}

/**
 * wpe_input_method_context_get_preedit_string:
 * @context: a #WPEInputMethodContext
 * @text: (out) (transfer full) (nullable): location to store the preedit string
 * @underlines: (out) (transfer full) (nullable) (element-type WPEInputMethodUnderline): location to store the underlines as a #GList of #WPEInputMethodUnderline
 * @cursor_offset: (out) (nullable): location to store the position of cursor in preedit string
 *
 * Get the pre-edit string and a list of WPEInputMethodUnderline.
 *
 * Get the current pre-edit string for the @context, and a list of WPEInputMethodUnderline to apply to the string.
 * The string will be displayed inserted at @cursor_offset.
 */
void wpe_input_method_context_get_preedit_string(WPEInputMethodContext* context, char **text, GList** underlines, guint* cursorOffset)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->get_preedit_string)
        imeClass->get_preedit_string(context, text, underlines, cursorOffset);
}

/**
 * wpe_input_method_context_filter_key_event:
 * @context: a #WPEInputMethodContext
 * @event: the key event
 *
 * Allow an input method to internally handle key press and release
 * events. If this function returns %TRUE, then no further processing
 * should be done for this key event.
 *
 * Returns: %TRUE if the input method handled the key event.
 **/
gboolean wpe_input_method_context_filter_key_event(WPEInputMethodContext* context, WPEEvent* event)
{
    g_return_val_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context), false);

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->filter_key_event)
        return imeClass->filter_key_event(context, event);
    return FALSE;
}

/**
 * wpe_input_method_context_focus_in:
 * @context: a #WPEInputMethodContext
 *
 * Notify @context that input associated has gained focus.
 */
void wpe_input_method_context_focus_in(WPEInputMethodContext* context)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->focus_in)
        imeClass->focus_in(context);
}

/**
 * wpe_input_method_context_focus_out:
 * @context: a #WPEInputMethodContext
 *
 * Notify @context that input associated has lost focus.
 */
void wpe_input_method_context_focus_out(WPEInputMethodContext* context)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->focus_out)
        imeClass->focus_out(context);
}

/**
 * wpe_input_method_context_set_cursor_area:
 * @context: a #WPEInputMethodContext
 * @x: the x coordinate of cursor location
 * @y: the y coordinate of cursor location
 * @width: the width of cursor area
 * @height: the height of cursor area
 *
 * Notify @context that cursor area changed in input associated.
 */
void wpe_input_method_context_set_cursor_area(WPEInputMethodContext* context, int x, int y, int width, int height)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->set_cursor_area)
        imeClass->set_cursor_area(context, x, y, width, height);
}

/**
 * wpe_input_method_context_set_surrounding:
 * @context: a #WPEInputMethodContext
 * @text: text surrounding the insertion point
 * @length: the length of @text, or -1 if @text is nul-terminated
 * @cursor_index: the byte index of the insertion cursor within @text.
 * @selection_index: the byte index of the selection cursor within @text.
 *
 * Notify @context that the context surrounding the cursor has changed.
 *
 * If there's no selection @selection_index is the same as @cursor_index.
 */
void wpe_input_method_context_set_surrounding(WPEInputMethodContext* context, const char* text, guint length, guint cursorIndex, guint selectionIndex)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->set_surrounding)
        imeClass->set_surrounding(context, text, length, cursorIndex, selectionIndex);
}

/**
 * wpe_input_method_context_reset:
 * @context: a #WPEInputMethodContext
 *
 * Reset the @context.
 *
 * This will typically cause the input to clear the preedit state.
 */
void wpe_input_method_context_reset(WPEInputMethodContext* context)
{
    g_return_if_fail(WPE_IS_INPUT_METHOD_CONTEXT(context));

    auto* imeClass = WPE_INPUT_METHOD_CONTEXT_GET_CLASS(context);
    if (imeClass->reset)
        imeClass->reset(context);
}
