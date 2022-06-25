/*
 * Copyright (C) 2016 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitWebViewSessionState_h
#define WebKitWebViewSessionState_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW_SESSION_STATE (webkit_web_view_session_state_get_type())

typedef struct _WebKitWebViewSessionState WebKitWebViewSessionState;

WEBKIT_API GType
webkit_web_view_session_state_get_type  (void);

WEBKIT_API WebKitWebViewSessionState *
webkit_web_view_session_state_new       (GBytes *data);

WEBKIT_API WebKitWebViewSessionState *
webkit_web_view_session_state_ref       (WebKitWebViewSessionState *state);

WEBKIT_API void
webkit_web_view_session_state_unref     (WebKitWebViewSessionState *state);

WEBKIT_API GBytes *
webkit_web_view_session_state_serialize (WebKitWebViewSessionState *state);

G_END_DECLS

#endif
