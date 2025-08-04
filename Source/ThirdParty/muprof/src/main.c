/* main.c
 *
 * Copyright 2025 Igalia S.L.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <fcntl.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <libdex.h>
#include <stdlib.h>
#include <sysprof-capture.h>

#include "sysprof-diagnostic.h"
#include "sysprof-profiler.h"

#define DEFAULT_BUFFER_SIZE (1024L * 1024L * 8L /* 8mb */)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SysprofCaptureWriter, sysprof_capture_writer_unref)

static SysprofRecording *active_recording = NULL;

static gboolean
sigint_handler (gpointer user_data)
{
  GMainLoop *main_loop = (GMainLoop *) user_data;
  static int count = 0;

  if (count >= 2)
    {
      g_main_loop_quit (main_loop);
      return G_SOURCE_REMOVE;
    }

  g_printerr ("\n");

  if (count == 0)
    {
      g_printerr ("%s\n", "Stopping profiler. Press twice more ^C to force exit.");
      sysprof_recording_stop_async (active_recording, NULL, NULL, NULL);
    }

  count++;

  return G_SOURCE_CONTINUE;
}

static void
diagnostics_items_changed_cb (GListModel *model,
                              guint       position,
                              guint       removed,
                              guint       added,
                              gpointer    user_data)
{
  g_assert (G_IS_LIST_MODEL (model));
  g_assert (user_data == NULL);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(SysprofDiagnostic) diagnostic = g_list_model_get_item (model, position+i);

      g_printerr ("%s: %s\n",
                  sysprof_diagnostic_get_domain (diagnostic),
                  sysprof_diagnostic_get_message (diagnostic));
    }
}

static void
sysprof_cli_wait_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  SysprofRecording *recording = (SysprofRecording *)object;
  g_autoptr(GError) error = NULL;
  GMainLoop *main_loop = (GMainLoop *) user_data;

  g_assert (SYSPROF_IS_RECORDING (recording));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!sysprof_recording_wait_finish (recording, result, &error))
    {
      if (error->domain != G_SPAWN_EXIT_ERROR)
        g_printerr ("Failed to complete recording: %s\n",
                    error->message);
    }

  g_main_loop_quit (main_loop);
}

static void
sysprof_cli_record_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  SysprofProfiler *profiler = (SysprofProfiler *)object;
  GMainLoop *main_loop = (GMainLoop *) user_data;
  g_autoptr(SysprofRecording) recording = NULL;
  g_autoptr(GListModel) diagnostics = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (SYSPROF_IS_PROFILER (profiler));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(recording = sysprof_profiler_record_finish (profiler, result, &error)))
    g_error ("Failed to start profiling session: %s", error->message);

  diagnostics = sysprof_recording_list_diagnostics (recording);
  g_signal_connect (diagnostics,
                    "items-changed",
                    G_CALLBACK (diagnostics_items_changed_cb),
                    NULL);
  diagnostics_items_changed_cb (diagnostics,
                                0,
                                0,
                                g_list_model_get_n_items (diagnostics),
                                NULL);

  sysprof_recording_wait_async (recording,
                                NULL,
                                sysprof_cli_wait_cb,
                                main_loop);

  g_set_object (&active_recording, recording);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(SysprofCaptureWriter) writer = NULL;
  g_autoptr(SysprofProfiler) profiler = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) child_argv = NULL;
  g_auto(GStrv) envs = NULL;
  GMainContext *main_context;
  const char *filename = "capture.syscap";
  gboolean version = FALSE;
  gboolean force = FALSE;
  int flags;
  int fd;
  int n_buffer_pages = (DEFAULT_BUFFER_SIZE / sysprof_getpagesize ());
  GOptionEntry main_entries[] = {
    { "env", 'e', 0, G_OPTION_ARG_STRING_ARRAY, &envs, "Set environment variable for spawned process. Can be used multiple times.", "VAR=VALUE" },
    { "force", 'f', 0, G_OPTION_ARG_NONE, &force, "Force overwrite the capture file" },
    { "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show program version" },
    { NULL }
  };

  dex_init ();
  sysprof_clock_init ();

  /* Before we start processing argv, look for "--" and take everything after
   * it as arguments as a "-c" replacement.
   */
  for (guint i = 1; i < argc; i++)
    {
      if (g_str_equal (argv[i], "--"))
        {
          GPtrArray *ar = g_ptr_array_new ();
          for (guint j = i + 1; j < argc; j++)
            g_ptr_array_add (ar, g_strdup (argv[j]));
          g_ptr_array_add (ar, NULL);
          child_argv = (char **)g_ptr_array_free (ar, FALSE);
          /* Skip everything from -- beyond */
          argc = i;
          break;
        }
    }

  if (child_argv == NULL)
    {
      g_printerr ("No child command passed");
      return EXIT_FAILURE;
    }

  context = g_option_context_new ("[CAPTURE_FILE] -- COMMAND ARGS — Sysprof-based microprofiler");
  g_option_context_add_main_entries (context, main_entries, NULL);

  g_option_context_set_summary (context, "\n\
Example:\n\
\n\
  muprof capture.syscap -- gtk4-widget-factory\n\
");

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  if (version)
    {
      g_printerr ("%s\n", PACKAGE_VERSION);
      return EXIT_SUCCESS;
    }

  if (argc > 2)
    {
      g_printerr ("Too many arguments were passed to muprof:");

      for (int i = 2; i < argc; i++)
        g_printerr (" %s", argv [i]);
      g_printerr ("\n");

      return EXIT_FAILURE;
    }
  main_loop = g_main_loop_new (NULL, FALSE);
  profiler = sysprof_profiler_new ();

  if (argc == 2)
    filename = argv[1];

  flags = O_CREAT | O_RDWR | O_CLOEXEC;
  if (!force)
    {
      /* if we don't force overwrite we want to ensure the file doesn't exist
       * and never overwrite it. O_EXCL will prevent opening in that case */
      flags |= O_EXCL;
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
        {
          /* Translators: %s is a file name. */
          g_printerr ("%s exists. Use --force to overwrite\n", filename);
          return EXIT_FAILURE;
        }
    }

  fd = g_open (filename, flags, 0640);
  if (fd == -1)
    {
      g_printerr ("Failed to open %s\n", filename);
      return EXIT_FAILURE;
    }

  writer = sysprof_capture_writer_new_from_fd (fd, n_buffer_pages * sysprof_getpagesize ());

  // Setup spawnable and environment
  {
    g_autoptr(SysprofSpawnable) spawnable = sysprof_spawnable_new ();
    g_auto(GStrv) current_env = g_get_environ ();

    sysprof_spawnable_set_cwd (spawnable, g_get_current_dir ());
    sysprof_spawnable_append_args (spawnable, (const char * const *)child_argv);
    sysprof_spawnable_set_environ (spawnable, (const char * const *)current_env);

    if (envs != NULL)
      {
        for (guint i = 0; envs[i]; i++)
          {
            g_autofree char *key = NULL;
            const char *eq;

            if (!(eq = strchr (envs[i], '=')))
              {
                sysprof_spawnable_setenv (spawnable, envs[i], "");
                continue;
              }

            key = g_strndup (envs[i], eq - envs[i]);
            sysprof_spawnable_setenv (spawnable, key, eq+1);
          }
      }

    sysprof_profiler_set_spawnable (profiler, spawnable);
  }

  sysprof_profiler_record_async (profiler,
                                 writer,
                                 NULL,
                                 sysprof_cli_record_cb,
                                 main_loop);

  g_unix_signal_add (SIGINT, sigint_handler, main_loop);
  g_unix_signal_add (SIGTERM, sigint_handler, main_loop);

  g_printerr ("Recording, press ^C to exit\n");

  g_main_loop_run (main_loop);

  g_printerr ("Saving capture to %s... ", filename);

  sysprof_capture_writer_flush (writer);

  main_context = g_main_loop_get_context (main_loop);
  while (g_main_context_pending (main_context))
    g_main_context_iteration (main_context, FALSE);

  sysprof_capture_writer_flush (writer);

  g_printerr ("done!\n");

  return EXIT_SUCCESS;
}
