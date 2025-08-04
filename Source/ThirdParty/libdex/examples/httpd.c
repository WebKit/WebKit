/* httpd.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libdex.h>
#include <libsoup/soup.h>
#include <unistd.h>

static const char *errbody = "<html><body><h1>Not Found</h1></body></html>";

typedef struct
{
  SoupServer        *server;
  SoupServerMessage *message;
  char              *path;
  GHashTable        *query;
} Request;

static void
request_free (gpointer data)
{
  Request *request = data;

  g_clear_pointer (&request->path, g_free);
  g_clear_pointer (&request->query, g_hash_table_unref);
  g_clear_object (&request->message);
  g_clear_object (&request->server);
  g_free (request);
}

static void
request_pause (Request *request)
{
#if SOUP_CHECK_VERSION(3, 2, 0)
  soup_server_message_pause (request->message);
#else
  soup_server_pause_message (request->server, request->message);
#endif
}

static void
request_unpause (Request *request)
{
#if SOUP_CHECK_VERSION(3, 2, 0)
  soup_server_message_unpause (request->message);
#else
  soup_server_unpause_message (request->server, request->message);
#endif
}

static DexFuture *
request_fiber (gpointer user_data)
{
  Request *request = user_data;
  SoupMessageHeaders *headers;
  const char *path = request->path;
  GFileInfo *file_info;
  GError *error = NULL;
  GFile *file;
  GFileType file_type;

  while (path[0] == '/')
    path++;

  if (path[0] == 0)
    path = ".";

  file = g_file_new_for_path (path);
  file_info = dex_await_object (dex_file_query_info (file,
                                                     G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                                     G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                     G_FILE_ATTRIBUTE_STANDARD_SIZE",",
                                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                     G_PRIORITY_DEFAULT),
                                &error);

  if (file_info != NULL)
    file_type = g_file_info_get_file_type (file_info);
  else
    file_type = G_FILE_TYPE_UNKNOWN;

  headers = soup_server_message_get_response_headers (request->message);

  if (soup_server_message_get_method (request->message) == SOUP_METHOD_HEAD)
    {
      char *length = g_strdup_printf ("%"G_GOFFSET_FORMAT, g_file_info_get_size (file_info));
      soup_message_headers_append (headers, "Content-Length", length);
      soup_server_message_set_status (request->message, SOUP_STATUS_OK, NULL);
      g_free (length);
      goto unpause;
    }

  /* TODO: It would be nice to use io_uring_prep_openat() via the
   * AIO backend here so that we can multiplex without hitting the
   * GIO async aio threadpool. That will require additions to
   * DexAioBackend to accomidate it, but would save us potentially
   * clogging the GIO pool as well as avoiding blocking on open()
   * in a fiber/worker.
   */

  if (file_type == G_FILE_TYPE_REGULAR)
    {
      SoupMessageBody *body = soup_server_message_get_response_body (request->message);
      goffset to_read = g_file_info_get_size (file_info);
      GInputStream *stream;
      gsize buflen = to_read;

      if (to_read > 4096*16)
        buflen = 1024*64 - 2*GLIB_SIZEOF_VOID_P;

      if (!(stream = dex_await_object (dex_file_read (file, G_PRIORITY_DEFAULT), &error)))
        goto respond_404;

      soup_message_headers_set_encoding (headers, SOUP_ENCODING_CHUNKED);
      soup_server_message_set_status (request->message, SOUP_STATUS_OK, NULL);

      for (;;)
        {
          guint8 *buffer = g_malloc (buflen);
          gssize len;

          len = dex_await_int64 (dex_input_stream_read (stream, buffer, MIN (to_read, buflen), G_PRIORITY_DEFAULT), &error);
          if (error != NULL || len <= 0)
            break;

          soup_message_body_append (body, SOUP_MEMORY_TAKE, buffer, len);

          to_read -= len;
          if (to_read <= 0)
            break;
        }

      soup_message_body_complete (body);
    }
  else if (file_type == G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *enumerator;
      GString *body;
      GList *files;
      gsize len;

      if (!(enumerator = dex_await_object (dex_file_enumerate_children (file,
                                                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                                                        G_FILE_ATTRIBUTE_STANDARD_SIZE",",
                                                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                                        G_PRIORITY_DEFAULT),
                                           &error)))
        goto respond_404;

      soup_server_message_set_status (request->message, SOUP_STATUS_OK, NULL);

      body = g_string_new (NULL);
      g_string_append_printf (body, "<html><body><h1>/%s</h1><ul>",
                              g_str_equal (path, ".") ? "" : path);

      while ((files = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 100, G_PRIORITY_DEFAULT), &error)))
        {
          for (const GList *iter = files; iter; iter = iter->next)
            {
              GFileInfo *info = iter->data;
              goffset size = g_file_info_get_size (info);

              g_string_append_printf (body,
                                      "<li><a href=\"%s/%s\">%s%s</a> - %"G_GOFFSET_FORMAT"</li>\n",
                                      path,
                                      g_file_info_get_name (info),
                                      g_file_info_get_display_name (info),
                                      g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY ? "/" : "",
                                      size);

              g_object_unref (info);
            }

          g_list_free (files);
        }

      g_string_append (body, "</h1></body></html>");
      len = body->len;

      soup_server_message_set_response (request->message,
                                        "text/html",
                                        SOUP_MEMORY_TAKE,
                                        g_string_free (body, FALSE),
                                        len);
    }
  else
    {
respond_404:
      soup_server_message_set_response (request->message,
                                        "text/html",
                                        SOUP_MEMORY_COPY,
                                        errbody,
                                        strlen (errbody));
    }

unpause:
  request_unpause (request);

  g_clear_object (&file_info);
  g_clear_object (&file);
  g_clear_error (&error);

  return NULL;
}

static void
handle_message (SoupServer        *server,
                SoupServerMessage *message,
                const char        *path,
                GHashTable        *query,
                gpointer           user_data)
{
  Request *request;
  const char *method;

  g_assert (SOUP_IS_SERVER (server));
  g_assert (SOUP_IS_SERVER_MESSAGE (message));
  g_assert (path != NULL);

  method = soup_server_message_get_method (message);

  if (method != SOUP_METHOD_GET && method != SOUP_METHOD_HEAD)
    {
      soup_server_message_set_status (message, SOUP_STATUS_NOT_IMPLEMENTED, NULL);
      return;
    }

  request = g_new0 (Request, 1);
  request->server = g_object_ref (server);
  request->message = g_object_ref (message);
  request->path = g_strdup (path);
  request->query = query ? g_hash_table_ref (query) : NULL;

  request_pause (request);

  dex_future_disown (dex_scheduler_spawn (NULL, 0, request_fiber, request, request_free));
}

static void
print_uris (SoupServer *server)
{
  GSList *uris;

  g_assert (SOUP_IS_SERVER (server));

  if ((uris = soup_server_get_uris (server)))
    {
      for (const GSList *iter = uris; iter; iter = iter->next)
        {
          GUri *uri = iter->data;
          char *str = g_uri_to_string (uri);
          g_printerr ("Listening on %s\n", str);
          g_free (str);
        }

      g_slist_free_full (g_steal_pointer (&uris), (GDestroyNotify)g_uri_unref);
    }
}

int
main (int argc,
      char *argv[])
{
  SoupServer *server;
  GMainLoop *main_loop;
  GError *error = NULL;

  dex_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  server = soup_server_new ("server-header", "libdex-httpd",
                            NULL);
  soup_server_add_handler (server, "/", handle_message, NULL, NULL);
  if (!soup_server_listen_all (server, 8080, 0, &error))
    g_error ("%s", error->message);
  print_uris (server);

  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);
  g_object_unref (server);

  return EXIT_SUCCESS;
}
