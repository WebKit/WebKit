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

#pragma once

#include "WebKitInputMethodContext.h"

#if ENABLE(WPE_PLATFORM)

#include <glib-object.h>

typedef struct _WPEView WPEView;

G_BEGIN_DECLS

#define WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_WPE (webkit_input_method_context_impl_wpe_get_type())
G_DECLARE_FINAL_TYPE (WebKitInputMethodContextImplWPE, webkit_input_method_context_impl_wpe, WEBKIT, INPUT_METHOD_CONTEXT_IMPL_WPE, WebKitInputMethodContext)

WebKitInputMethodContext* webkitInputMethodContextImplWPENew(WPEView *view);

G_END_DECLS

#endif // ENABLE(WPE_PLATFORM)
