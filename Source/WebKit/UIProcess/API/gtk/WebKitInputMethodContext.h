/*
 * Copyright (C) 2012 Igalia S.L.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitInputMethodContext_h
#define WebKitInputMethodContext_h

#include <glib-object.h>
#include <gdk/gdk.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_INPUT_METHOD_CONTEXT            (webkit_input_method_context_get_type())
#define WEBKIT_INPUT_METHOD_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_INPUT_METHOD_CONTEXT, WebKitInputMethodContext))
#define WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT, WebKitInputMethodContextClass))
#define WEBKIT_IS_INPUT_METHOD_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_INPUT_METHOD_CONTEXT))
#define WEBKIT_IS_INPUT_METHOD_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT))
#define WEBKIT_INPUT_METHOD_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT, WebKitInputMethodContextClass))

#define WEBKIT_TYPE_INPUT_METHOD_UNDERLINE          (webkit_input_method_underline_get_type())

/**
 * WebKitInputPurpose:
 * @WEBKIT_INPUT_PURPOSE_FREE_FORM: Editable element expects any characters
 * @WEBKIT_INPUT_PURPOSE_DIGITS: Editable element expects digits
 * @WEBKIT_INPUT_PURPOSE_NUMBER: Editable element expects a number
 * @WEBKIT_INPUT_PURPOSE_PHONE: Editable element expects a telephone
 * @WEBKIT_INPUT_PURPOSE_URL: Editable element expects a URL
 * @WEBKIT_INPUT_PURPOSE_EMAIL: Editable element expects an email
 * @WEBKIT_INPUT_PURPOSE_PASSWORD: Editable element expects a password
 *
 * Enum values used to describe the primary purpose of the active editable element.
 *
 * Since: 2.28
 */
typedef enum {
    WEBKIT_INPUT_PURPOSE_FREE_FORM,
    WEBKIT_INPUT_PURPOSE_DIGITS,
    WEBKIT_INPUT_PURPOSE_NUMBER,
    WEBKIT_INPUT_PURPOSE_PHONE,
    WEBKIT_INPUT_PURPOSE_URL,
    WEBKIT_INPUT_PURPOSE_EMAIL,
    WEBKIT_INPUT_PURPOSE_PASSWORD
} WebKitInputPurpose;

/**
 * WebKitInputHints:
 * @WEBKIT_INPUT_HINT_NONE: No special behavior suggested
 * @WEBKIT_INPUT_HINT_SPELLCHECK: Suggest spell checking
 * @WEBKIT_INPUT_HINT_LOWERCASE: Suggest to not autocapitlize
 * @WEBKIT_INPUT_HINT_UPPERCASE_CHARS: Suggest to capitalize all text
 * @WEBKIT_INPUT_HINT_UPPERCASE_WORDS: Suggest to capitalize the first character of each word
 * @WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES: Suggest to capitalize the first word of each sentence
 * @WEBKIT_INPUT_HINT_INHIBIT_OSK: Suggest to not show an onscreen keyboard
 *
 * Enum values used to describe hints that might be taken into account by input methods.
 *
 * Since: 2.28
 */
typedef enum {
    WEBKIT_INPUT_HINT_NONE                = 0,
    WEBKIT_INPUT_HINT_SPELLCHECK          = 1 << 0,
    WEBKIT_INPUT_HINT_LOWERCASE           = 1 << 1,
    WEBKIT_INPUT_HINT_UPPERCASE_CHARS     = 1 << 2,
    WEBKIT_INPUT_HINT_UPPERCASE_WORDS     = 1 << 3,
    WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES = 1 << 4,
    WEBKIT_INPUT_HINT_INHIBIT_OSK         = 1 << 5
} WebKitInputHints;

typedef struct _WebKitInputMethodContext        WebKitInputMethodContext;
typedef struct _WebKitInputMethodContextClass   WebKitInputMethodContextClass;
typedef struct _WebKitInputMethodContextPrivate WebKitInputMethodContextPrivate;
typedef struct _WebKitInputMethodUnderline      WebKitInputMethodUnderline;

struct _WebKitInputMethodContext {
    GObject parent;

    /*< private >*/
    WebKitInputMethodContextPrivate *priv;
};

struct _WebKitInputMethodContextClass {
    GObjectClass parent_class;

    /* Signals */
    void     (* preedit_started)    (WebKitInputMethodContext *context);
    void     (* preedit_changed)    (WebKitInputMethodContext *context);
    void     (* preedit_finished)   (WebKitInputMethodContext *context);
    void     (* committed)          (WebKitInputMethodContext *context,
                                     const char               *text);
    void     (* delete_surrounding) (WebKitInputMethodContext *context,
                                     int                       offset,
                                     guint                     n_chars);

    /* Virtual functions */
    void     (* set_enable_preedit) (WebKitInputMethodContext *context,
                                     gboolean                  enabled);
    void     (* get_preedit)        (WebKitInputMethodContext *context,
                                     gchar                   **text,
                                     GList                   **underlines,
                                     guint                    *cursor_offset);
    gboolean (* filter_key_event)   (WebKitInputMethodContext *context,
                                     GdkEventKey              *key_event);
    void     (* notify_focus_in)    (WebKitInputMethodContext *context);
    void     (* notify_focus_out)   (WebKitInputMethodContext *context);
    void     (* notify_cursor_area) (WebKitInputMethodContext *context,
                                     int                       x,
                                     int                       y,
                                     int                       width,
                                     int                       height);
    void     (* notify_surrounding) (WebKitInputMethodContext *context,
                                     const gchar              *text,
                                     guint                     length,
                                     guint                     cursor_index,
                                     guint                     selection_index);
    void     (* reset)              (WebKitInputMethodContext *context);

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
    void (*_webkit_reserved4) (void);
    void (*_webkit_reserved5) (void);
    void (*_webkit_reserved6) (void);
    void (*_webkit_reserved7) (void);
};

WEBKIT_API GType
webkit_input_method_context_get_type           (void);

WEBKIT_API void
webkit_input_method_context_set_enable_preedit (WebKitInputMethodContext   *context,
                                                gboolean                    enabled);

WEBKIT_API void
webkit_input_method_context_get_preedit        (WebKitInputMethodContext   *context,
                                                char                      **text,
                                                GList                     **underlines,
                                                guint                      *cursor_offset);

WEBKIT_API gboolean
webkit_input_method_context_filter_key_event   (WebKitInputMethodContext   *context,
                                                GdkEventKey                *key_event);

WEBKIT_API void
webkit_input_method_context_notify_focus_in    (WebKitInputMethodContext   *context);

WEBKIT_API void
webkit_input_method_context_notify_focus_out   (WebKitInputMethodContext   *context);

WEBKIT_API void
webkit_input_method_context_notify_cursor_area (WebKitInputMethodContext   *context,
                                                int                         x,
                                                int                         y,
                                                int                         width,
                                                int                         height);

WEBKIT_API void
webkit_input_method_context_notify_surrounding (WebKitInputMethodContext   *context,
                                                const gchar                *text,
                                                int                         length,
                                                guint                       cursor_index,
                                                guint                       selection_index);

WEBKIT_API void
webkit_input_method_context_reset              (WebKitInputMethodContext   *context);


WEBKIT_API GType
webkit_input_method_underline_get_type         (void);

WEBKIT_API WebKitInputMethodUnderline *
webkit_input_method_underline_new              (guint                       start_offset,
                                                guint                       end_offset);

WEBKIT_API WebKitInputMethodUnderline *
webkit_input_method_underline_copy             (WebKitInputMethodUnderline *underline);

WEBKIT_API void
webkit_input_method_underline_free             (WebKitInputMethodUnderline *underline);

WEBKIT_API void
webkit_input_method_underline_set_color        (WebKitInputMethodUnderline *underline,
                                                const GdkRGBA              *rgba);

WEBKIT_API WebKitInputPurpose
webkit_input_method_context_get_input_purpose  (WebKitInputMethodContext   *context);

WEBKIT_API void
webkit_input_method_context_set_input_purpose  (WebKitInputMethodContext   *context,
                                                WebKitInputPurpose          purpose);

WEBKIT_API WebKitInputHints
webkit_input_method_context_get_input_hints    (WebKitInputMethodContext   *context);

WEBKIT_API void
webkit_input_method_context_set_input_hints    (WebKitInputMethodContext   *context,
                                                WebKitInputHints            hints);


G_END_DECLS

#endif
