/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-cache-private.h:
 *
 * Copyright (C) 2010 Igalia, S.L.
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

#ifndef WEBKIT_SOUP_CACHE_PRIVATE_H
#define WEBKIT_SOUP_CACHE_PRIVATE_H 1

#include "soup-cache.h"
#include <libsoup/soup-message.h>

G_BEGIN_DECLS

WebKitSoupCacheResponse webkit_soup_cache_has_response (WebKitSoupCache   *cache,
							SoupMessage *msg);
GInputStream *webkit_soup_cache_send_response (WebKitSoupCache   *cache,
					       SoupMessage *msg);
WebKitSoupCacheability  webkit_soup_cache_get_cacheability (WebKitSoupCache   *cache,
							    SoupMessage *msg);
SoupMessage *webkit_soup_cache_generate_conditional_request (WebKitSoupCache   *cache,
							     SoupMessage *original);

G_END_DECLS

#endif /* WEBKIT_SOUP_CACHE_PRIVATE_H */
