/*
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
 * Copyright (C) 2026 Leica Geosystems AG
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib-object.h>
#include <wpe/WPEInputMethodContext.h>

typedef struct _WPEView WPEView;

G_BEGIN_DECLS

#define WPE_TYPE_INPUT_METHOD_CONTEXT_QT (qt_input_method_context_impl_wpe_get_type())
G_DECLARE_FINAL_TYPE(QtInputMethodContextImplWPE, qt_input_method_context_impl_wpe, WPE, INPUT_METHOD_CONTEXT_QT, WPEInputMethodContext)

WPEInputMethodContext* qt_input_method_context_impl_wpe_new();

char* wpe_get_surrounding_text(WPEInputMethodContext*);
uint32_t wpe_get_surrounding_cursor_index(WPEInputMethodContext*);
uint32_t wpe_get_surrounding_anchor_index(WPEInputMethodContext*);
bool wpe_input_method_context_keyboard_session_active(WPEInputMethodContext*);
WPEInputHints wpe_get_hints(WPEInputMethodContext*);
WPEInputPurpose wpe_get_purpose(WPEInputMethodContext*);
void wpe_get_cursor_rect(WPEInputMethodContext*, int* x, int* y, int* width, int* height);

G_END_DECLS
