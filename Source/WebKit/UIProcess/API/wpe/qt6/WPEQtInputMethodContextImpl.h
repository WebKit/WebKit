// SPDX-License-Identifier: MIT
/*
 * Copyright (C) Leica Geosystems AG
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
 */

#pragma once

#include <glib-object.h>
#include <wpe/WPEInputMethodContext.h>

typedef struct _WPEView WPEView;

G_BEGIN_DECLS

#define WPE_TYPE_INPUT_METHOD_CONTEXT_QT (qt_input_method_context_impl_wpe_get_type())
G_DECLARE_FINAL_TYPE(QtInputMethodContextImplWPE, qt_input_method_context_impl_wpe, WPE, INPUT_METHOD_CONTEXT_QT, WPEInputMethodContext)

WPEInputMethodContext* qt_input_method_context_impl_wpe_new();

void wpe_set_preedit(WPEInputMethodContext* ctx, const char* text, GList* underlines, int32_t cursor_index);

char* wpe_get_surrounding_text(WPEInputMethodContext*);
uint32_t wpe_get_surrounding_cursor_index(WPEInputMethodContext*);
uint32_t wpe_get_surrounding_anchor_index(WPEInputMethodContext*);
WPEInputHints wpe_get_hints(WPEInputMethodContext*);
WPEInputPurpose wpe_get_purpose(WPEInputMethodContext*);

G_END_DECLS
