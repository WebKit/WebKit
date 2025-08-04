/* wget.c
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

#include <unistd.h>

#include <libsoup/soup.h>
#include <libdex.h>

#define PRIO G_PRIORITY_DEFAULT

static DexFuture *session_send (SoupSession *session,
                                SoupMessage *message,
                                int          priority);

static char *output_document;
static char *url;
static GMainLoop *main_loop;
static int exit_code;
static const GOptionEntry entries[] = {
  { "output-document", 'o', 0,
    G_OPTION_ARG_FILENAME, &output_document,
    "write documents to FILE", "FILE" },
  { NULL }
};

static DexFuture *
wget (gpointer user_data)
{
  g_autoptr(GOutputStream) output = NULL;
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(SoupSession) session = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  gssize len;

  /* Create SoupSession used to manage connections */
  session = soup_session_new_with_options ("user-agent", "libdex-wget",
                                           "accept-language-auto", TRUE,
                                           "timeout", 15,
                                           NULL);

  /* Create SoupMessage to submit request and get response */
  if (!(message = soup_message_new ("GET", url)))
    return dex_future_new_reject (G_URI_ERROR,
                                  G_URI_ERROR_FAILED,
                                  "Failed to parse url \"%s\"", url);

  /* Suspend until we get a response or error */
  if (!(input = dex_await_object (session_send (session, message, PRIO), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* If output_document is not specified, use the URI to guess a name */
  if (output_document == NULL)
    {
      GUri *uri = soup_message_get_uri (message);
      const char *path = g_uri_get_path (uri);

      /* Try to get a reasonable name for the file */
      output_document = path ? g_path_get_basename (path) : g_strdup ("output");

      /* Avoid https://example.com/ having a filename of "/" */
      if (output_document == NULL || output_document[0] == '/')
        {
          g_free (output_document);
          output_document = g_strdup ("index.html");
        }
    }

  /* Suspend while the file at @output_document is created or replaced */
  file = g_file_new_for_path (output_document);
  if (!(output = dex_await_object (dex_file_replace (file, NULL, TRUE, G_FILE_CREATE_REPLACE_DESTINATION, PRIO), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Suspend while the input stream is spliced to our output stream */
  len = dex_await_int64 (dex_output_stream_splice (output, input, 0, PRIO), &error);
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Suspend while we wait for @output to close/sync */
  if (!dex_await_boolean (dex_output_stream_close (output, PRIO), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Return the length copied back */
  return dex_future_new_for_int64 (len);
}

static DexFuture *
fiber_completed (DexFuture *completed,
                 gpointer   value)
{
  GError *error = NULL;
  gssize len;

  /* We can "await" here because we know @completed is a resolved
   * or rejected future. Otherwise, dex_await() only works on a fiber
   * to be able to suspend execution until the result is ready.
   */
  len = dex_await_int64 (dex_ref (completed), &error);

  /* Give the user some information on success/failure */
  if (error != NULL)
    g_printerr ("error: %s\n", error->message);
  else
    g_printerr ("wrote %"G_GSSIZE_FORMAT" bytes to \"%s\".\n", len, output_document);

  /* Set exit code for program and exit main loop */
  exit_code = error != NULL ? EXIT_FAILURE : EXIT_SUCCESS;
  g_main_loop_quit (main_loop);
  return NULL;
}

int
main (int argc,
      char *argv[])
{
  GOptionContext *context;
  DexFuture *future;
  GError *error = NULL;

  dex_init ();

  context = g_option_context_new ("- a non-interactive network retriever");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return EXIT_FAILURE;
    }

  if (argc != 2)
    {
      g_printerr ("usage: %s [OPTIONS...] URL\n", argv[0]);
      return EXIT_FAILURE;
    }

  url = g_strdup (argv[1]);
  main_loop = g_main_loop_new (NULL, FALSE);

  /* Spawn a fiber */
  future = dex_scheduler_spawn (NULL, 0, wget, NULL, NULL);

  /* Handle resolve/reject of future and exit */
  future = dex_future_finally (future, fiber_completed, NULL, NULL);

  g_main_loop_run (main_loop);

  dex_unref (future);
  g_option_context_free (context);
  g_main_loop_unref (main_loop);
  g_free (output_document);
  g_free (url);

  return exit_code;
}

/* This is a GAsyncResult wrapper which converts it into a DexFuture.
 * DexAsyncPair can be used for simple pairs currently, but lacks support
 * for additional parameters. I hope to add that in the future though.
 */
static void
session_send_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GInputStream *stream;
  GError *error = NULL;

  stream = soup_session_send_finish (SOUP_SESSION (object), result, &error);

  if (error == NULL)
    dex_async_pair_return_object (async_pair, stream);
  else
    dex_async_pair_return_error (async_pair, error);

  dex_unref (async_pair);
}

static DexFuture *
session_send (SoupSession *session,
              SoupMessage *message,
              int          priority)
{
  DexAsyncPair *async_pair;

  g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);
  g_return_val_if_fail (SOUP_IS_MESSAGE (message), NULL);

  async_pair = (DexAsyncPair *)g_type_create_instance (DEX_TYPE_ASYNC_PAIR);
  soup_session_send_async (session, message, priority,
                           dex_async_pair_get_cancellable (async_pair),
                           session_send_cb,
                           dex_ref (async_pair));
  return DEX_FUTURE (async_pair);
}
