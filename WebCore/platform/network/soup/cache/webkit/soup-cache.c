/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-cache.c
 *
 * Copyright (C) 2009, 2010 Igalia S.L.
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

/* TODO:
 * - Need to hook the feature in the sync SoupSession.
 * - Need more tests.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "soup-cache.h"
#include "soup-cache-private.h"
#include <libsoup/soup.h>
#include <gio/gio.h>
#include <stdlib.h>

static SoupSessionFeatureInterface *webkit_soup_cache_default_feature_interface;
static void webkit_soup_cache_session_feature_init (SoupSessionFeatureInterface *feature_interface, gpointer interface_data);

#define DEFAULT_MAX_SIZE 50 * 1024 * 1024
#define MAX_ENTRY_DATA_PERCENTAGE 10 /* Percentage of the total size
	                                of the cache that can be
	                                filled by a single entry */

typedef struct _WebKitSoupCacheEntry {
	char *key;
	char *filename;
	guint freshness_lifetime;
	gboolean must_revalidate;
	GString *data;
	gsize pos;
	gsize length;
	time_t corrected_initial_age;
	time_t response_time;
	gboolean writing;
	gboolean dirty;
	gboolean got_body;
	gboolean being_validated;
	SoupMessageHeaders *headers;
	GOutputStream *stream;
	GError *error;
	guint hits;
	GCancellable *cancellable;
} WebKitSoupCacheEntry;

struct _WebKitSoupCachePrivate {
	char *cache_dir;
	GHashTable *cache;
	guint n_pending;
	SoupSession *session;
	WebKitSoupCacheType cache_type;
	guint size;
	guint max_size;
	guint max_entry_data_size; /* Computed value. Here for performance reasons */
	GList *lru_start;
};

typedef struct {
	WebKitSoupCache *cache;
	WebKitSoupCacheEntry *entry;
	SoupMessage *msg;
	gulong got_chunk_handler;
	gulong got_body_handler;
	gulong restarted_handler;
} WebKitSoupCacheWritingFixture;

enum {
	PROP_0,
	PROP_CACHE_DIR,
	PROP_CACHE_TYPE
};

#define WEBKIT_SOUP_CACHE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), WEBKIT_TYPE_SOUP_CACHE, WebKitSoupCachePrivate))

G_DEFINE_TYPE_WITH_CODE (WebKitSoupCache, webkit_soup_cache, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE,
						webkit_soup_cache_session_feature_init))

static gboolean webkit_soup_cache_entry_remove (WebKitSoupCache *cache, WebKitSoupCacheEntry *entry);
static void make_room_for_new_entry (WebKitSoupCache *cache, guint length_to_add);
static gboolean cache_accepts_entries_of_size (WebKitSoupCache *cache, guint length_to_add);

static WebKitSoupCacheability
get_cacheability (WebKitSoupCache *cache, SoupMessage *msg)
{
	WebKitSoupCacheability cacheability;
	const char *cache_control;

	/* 1. The request method must be cacheable */
	if (msg->method == SOUP_METHOD_GET)
		cacheability = WEBKIT_SOUP_CACHE_CACHEABLE;
	else if (msg->method == SOUP_METHOD_HEAD ||
		 msg->method == SOUP_METHOD_TRACE ||
		 msg->method == SOUP_METHOD_CONNECT)
		return WEBKIT_SOUP_CACHE_UNCACHEABLE;
	else
		return (WEBKIT_SOUP_CACHE_UNCACHEABLE | WEBKIT_SOUP_CACHE_INVALIDATES);

	cache_control = soup_message_headers_get (msg->response_headers, "Cache-Control");
	if (cache_control) {
		GHashTable *hash;
		WebKitSoupCachePrivate *priv = WEBKIT_SOUP_CACHE_GET_PRIVATE (cache);

		hash = soup_header_parse_param_list (cache_control);

		/* Shared caches MUST NOT store private resources */
		if (priv->cache_type == WEBKIT_SOUP_CACHE_SHARED) {
			if (g_hash_table_lookup_extended (hash, "private", NULL, NULL)) {
				soup_header_free_param_list (hash);
				return WEBKIT_SOUP_CACHE_UNCACHEABLE;
			}
		}

		/* 2. The 'no-store' cache directive does not appear in the
		 * headers
		 */
		if (g_hash_table_lookup_extended (hash, "no-store", NULL, NULL)) {
			soup_header_free_param_list (hash);
			return WEBKIT_SOUP_CACHE_UNCACHEABLE;
		}

		/* This does not appear in section 2.1, but I think it makes
		 * sense to check it too?
		 */
		if (g_hash_table_lookup_extended (hash, "no-cache", NULL, NULL)) {
			soup_header_free_param_list (hash);
			return WEBKIT_SOUP_CACHE_UNCACHEABLE;
		}
	}

	switch (msg->status_code) {
	case SOUP_STATUS_PARTIAL_CONTENT:
		/* We don't cache partial responses, but they only
		 * invalidate cached full responses if the headers
		 * don't match.
		 */
		cacheability = WEBKIT_SOUP_CACHE_UNCACHEABLE;
		break;

	case SOUP_STATUS_NOT_MODIFIED:
		/* A 304 response validates an existing cache entry */
		cacheability = WEBKIT_SOUP_CACHE_VALIDATES;
		break;

	case SOUP_STATUS_MULTIPLE_CHOICES:
	case SOUP_STATUS_MOVED_PERMANENTLY:
	case SOUP_STATUS_GONE:
		/* FIXME: cacheable unless indicated otherwise */
		cacheability = WEBKIT_SOUP_CACHE_UNCACHEABLE;
		break;

	case SOUP_STATUS_FOUND:
	case SOUP_STATUS_TEMPORARY_REDIRECT:
		/* FIXME: cacheable if explicitly indicated */
		cacheability = WEBKIT_SOUP_CACHE_UNCACHEABLE;
		break;

	case SOUP_STATUS_SEE_OTHER:
	case SOUP_STATUS_FORBIDDEN:
	case SOUP_STATUS_NOT_FOUND:
	case SOUP_STATUS_METHOD_NOT_ALLOWED:
		return (WEBKIT_SOUP_CACHE_UNCACHEABLE | WEBKIT_SOUP_CACHE_INVALIDATES);

	default:
		/* Any 5xx status or any 4xx status not handled above
		 * is uncacheable but doesn't break the cache.
		 */
		if ((msg->status_code >= SOUP_STATUS_BAD_REQUEST &&
		     msg->status_code <= SOUP_STATUS_FAILED_DEPENDENCY) ||
		    msg->status_code >= SOUP_STATUS_INTERNAL_SERVER_ERROR)
			return WEBKIT_SOUP_CACHE_UNCACHEABLE;

		/* An unrecognized 2xx, 3xx, or 4xx response breaks
		 * the cache.
		 */
		if ((msg->status_code > SOUP_STATUS_PARTIAL_CONTENT &&
		     msg->status_code < SOUP_STATUS_MULTIPLE_CHOICES) ||
		    (msg->status_code > SOUP_STATUS_TEMPORARY_REDIRECT &&
		     msg->status_code < SOUP_STATUS_INTERNAL_SERVER_ERROR))
			return (WEBKIT_SOUP_CACHE_UNCACHEABLE | WEBKIT_SOUP_CACHE_INVALIDATES);
		break;
	}

	return cacheability;
}

static void
webkit_soup_cache_entry_free (WebKitSoupCacheEntry *entry, gboolean purge)
{
	if (purge) {
		GFile *file = g_file_new_for_path (entry->filename);
		g_file_delete (file, NULL, NULL);
		g_object_unref (file);
	}

	g_free (entry->filename);
	entry->filename = NULL;
	g_free (entry->key);
	entry->key = NULL;

	if (entry->headers) {
		soup_message_headers_free (entry->headers);
		entry->headers = NULL;
	}

	if (entry->data) {
		g_string_free (entry->data, TRUE);
		entry->data = NULL;
	}
	if (entry->error) {
		g_error_free (entry->error);
		entry->error = NULL;
	}
	if (entry->cancellable) {
		g_object_unref (entry->cancellable);
		entry->cancellable = NULL;
	}

	g_slice_free (WebKitSoupCacheEntry, entry);
}

static void
copy_headers (const char *name, const char *value, SoupMessageHeaders *headers)
{
	soup_message_headers_append (headers, name, value);
}

static void
update_headers (const char *name, const char *value, SoupMessageHeaders *headers)
{
	if (soup_message_headers_get (headers, name))
		soup_message_headers_replace (headers, name, value);
	else
		soup_message_headers_append (headers, name, value);
}

static guint
webkit_soup_cache_entry_get_current_age (WebKitSoupCacheEntry *entry)
{
	time_t now = time (NULL);
	time_t resident_time;

	resident_time = now - entry->response_time;
	return entry->corrected_initial_age + resident_time;
}

static gboolean
webkit_soup_cache_entry_is_fresh_enough (WebKitSoupCacheEntry *entry, gint min_fresh)
{
	guint limit = (min_fresh == -1) ? webkit_soup_cache_entry_get_current_age (entry) : (guint) min_fresh;
	return entry->freshness_lifetime > limit;
}

static char *
soup_message_get_cache_key (SoupMessage *msg)
{
	SoupURI *uri = soup_message_get_uri (msg);
	return soup_uri_to_string (uri, FALSE);
}

static void
webkit_soup_cache_entry_set_freshness (WebKitSoupCacheEntry *entry, SoupMessage *msg, WebKitSoupCache *cache)
{
	const char *cache_control;
	const char *expires, *date, *last_modified;
	GHashTable *hash;

	hash = NULL;

	cache_control = soup_message_headers_get (entry->headers, "Cache-Control");
	if (cache_control) {
		const char *max_age, *s_maxage;
		gint64 freshness_lifetime = 0;
		WebKitSoupCachePrivate *priv = WEBKIT_SOUP_CACHE_GET_PRIVATE (cache);

		hash = soup_header_parse_param_list (cache_control);

		/* Should we re-validate the entry when it goes stale */
		entry->must_revalidate = g_hash_table_lookup_extended (hash, "must-revalidate", NULL, NULL);

		/* Section 2.3.1 */
		if (priv->cache_type == WEBKIT_SOUP_CACHE_SHARED) {
			s_maxage = g_hash_table_lookup (hash, "s-maxage");
			if (s_maxage) {
				freshness_lifetime = g_ascii_strtoll (s_maxage, NULL, 10);
				if (freshness_lifetime) {
					/* Implies proxy-revalidate. TODO: is it true? */
					entry->must_revalidate = TRUE;
					soup_header_free_param_list (hash);
					return;
				}
			}
		}

		/* If 'max-age' cache directive is present, use that */
		max_age = g_hash_table_lookup (hash, "max-age");
		if (max_age)
			freshness_lifetime = g_ascii_strtoll (max_age, NULL, 10);

		if (freshness_lifetime) {
			entry->freshness_lifetime = (guint)MIN (freshness_lifetime, G_MAXUINT32);
			soup_header_free_param_list (hash);
			return;
		}
	}

	if (hash != NULL)
		soup_header_free_param_list (hash);

	/* If the 'Expires' response header is present, use its value
	 * minus the value of the 'Date' response header
	 */
	expires = soup_message_headers_get (entry->headers, "Expires");
	date = soup_message_headers_get (entry->headers, "Date");
	if (expires && date) {
		SoupDate *expires_d, *date_d;
		time_t expires_t, date_t;

		expires_d = soup_date_new_from_string (expires);
		if (expires_d) {
			date_d = soup_date_new_from_string (date);

			expires_t = soup_date_to_time_t (expires_d);
			date_t = soup_date_to_time_t (date_d);

			soup_date_free (expires_d);
			soup_date_free (date_d);

			if (expires_t && date_t) {
				entry->freshness_lifetime = (guint)MAX (expires_t - date_t, 0);
				return;
			}
		} else {
			/* If Expires is not a valid date we should
			   treat it as already expired, see section
			   3.3 */
			entry->freshness_lifetime = 0;
			return;
		}
	}

	/* Otherwise an heuristic may be used */

	/* Heuristics MUST NOT be used with these status codes
	   (section 2.3.1.1) */
	if (msg->status_code != SOUP_STATUS_OK &&
	    msg->status_code != SOUP_STATUS_NON_AUTHORITATIVE &&
	    msg->status_code != SOUP_STATUS_PARTIAL_CONTENT &&
	    msg->status_code != SOUP_STATUS_MULTIPLE_CHOICES &&
	    msg->status_code != SOUP_STATUS_MOVED_PERMANENTLY &&
	    msg->status_code != SOUP_STATUS_GONE)
		goto expire;

	/* TODO: attach warning 113 if response's current_age is more
	   than 24h (section 2.3.1.1) when using heuristics */

	/* Last-Modified based heuristic */
	last_modified = soup_message_headers_get (entry->headers, "Last-Modified");
	if (last_modified) {
		SoupDate *soup_date;
		time_t now, last_modified_t;

		soup_date = soup_date_new_from_string (last_modified);
		last_modified_t = soup_date_to_time_t (soup_date);
		now = time (NULL);

#define HEURISTIC_FACTOR 0.1 /* From Section 2.3.1.1 */

		entry->freshness_lifetime = MAX (0, (now - last_modified_t) * HEURISTIC_FACTOR);
		soup_date_free (soup_date);
	}

	return;

 expire:
	/* If all else fails, make the entry expire immediately */
	entry->freshness_lifetime = 0;
}

static WebKitSoupCacheEntry *
webkit_soup_cache_entry_new (WebKitSoupCache *cache, SoupMessage *msg, time_t request_time, time_t response_time)
{
	WebKitSoupCacheEntry *entry;
	SoupMessageHeaders *headers;
	const char *date;
	char *md5;

	entry = g_slice_new0 (WebKitSoupCacheEntry);
	entry->dirty = FALSE;
	entry->writing = FALSE;
	entry->got_body = FALSE;
	entry->being_validated = FALSE;
	entry->data = g_string_new (NULL);
	entry->pos = 0;
	entry->error = NULL;

	/* key & filename */
	entry->key = soup_message_get_cache_key (msg);
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, entry->key, -1);
	entry->filename = g_build_filename (cache->priv->cache_dir, md5, NULL);
	g_free (md5);

	/* Headers */
	headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
	soup_message_headers_foreach (msg->response_headers,
				      (SoupMessageHeadersForeachFunc)copy_headers,
				      headers);
	entry->headers = headers;

	/* LRU list */
	entry->hits = 0;

	/* Section 2.3.1, Freshness Lifetime */
	webkit_soup_cache_entry_set_freshness (entry, msg, cache);

	/* Section 2.3.2, Calculating Age */
	date = soup_message_headers_get (entry->headers, "Date");

	if (date) {
		SoupDate *soup_date;
		const char *age;
		time_t date_value, apparent_age, corrected_received_age, response_delay, age_value = 0;

		soup_date = soup_date_new_from_string (date);
		date_value = soup_date_to_time_t (soup_date);
		soup_date_free (soup_date);

		age = soup_message_headers_get (entry->headers, "Age");
		if (age)
			age_value = g_ascii_strtoll (age, NULL, 10);

		entry->response_time = response_time;
		apparent_age = MAX (0, entry->response_time - date_value);
		corrected_received_age = MAX (apparent_age, age_value);
		response_delay = entry->response_time - request_time;
		entry->corrected_initial_age = corrected_received_age + response_delay;
	} else {
		/* Is this correct ? */
		entry->corrected_initial_age = time (NULL);
	}

	return entry;
}

static void
webkit_soup_cache_writing_fixture_free (WebKitSoupCacheWritingFixture *fixture)
{
	/* Free fixture. And disconnect signals, we don't want to
	   listen to more SoupMessage events as we're finished with
	   this resource */
	if (g_signal_handler_is_connected (fixture->msg, fixture->got_chunk_handler))
		g_signal_handler_disconnect (fixture->msg, fixture->got_chunk_handler);
	if (g_signal_handler_is_connected (fixture->msg, fixture->got_body_handler))
		g_signal_handler_disconnect (fixture->msg, fixture->got_body_handler);
	if (g_signal_handler_is_connected (fixture->msg, fixture->restarted_handler))
		g_signal_handler_disconnect (fixture->msg, fixture->restarted_handler);
	g_object_unref (fixture->msg);
	g_object_unref (fixture->cache);
	g_slice_free (WebKitSoupCacheWritingFixture, fixture);
}

static void
close_ready_cb (GObject *source, GAsyncResult *result, WebKitSoupCacheWritingFixture *fixture)
{
	WebKitSoupCacheEntry *entry = fixture->entry;
	WebKitSoupCache *cache = fixture->cache;
	GOutputStream *stream = G_OUTPUT_STREAM (source);
	goffset content_length;

	g_warn_if_fail (entry->error == NULL);

	/* FIXME: what do we do on error ? */

	if (stream) {
		g_output_stream_close_finish (stream, result, NULL);
		g_object_unref (stream);
	}
	entry->stream = NULL;

	content_length = soup_message_headers_get_content_length (entry->headers);

	/* If the process was cancelled, then delete the entry from
	   the cache. Do it also if the size of a chunked resource is
	   too much for the cache */
	if (g_cancellable_is_cancelled (entry->cancellable)) {
		entry->dirty = FALSE;
		webkit_soup_cache_entry_remove (cache, entry);
		webkit_soup_cache_entry_free (entry, TRUE);
		entry = NULL;
	} else if ((soup_message_headers_get_encoding (entry->headers) == SOUP_ENCODING_CHUNKED) ||
		   entry->length != (gsize) content_length) {
		/** Two options here:
		 *
		 * 1. "chunked" data, entry was temporarily added to
		 * cache (as content-length is 0) and now that we have
		 * the actual size we have to evaluate if we want it
		 * in the cache or not
		 *
		 * 2. Content-Length has a different value than actual
		 * length, means that the content was encoded for
		 * transmission (typically compressed) and thus we
		 * have to substract the content-length value that was
		 * added to the cache and add the unencoded length
		 **/
		gint length_to_add = entry->length - content_length;

		/* Make room in cache if needed */
		if (cache_accepts_entries_of_size (cache, length_to_add)) {
			make_room_for_new_entry (cache, length_to_add);

			cache->priv->size += length_to_add;
		} else {
			entry->dirty = FALSE;
			webkit_soup_cache_entry_remove (cache, entry);
			webkit_soup_cache_entry_free (entry, TRUE);
			entry = NULL;
		}
	}

	if (entry) {
		/* Get rid of the GString in memory for the resource now */
		if (entry->data) {
			g_string_free (entry->data, TRUE);
			entry->data = NULL;
		}

		entry->dirty = FALSE;
		entry->writing = FALSE;
		entry->got_body = FALSE;
		entry->pos = 0;

		g_object_unref (entry->cancellable);
		entry->cancellable = NULL;
	}

	cache->priv->n_pending--;

	/* Frees */
	webkit_soup_cache_writing_fixture_free (fixture);
}

static void
write_ready_cb (GObject *source, GAsyncResult *result, WebKitSoupCacheWritingFixture *fixture)
{
	GOutputStream *stream = G_OUTPUT_STREAM (source);
	GError *error = NULL;
	gssize write_size;
	WebKitSoupCacheEntry *entry = fixture->entry;

	if (g_cancellable_is_cancelled (entry->cancellable)) {
		g_output_stream_close_async (stream,
					     G_PRIORITY_LOW,
					     entry->cancellable,
					     (GAsyncReadyCallback)close_ready_cb,
					     fixture);
		return;
	}

	write_size = g_output_stream_write_finish (stream, result, &error);
	if (write_size <= 0 || error) {
		if (error)
			entry->error = error;
		g_output_stream_close_async (stream,
					     G_PRIORITY_LOW,
					     entry->cancellable,
					     (GAsyncReadyCallback)close_ready_cb,
					     fixture);
		/* FIXME: We should completely stop caching the
		   resource at this point */
	} else {
		entry->pos += write_size;

		/* Are we still writing and is there new data to write
		   already ? */
		if (entry->data && entry->pos < entry->data->len) {
			g_output_stream_write_async (entry->stream,
						     entry->data->str + entry->pos,
						     entry->data->len - entry->pos,
						     G_PRIORITY_LOW,
						     entry->cancellable,
						     (GAsyncReadyCallback)write_ready_cb,
						     fixture);
		} else {
			entry->writing = FALSE;

			if (entry->got_body) {
				/* If we already received 'got-body'
				   and we have written all the data,
				   we can close the stream */
				g_output_stream_close_async (entry->stream,
							     G_PRIORITY_LOW,
							     entry->cancellable,
							     (GAsyncReadyCallback)close_ready_cb,
							     fixture);
			}
		}
	}
}

static void
msg_got_chunk_cb (SoupMessage *msg, SoupBuffer *chunk, WebKitSoupCacheWritingFixture *fixture)
{
	WebKitSoupCacheEntry *entry = fixture->entry;

	g_return_if_fail (chunk->data && chunk->length);
	g_return_if_fail (entry);

	/* Ignore this if the writing or appending was cancelled */
	if (!g_cancellable_is_cancelled (entry->cancellable)) {
		g_string_append_len (entry->data, chunk->data, chunk->length);
		entry->length = entry->data->len;

		if (!cache_accepts_entries_of_size (fixture->cache, entry->length)) {
			/* Quickly cancel the caching of the resource */
			g_cancellable_cancel (entry->cancellable);
		}
	}

	/* FIXME: remove the error check when we cancel the caching at
	   the first write error */
	/* Only write if the entry stream is ready */
	if (entry->writing == FALSE && entry->error == NULL && entry->stream) {
		GString *data = entry->data;
		entry->writing = TRUE;
		g_output_stream_write_async (entry->stream,
					     data->str + entry->pos,
					     data->len - entry->pos,
					     G_PRIORITY_LOW,
					     entry->cancellable,
					     (GAsyncReadyCallback)write_ready_cb,
					     fixture);
	}
}

static void
msg_got_body_cb (SoupMessage *msg, WebKitSoupCacheWritingFixture *fixture)
{
	WebKitSoupCacheEntry *entry = fixture->entry;
	g_return_if_fail (entry);

	entry->got_body = TRUE;

	if (!entry->stream && entry->pos != entry->length)
		/* The stream is not ready to be written but we still
		   have data to write, we'll write it when the stream
		   is opened for writing */
		return;


	if (entry->pos != entry->length) {
		/* If we still have data to write, write it,
		   write_ready_cb will close the stream */
		if (entry->writing == FALSE && entry->error == NULL && entry->stream) {
			g_output_stream_write_async (entry->stream,
						     entry->data->str + entry->pos,
						     entry->data->len - entry->pos,
						     G_PRIORITY_LOW,
						     entry->cancellable,
						     (GAsyncReadyCallback)write_ready_cb,
						     fixture);
		}
		return;
	}

	if (entry->stream && !entry->writing)
		g_output_stream_close_async (entry->stream,
					     G_PRIORITY_LOW,
					     entry->cancellable,
					     (GAsyncReadyCallback)close_ready_cb,
					     fixture);
}

static gboolean
webkit_soup_cache_entry_remove (WebKitSoupCache *cache, WebKitSoupCacheEntry *entry)
{
	GList *lru_item;

	/* if (entry->dirty && !g_cancellable_is_cancelled (entry->cancellable)) { */
	if (entry->dirty) {
		g_cancellable_cancel (entry->cancellable);
		return FALSE;
	}

	g_assert (!entry->dirty);
	g_assert (g_list_length (cache->priv->lru_start) == g_hash_table_size (cache->priv->cache));

	/* Remove from cache */
	if (!g_hash_table_remove (cache->priv->cache, entry->key))
		return FALSE;

	/* Remove from LRU */
	lru_item = g_list_find (cache->priv->lru_start, entry);
	cache->priv->lru_start = g_list_delete_link (cache->priv->lru_start, lru_item);

	/* Adjust cache size */
	cache->priv->size -= entry->length;

	g_assert (g_list_length (cache->priv->lru_start) == g_hash_table_size (cache->priv->cache));

	return TRUE;
}

static gint
lru_compare_func (gconstpointer a, gconstpointer b)
{
	WebKitSoupCacheEntry *entry_a = (WebKitSoupCacheEntry *)a;
	WebKitSoupCacheEntry *entry_b = (WebKitSoupCacheEntry *)b;

	/** The rationale of this sorting func is
	 *
	 * 1. sort by hits -> LRU algorithm, then
	 *
	 * 2. sort by freshness lifetime, we better discard first
	 * entries that are close to expire
	 *
	 * 3. sort by size, replace first small size resources as they
	 * are cheaper to download
	 **/

	/* Sort by hits */
	if (entry_a->hits != entry_b->hits)
		return entry_a->hits - entry_b->hits;

	/* Sort by freshness_lifetime */
	if (entry_a->freshness_lifetime != entry_b->freshness_lifetime)
		return entry_a->freshness_lifetime - entry_b->freshness_lifetime;

	/* Sort by size */
	return entry_a->length - entry_b->length;
}

static gboolean
cache_accepts_entries_of_size (WebKitSoupCache *cache, guint length_to_add)
{
	/* We could add here some more heuristics. TODO: review how
	   this is done by other HTTP caches */

	return length_to_add <= cache->priv->max_entry_data_size;
}

static void
make_room_for_new_entry (WebKitSoupCache *cache, guint length_to_add)
{
	GList *lru_entry = cache->priv->lru_start;

	/* Check that there is enough room for the new entry. This is
	   an approximation as we're not working out the size of the
	   cache file or the size of the headers for performance
	   reasons. TODO: check if that would be really that expensive */

	while (lru_entry &&
	       (length_to_add + cache->priv->size > cache->priv->max_size)) {
		WebKitSoupCacheEntry *old_entry = (WebKitSoupCacheEntry *)lru_entry->data;

		/* Discard entries. Once cancelled resources will be
		 * freed in close_ready_cb
		 */
		if (webkit_soup_cache_entry_remove (cache, old_entry)) {
			webkit_soup_cache_entry_free (old_entry, TRUE);
			lru_entry = cache->priv->lru_start;
		} else
			lru_entry = g_list_next (lru_entry);
	}
}

static gboolean
webkit_soup_cache_entry_insert_by_key (WebKitSoupCache *cache,
				       const char *key,
				       WebKitSoupCacheEntry *entry,
				       gboolean sort)
{
	guint length_to_add = 0;

	if (soup_message_headers_get_encoding (entry->headers) != SOUP_ENCODING_CHUNKED)
		length_to_add = soup_message_headers_get_content_length (entry->headers);

	/* Check if we are going to store the resource depending on its size */
	if (length_to_add) {
		if (!cache_accepts_entries_of_size (cache, length_to_add))
			return FALSE;

		/* Make room for new entry if needed */
		make_room_for_new_entry (cache, length_to_add);
	}

	g_hash_table_insert (cache->priv->cache, g_strdup (key), entry);

	/* Compute new cache size */
	cache->priv->size += length_to_add;

	/* Update LRU */
	if (sort)
		cache->priv->lru_start = g_list_insert_sorted (cache->priv->lru_start, entry, lru_compare_func);
	else
		cache->priv->lru_start = g_list_prepend (cache->priv->lru_start, entry);

	g_assert (g_list_length (cache->priv->lru_start) == g_hash_table_size (cache->priv->cache));

	return TRUE;
}

static void
msg_restarted_cb (SoupMessage *msg, WebKitSoupCacheEntry *entry)
{
	/* FIXME: What should we do here exactly? */
}

static void
append_to_ready_cb (GObject *source, GAsyncResult *result, WebKitSoupCacheWritingFixture *fixture)
{
	GFile *file = (GFile *)source;
	GOutputStream *stream;
	WebKitSoupCacheEntry *entry = fixture->entry;

	stream = (GOutputStream *)g_file_append_to_finish (file, result, &entry->error);

	if (g_cancellable_is_cancelled (entry->cancellable) || entry->error) {
		fixture->cache->priv->n_pending--;
		entry->dirty = FALSE;
		webkit_soup_cache_entry_remove (fixture->cache, entry);
		webkit_soup_cache_entry_free (entry, TRUE);
		webkit_soup_cache_writing_fixture_free (fixture);
		return;
	}

	entry->stream = g_object_ref (stream);
	g_object_unref (file);

	/* If we already got all the data we have to initiate the
	   writing here, since we won't get more 'got-chunk'
	   signals */
	if (entry->got_body) {
		GString *data = entry->data;

		/* It could happen that reading the data from server
		   was completed before this happens. In that case
		   there is no data */
		if (data) {
			entry->writing = TRUE;
			g_output_stream_write_async (entry->stream,
						     data->str + entry->pos,
						     data->len - entry->pos,
						     G_PRIORITY_LOW,
						     entry->cancellable,
						     (GAsyncReadyCallback)write_ready_cb,
						     fixture);
		}
	}
}

typedef struct {
	time_t request_time;
	SoupSessionFeature *feature;
	gulong got_headers_handler;
} RequestHelper;

static void
msg_got_headers_cb (SoupMessage *msg, gpointer user_data)
{
	WebKitSoupCache *cache;
	WebKitSoupCacheability cacheable;
	RequestHelper *helper;
	time_t request_time, response_time;

	response_time = time (NULL);

	helper = (RequestHelper *)user_data;
	cache = WEBKIT_SOUP_CACHE (helper->feature);
	request_time = helper->request_time;
	g_signal_handlers_disconnect_by_func (msg, msg_got_headers_cb, user_data);
	g_slice_free (RequestHelper, helper);

	cacheable = webkit_soup_cache_get_cacheability (cache, msg);

	if (cacheable & WEBKIT_SOUP_CACHE_CACHEABLE) {
		WebKitSoupCacheEntry *entry;
		char *key;
		GFile *file;
		WebKitSoupCacheWritingFixture *fixture;

		/* Check if we are already caching this resource */
		key = soup_message_get_cache_key (msg);
		entry = g_hash_table_lookup (cache->priv->cache, key);
		g_free (key);

		if (entry && entry->dirty)
			return;

		/* Create a new entry, deleting any old one if present */
		if (entry) {
			webkit_soup_cache_entry_remove (cache, entry);
			webkit_soup_cache_entry_free (entry, TRUE);
		}

		entry = webkit_soup_cache_entry_new (cache, msg, request_time, response_time);
		entry->hits = 1;

		/* Do not continue if it can not be stored */
		if (!webkit_soup_cache_entry_insert_by_key (cache, (const gchar *)entry->key, entry, TRUE)) {
			webkit_soup_cache_entry_free (entry, TRUE);
			return;
		}

		fixture = g_slice_new0 (WebKitSoupCacheWritingFixture);
		fixture->cache = g_object_ref (cache);
		fixture->entry = entry;
		fixture->msg = g_object_ref (msg);

		/* We connect now to these signals and buffer the data
		   if it comes before the file is ready for writing */
		fixture->got_chunk_handler =
			g_signal_connect (msg, "got-chunk", G_CALLBACK (msg_got_chunk_cb), fixture);
		fixture->got_body_handler =
			g_signal_connect (msg, "got-body", G_CALLBACK (msg_got_body_cb), fixture);
		fixture->restarted_handler =
			g_signal_connect (msg, "restarted", G_CALLBACK (msg_restarted_cb), entry);

		/* Prepare entry */
		file = g_file_new_for_path (entry->filename);
		cache->priv->n_pending++;

		entry->dirty = TRUE;
		entry->cancellable = g_cancellable_new ();
		g_file_append_to_async (file, 0,
					G_PRIORITY_LOW, entry->cancellable,
					(GAsyncReadyCallback)append_to_ready_cb,
					fixture);
	} else if (cacheable & WEBKIT_SOUP_CACHE_INVALIDATES) {
		char *key;
		WebKitSoupCacheEntry *entry;

		key = soup_message_get_cache_key (msg);
		entry = g_hash_table_lookup (cache->priv->cache, key);
		g_free (key);

		if (entry) {
			if (webkit_soup_cache_entry_remove (cache, entry))
				webkit_soup_cache_entry_free (entry, TRUE);
		}
	} else if (cacheable & WEBKIT_SOUP_CACHE_VALIDATES) {
		char *key;
		WebKitSoupCacheEntry *entry;

		key = soup_message_get_cache_key (msg);
		entry = g_hash_table_lookup (cache->priv->cache, key);
		g_free (key);

		/* It's possible to get a CACHE_VALIDATES with no
		 * entry in the hash table. This could happen if for
		 * example the soup client is the one creating the
		 * conditional request.
		 */
		if (entry) {
			entry->being_validated = FALSE;

			/* We update the headers of the existing cache item,
			   plus its age */
			soup_message_headers_foreach (msg->response_headers,
						      (SoupMessageHeadersForeachFunc)update_headers,
						      entry->headers);
			webkit_soup_cache_entry_set_freshness (entry, msg, cache);
		}
	}
}

GInputStream *
webkit_soup_cache_send_response (WebKitSoupCache *cache, SoupMessage *msg)
{
	char *key;
	WebKitSoupCacheEntry *entry;
	char *current_age;
	GInputStream *stream = NULL;
	GFile *file;

	g_return_val_if_fail (WEBKIT_IS_SOUP_CACHE (cache), NULL);
	g_return_val_if_fail (SOUP_IS_MESSAGE (msg), NULL);

	key = soup_message_get_cache_key (msg);
	entry = g_hash_table_lookup (cache->priv->cache, key);
	g_return_val_if_fail (entry, NULL);

	/* If we are told to send a response from cache any validation
	   in course is over by now */
	entry->being_validated = FALSE;

	/* Headers */
	soup_message_headers_foreach (entry->headers,
				      (SoupMessageHeadersForeachFunc)update_headers,
				      msg->response_headers);

	/* Add 'Age' header with the current age */
	current_age = g_strdup_printf ("%d", webkit_soup_cache_entry_get_current_age (entry));
	soup_message_headers_replace (msg->response_headers,
				      "Age",
				      current_age);
	g_free (current_age);

	/* TODO: the original idea was to save reads, but current code
	   assumes that a stream is always returned. Need to reach
	   some agreement here. Also we have to handle the situation
	   were the file was no longer there (for example files
	   removed without notifying the cache */
	file = g_file_new_for_path (entry->filename);
	stream = (GInputStream *)g_file_read (file, NULL, NULL);

	return stream;
}

static void
request_started (SoupSessionFeature *feature, SoupSession *session,
		 SoupMessage *msg, SoupSocket *socket)
{
	RequestHelper *helper = g_slice_new0 (RequestHelper);
	helper->request_time = time (NULL);
	helper->feature = feature;
	helper->got_headers_handler = g_signal_connect (msg, "got-headers",
							G_CALLBACK (msg_got_headers_cb),
							helper);
}

static void
attach (SoupSessionFeature *feature, SoupSession *session)
{
	WebKitSoupCache *cache = WEBKIT_SOUP_CACHE (feature);
	cache->priv->session = session;

	webkit_soup_cache_default_feature_interface->attach (feature, session);
}

static void
webkit_soup_cache_session_feature_init (SoupSessionFeatureInterface *feature_interface,
					gpointer interface_data)
{
	webkit_soup_cache_default_feature_interface =
		g_type_default_interface_peek (SOUP_TYPE_SESSION_FEATURE);

	feature_interface->attach = attach;
	feature_interface->request_started = request_started;
}

static void
webkit_soup_cache_init (WebKitSoupCache *cache)
{
	WebKitSoupCachePrivate *priv;

	priv = cache->priv = WEBKIT_SOUP_CACHE_GET_PRIVATE (cache);

	priv->cache = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify)g_free,
					     NULL);

	/* LRU */
	priv->lru_start = NULL;

	/* */
	priv->n_pending = 0;

	/* Cache size */
	priv->max_size = DEFAULT_MAX_SIZE;
	priv->max_entry_data_size = priv->max_size / MAX_ENTRY_DATA_PERCENTAGE;
	priv->size = 0;
}

static void
remove_cache_item (gpointer key,
		   gpointer value,
		   WebKitSoupCache *cache)
{
	WebKitSoupCacheEntry *entry = g_hash_table_lookup (cache->priv->cache, (const gchar *)key);
	if (webkit_soup_cache_entry_remove (cache, entry))
		webkit_soup_cache_entry_free (entry, FALSE);
}

static void
webkit_soup_cache_finalize (GObject *object)
{
	WebKitSoupCachePrivate *priv;

	priv = WEBKIT_SOUP_CACHE (object)->priv;

	g_hash_table_foreach (priv->cache, (GHFunc)remove_cache_item, object);
	g_hash_table_destroy (priv->cache);
	g_free (priv->cache_dir);

	g_list_free (priv->lru_start);
	priv->lru_start = NULL;

	G_OBJECT_CLASS (webkit_soup_cache_parent_class)->finalize (object);
}

static void
webkit_soup_cache_set_property (GObject *object, guint prop_id,
				const GValue *value, GParamSpec *pspec)
{
	WebKitSoupCachePrivate *priv = WEBKIT_SOUP_CACHE (object)->priv;

	switch (prop_id) {
	case PROP_CACHE_DIR:
		priv->cache_dir = g_value_dup_string (value);
		/* Create directory if it does not exist (FIXME: should we?) */
		if (!g_file_test (priv->cache_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			g_mkdir_with_parents (priv->cache_dir, 0700);
		break;
	case PROP_CACHE_TYPE:
		priv->cache_type = g_value_get_enum (value);
		/* TODO: clear private entries and issue a warning if moving to shared? */
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
webkit_soup_cache_get_property (GObject *object, guint prop_id,
				GValue *value, GParamSpec *pspec)
{
	WebKitSoupCachePrivate *priv = WEBKIT_SOUP_CACHE (object)->priv;

	switch (prop_id) {
	case PROP_CACHE_DIR:
		g_value_set_string (value, priv->cache_dir);
		break;
	case PROP_CACHE_TYPE:
		g_value_set_enum (value, priv->cache_type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
webkit_soup_cache_constructed (GObject *object)
{
	WebKitSoupCachePrivate *priv;

	priv = WEBKIT_SOUP_CACHE (object)->priv;

	if (!priv->cache_dir) {
		/* Set a default cache dir, different for each user */
		priv->cache_dir = g_build_filename (g_get_user_cache_dir (),
						    "httpcache",
						    NULL);
		if (!g_file_test (priv->cache_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			g_mkdir_with_parents (priv->cache_dir, 0700);
	}

	if (G_OBJECT_CLASS (webkit_soup_cache_parent_class)->constructed)
		G_OBJECT_CLASS (webkit_soup_cache_parent_class)->constructed (object);
}

#define WEBKIT_SOUP_CACHE_TYPE_TYPE (webkit_soup_cache_type_get_type ())
static GType
webkit_soup_cache_type_get_type (void)
{
	static GType cache_type = 0;

	static const GEnumValue cache_types[] = {
		{ WEBKIT_SOUP_CACHE_SINGLE_USER, "Single user cache", "user" },
		{ WEBKIT_SOUP_CACHE_SHARED, "Shared cache", "shared" },
		{ 0, NULL, NULL }
	};

	if (!cache_type) {
		cache_type = g_enum_register_static ("WebKitSoupCacheTypeType", cache_types);
	}
	return cache_type;
}

static void
webkit_soup_cache_class_init (WebKitSoupCacheClass *cache_class)
{
	GObjectClass *gobject_class = (GObjectClass *)cache_class;

	gobject_class->finalize = webkit_soup_cache_finalize;
	gobject_class->constructed = webkit_soup_cache_constructed;
	gobject_class->set_property = webkit_soup_cache_set_property;
	gobject_class->get_property = webkit_soup_cache_get_property;

	cache_class->get_cacheability = get_cacheability;

	g_object_class_install_property (gobject_class, PROP_CACHE_DIR,
					 g_param_spec_string ("cache-dir",
							      "Cache directory",
							      "The directory to store the cache files",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_CACHE_TYPE,
					 g_param_spec_enum ("cache-type",
							    "Cache type",
							    "Whether the cache is private or shared",
							    WEBKIT_SOUP_CACHE_TYPE_TYPE,
							    WEBKIT_SOUP_CACHE_SINGLE_USER,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (cache_class, sizeof (WebKitSoupCachePrivate));
}

/**
 * webkit_soup_cache_new:
 * @cache_dir: the directory to store the cached data, or %NULL to use the default one
 * @cache_type: the #WebKitSoupCacheType of the cache
 *
 * Creates a new #WebKitSoupCache.
 *
 * Returns: a new #WebKitSoupCache
 *
 * Since: 2.28
 **/
WebKitSoupCache *
webkit_soup_cache_new (const char *cache_dir, WebKitSoupCacheType cache_type)
{
	return g_object_new (WEBKIT_TYPE_SOUP_CACHE,
			     "cache-dir", cache_dir,
			     "cache-type", cache_type,
			     NULL);
}

/**
 * webkit_soup_cache_has_response:
 * @cache: a #WebKitSoupCache
 * @msg: a #SoupMessage
 *
 * This function calculates whether the @cache object has a proper
 * response for the request @msg given the flags both in the request
 * and the cached reply and the time ellapsed since it was cached.
 *
 * Returns: whether or not the @cache has a valid response for @msg
 **/
WebKitSoupCacheResponse
webkit_soup_cache_has_response (WebKitSoupCache *cache, SoupMessage *msg)
{
	char *key;
	WebKitSoupCacheEntry *entry;
	const char *cache_control;
	GHashTable *hash;
	gpointer value;
	gboolean must_revalidate;
	int max_age, max_stale, min_fresh;
	GList *lru_item, *item;

	key = soup_message_get_cache_key (msg);
	entry = g_hash_table_lookup (cache->priv->cache, key);

	/* 1. The presented Request-URI and that of stored response
	 * match
	 */
	if (!entry)
		return WEBKIT_SOUP_CACHE_RESPONSE_STALE;

	/* Increase hit count. Take sorting into account */
	entry->hits++;
	lru_item = g_list_find (cache->priv->lru_start, entry);
	item = lru_item;
	while (item->next && lru_compare_func (item->data, item->next->data) > 0)
		item = g_list_next (item);

	if (item != lru_item) {
		cache->priv->lru_start = g_list_remove_link (cache->priv->lru_start, lru_item);
		item = g_list_insert_sorted (item, lru_item->data, lru_compare_func);
		g_list_free (lru_item);
	}

	if (entry->dirty || entry->being_validated)
		return WEBKIT_SOUP_CACHE_RESPONSE_STALE;

	/* 2. The request method associated with the stored response
	 *  allows it to be used for the presented request
	 */

	/* In practice this means we only return our resource for GET,
	 * cacheability for other methods is a TODO in the RFC
	 * (TODO: although we could return the headers for HEAD
	 * probably).
	 */
	if (msg->method != SOUP_METHOD_GET)
		return WEBKIT_SOUP_CACHE_RESPONSE_STALE;

	/* 3. Selecting request-headers nominated by the stored
	 * response (if any) match those presented.
	 */

	/* TODO */

	/* 4. The request is a conditional request issued by the client.
	 */
	if (soup_message_headers_get (msg->request_headers, "If-Modified-Since") ||
	    soup_message_headers_get (msg->request_headers, "If-None-Match"))
		return WEBKIT_SOUP_CACHE_RESPONSE_STALE;

	/* 5. The presented request and stored response are free from
	 * directives that would prevent its use.
	 */

	must_revalidate = FALSE;
	max_age = max_stale = min_fresh = -1;

	cache_control = soup_message_headers_get (msg->request_headers, "Cache-Control");
	if (cache_control) {
		hash = soup_header_parse_param_list (cache_control);

		if (g_hash_table_lookup_extended (hash, "no-store", NULL, NULL)) {
			g_hash_table_destroy (hash);
			return WEBKIT_SOUP_CACHE_RESPONSE_STALE;
		}

		if (g_hash_table_lookup_extended (hash, "no-cache", NULL, NULL)) {
			entry->must_revalidate = TRUE;
		}

		if (g_hash_table_lookup_extended (hash, "max-age", NULL, &value)) {
			max_age = (int)MIN (g_ascii_strtoll (value, NULL, 10), G_MAXINT32);
		}

		/* max-stale can have no value set, we need to use _extended */
		if (g_hash_table_lookup_extended (hash, "max-stale", NULL, &value)) {
			if (value)
				max_stale = (int)MIN (g_ascii_strtoll (value, NULL, 10), G_MAXINT32);
			else
				max_stale = G_MAXINT32;
		}

		value = g_hash_table_lookup (hash, "min-fresh");
		if (value)
			min_fresh = (int)MIN (g_ascii_strtoll (value, NULL, 10), G_MAXINT32);

		g_hash_table_destroy (hash);

		if (max_age != -1) {
			guint current_age = webkit_soup_cache_entry_get_current_age (entry);

			/* If we are over max-age and max-stale is not
			   set, do not use the value from the cache
			   without validation */
			if ((guint) max_age <= current_age && max_stale == -1)
				return WEBKIT_SOUP_CACHE_RESPONSE_NEEDS_VALIDATION;
		}
	}

	/* 6. The stored response is either: fresh, allowed to be
	 * served stale or succesfully validated
	 */
	/* TODO consider also proxy-revalidate & s-maxage */
	if (entry->must_revalidate)
		return WEBKIT_SOUP_CACHE_RESPONSE_NEEDS_VALIDATION;

	if (!webkit_soup_cache_entry_is_fresh_enough (entry, min_fresh)) {
		/* Not fresh, can it be served stale? */
		if (max_stale != -1) {
			/* G_MAXINT32 means we accept any staleness */
			if (max_stale == G_MAXINT32)
				return WEBKIT_SOUP_CACHE_RESPONSE_FRESH;

			if ((webkit_soup_cache_entry_get_current_age (entry) - entry->freshness_lifetime) <= (guint) max_stale)
				return WEBKIT_SOUP_CACHE_RESPONSE_FRESH;
		}

		return WEBKIT_SOUP_CACHE_RESPONSE_NEEDS_VALIDATION;
	}

	return WEBKIT_SOUP_CACHE_RESPONSE_FRESH;
}

/**
 * webkit_soup_cache_get_cacheability:
 * @cache: a #WebKitSoupCache
 * @msg: a #SoupMessage
 *
 * Calculates whether the @msg can be cached or not.
 *
 * Returns: a #WebKitSoupCacheability value indicating whether the @msg can be cached or not.
 **/
WebKitSoupCacheability
webkit_soup_cache_get_cacheability (WebKitSoupCache *cache, SoupMessage *msg)
{
	g_return_val_if_fail (WEBKIT_IS_SOUP_CACHE (cache), WEBKIT_SOUP_CACHE_UNCACHEABLE);
	g_return_val_if_fail (SOUP_IS_MESSAGE (msg), WEBKIT_SOUP_CACHE_UNCACHEABLE);

	return WEBKIT_SOUP_CACHE_GET_CLASS (cache)->get_cacheability (cache, msg);
}

static gboolean
force_flush_timeout (gpointer data)
{
	gboolean *forced = (gboolean *)data;
	*forced = TRUE;

	return FALSE;
}

/**
 * webkit_soup_cache_flush:
 * @cache: a #WebKitSoupCache
 * @session: the #SoupSession associated with the @cache
 *
 * This function will force all pending writes in the @cache to be
 * committed to disk. For doing so it will iterate the #GMainContext
 * associated with the @session (which can be the default one) as long
 * as needed.
 **/
void
webkit_soup_cache_flush (WebKitSoupCache *cache)
{
	GMainContext *async_context;
	SoupSession *session;
	guint timeout_id;
	gboolean forced = FALSE;

	g_return_if_fail (WEBKIT_IS_SOUP_CACHE (cache));

	session = cache->priv->session;
	g_return_if_fail (SOUP_IS_SESSION (session));
	async_context = soup_session_get_async_context (session);

	/* We give cache 10 secs to finish */
	timeout_id = g_timeout_add (10000, force_flush_timeout, &forced);

	while (!forced && cache->priv->n_pending > 0)
		g_main_context_iteration (async_context, FALSE);

	if (!forced)
		g_source_remove (timeout_id);
	else
		g_warning ("Cache flush finished despite %d pending requests", cache->priv->n_pending);
}

static void
clear_cache_item (gpointer key,
		  gpointer value,
		  WebKitSoupCache *cache)
{
	WebKitSoupCacheEntry *entry = g_hash_table_lookup (cache->priv->cache, (const gchar *)key);
	if (webkit_soup_cache_entry_remove (cache, entry))
		webkit_soup_cache_entry_free (entry, TRUE);
}

/**
 * webkit_soup_cache_clear:
 * @cache: a #WebKitSoupCache
 *
 * Will remove all entries in the @cache plus all the cache files
 * associated with them.
 **/
void
webkit_soup_cache_clear (WebKitSoupCache *cache)
{
	GHashTable *hash;

	g_return_if_fail (WEBKIT_IS_SOUP_CACHE (cache));

	hash = cache->priv->cache;
	g_return_if_fail (hash);

	g_hash_table_foreach (hash, (GHFunc)clear_cache_item, cache);
}

SoupMessage *
webkit_soup_cache_generate_conditional_request (WebKitSoupCache *cache, SoupMessage *original)
{
	SoupMessage *msg;
	SoupURI *uri;
	WebKitSoupCacheEntry *entry;
	char *key;
	const char *value;

	g_return_val_if_fail (WEBKIT_IS_SOUP_CACHE (cache), NULL);
	g_return_val_if_fail (SOUP_IS_MESSAGE (original), NULL);

	/* First copy the data we need from the original message */
	uri = soup_message_get_uri (original);
	msg = soup_message_new_from_uri (original->method, uri);

	soup_message_headers_foreach (original->request_headers,
				      (SoupMessageHeadersForeachFunc)copy_headers,
				      msg->request_headers);

	/* Now add the validator entries in the header from the cached
	   data */
	key = soup_message_get_cache_key (original);
	entry = g_hash_table_lookup (cache->priv->cache, key);
	g_free (key);

	g_return_val_if_fail (entry, NULL);

	entry->being_validated = TRUE;

	value = soup_message_headers_get (entry->headers, "Last-Modified");
	if (value)
		soup_message_headers_append (msg->request_headers,
					     "If-Modified-Since",
					     value);
	value = soup_message_headers_get (entry->headers, "ETag");
	if (value)
		soup_message_headers_append (msg->request_headers,
					     "If-None-Match",
					     value);
	return msg;
}

#define WEBKIT_SOUP_CACHE_FILE "soup.cache"

#define WEBKIT_SOUP_CACHE_HEADERS_FORMAT "{ss}"
#define WEBKIT_SOUP_CACHE_PHEADERS_FORMAT "(ssbuuuuua" WEBKIT_SOUP_CACHE_HEADERS_FORMAT ")"
#define WEBKIT_SOUP_CACHE_ENTRIES_FORMAT "a" WEBKIT_SOUP_CACHE_PHEADERS_FORMAT

/* Basically the same format than above except that some strings are
   prepended with &. This way the GVariant returns a pointer to the
   data instead of duplicating the string */
#define WEBKIT_SOUP_CACHE_DECODE_HEADERS_FORMAT "{&s&s}"

static void
pack_entry (gpointer data,
	    gpointer user_data)
{
	WebKitSoupCacheEntry *entry = (WebKitSoupCacheEntry *) data;
	SoupMessageHeadersIter iter;
	const gchar *header_key, *header_value;
	GVariantBuilder *headers_builder;
	GVariantBuilder *entries_builder = (GVariantBuilder *)user_data;

	/* Do not store non-consolidated entries */
	if (entry->dirty || entry->writing || !entry->key)
		return;

	/* Pack headers */
	headers_builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
	soup_message_headers_iter_init (&iter, entry->headers);
	while (soup_message_headers_iter_next (&iter, &header_key, &header_value)) {
		if (g_utf8_validate (header_value, -1, NULL))
			g_variant_builder_add (headers_builder, WEBKIT_SOUP_CACHE_HEADERS_FORMAT,
					       header_key, header_value);
	}

	/* Entry data */
	g_variant_builder_add (entries_builder, WEBKIT_SOUP_CACHE_PHEADERS_FORMAT,
			       entry->key, entry->filename, entry->must_revalidate,
			       entry->freshness_lifetime, entry->corrected_initial_age,
			       entry->response_time, entry->hits, entry->length, headers_builder);

	g_variant_builder_unref (headers_builder);
}

void
webkit_soup_cache_dump (WebKitSoupCache *cache)
{
	WebKitSoupCachePrivate *priv = WEBKIT_SOUP_CACHE_GET_PRIVATE (cache);
	gchar *filename;
	GVariantBuilder *entries_builder;
	GVariant *cache_variant;

	if (!g_list_length (cache->priv->lru_start))
		return;

	/* Create the builder and iterate over all entries */
	entries_builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
	g_list_foreach (cache->priv->lru_start, pack_entry, entries_builder);

	/* Serialize and dump */
	cache_variant = g_variant_new (WEBKIT_SOUP_CACHE_ENTRIES_FORMAT, entries_builder);
	g_variant_builder_unref (entries_builder);

	filename = g_build_filename (priv->cache_dir, WEBKIT_SOUP_CACHE_FILE, NULL);
	g_file_set_contents (filename, (const gchar *)g_variant_get_data (cache_variant),
			     g_variant_get_size (cache_variant), NULL);
	g_free (filename);
	g_variant_unref (cache_variant);
}

void
webkit_soup_cache_load (WebKitSoupCache *cache)
{
	gchar *filename = NULL, *contents = NULL;
	GVariant *cache_variant;
	GVariantIter *entries_iter, *headers_iter;
	GVariantType *variant_format;
	gsize length;
	WebKitSoupCacheEntry *entry;
	WebKitSoupCachePrivate *priv = cache->priv;

	filename = g_build_filename (priv->cache_dir, WEBKIT_SOUP_CACHE_FILE, NULL);
	if (!g_file_get_contents (filename, &contents, &length, NULL)) {
		g_free (filename);
		return;
	}
	g_free (filename);

	variant_format = g_variant_type_new (WEBKIT_SOUP_CACHE_ENTRIES_FORMAT);
	cache_variant = g_variant_new_from_data (variant_format, (const gchar *)contents, length, FALSE, NULL, NULL);
	g_variant_type_free (variant_format);

	g_variant_get (cache_variant, WEBKIT_SOUP_CACHE_ENTRIES_FORMAT, &entries_iter);
	entry = g_slice_new0 (WebKitSoupCacheEntry);

	while (g_variant_iter_loop (entries_iter, WEBKIT_SOUP_CACHE_PHEADERS_FORMAT,
				    &entry->key, &entry->filename, &entry->must_revalidate,
				    &entry->freshness_lifetime, &entry->corrected_initial_age,
				    &entry->response_time, &entry->hits, &entry->length,
                                    &headers_iter)) {
		const gchar *header_key, *header_value;

		/* SoupMessage Headers */
		entry->headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
		while (g_variant_iter_loop (headers_iter, WEBKIT_SOUP_CACHE_DECODE_HEADERS_FORMAT, &header_key, &header_value))
			soup_message_headers_append (entry->headers, header_key, header_value);

		/* Insert in cache */
		if (!webkit_soup_cache_entry_insert_by_key (cache, (const gchar *)entry->key, entry, FALSE))
			webkit_soup_cache_entry_free (entry, TRUE);

		/* New entry for the next iteration. This creates an
		   extra object the last iteration but it's worth it
		   as we save several if's */
		entry = g_slice_new0 (WebKitSoupCacheEntry);
	}
	/* Remove last created entry */
	g_slice_free (WebKitSoupCacheEntry, entry);

	/* Sort LRU (shouldn't be needed). First reverse as elements
	 * are always prepended when inserting
	 */
	cache->priv->lru_start = g_list_reverse (cache->priv->lru_start);
	cache->priv->lru_start = g_list_sort (cache->priv->lru_start, lru_compare_func);

	/* frees */
	g_variant_iter_free (entries_iter);
	g_variant_unref (cache_variant);
}

void
webkit_soup_cache_set_max_size (WebKitSoupCache *cache,
				guint max_size)
{
	cache->priv->max_size = max_size;
	cache->priv->max_entry_data_size = cache->priv->max_size / MAX_ENTRY_DATA_PERCENTAGE;
}

guint
webkit_soup_cache_get_max_size (WebKitSoupCache *cache)
{
	return cache->priv->max_size;
}
