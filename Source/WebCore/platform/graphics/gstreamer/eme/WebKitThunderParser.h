/* GStreamer Thunder Parser
 *
 * Copyright (C) 2025 Comcast Inc.
 * Copyright (C) 2025 Igalia S.L.
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

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)

#include <glib-object.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_THUNDER_PARSER          (webkit_media_thunder_parser_get_type())
#define WEBKIT_MEDIA_THUNDER_PARSER(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_THUNDER_PARSER, WebKitMediaThunderParser))

GType webkit_media_thunder_parser_get_type(void);

G_END_DECLS

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)
