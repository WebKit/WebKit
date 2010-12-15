/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-cache.h:
 *
 * Copyright (C) 2009, 2010 Igalia, S.L.
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

#ifndef WEBKIT_SOUP_CACHE_H
#define WEBKIT_SOUP_CACHE_H 1

#if BUILDING_GTK__
#include <webkit/webkitdefines.h>
#else
#ifndef WEBKIT_API
#define WEBKIT_API
#endif
#endif

#include <libsoup/soup-types.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_CACHE            (webkit_soup_cache_get_type ())
#define WEBKIT_SOUP_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_SOUP_CACHE, WebKitSoupCache))
#define WEBKIT_SOUP_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_SOUP_CACHE, WebKitSoupCacheClass))
#define WEBKIT_IS_SOUP_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_SOUP_CACHE))
#define WEBKIT_IS_SOUP_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), WEBKIT_TYPE_SOUP_CACHE))
#define WEBKIT_SOUP_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_SOUP_CACHE, WebKitSoupCacheClass))

typedef struct _WebKitSoupCache WebKitSoupCache;
typedef struct _WebKitSoupCachePrivate WebKitSoupCachePrivate;

typedef enum {
	WEBKIT_SOUP_CACHE_CACHEABLE = (1 << 0),
	WEBKIT_SOUP_CACHE_UNCACHEABLE = (1 << 1),
	WEBKIT_SOUP_CACHE_INVALIDATES = (1 << 2),
	WEBKIT_SOUP_CACHE_VALIDATES = (1 << 3)
} WebKitSoupCacheability;

typedef enum {
	WEBKIT_SOUP_CACHE_RESPONSE_FRESH,
	WEBKIT_SOUP_CACHE_RESPONSE_NEEDS_VALIDATION,
	WEBKIT_SOUP_CACHE_RESPONSE_STALE
} WebKitSoupCacheResponse;

typedef enum {
	WEBKIT_SOUP_CACHE_SINGLE_USER,
	WEBKIT_SOUP_CACHE_SHARED
} WebKitSoupCacheType;

struct _WebKitSoupCache {
	GObject parent_instance;

	WebKitSoupCachePrivate *priv;
};

typedef struct {
	GObjectClass parent_class;

	/* methods */
	WebKitSoupCacheability (*get_cacheability)(WebKitSoupCache *cache, SoupMessage *msg);

	/* Padding for future expansion */
	void (*_libsoup_reserved1)(void);
	void (*_libsoup_reserved2)(void);
	void (*_libsoup_reserved3)(void);
} WebKitSoupCacheClass;

WEBKIT_API GType             webkit_soup_cache_get_type (void);
WEBKIT_API WebKitSoupCache  *webkit_soup_cache_new (const char  *cache_dir,
						    WebKitSoupCacheType cache_type);
WEBKIT_API void              webkit_soup_cache_flush (WebKitSoupCache   *cache);
WEBKIT_API void              webkit_soup_cache_clear (WebKitSoupCache   *cache);

WEBKIT_API void              webkit_soup_cache_dump (WebKitSoupCache *cache);
WEBKIT_API void              webkit_soup_cache_load (WebKitSoupCache *cache);

WEBKIT_API void              webkit_soup_cache_set_max_size (WebKitSoupCache *cache,
							     guint max_size);
WEBKIT_API guint             webkit_soup_cache_get_max_size (WebKitSoupCache *cache);

G_END_DECLS


#endif /* WEBKIT_SOUP_CACHE_H */

