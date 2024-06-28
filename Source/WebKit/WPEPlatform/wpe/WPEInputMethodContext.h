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

#ifndef WPEInputMethodContext_h
#define WPEInputMethodContext_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEColor.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_INPUT_METHOD_UNDERLINE (wpe_input_method_underline_get_type())
#define WPE_TYPE_INPUT_METHOD_CONTEXT   (wpe_input_method_context_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEInputMethodContext, wpe_input_method_context, WPE, INPUT_METHOD_CONTEXT, GObject)

typedef struct _WPEDisplay WPEDisplay;
typedef struct _WPEEvent WPEEvent;
typedef struct _WPEView WPEView;

/**
 * WPEInputPurpose:
 * @WPE_INPUT_PURPOSE_FREE_FORM: Allow any character
 * @WPE_INPUT_PURPOSE_ALPHA: Allow only alphabetic characters
 * @WPE_INPUT_PURPOSE_DIGITS: Allow only digits
 * @WPE_INPUT_PURPOSE_NUMBER: Edited field expects numbers
 * @WPE_INPUT_PURPOSE_PHONE: Edited field expects phone number
 * @WPE_INPUT_PURPOSE_URL: Edited field expects URL
 * @WPE_INPUT_PURPOSE_EMAIL: Edited field expects email address
 * @WPE_INPUT_PURPOSE_NAME: Edited field expects the name of a person
 * @WPE_INPUT_PURPOSE_PASSWORD: Like %WPE_INPUT_PURPOSE_FREE_FORM, but characters are hidden
 * @WPE_INPUT_PURPOSE_PIN: Like %WPE_INPUT_PURPOSE_DIGITS, but characters are hidden
 * @WPE_INPUT_PURPOSE_TERMINAL: Allow any character, in addition to control codes
 *
 * Describes primary purpose of the input widget.
 *
 * This information is useful for on-screen keyboards and similar input
 * methods to decide which keys should be presented to the user.
 */
typedef enum
{
  WPE_INPUT_PURPOSE_FREE_FORM,
  WPE_INPUT_PURPOSE_ALPHA,
  WPE_INPUT_PURPOSE_DIGITS,
  WPE_INPUT_PURPOSE_NUMBER,
  WPE_INPUT_PURPOSE_PHONE,
  WPE_INPUT_PURPOSE_URL,
  WPE_INPUT_PURPOSE_EMAIL,
  WPE_INPUT_PURPOSE_NAME,
  WPE_INPUT_PURPOSE_PASSWORD,
  WPE_INPUT_PURPOSE_PIN,
  WPE_INPUT_PURPOSE_TERMINAL,
} WPEInputPurpose;

/**
 * WPEInputHints:
 * @WPE_INPUT_HINT_NONE: No special behaviour suggested
 * @WPE_INPUT_HINT_SPELLCHECK: Suggest checking for typos
 * @WPE_INPUT_HINT_NO_SPELLCHECK: Suggest not checking for typos
 * @WPE_INPUT_HINT_WORD_COMPLETION: Suggest word completion
 * @WPE_INPUT_HINT_LOWERCASE: Suggest to convert all text to lowercase
 * @WPE_INPUT_HINT_UPPERCASE_CHARS: Suggest to capitalize all text
 * @WPE_INPUT_HINT_UPPERCASE_WORDS: Suggest to capitalize the first
 *   character of each word
 * @WPE_INPUT_HINT_UPPERCASE_SENTENCES: Suggest to capitalize the
 *   first word of each sentence
 * @WPE_INPUT_HINT_INHIBIT_OSK: Suggest to not show an onscreen keyboard
 *   (e.g for a calculator that already has all the keys).
 * @WPE_INPUT_HINT_VERTICAL_WRITING: The text is vertical
 * @WPE_INPUT_HINT_EMOJI: Suggest offering Emoji support
 * @WPE_INPUT_HINT_NO_EMOJI: Suggest not offering Emoji support
 * @WPE_INPUT_HINT_PRIVATE: Request that the input method should not
 *    update personalized data (like typing history)
 *
 * Describes hints that might be taken into account by input methods
 * or applications.
 *
 * Note that input methods may already tailor their behaviour according
 * to the [enum@InputPurpose] of the entry.
 *
 * Some common sense is expected when using these flags - mixing
 * %WPE_INPUT_HINT_LOWERCASE with any of the uppercase hints makes no sense.
 */
typedef enum
{
  WPE_INPUT_HINT_NONE                = 0,
  WPE_INPUT_HINT_SPELLCHECK          = 1 << 0,
  WPE_INPUT_HINT_NO_SPELLCHECK       = 1 << 1,
  WPE_INPUT_HINT_WORD_COMPLETION     = 1 << 2,
  WPE_INPUT_HINT_LOWERCASE           = 1 << 3,
  WPE_INPUT_HINT_UPPERCASE_CHARS     = 1 << 4,
  WPE_INPUT_HINT_UPPERCASE_WORDS     = 1 << 5,
  WPE_INPUT_HINT_UPPERCASE_SENTENCES = 1 << 6,
  WPE_INPUT_HINT_INHIBIT_OSK         = 1 << 7,
  WPE_INPUT_HINT_VERTICAL_WRITING    = 1 << 8,
  WPE_INPUT_HINT_EMOJI               = 1 << 9,
  WPE_INPUT_HINT_NO_EMOJI            = 1 << 10,
  WPE_INPUT_HINT_PRIVATE             = 1 << 11,
} WPEInputHints;

typedef struct _WPEInputMethodUnderline WPEInputMethodUnderline;

struct _WPEInputMethodContextClass
{
    GObjectClass parent_class;

    void     (* get_preedit_string) (WPEInputMethodContext *context,
                                     gchar                 **text,
                                     GList                 **underlines,
                                     guint                 *cursor_offset);
    gboolean (* filter_key_event)   (WPEInputMethodContext *context,
			                         WPEEvent              *event);
    void     (* focus_in)           (WPEInputMethodContext *context);
    void     (* focus_out)          (WPEInputMethodContext *context);
    void     (* set_cursor_area)    (WPEInputMethodContext *context,
                                     int                    x,
                                     int                    y,
                                     int                    width,
                                     int                    height);
    void     (* set_surrounding)    (WPEInputMethodContext *context,
                                     const gchar           *text,
                                     guint                  length,
                                     guint                  cursor_index,
                                     guint                  selection_index);
    void     (* reset)              (WPEInputMethodContext *context);

    gpointer padding[32];
};

WPE_API WPEInputMethodContext   *wpe_input_method_context_new                (WPEView                 *view);
WPE_API WPEView                 *wpe_input_method_context_get_view           (WPEInputMethodContext   *context);
WPE_API WPEDisplay              *wpe_input_method_context_get_display        (WPEInputMethodContext   *context);
WPE_API void                     wpe_input_method_context_get_preedit_string (WPEInputMethodContext   *context,
                                                                              char                    **text,
                                                                              GList                   **underlines,
                                                                              guint                   *cursor_offset);
WPE_API gboolean                 wpe_input_method_context_filter_key_event   (WPEInputMethodContext   *context,
                                                                              WPEEvent                *event);
WPE_API void                     wpe_input_method_context_focus_in           (WPEInputMethodContext   *context);
WPE_API void                     wpe_input_method_context_focus_out          (WPEInputMethodContext   *context);
WPE_API void                     wpe_input_method_context_set_cursor_area    (WPEInputMethodContext   *context,
                                                                              int                      x,
                                                                              int                      y,
                                                                              int                      width,
                                                                              int                      height);
WPE_API void                     wpe_input_method_context_set_surrounding    (WPEInputMethodContext   *context,
                                                                              const char              *text,
                                                                              guint                    length,
                                                                              guint                    cursor_index,
                                                                              guint                    selection_index);
WPE_API void                     wpe_input_method_context_reset              (WPEInputMethodContext   *context);

WPE_API GType                    wpe_input_method_underline_get_type         (void);
WPE_API WPEInputMethodUnderline *wpe_input_method_underline_new              (guint                    start_offset,
                                                                              guint                    end_offset);
WPE_API WPEInputMethodUnderline *wpe_input_method_underline_copy             (WPEInputMethodUnderline *underline);
WPE_API void                     wpe_input_method_underline_free             (WPEInputMethodUnderline *underline);
WPE_API guint                    wpe_input_method_underline_get_start_offset (WPEInputMethodUnderline *underline);
WPE_API guint                    wpe_input_method_underline_get_end_offset   (WPEInputMethodUnderline *underline);
WPE_API void                     wpe_input_method_underline_set_color        (WPEInputMethodUnderline *underline,
                                                                              const WPEColor          *color);
WPE_API const WPEColor          *wpe_input_method_underline_get_color        (WPEInputMethodUnderline *underline);

G_END_DECLS

#endif /* WPEInputMethodContext_h */
