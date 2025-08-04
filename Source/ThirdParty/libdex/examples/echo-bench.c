/* echo-bench.c
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

typedef struct _Worker
{
  gint64 conn_attempts;
  gint64 conn_failures;
  gint64 conn_success;
  gint64 conn_closed;
  gint64 bytes_sent;
  gint64 bytes_received;
} Worker;

static DexScheduler *thread_pool;
static GSocketConnectable *socket_address;
static gchar *buf;
static gsize buflen;
static GPtrArray *fibers;
static Worker *workers;
static guint n_workers;
static gboolean in_shutdown;
static GTimer *timer;

static DexFuture *
worker_fiber (gpointer user_data)
{
  g_autoptr(GSocketClient) client = NULL;
  Worker *worker = user_data;
  g_autofree char *inbuf = g_malloc (buflen);

  client = g_socket_client_new ();

  while (!g_atomic_int_get (&in_shutdown))
    {
      g_autoptr(GSocketConnection) connection = NULL;
      GOutputStream *output;
      GInputStream *input;
      gssize len;

      worker->conn_attempts++;

      if (!(connection = dex_await_object (dex_socket_client_connect (client, socket_address), NULL)))
        {
          worker->conn_failures++;
          break;
        }

      worker->conn_success++;

      output = g_io_stream_get_output_stream (G_IO_STREAM (connection));
      input = g_io_stream_get_input_stream (G_IO_STREAM (connection));

      if ((len = dex_await_int64 (dex_output_stream_write (output, buf, buflen, G_PRIORITY_DEFAULT), NULL)) <= 0)
        break;
      worker->bytes_sent += len;

      if ((len = dex_await_int64 (dex_input_stream_read (input, inbuf, buflen, G_PRIORITY_DEFAULT), NULL)) <= 0)
        break;
      worker->bytes_received += len;

      dex_await (dex_io_stream_close (G_IO_STREAM (connection), G_PRIORITY_DEFAULT), NULL);
    }

  return NULL;
}

static DexFuture *
shutdown_cb (DexFuture *completed,
             gpointer   user_data)
{
  /* Signal to workers they should complete */
  g_atomic_int_set (&in_shutdown, TRUE);

  /* No need to wait for workers */
  return NULL;
}

static DexFuture *
quit_cb (DexFuture *completed,
         gpointer   user_data)
{
  GMainLoop *main_loop = user_data;
  g_main_loop_quit (main_loop);
  return NULL;
}

static gboolean
print_live_status (gpointer data)
{
  Worker total = {0};
  double duration = g_timer_elapsed (timer, NULL);
  g_autofree char *sent_per_sec = NULL;
  g_autofree char *recv_per_sec = NULL;

  for (guint i = 0; i < n_workers; i++)
    {
      total.conn_attempts += workers[i].conn_attempts;
      total.conn_failures += workers[i].conn_failures;
      total.conn_success += workers[i].conn_success;
      total.conn_closed += workers[i].conn_closed;
      total.bytes_sent += workers[i].bytes_sent;
      total.bytes_received += workers[i].bytes_received;
    }

  sent_per_sec = g_format_size (total.bytes_sent/duration);
  recv_per_sec = g_format_size (total.bytes_received/duration);

  g_printerr ("\n");
  g_printerr ("  req: succ=%"G_GINT64_FORMAT" (per-sec %0.2lf) fail=%"G_GINT64_FORMAT" (per-sec=%0.2lf)\n",
              total.conn_success, total.conn_success/duration,
              total.conn_failures, total.conn_failures/duration);
  g_printerr ("bytes: sent=%"G_GINT64_FORMAT" (per-sec %s) recv=%"G_GINT64_FORMAT" (per-sec %s)\n",
              total.bytes_sent, sent_per_sec,
              total.bytes_received, recv_per_sec);

  return G_SOURCE_CONTINUE;
}

static void
print_results (void)
{
  print_live_status (NULL);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *address = NULL;
  g_autofree char *message = NULL;
  int length = 0;
  int duration = 0;
  int number = 0;

  GOptionEntry entries[] = {
    { "address", 'a', 0, G_OPTION_ARG_STRING, &address, "Target echo server adderss.", "0.0.0.0:8080" },
    { "length", 'l', 0, G_OPTION_ARG_INT, &length, "Target message length.", "BYTES" },
    { "duration", 'd', 0, G_OPTION_ARG_INT, &duration, "Test duration in seconds.", "SECONDS" },
    { "number", 'c', 0, G_OPTION_ARG_INT, &number, "Number of concurrent connections.", "CONNECTIONS" },
    { "message", 'm', 0, G_OPTION_ARG_STRING, &message, "A custom message to send.", "MSG" },
    { NULL }
  };

  context = g_option_context_new ("- Simple echo benchmark client");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error) ||
      address == NULL ||
      !(socket_address = g_network_address_parse (address, 8080, &error)))
    {
      g_autofree char *help = g_option_context_get_help (context, TRUE, NULL);

      if (error != NULL)
        g_printerr ("%s\n\n", error->message);
      g_printerr ("%s\n", help);

      return EXIT_FAILURE;
    }

  /* Setup some defaults */
  if (duration <= 0)
    duration = 60;
  if (number <= 0)
    number = 1000;
  if (length <= 0)
    length = 512;

  /* Rewrite the address to show the user what we think we parsed */
  g_free (address);
  address = g_socket_connectable_to_string (socket_address);

  /* The message to send */
  if (message != NULL)
    {
      buf = g_steal_pointer (&message);
      buflen = strlen (buf);
      length = buflen;

      g_printerr ("Using custom message:\n\n"
                  "================================\n"
                  "%s\n"
                  "================================\n",
                  buf);
    }
  else
    {
      buflen = length;
      buf = g_strnfill (buflen, 'X');
    }

  g_printerr ("Benchmarking: %s\n", address);
  g_printerr ("%u clients, running %u bytes, %u sec.\n", number, length, duration);

  /* Space for the workers to track information */
  n_workers = number;
  workers = g_new0 (Worker, n_workers);

  /* Setup main loop for this thread and thread pool for the others
   * where we'll push fibers for clients.
   */
  main_loop = g_main_loop_new (NULL, FALSE);
  thread_pool = dex_thread_pool_scheduler_new ();
  timer = g_timer_new ();

  /* Hold a reference to the fibers so we can join them */
  fibers = g_ptr_array_new_with_free_func (dex_unref);
  for (int i = 0; i < number; i++)
    g_ptr_array_add (fibers,
                     dex_scheduler_spawn (thread_pool, 0, worker_fiber, &workers[i], NULL));

  /* After @duration seconds, reject */
  future = dex_timeout_new_seconds (duration);

  /* Handle that by shutting down somewhat gracefully */
  future = dex_future_finally (future, shutdown_cb, NULL, NULL);

  /* When that completes, quit the main loop */
  future = dex_future_finally (future, quit_cb, main_loop, NULL);

  /* Periodically print out worker status */
  g_timeout_add_seconds (1, print_live_status, NULL);

  g_main_loop_run (main_loop);

  print_results ();

  /* Cleanup state */
  g_object_unref (socket_address);
  g_ptr_array_unref (fibers);
  g_free (workers);
  g_free (buf);

  return EXIT_SUCCESS;
}
