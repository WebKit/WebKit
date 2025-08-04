/* cp.c
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
#include <unistd.h>

#define return_if_error(error) \
  G_STMT_START { \
    if (error) \
      return dex_future_new_for_error (error); \
  } G_STMT_END

static DexFuture *copy (gpointer user_data);

static DexScheduler *thread_pool;
static gboolean verbose;
static GMainLoop *main_loop;

typedef struct
{
  GFile *from;
  GFile *to;
  guint recursive : 1;
} Copy;

static Copy *
copy_new (GFile    *from,
          GFile    *to,
          gboolean  recursive)
{
  Copy *cp = g_new0 (Copy, 1);
  cp->from = from; /* take ownership */
  cp->to = to; /* take ownership */
  cp->recursive = !!recursive;
  return cp;
}

static void
copy_free (Copy *cp)
{
  g_clear_object (&cp->from);
  g_clear_object (&cp->to);
  g_free (cp);
}

static DexFuture *
copy_regular (Copy *cp)
{
  g_autoptr(GInputStream) input = NULL;
  g_autoptr(GOutputStream) output = NULL;
  GError *error = NULL;
  G_GNUC_UNUSED gssize len;

  if (verbose)
    g_printerr ("%s => %s\n", g_file_peek_path (cp->from), g_file_peek_path (cp->to));

  input = dex_await_object (dex_file_read (cp->from, G_PRIORITY_DEFAULT), &error);
  return_if_error (error);

  output = dex_await_object (dex_file_replace (cp->to, NULL, FALSE,
                                               G_FILE_CREATE_REPLACE_DESTINATION,
                                               G_PRIORITY_DEFAULT),
                             &error);
  return_if_error (error);

  len = dex_await_int64 (dex_output_stream_splice (output, input,
                                                   (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                                    G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                                   G_PRIORITY_DEFAULT),
                         &error);
  return_if_error (error);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
copy_directory (Copy *cp)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) futures = g_ptr_array_new_with_free_func (dex_unref);
  GError *error = NULL;

  enumerator = dex_await_object (dex_file_enumerate_children (cp->from,
                                                              G_FILE_ATTRIBUTE_STANDARD_NAME",",
                                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                              G_PRIORITY_DEFAULT),
                                 &error);
  return_if_error (error);

  if (verbose)
    g_printerr ("%s/ => %s/\n", g_file_peek_path (cp->from), g_file_peek_path (cp->to));

  dex_await (dex_file_make_directory (cp->to, G_PRIORITY_DEFAULT), &error);
  return_if_error (error);

  for (;;)
    {
      g_autolist(GFileInfo) files = NULL;

      files = dex_await_boxed (dex_file_enumerator_next_files (enumerator, 100, G_PRIORITY_DEFAULT),
                               &error);
      return_if_error (error);

      if (files == NULL)
        break;

      for (const GList *iter = files; iter; iter = iter->next)
        {
          GFileInfo *info = iter->data;

          g_ptr_array_add (futures,
                           dex_scheduler_spawn (thread_pool,
                                                0,
                                                copy,
                                                copy_new (g_file_enumerator_get_child (enumerator, info),
                                                          g_file_get_child (cp->to, g_file_info_get_name (info)),
                                                          cp->recursive),
                                                (GDestroyNotify) copy_free));
        }
    }

  if (futures->len > 0)
    {
      dex_await (dex_future_allv ((DexFuture **)(gpointer)futures->pdata, futures->len), &error);
      return_if_error (error);
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
copy_fallback (Copy *cp)
{
  if (verbose)
    g_print ("%s => %s\n", g_file_peek_path (cp->from), g_file_peek_path (cp->to));

  /* Fallback to internal GIO copying semantics, which can't handle
   * directories or things of that nature.
   *
   * This returns a future, which the fiber scheduler will yield for
   * us as part of fiber completion.
   */
 return dex_file_copy (cp->from,
                       cp->to,
                       G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA,
                       G_PRIORITY_DEFAULT);
}

static DexFuture *
copy (gpointer user_data)
{
  Copy *cp = user_data;
  g_autoptr(GFileInfo) info = NULL;
  GFileType file_type;
  GError *error = NULL;

  info = dex_await_object (dex_file_query_info (cp->from,
                                                G_FILE_ATTRIBUTE_STANDARD_SIZE","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                                G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET",",
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                G_PRIORITY_DEFAULT),
                           &error);
  return_if_error (error);

  file_type = g_file_info_get_file_type (info);

  if (file_type == G_FILE_TYPE_REGULAR)
    return copy_regular (cp);

  if (file_type == G_FILE_TYPE_DIRECTORY)
    {
      if (!cp->recursive)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "%s is a directory and -r is not set",
                                      g_file_peek_path (cp->from));
      return copy_directory (cp);
    }

  return copy_fallback (cp);
}

static DexFuture *
quit_cb (DexFuture *future,
         gpointer   user_data)
{
  GError *error = NULL;

  if (!dex_await (dex_ref (future), &error))
    g_error ("%s", error->message);

  g_main_loop_quit (main_loop);

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  gboolean recursive = FALSE;
  GOptionEntry entries[] = {
    { "recursive", 'r', 0, G_OPTION_ARG_NONE, &recursive, "Copy directory recursively" },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Explain what is being done" },
    { NULL }
  };
  GOptionContext *context;
  DexFuture *future;
  GError *error = NULL;

  dex_init ();

  context = g_option_context_new ("[OPTIONS...] SOURCE DEST - copy files");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("%s", error->message);

  if (argc != 3)
    {
      g_printerr ("usage: %s SOURCE DEST\n", argv[0]);
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);
  thread_pool = dex_thread_pool_scheduler_new ();
  future = dex_scheduler_spawn (NULL,
                                0,
                                copy,
                                copy_new (g_file_new_for_commandline_arg (argv[1]),
                                          g_file_new_for_commandline_arg (argv[2]),
                                          recursive),
                                (GDestroyNotify)copy_free);
  future = dex_future_finally (future, quit_cb, NULL, NULL);

  g_main_loop_run (main_loop);

  dex_unref (future);
  g_option_context_free (context);
  g_main_loop_unref (main_loop);

  return EXIT_SUCCESS;
}
